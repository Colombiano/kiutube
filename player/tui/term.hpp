// PT: Terminal layer própria em C++ (decisão arquitetural 3): raw mode RAII
//     sobre termios, tabela de escapes compatível com termbox2 (MIT, levantada
//     de src/ui/termbox2_input.h) e input como corrotina awaitable — sem glib,
//     sem GIOChannel. Substitui o termbox2 no caminho do produto.
// EN: Own C++ terminal layer (architectural decision 3): RAII raw mode over
//     termios, escape table compatible with termbox2 (MIT, lifted from
//     src/ui/termbox2_input.h) and input as an awaitable coroutine — no glib,
//     no GIOChannel. Replaces termbox2 in the product path.
// ES: Terminal layer propia en C++ (decisión arquitectónica 3): raw mode RAII
//     sobre termios, tabla de escapes compatible con termbox2 (MIT, levantada
//     de src/ui/termbox2_input.h) y input como corrutina awaitable — sin glib,
//     sin GIOChannel. Reemplaza al termbox2 en la ruta del producto.
// ZH: 自研 C++ 终端层（架构决策 3）：基于 termios 的 RAII raw mode、与 termbox2
//     兼容的转义表（MIT，提取自 src/ui/termbox2_input.h），以及以 awaitable
//     协程表示的输入 —— 不使用 glib/GIOChannel。在产品路径上替代 termbox2。
#pragma once

#include "krt/task.hpp"

#include <cstdint>
#include <string>
#include <termios.h>
#include <vector>

namespace tui
{

// PT: Modificadores bitmask. / EN: Modifiers bitmask.
// ES: Modificadores bitmask. / ZH: 修饰键位掩码。
enum Mod : std::uint8_t
{
        MOD_NONE = 0,
        MOD_SHIFT = 1u << 0,
        MOD_CTRL = 1u << 1,
        MOD_ALT = 1u << 2
};

// PT: Teclas especiais; caracteres UTF-8 chegam como Key::Char com `ch`.
// EN: Special keys; UTF-8 characters arrive as Key::Char with `ch`.
// ES: Teclas especiales; caracteres UTF-8 llegan como Key::Char con `ch`.
// ZH: 特殊键；UTF-8 字符以 Key::Char 加 `ch` 表示。
enum class Key : std::uint16_t
{
        Char,
        Enter,
        Tab,
        Backspace,
        Esc,
        Up,
        Down,
        Left,
        Right,
        Home,
        End,
        Insert,
        Delete,
        PageUp,
        PageDown,
        F1,
        F2,
        F3,
        F4,
        F5,
        F6,
        F7,
        F8,
        F9,
        F10,
        F11,
        F12
};

struct KeyEvent
{
        Key key = Key::Char;
        std::uint32_t ch = 0;  // codepoint quando key==Char / when key==Char / cuando key==Char / key==Char 时的码点
        std::uint8_t mods = MOD_NONE;
};

class Term
{
      public:
        // PT: Entra em raw mode + alternate screen; lança se stdin não for tty.
        // EN: Enters raw mode + alternate screen; throws if stdin is not a tty.
        // ES: Entra en raw mode + alternate screen; lanza si stdin no es tty.
        // ZH: 进入 raw mode + alternate screen；若 stdin 不是 tty 则抛异常。
        Term();
        // PT: Restaura termios e sai do alternate screen. / EN: Restores termios and leaves alternate screen.
        // ES: Restaura termios y sale del alternate screen. / ZH: 恢复 termios 并退出 alternate screen。
        ~Term();

        Term(const Term &) = delete;
        Term &operator=(const Term &) = delete;

        // PT: Próximo evento de teclado (só termina com o scheduler rodando).
        // EN: Next keyboard event (only completes with the scheduler running).
        // ES: Próximo evento de teclado (solo termina con el scheduler corriendo).
        // ZH: 下一个键盘事件（仅在调度器运行时才会完成）。
        [[nodiscard]] krt::Task<KeyEvent> next_event();

        // PT: Envia segmentos do CellBuffer num único writev. / EN: Sends CellBuffer segments in a single writev.
        // ES: Envía segmentos del CellBuffer en un único writev. / ZH: 以单次 writev 发送 CellBuffer 的段。
        void write_segments(const std::vector<std::string> &segments) const;

        [[nodiscard]] int rows() const
        {
                return rows_;
        }
        [[nodiscard]] int cols() const
        {
                return cols_;
        }

      private:
        [[nodiscard]] bool parse_pending(KeyEvent &out); // consome 1 evento de pending_ ou false se incompleto

        int fd_ = 0;
        termios saved_{};
        bool raw_ = false;
        int rows_ = 24;
        int cols_ = 80;
        std::string pending_; // bytes lidos ainda não consumidos / unconsumed read bytes / bytes leídos no consumidos / 已读未消费的字节
};

} // namespace tui
