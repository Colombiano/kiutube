// PT: Implementação de tui::Term — parsing de escapes compatível com
//     termbox2 (tabela constexpr extraída de src/ui/termbox2_input.h, MIT).
// EN: tui::Term implementation — escape parsing compatible with termbox2
//     (constexpr table extracted from src/ui/termbox2_input.h, MIT).
// ES: Implementación de tui::Term — parsing de escapes compatible con
//     termbox2 (tabla constexpr extraída de src/ui/termbox2_input.h, MIT).
// ZH: tui::Term 实现 —— 与 termbox2 兼容的转义解析（constexpr 表提取自
//     src/ui/termbox2_input.h，MIT）。
#include "tui/term.hpp"

#include "krt/scheduler.hpp"

#include <sys/ioctl.h>
#include <sys/uio.h>
#include <unistd.h>

#include <optional>
#include <poll.h>
#include <stdexcept>

namespace tui
{
namespace
{

// PT: Entrada da tabela: sequência de escape -> tecla + modificadores.
// EN: Table entry: escape sequence -> key + modifiers.
// ES: Entrada de la tabla: secuencia de escape -> tecla + modificadores.
// ZH: 表项：转义序列 -> 键 + 修饰键。
struct SeqEntry
{
        const char *seq;
        Key key;
        std::uint8_t mods;
};

// PT: Tabela levantada do termbox2 (semântica xterm): base + setas com
//     modificadores 1;N{A,B,C,D,H,F}. Extensões (kitty etc.) ficam para o M4.
// EN: Table lifted from termbox2 (xterm semantics): base + arrows with
//     modifiers 1;N{A,B,C,D,H,F}. Extensions (kitty etc.) land in M4.
// ES: Tabla levantada del termbox2 (semántica xterm): base + flechas con
//     modificadores 1;N{A,B,C,D,H,F}. Extensiones (kitty etc.) van al M4.
// ZH: 提取自 termbox2 的表（xterm 语义）：基础序列 + 1;N{A,B,C,D,H,F} 修饰方向键。
//     扩展（kitty 等）留到 M4。
constexpr SeqEntry kSeqs[] = {
    {"\x1b[A", Key::Up, MOD_NONE},     {"\x1b[B", Key::Down, MOD_NONE},
    {"\x1b[C", Key::Right, MOD_NONE},  {"\x1b[D", Key::Left, MOD_NONE},
    {"\x1b[H", Key::Home, MOD_NONE},   {"\x1b[F", Key::End, MOD_NONE},
    {"\x1b[Z", Key::Tab, MOD_SHIFT},

    {"\x1b[1~", Key::Home, MOD_NONE},     {"\x1b[2~", Key::Insert, MOD_NONE},
    {"\x1b[3~", Key::Delete, MOD_NONE},   {"\x1b[4~", Key::End, MOD_NONE},
    {"\x1b[5~", Key::PageUp, MOD_NONE},   {"\x1b[6~", Key::PageDown, MOD_NONE},
    {"\x1b[7~", Key::Home, MOD_NONE},     {"\x1b[8~", Key::End, MOD_NONE},

    {"\x1b[11~", Key::F1, MOD_NONE},  {"\x1b[12~", Key::F2, MOD_NONE},
    {"\x1b[13~", Key::F3, MOD_NONE},  {"\x1b[14~", Key::F4, MOD_NONE},
    {"\x1b[15~", Key::F5, MOD_NONE},  {"\x1b[17~", Key::F6, MOD_NONE},
    {"\x1b[18~", Key::F7, MOD_NONE},  {"\x1b[19~", Key::F8, MOD_NONE},
    {"\x1b[20~", Key::F9, MOD_NONE},  {"\x1b[21~", Key::F10, MOD_NONE},
    {"\x1b[23~", Key::F11, MOD_NONE}, {"\x1b[24~", Key::F12, MOD_NONE},

    {"\x1bOP", Key::F1, MOD_NONE}, {"\x1bOQ", Key::F2, MOD_NONE},
    {"\x1bOR", Key::F3, MOD_NONE}, {"\x1bOS", Key::F4, MOD_NONE},

    // PT: setas com modificadores (2=Shift 3=Alt 4=S+A 5=Ctrl 6=S+C 7=A+C 8=todos).
    // EN: arrows with modifiers (2=Shift 3=Alt 4=S+A 5=Ctrl 6=S+C 7=A+C 8=all).
    // ES: flechas con modificadores (2=Shift 3=Alt 4=S+A 5=Ctrl 6=S+C 7=A+C 8=todos).
    // ZH: 带修饰键的方向键（2=Shift 3=Alt 4=S+A 5=Ctrl 6=S+C 7=A+C 8=全部）。
    {"\x1b[1;2A", Key::Up, MOD_SHIFT},          {"\x1b[1;3A", Key::Up, MOD_ALT},
    {"\x1b[1;4A", Key::Up, MOD_SHIFT | MOD_ALT}, {"\x1b[1;5A", Key::Up, MOD_CTRL},
    {"\x1b[1;6A", Key::Up, MOD_SHIFT | MOD_CTRL}, {"\x1b[1;7A", Key::Up, MOD_ALT | MOD_CTRL},
    {"\x1b[1;8A", Key::Up, MOD_SHIFT | MOD_ALT | MOD_CTRL},
    {"\x1b[1;2B", Key::Down, MOD_SHIFT},          {"\x1b[1;3B", Key::Down, MOD_ALT},
    {"\x1b[1;4B", Key::Down, MOD_SHIFT | MOD_ALT}, {"\x1b[1;5B", Key::Down, MOD_CTRL},
    {"\x1b[1;6B", Key::Down, MOD_SHIFT | MOD_CTRL}, {"\x1b[1;7B", Key::Down, MOD_ALT | MOD_CTRL},
    {"\x1b[1;8B", Key::Down, MOD_SHIFT | MOD_ALT | MOD_CTRL},
    {"\x1b[1;2C", Key::Right, MOD_SHIFT},          {"\x1b[1;3C", Key::Right, MOD_ALT},
    {"\x1b[1;4C", Key::Right, MOD_SHIFT | MOD_ALT}, {"\x1b[1;5C", Key::Right, MOD_CTRL},
    {"\x1b[1;6C", Key::Right, MOD_SHIFT | MOD_CTRL}, {"\x1b[1;7C", Key::Right, MOD_ALT | MOD_CTRL},
    {"\x1b[1;8C", Key::Right, MOD_SHIFT | MOD_ALT | MOD_CTRL},
    {"\x1b[1;2D", Key::Left, MOD_SHIFT},          {"\x1b[1;3D", Key::Left, MOD_ALT},
    {"\x1b[1;4D", Key::Left, MOD_SHIFT | MOD_ALT}, {"\x1b[1;5D", Key::Left, MOD_CTRL},
    {"\x1b[1;6D", Key::Left, MOD_SHIFT | MOD_CTRL}, {"\x1b[1;7D", Key::Left, MOD_ALT | MOD_CTRL},
    {"\x1b[1;8D", Key::Left, MOD_SHIFT | MOD_ALT | MOD_CTRL},
    {"\x1b[1;2H", Key::Home, MOD_SHIFT},          {"\x1b[1;3H", Key::Home, MOD_ALT},
    {"\x1b[1;5H", Key::Home, MOD_CTRL},
    {"\x1b[1;2F", Key::End, MOD_SHIFT},           {"\x1b[1;3F", Key::End, MOD_ALT},
    {"\x1b[1;5F", Key::End, MOD_CTRL},
};

// PT: Decodifica 1 codepoint UTF-8; retorna bytes consumidos (0 = inválido/incompleto).
// EN: Decodes 1 UTF-8 codepoint; returns bytes consumed (0 = invalid/incomplete).
// ES: Decodifica 1 codepoint UTF-8; devuelve bytes consumidos (0 = inválido/incompleto).
// ZH: 解码 1 个 UTF-8 码点；返回消费的字节数（0 = 无效/不完整）。
std::size_t utf8_decode(std::string_view in, std::uint32_t &cp)
{
        if (in.empty())
                return 0;
        const auto b0 = static_cast<unsigned char>(in[0]);
        if (b0 < 0x80)
        {
                cp = b0;
                return 1;
        }
        int len = 0;
        std::uint32_t acc = 0;
        if ((b0 & 0xe0) == 0xc0)
        {
                len = 2;
                acc = b0 & 0x1f;
        }
        else if ((b0 & 0xf0) == 0xe0)
        {
                len = 3;
                acc = b0 & 0x0f;
        }
        else if ((b0 & 0xf8) == 0xf0)
        {
                len = 4;
                acc = b0 & 0x07;
        }
        else
                return 0;
        if (in.size() < static_cast<std::size_t>(len))
                return 0;
        for (int i = 1; i < len; ++i)
        {
                const auto b = static_cast<unsigned char>(in[i]);
                if ((b & 0xc0) != 0x80)
                        return 0;
                acc = (acc << 6) | (b & 0x3f);
        }
        cp = acc;
        return static_cast<std::size_t>(len);
}

} // namespace

Term::Term()
{
        // PT: Exige tty: sem terminal real (ou PTY de teste) não há TUI.
        // EN: Requires a tty: no real terminal (or test PTY), no TUI.
        // ES: Exige tty: sin terminal real (o PTY de prueba), no hay TUI.
        // ZH: 要求 tty：没有真实终端（或测试 PTY）就没有 TUI。
        if (!isatty(fd_))
                throw std::runtime_error("tui::Term: stdin nao e um tty / not a tty / no es tty / 不是 tty");

        if (tcgetattr(fd_, &saved_) < 0)
                throw std::runtime_error("tui::Term: tcgetattr falhou / failed / fallo / 失败");

        termios raw = saved_;
        cfmakeraw(&raw);
        if (tcsetattr(fd_, TCSAFLUSH, &raw) < 0)
                throw std::runtime_error("tui::Term: tcsetattr falhou / failed / fallo / 失败");
        raw_ = true;

        winsize ws{};
        if (ioctl(fd_, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0 && ws.ws_col > 0)
        {
                rows_ = ws.ws_row;
                cols_ = ws.ws_col;
        }

        // PT: alternate screen + esconde cursor; dtor reverte. / EN: alternate screen + hide cursor; dtor reverts.
        // ES: alternate screen + oculta cursor; dtor revierte. / ZH: 进入 alternate screen 并隐藏光标；dtor 还原。
        const char init[] = "\x1b[?1049h\x1b[?25l";
        if (write(STDOUT_FILENO, init, sizeof(init) - 1) < 0)
        {
        }
}

Term::~Term()
{
        const char fini[] = "\x1b[?25h\x1b[?1049l";
        if (write(STDOUT_FILENO, fini, sizeof(fini) - 1) < 0)
        {
        }
        if (raw_)
                tcsetattr(fd_, TCSAFLUSH, &saved_);
}

void Term::write_segments(const std::vector<std::string> &segments) const
{
        // PT: Um único writev: poucas syscalls por frame, mesmo com muitas linhas sujas.
        // EN: A single writev: few syscalls per frame even with many dirty lines.
        // ES: Un único writev: pocas syscalls por frame aunque haya muchas líneas sucias.
        // ZH: 单次 writev：即使脏行很多，每帧的系统调用也很少。
        iovec iov[64];
        std::size_t n = 0;
        for (const auto &seg : segments)
        {
                if (n >= 64)
                        break;
                iov[n].iov_base = const_cast<char *>(seg.data());
                iov[n].iov_len = seg.size();
                ++n;
        }
        if (n > 0)
                writev(STDOUT_FILENO, iov, static_cast<int>(n));
}

bool Term::parse_pending(KeyEvent &out)
{
        if (pending_.empty())
                return false;

        const auto b0 = static_cast<unsigned char>(pending_[0]);

        // PT: ESC: decide entre Esc sozinho e início de sequência CSI/SS3.
        // EN: ESC: decides between lone Esc and start of a CSI/SS3 sequence.
        // ES: ESC: decide entre Esc solo e inicio de secuencia CSI/SS3.
        // ZH: ESC：区分单独的 Esc 与 CSI/SS3 序列的开始。
        if (b0 == 0x1b)
        {
                if (pending_.size() == 1)
                {
                        // PT: Só ESC na mão: se não há mais bytes prontos, é Esc.
                        // EN: Only ESC in hand: if no more bytes ready, it is Esc.
                        // ES: Solo ESC en mano: si no hay más bytes listos, es Esc.
                        // ZH: 手头只有 ESC：若没有更多就绪字节，即为 Esc。
                        pollfd pfd{fd_, POLLIN, 0};
                        if (poll(&pfd, 1, 0) <= 0)
                        {
                                pending_.clear();
                                out = KeyEvent{Key::Esc, 0, MOD_NONE};
                                return true;
                        }
                        return false; // PT: há bytes a caminho; lê e re-tenta. EN: bytes on the way; read and retry.
                                     // ES: hay bytes en camino; lee y reintenta. ZH: 尚有字节在途；读取后重试。
                }
                const char second = pending_[1];
                if (second != '[' && second != 'O')
                {
                        // PT: ALT+char: M0 trata como Esc e mantém o resto.
                        // EN: ALT+char: M0 treats it as Esc and keeps the rest.
                        // ES: ALT+char: M0 lo trata como Esc y mantiene el resto.
                        // ZH: ALT+字符：M0 视为 Esc 并保留其余字节。
                        pending_.erase(0, 1);
                        out = KeyEvent{Key::Esc, 0, MOD_ALT};
                        return true;
                }
                // PT: Casa a sequência mais longa possível contra a tabela.
                // EN: Match the longest possible sequence against the table.
                // ES: Casar la secuencia más larga posible contra la tabla.
                // ZH: 与表进行最长匹配。
                std::size_t best = 0;
                const SeqEntry *hit = nullptr;
                bool prefix_of_something = false;
                for (const auto &e : kSeqs)
                {
                        const std::string_view seq(e.seq);
                        if (pending_.compare(0, seq.size(), seq) == 0)
                        {
                                if (seq.size() > best)
                                {
                                        best = seq.size();
                                        hit = &e;
                                }
                        }
                        else if (seq.compare(0, pending_.size(), pending_) == 0)
                        {
                                prefix_of_something = true;
                        }
                }
                if (hit)
                {
                        pending_.erase(0, best);
                        out = KeyEvent{hit->key, 0, hit->mods};
                        return true;
                }
                if (prefix_of_something)
                        // PT: sequência incompleta: lê mais bytes. EN: incomplete sequence: read more bytes.
                        // ES: secuencia incompleta: lee más bytes. ZH: 序列不完整：读取更多字节。
                        return false;
                // PT: CSI desconhecido: descarta o ESC e segue (não trava o input).
                // EN: Unknown CSI: drop the ESC and move on (never blocks input).
                // ES: CSI desconocido: descarta el ESC y sigue (no bloquea el input).
                // ZH: 未知 CSI：丢弃 ESC 继续（绝不阻塞输入）。
                pending_.erase(0, 1);
                out = KeyEvent{Key::Esc, 0, MOD_NONE};
                return true;
        }

        // PT: Enter/Backspace/Tab e Ctrl+letra (0x01-0x1a exceto os acima).
        // EN: Enter/Backspace/Tab and Ctrl+letter (0x01-0x1a except the above).
        // ES: Enter/Backspace/Tab y Ctrl+letra (0x01-0x1a excepto los anteriores).
        // ZH: Enter/Backspace/Tab 以及 Ctrl+字母（0x01-0x1a，除外上述）。
        if (b0 == 0x0d)
        {
                pending_.erase(0, 1);
                out = KeyEvent{Key::Enter, 0, MOD_NONE};
                return true;
        }
        if (b0 == 0x7f || b0 == 0x08)
        {
                pending_.erase(0, 1);
                out = KeyEvent{Key::Backspace, 0, MOD_NONE};
                return true;
        }
        if (b0 == 0x09)
        {
                pending_.erase(0, 1);
                out = KeyEvent{Key::Tab, 0, MOD_NONE};
                return true;
        }
        if (b0 < 0x20)
        {
                pending_.erase(0, 1);
                out = KeyEvent{Key::Char, static_cast<std::uint32_t>('a' + b0 - 1), MOD_CTRL};
                return true;
        }

        // PT: Caractere UTF-8 (pode precisar de mais bytes). / EN: UTF-8 character (may need more bytes).
        // ES: Carácter UTF-8 (puede necesitar más bytes). / ZH: UTF-8 字符（可能需要更多字节）。
        std::uint32_t cp = 0;
        const std::size_t used = utf8_decode(pending_, cp);
        if (used == 0)
        {
                if (pending_.size() >= 4)
                        pending_.erase(0, 1); // byte inválido: descarta / invalid byte: drop / byte inválido: descarta / 无效字节：丢弃
                return false;
        }
        pending_.erase(0, used);
        out = KeyEvent{Key::Char, cp, MOD_NONE};
        return true;
}

krt::Task<KeyEvent> Term::next_event()
{
        // PT: Loop: consome do buffer pending_; se vazio/incompleto, co_await fd.
        // EN: Loop: consume from the pending buffer; if empty/incomplete, co_await fd.
        // ES: Bucle: consume del buffer pending; si vacío/incompleto, co_await fd.
        // ZH: 循环：从 pending 缓冲区消费；若为空/不完整，则 co_await fd。
        for (;;)
        {
                KeyEvent ev{};
                if (parse_pending(ev))
                        co_return ev;
                co_await krt::fd_readable(fd_);
                char buf[128];
                const ssize_t n = read(fd_, buf, sizeof(buf));
                if (n <= 0)
                        co_return KeyEvent{Key::Esc, 0, MOD_NONE}; // EOF/erro: sinaliza como Esc / EOF/error: signals as Esc / EOF/erro: señaliza como Esc / EOF/错误：以 Esc 表示
                pending_.append(buf, static_cast<std::size_t>(n));
        }
}

} // namespace tui
