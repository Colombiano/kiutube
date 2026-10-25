// PT: kiutube — marco M3b/M3c: biblioteca com busca fuzzy (métrica do
//     Typesense) via picker na TUI + letras do LRCLIB com view própria.
//     'l' abre a biblioteca (digite para filtrar, Enter toca, ESC fecha);
//     'g' busca a letra da faixa atual (rede em offload, nunca bloqueia).
// EN: kiutube — milestone M3b/M3c: library with Typesense-metric fuzzy
//     search through an in-TUI picker + LRCLIB lyrics with their own view.
//     'l' opens the library (type to filter, Enter plays, ESC closes); 'g'
//     fetches lyrics for the current track (network offloaded, never blocks).
// ES: kiutube — hito M3b/M3c: biblioteca con búsqueda fuzzy (métrica de
//     Typesense) vía picker en la TUI + letras de LRCLIB con vista propia.
//     'l' abre la biblioteca (teclea para filtrar, Enter reproduce, ESC
//     cierra); 'g' busca la letra de la pista actual (red en offload).
// ZH: kiutube —— 里程碑 M3b/M3c：带模糊搜索（Typesense 度量）的曲库
//     TUI 选择器 + LRCLIB 歌词视图。'l' 打开曲库（输入即过滤，Enter 播放，
//     ESC 关闭）；'g' 获取当前曲目歌词（网络经 offload，绝不阻塞）。
#include "app/bindings.hpp"
#include "app/man.hpp"
#include "app/paths.hpp"
#include "app/settings.hpp"
#include "app/theme.hpp"
#include "audio/engine.hpp"
#include "gui/viz_window.hpp"
#include "krt/log.hpp"
#include "krt/scheduler.hpp"
#include "library/fuzzy.hpp"
#include "library/library.hpp"
#include "library/canvas.hpp"
#include "library/cover.hpp"
#include "net/lrclib.hpp"
#include "net/translate.hpp"
#include "net/ytm.hpp"
#include "script/lua_state.hpp"
#include "sys/mpris.hpp"
#include "tui/cell_buffer.hpp"
#include "tui/term.hpp"
#include "tui/visualizer.hpp"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <exception>
#include <filesystem>
#include <fstream>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

using namespace std::chrono_literals;

// PT: Definida mais abaixo (usada pelo CLI --ytm antes da definição).
// EN: Defined further below (used by the --ytm CLI before its definition).
// ES: Definida más abajo (usada por el CLI --ytm antes de su definición).
// ZH: 定义在下方（CLI --ytm 在其定义之前使用）。
std::string sanitize_filename(const std::string &in);

int cli_ytm(int argc, char **argv, audio::Engine &engine);

std::string_view trim(std::string_view s)
{
        while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
                s.remove_prefix(1);
        while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r'))
                s.remove_suffix(1);
        return s;
}

// PT: kewrc legado (read-only) — igual ao M1/M2. / EN: legacy kewrc (read-only) — same as M1/M2.
// ES: kewrc legado (read-only) — igual al M1/M2. / ZH: 旧 kewrc（只读）—— 与 M1/M2 相同。
void load_kewrc(const std::filesystem::path &path, app::Settings &st, app::BindingMap &map)
{
        std::ifstream f(path);
        if (!f)
                return;
        std::string line;
        while (std::getline(f, line))
        {
                const auto content = trim(line);
                if (content.empty() || content.front() == '#' || content.front() == ';')
                        continue;
                const auto eq = content.find('=');
                if (eq == std::string_view::npos)
                        continue;
                const auto key = trim(content.substr(0, eq));
                const auto value = trim(content.substr(eq + 1));
                if (key == "bind")
                {
                        const auto comma = value.find(',');
                        if (comma == std::string_view::npos)
                                continue;
                        const auto spec = app::parse_key(trim(value.substr(0, comma)));
                        const auto event = app::event_from_name(trim(value.substr(comma + 1)));
                        if (spec && event)
                                map.add_or_replace({*spec, *event});
                        continue;
                }
                app::apply_line(st, key, value);
        }
}

std::string mmss(std::chrono::microseconds us)
{
        const auto s = std::chrono::duration_cast<std::chrono::seconds>(us).count();
        return std::format("{:02}:{:02}", s / 60, s % 60);
}

// PT: CLI headless --ytm search|play (paridade com o kiutube C). search
//     lista e sai; play baixa e DEIXA o TUI abrir tocando (retorna -1).
// EN: Headless --ytm search|play CLI (parity with C kiutube). search lists
//     and exits; play downloads and lets the TUI open playing (returns -1).
// ES: CLI headless --ytm search|play (paridad con kiutube C).
// ZH: 无头 --ytm search|play CLI（与 C 版 kiutube 对齐）。search 列出后
//     退出；play 下载后进入 TUI 播放（返回 -1）。
int cli_ytm(int argc, char **argv, audio::Engine &engine)
{
        if (argc < 4)
        {
                std::fprintf(stderr, "uso / usage / uso / 用法: kiutube --ytm search|play \"<query>\"\n");
                return 2;
        }
        const std::string sub = argv[2];
        std::string query;
        for (int i = 3; i < argc; ++i)
        {
                if (i > 3)
                        query += " ";
                query += argv[i];
        }
        if (sub == "search")
        {
                const auto r = net::ytm_search(query, 10);
                if (!r.has_value())
                        return 1;
                int n = 0;
                for (const auto &t : *r)
                {
                        ++n;
                        std::printf("%2d) %-12s %s — %s (%d:%02d)\n", n, t.video_id.c_str(), t.title.c_str(),
                                    t.artist.c_str(), t.duration_seconds / 60, t.duration_seconds % 60);
                }
                return 0;
        }
        if (sub == "play")
        {
                const auto r = net::ytm_search(query, 1);
                if (!r.has_value() || r->empty())
                        return 1;
                const std::string base_dir = std::getenv("XDG_CACHE_HOME")
                                                 ? std::string(std::getenv("XDG_CACHE_HOME")) + "/kiutube/streams"
                                                 : std::string(std::getenv("HOME")) + "/.cache/kiutube/streams";
                const auto &t = (*r)[0];
                const auto path =
                        net::ytm_download(t.video_id, base_dir + "/" + sanitize_filename(t.artist) + " - " +
                                                          sanitize_filename(t.title) + " [" + t.video_id + "]");
                if (!path.has_value())
                        return 1;
                std::fprintf(stderr, "kiutube: tocando / playing / reproduciendo / 播放: %s\n",
                             path->c_str());
                engine.enqueue(*path);
                engine.play();
                return -1; // PT: segue pro TUI / continue to the TUI / 进入 TUI
        }
        std::fprintf(stderr, "uso / usage / uso / 用法: kiutube --ytm search|play \"<query>\"\n");
        return 2;
}

// PT: "Uploader - Artista - Título [videoId].webm" -> (artista, título):
//     artista = antes do PRIMEIRO " - "; título = depois do ÚLTIMO " - "
//     (nomes YTM duplicam o artista; o miolo é ruído de upload).
// EN: "Uploader - Artist - Title [videoId].webm" -> (artist, title): artist
//     = before the FIRST " - "; title = after the LAST " - " (YTM names
//     duplicate the artist; the middle is upload noise).
// ES: "Uploader - Artista - Título [videoId].webm" -> (artista, título):
//     artista = antes del PRIMER " - "; título = después del ÚLTIMO " - ".
// ZH: "上传者 - 艺人 - 标题 [videoId].webm" -> (艺人, 标题)：艺人取第一个 " - "
//     之前；标题取最后一个 " - " 之后（YTM 命名常重复艺人，中间是上传噪声）。
std::pair<std::string, std::string> split_artist_title(std::string_view filename)
{
        std::string base(filename);
        if (const auto dot = base.find_last_of('.'); dot != std::string::npos)
                base.erase(dot);
        if (const auto bracket = base.find('['); bracket != std::string::npos)
        {
                base.erase(bracket);
                while (!base.empty() && base.back() == ' ')
                        base.pop_back();
        }
        const auto first = base.find(" - ");
        if (first == std::string::npos)
                return {"", base};
        const auto last = base.rfind(" - ");
        const std::string artist = base.substr(0, first);
        const std::string title = last > first ? base.substr(last + 3) : base.substr(first + 3);
        return {artist, title};
}

// PT: Estado do picker: biblioteca (fuzzy Typesense) ou busca YTM (F8:
//     prompt -> resultados -> download+play).
// EN: Picker state: library (Typesense fuzzy) or YTM search (F8: prompt ->
//     results -> download+play).
// ES: Estado del picker: biblioteca o búsqueda YTM (F8: prompt ->
//     resultados -> descarga+reproducción).
// ZH: 选择器状态：曲库（Typesense 模糊搜索）或 YTM 搜索（F8：输入 ->
//     结果 -> 下载并播放）。
// PT: Codifica um codepoint em UTF-8 (para put_cell com símbolos chafa).
// EN: Encodes one codepoint as UTF-8 (for put_cell with chafa symbols).
// ES: Codifica un codepoint en UTF-8. / ZH: 将码点编码为 UTF-8（供 put_cell）。
std::string utf8_of(unsigned int cp)
{
        std::string out;
        if (cp < 0x80)
                out += static_cast<char>(cp);
        else if (cp < 0x800)
        {
                out += static_cast<char>(0xc0 | (cp >> 6));
                out += static_cast<char>(0x80 | (cp & 0x3f));
        }
        else if (cp < 0x10000)
        {
                out += static_cast<char>(0xe0 | (cp >> 12));
                out += static_cast<char>(0x80 | ((cp >> 6) & 0x3f));
                out += static_cast<char>(0x80 | (cp & 0x3f));
        }
        else
        {
                out += static_cast<char>(0xf0 | (cp >> 18));
                out += static_cast<char>(0x80 | ((cp >> 12) & 0x3f));
                out += static_cast<char>(0x80 | ((cp >> 6) & 0x3f));
                out += static_cast<char>(0x80 | (cp & 0x3f));
        }
        return out;
}

// PT: Título exibido de um caminho de fila: stem do arquivo (a biblioteca
//     taglib tem tags ricas; a fila só tem o path). Mesma gramática do
//     split_artist_title, sem cortar o "[id]".
// EN: Display title of a queue path: file stem (the taglib library has rich
//     tags; the queue only has the path). Same grammar as split_artist_title
//     without cutting the "[id]".
// ZH: 队列路径的显示标题：文件名主干（taglib 曲库才有富标签；队列只有路径）。
std::string stem_title(std::string_view path)
{
        std::string base = std::filesystem::path(path).filename().string();
        if (const auto dot = base.find_last_of('.'); dot != std::string::npos)
                base.erase(dot);
        return base;
}
enum class PickerSource
{
        Library,
        Ytm
};

struct Picker
{
        bool active = false;
        PickerSource source = PickerSource::Library;
        std::string filter;
        std::vector<library::Scored> matches;  // PT: fonte=Library / source=Library
        std::vector<int> ytm_filtered;         // PT: índices em ytm_results / indices into ytm_results
        std::vector<net::YtmResult> ytm_results;
        bool ytm_prompt = false; // PT: fase de digitação da query / query typing phase
        bool ytm_busy = false;   // PT: buscando ou baixando / searching or downloading
        std::string status;      // PT: mensagem de status exibida / shown status message
        std::size_t sel = 0;
};

// PT: Estado da view de letras: original + tradução (F9 alterna).
// EN: Lyrics view state: original + translation (F9 toggles).
// ES: Estado de la vista de letras: original + traducción (F9 alterna).
// ZH: 歌词视图状态：原文 + 译文（F9 切换）。
struct LyricsPane
{
        bool active = false;
        std::string text;
        std::string translated;
        bool has_translation = false;
        bool show_translation = false;
        bool loading = false;
        bool not_found = false;
};

// PT: View da fila (playlist): snapshot na abertura; Up/Down navegam, Enter
//     pula (engine.jump_to), ESC fecha. A snapshot congela na abertura para
//     o cursor não "andar" sozinho quando o YTM baixa algo novo.
// EN: Queue view (playlist): snapshot on open; Up/Down navigate, Enter jumps
//     (engine.jump_to), ESC closes. The snapshot freezes on open so the
//     cursor doesn't drift when YTM downloads something new.
// ES: Vista de la cola: snapshot al abrir; Up/Down navegan, Enter salta.
// ZH: 队列视图：打开时取快照；Up/Down 导航，Enter 跳转，ESC 关闭。
struct PlaylistPane
{
        bool active = false;
        std::vector<std::string> items;
        std::size_t sel = 0;
};

// PT: View de detalhe da faixa atual ('i'): metadados + transporte + path.
// EN: Current-track detail view ('i'): metadata + transport + path.
// ES: Vista de detalle de la pista actual ('i').
// ZH: 当前曲目详情视图（'i'）：元数据 + 传输 + 路径。
struct TrackPane
{
        bool active = false;
};

// PT: Capa chafa na TUI ('c'): renderiza 1x por faixa (cover_for resolve o
//     arquivo; Canvas decodifica e rasteriza em células coloridas).
// EN: chafa cover in the TUI ('c'): renders once per track (cover_for
//     resolves the file; Canvas decodes and rasterizes into colored cells).
// ES: Carátula chafa en la TUI ('c'): renderiza 1x por pista.
// ZH: TUI 中的 chafa 封面（'c'）：每曲目渲染一次。
struct CoverPane
{
        bool active = false;
        std::string rendered_path; // PT: faixa já renderizada / track already rendered / 已渲染曲目
        std::string image_path;    // PT: imagem usada (vazio = sem capa) / image used (empty = none) / 使用的图像
        cover::Canvas canvas{24, 12};
};

struct Ui
{
        tui::Term term;
        tui::CellBuffer buf;
        audio::Engine &engine;
        app::Settings &settings;
        std::string last_key = "-";
        const std::vector<library::Track> *tracks = nullptr; // PT: preenchido após o scan / set after scan
        Picker picker;
        LyricsPane lyrics;
        PlaylistPane playlist;
        TrackPane trackinfo;
        CoverPane cover;
        // PT: Temas: registro carregado no boot e o tema ativo (ponteiro
        //     estável dentro de `themes`). 't' cicla; --theme/settings.theme
        //     escolhem o inicial.
        // EN: Themes: registry loaded at boot and the active theme (stable
        //     pointer into `themes`). 't' cycles; --theme/settings.theme
        //     pick the initial one.
        // ZH: 主题：启动时加载注册表；'t' 循环切换；--theme/settings.theme
        //     选择初始主题。
        std::vector<app::Theme> themes;
        const app::Theme *theme = nullptr;
        // PT: Cache do título raw: se já foi escrito e a largura não
        //     mudou, não reescreve (a linha 1 fica intocada entre redraws).
        // EN: Raw title cache: once written, not rewritten unless the
        //     width changes (line 1 stays untouched between redraws).
        // ZH: raw 标题缓存：宽度不变则不重写（行 1 在重绘间保持不变）。
        bool title_drawn_ = false;
        int title_drawn_cols_ = 0;
        viz::Visualizer viz;
        // PT: Janela gráfica (thread própria). O callback roda NO SCHEDULER
        //     (post_function) quando o usuário fecha a janela: alinha o modo.
        // EN: GUI window (own thread). The callback runs ON THE SCHEDULER
        //     (post_function) when the user closes the window: aligns mode.
        // ES: Ventana gráfica (hilo propio). El callback corre EN EL
        //     SCHEDULER cuando el usuario cierra la ventana.
        // ZH: 图形窗口（独立线程）。用户关闭窗口时回调在调度器上运行
        //     （post_function）：对齐模式。
        gui::VizWindow vizwin;

        Ui(audio::Engine &e, app::Settings &s)
            : buf(24, 100), engine(e), settings(s),
              vizwin(e, [this] {
                      viz.set_mode(viz::Mode::Off);
                      draw();
              })
        {
                buf = tui::CellBuffer(term.rows(), term.cols());
                themes = app::load_themes();
                apply_theme(settings.theme);
        }

        ~Ui() = default; // PT: vizwin (membro último) é destruída primeiro, fechando a thread GUI. / vizwin (last member) is destroyed first, closing the GUI thread.

        // PT: Ícone Nerd Font (Font Awesome, PUA) ou fallback ASCII,
        //     conforme settings.nerd_fonts. Glifos de 3 bytes cabem na
        //     célula (ch[8]).
        // EN: Nerd Font icon (Font Awesome, PUA) or ASCII fallback,
        //     per settings.nerd_fonts. 3-byte glyphs fit the cell (ch[8]).
        // ZH: 依 settings.nerd_fonts 返回 Nerd Font 图标（Font Awesome，
        //     PUA）或 ASCII 回退。3 字节 glif 可容纳于单元格（ch[8]）。
        [[nodiscard]] const char *ic(const char *nerd, const char *ascii) const
        {
                return settings.nerd_fonts ? nerd : ascii;
        }

        // PT: Título em raw (fora da grade): FIGlet ASCII-art "kiutube"
        //     (fonte standard, gerada pelo figlet) em negrito + cor accent.
        //     Reescrito só quando a largura muda (cache); a grade nunca
        //     toca as linhas do banner. Terminais estreitos: linhas
        //     clipadas em bytes (banner ~50 cols).
        // EN: Raw title (outside the grid): FIGlet ASCII-art "kiutube"
        //     (standard font, generated by figlet) in bold + accent color.
        //     Rewritten only when the width changes (cached); the grid
        //     never touches the banner lines. Narrow terminals: lines
        //     clipped in bytes (banner ~50 cols).
        // ZH: raw 标题（在网格之外）：FIGlet ASCII-art "kiutube"（standard
        //     字体，figlet 生成），粗体 + accent 色。仅在宽度变化时重写。
        void write_title_raw()
        {
                static constexpr const char *kBanner = R"banner( _    _       _         _
| | _(_)_   _| |_ _   _| |__   ___
| |/ / | | | | __| | | | '_ \ / _ \
|   <| | |_| | |_| |_| | |_) |  __/
|_|\_\|\__,_|\__|\__,_|_.__/ \___|)banner";
                const auto accent = themed("accent");
                const std::string sgr =
                    accent.set ? (tui::truecolor_enabled()
                                      ? std::format("\x1b[1;38;2;{};{};{}m", accent.rgb.r, accent.rgb.g,
                                                    accent.rgb.b)
                                      : std::format("\x1b[1;38;5;{}m", tui::quantize256(accent.rgb)))
                               : std::string("\x1b[1m");
                const std::size_t limit = static_cast<std::size_t>(std::max(10, buf.cols() - 2));
                std::string out;
                int row = 1;
                std::string_view rest(kBanner);
                while (!rest.empty())
                {
                        const auto nl = rest.find('\n');
                        const std::string_view line = rest.substr(0, nl);
                        out += std::format("\x1b[{};2H{}{}\x1b[0m", row++, sgr,
                                           std::string(line.substr(0, limit)));
                        if (nl == std::string_view::npos)
                                break;
                        rest.remove_prefix(nl + 1);
                }
                term.write_segments({std::move(out)});
        }

        // PT: Cor de uma chave do tema ativo com fallback no default; sem
        //     definição em nenhum = herda o terminal ({}).
        // EN: A theme key's color with fallback to the default theme; unset
        //     in both = inherit the terminal ({}).
        // ZH: 主题键颜色，回退 default 主题；均未定义 = 继承终端（{}）。
        [[nodiscard]] tui::Color themed(std::string_view key) const
        {
                if (theme)
                        if (auto c = theme->get(key); c.set)
                                return c;
                return app::default_theme().get(key);
        }

        // PT: Cor concreta para a paleta do visualizador: chave viz.* do
        //     tema, derivada de trackview.visualizer/accent, ou o default.
        // EN: A concrete color for the visualizer palette: the theme's
        //     viz.* key, derived from trackview.visualizer/accent, or the
        //     default. ZH: 可视化器调色板取色：viz.* 键，或派生，或默认。
        [[nodiscard]] tui::Rgb viz_color(std::string_view key, std::string_view alt, tui::Rgb fb) const
        {
                if (theme)
                {
                        const auto it = theme->colors.find(std::string(key));
                        if (it != theme->colors.end())
                                return it->second;
                        const auto a = theme->colors.find(std::string(alt));
                        if (a != theme->colors.end())
                                return a->second;
                }
                return fb;
        }

        // PT: Ativa um tema por id (desconhecido cai no "default") e
        //     espelha a paleta no visualizador.
        // EN: Activates a theme by id (unknown falls back to "default") and
        //     mirrors the palette onto the visualizer.
        // ZH: 按 id 激活主题（未知回退 "default"），并同步可视化器调色板。
        void apply_theme(const std::string &id)
        {
                theme = app::find_theme(themes, id);
                if (!theme)
                        theme = app::find_theme(themes, "default");
                const auto &d = app::default_theme().colors;
                const auto fb = [&d](const char *k) {
                        const auto it = d.find(k);
                        return it != d.end() ? it->second : tui::Rgb{255, 255, 255};
                };
                viz.set_palette(viz_color("viz.low", "trackview.visualizer", fb("viz.low")),
                                viz_color("viz.mid", "trackview.visualizer", fb("viz.mid")),
                                viz_color("viz.high", "accent", fb("viz.high")),
                                viz_color("viz.peak", "accent", fb("viz.peak")));
        }

        // PT: Próximo tema da lista (ordem do --themes); usado pela tecla 't'.
        // EN: Next theme in the list (--themes order); used by the 't' key.
        // ZH: 列表中的下一个主题（--themes 顺序）；'t' 键使用。
        [[nodiscard]] std::string next_theme_id() const
        {
                if (themes.empty())
                        return "default";
                const std::string cur = theme ? theme->id : "default";
                for (std::size_t i = 0; i < themes.size(); ++i)
                        if (themes[i].id == cur)
                                return themes[(i + 1) % themes.size()].id;
                return themes.front().id;
        }

        void draw()
        {
                const auto cur = engine.current();
                const auto pos = engine.position();

                // PT: Drena o tap do engine e alimenta o visualizador a cada
                //     redraw — EXCETO quando a janela gráfica está aberta:
                //     o tap é SPSC (um consumidor), e a janela o drena.
                // EN: Drains the engine tap and feeds the visualizer on every
                //     redraw — EXCEPT when the GUI window is open: the tap is
                //     SPSC (one consumer), and the window drains it.
                // ES: Drena el tap del engine en cada redraw — EXCEPTO cuando
                //     la ventana gráfica está abierta (tap SPSC).
                // ZH: 每次重绘时排空引擎 tap —— 图形窗口打开时除外：tap 为
                //     SPSC（单消费者），由窗口排空。
                const bool gui_open = vizwin.is_open();
                if (!gui_open)
                {
                        float chunk[16384];
                        const std::size_t got = engine.read_viz({chunk, 16384});
                        if (got > 0)
                                viz.feed({chunk, got * 2});
                }

                buf.clear();
                // PT: Cabeçalho pedido pelo usuário: linha 1 = nome+descrição
                //     em NEGRITO + LARGURA DUPLA (raw, fora da grade — ver
                //     write_title_raw); linhas 3-4 = ajuda curta das teclas
                //     (PT completo na 3, restante do PT + EN na 4 — ES/ZH
                //     no `kiutube --help`); linhas 6-8 = o bloco da faixa
                //     (título, barra+tempos, config); visualizador a partir
                //     da linha 10.
                // EN: User-requested header: line 1 = name+tagline in BOLD
                //     DOUBLE-WIDTH (raw, outside the grid — see
                //     write_title_raw); lines 3-4 = short key help (full PT
                //     on 3, rest of PT + EN on 4 — ES/ZH in
                //     `kiutube --help`); lines 6-8 = the track block
                //     (title, bar+times, config); visualizer from row 10.
                // ZH: 用户要求的页眉：第 1 行 = 粗体双倍宽名称+简介（raw，
                //     在网格之外 —— 见 write_title_raw）；第 3-4 行 = 快捷键
                //     简明帮助；第 6-8 行 = 曲目块；可视化器从第 10 行起。
                buf.put(7, 2,
                        std::string(ic("\xef\x80\xa5 ", "")) +
                            "player de musica para terminal (reescrita C++20 do kew)",
                        themed("textMuted"));
                buf.put(9, 2,
                        std::string(ic("\xef\x81\x8b ", "")) + "espaco pausa/toca · " +
                            ic("\xef\x81\x91 ", "") + "n proxima · " + ic("\xef\x83\xac ", "") +
                            "<-/-> seek +/-5s · " + ic("\xef\x80\xa8 ", "") + "+/- volume · " +
                            ic("\xef\x80\x82 ", "") + "l biblioteca · " + ic("\xef\x83\xb6 ", "") +
                            "g letra · " + ic("\xef\x85\xaa ", "") + "F8 YouTube Music · " +
                            ic("\xef\x86\xab ", "") + "F9 traducao",
                        themed("textMuted"));
                buf.put(10, 2,
                        std::string(ic("\xef\x82\x80 ", "")) + "v visualizador · " + ic("\xef\x83\x8a ", "") +
                            "p fila · " + ic("\xef\x84\xa9 ", "") + "i faixa · " + ic("\xef\x80\xbe ", "") +
                            "c capa · " + ic("\xef\x87\xbc ", "") + "t tema · " + ic("\xef\x82\x8b ", "") +
                            "q sai | space pause · n next · l library · g lyrics · F8 YTM · v viz · p queue · "
                            "c cover · t theme · q quit",
                        themed("textMuted"));
                if (cur.path.empty())
                {
                        // PT: Estado vazio: dica de uso + última tecla.
                        // EN: Empty state: usage hint + last key.
                        // ZH: 空状态：用法提示 + 最近按键。
                        buf.put(6, 2,
                                std::string(settings.nerd_fonts ? "\xef\x80\xa5 " : "") +
                                    "play <arquivo / file / archivo / \xe6\x96\x87\xe4\xbb\xb6> ...",
                                themed("textMuted"));
                        buf.put(13, 2, "key " + last_key);
                }
                else
                {
                        buf.put(12, 2, std::string(ic("\xef\x80\xa5 ", "")) + cur.title, themed("nowplaying"));
                        const auto shown_pos = std::min(pos, cur.duration);
                        // PT: Barra de progresso com largura adaptativa
                        //     (cols/4, 10..30) + tempos. Cores
                        //     progress.filled/empty do tema.
                        // EN: Progress bar with adaptive width (cols/4,
                        //     10..30) + times. Theme progress colors.
                        // ZH: 进度条宽度自适应（cols/4，10..30）+ 时间；
                        //     颜色取主题 progress.filled/empty。
                        const double frac = cur.duration.count() > 0
                                                ? static_cast<double>(shown_pos.count()) / cur.duration.count()
                                                : 0.0;
                        const int bw = std::clamp(buf.cols() / 4, 10, 30);
                        const auto filled = themed("progress.filled");
                        const auto empty = themed("progress.empty");
                        for (int i = 0; i < bw; ++i)
                        {
                                const bool on = i < static_cast<int>(frac * bw + 0.5);
                                if (filled.set && empty.set)
                                        buf.put_cell(13, 2 + i, " ", tui::Color{}, on ? filled : empty);
                                else
                                        buf.put_cell(13, 2 + i, on ? "\xe2\x96\x88" : "\xe2\x96\x91",
                                                     tui::Color{}, tui::Color{});
                        }
                        // PT: Ícones Nerd Font (Font Awesome, presente em
                        //     toda Nerd Font) no lugar de |> / ||. Sem a
                        //     fonte (toggle off), cai no ASCII histórico.
                        // EN: Nerd Font icons (Font Awesome, present in
                        //     every Nerd Font) instead of |> / ||. Without
                        //     the font (toggle off), falls back to ASCII.
                        // ZH: Nerd Font 图标（Font Awesome）替代 |> / ||；
                        //     无字体（开关关闭）时回退 ASCII。
                        const std::string sym = engine.is_playing()
                                                    ? (settings.nerd_fonts ? " \xef\x81\x8b " : "  |>")
                                                    : (settings.nerd_fonts ? " \xef\x81\x8c " : "  ||");
                        const std::string line =
                            " " + mmss(shown_pos) + " / " + mmss(cur.duration) + sym + "  " +
                            std::string(ic("\xef\x80\xa8 ", "")) +
                            std::to_string(static_cast<int>(engine.volume() * 100.0f + 0.5f)) + "%  " +
                            ic("\xef\x83\x8a ", "") + std::to_string(engine.queue_size()) + "  key " + last_key;
                        buf.put(13, 2 + bw + 1, line, themed("text"));
                }
                std::string cfg = "volume=" + std::to_string(settings.volume) + " shuffle=" +
                                  (settings.shuffle ? "on" : "off") + " visualizer=" +
                                  (settings.visualizer ? "on" : "off") +
                                  (tracks ? " lib=" + std::to_string(tracks->size()) : "") +
                                  " theme=" + (theme ? theme->id : "default");
                buf.put(14, 2, cfg, themed("textMuted"));

                // PT: Área do visualizador: a partir da linha 10 (após o
                //     cabeçalho com ajuda de teclas) até a penúltima linha —
                //     adaptativa ao tamanho do terminal. settings.visualizer
                //     = off força o modo Off. Capa ativa toma a área
                //     (painéis/pickers desenham por cima quando abertos).
                // EN: Visualizer area: from row 10 (after the header with
                //     key help) to the penultimate line — adaptive to the
                //     terminal size. settings.visualizer = off forces Off.
                //     An active cover takes the area (panes/pickers draw
                //     over it when open).
                // ZH: 可视化器区域：从第 10 行（含快捷键帮助的页眉之后）到
                //     倒数第二行 —— 随终端尺寸自适应。封面激活时占用该区域。
                if (!settings.visualizer && viz.mode() != viz::Mode::Off)
                        viz.set_mode(viz::Mode::Off);
                if (cover.active)
                {
                        ensure_cover(cur);
                        draw_cover();
                }
                else if (!gui_open && viz.mode() != viz::Mode::Off)
                        viz.draw(buf, 16, 2, std::max(3, buf.rows() - 18), buf.cols() - 4);

                if (picker.active && (picker.source == PickerSource::Ytm || tracks))
                        draw_picker();
                if (lyrics.active)
                        draw_lyrics();
                if (playlist.active)
                        draw_playlist();
                if (trackinfo.active)
                        draw_trackinfo();
                term.write_segments(buf.flush_diff());
                // PT: Título em raw (linha 1, fora da grade): reescrito só
                //     na primeira vez ou quando a largura muda (a grade não
                //     gerencia essa linha).
                // EN: Raw title (line 1, outside the grid): rewritten only
                //     on the first frame or on width change (the grid does
                //     not manage this line).
                // ZH: raw 标题（第 1 行，网格之外）：仅在首帧或宽度变化时重写。
                if (!title_drawn_ || title_drawn_cols_ != buf.cols())
                {
                        title_drawn_ = true;
                        title_drawn_cols_ = buf.cols();
                        write_title_raw();
                }
        }

        void draw_picker()
        {
                if (picker.source == PickerSource::Ytm)
                {
                        draw_ytm_picker();
                        return;
                }
                buf.put(16, 2, std::string(ic("\xef\x80\x82 ", "")) + "LIB> " + picker.filter + "_");
                const std::size_t max_rows = 8;
                if (picker.sel >= picker.matches.size())
                        picker.sel = picker.matches.empty() ? 0 : picker.matches.size() - 1;
                const std::size_t start = picker.sel >= max_rows ? picker.sel - max_rows + 1 : 0;
                for (std::size_t r = 0; r < max_rows && start + r < picker.matches.size(); ++r)
                {
                        const auto &m = picker.matches[start + r];
                        const auto &t = (*tracks)[static_cast<std::size_t>(m.index)];
                        std::string line = std::string(ic("\xef\x81\x94 ", "> ")) + t.title;
                        if (!t.artist.empty())
                                line += "  --  " + t.artist;
                        buf.put(17 + r, 2, line, start + r == picker.sel ? themed("playlist.playing") : tui::Color{});
                }
                if (picker.matches.empty())
                        buf.put(14, 2, "(vazio / empty / vacío / 空)");
        }

        // PT: Picker YTM (F8): fase de prompt (digita a query) e fase de
        //     resultados (navega + Enter baixa e toca). Status em 4 idiomas.
        // EN: YTM picker (F8): prompt phase (type the query) and results
        //     phase (navigate + Enter downloads and plays).
        // ES: Picker YTM (F8): fase de prompt y fase de resultados.
        // ZH: YTM 选择器（F8）：输入阶段与结果阶段（导航 + Enter 下载并播放）。
        void draw_ytm_picker()
        {
                if (picker.ytm_prompt)
                {
                        buf.put(16, 2, std::string(ic("\xef\x85\xaa ", "")) + "YTM> " + picker.filter + "_");
                        buf.put(17, 2, "Enter: buscar / search / buscar / 搜索   ESC: "
                                       "cancelar / cancel / cancelar / 取消");
                }
                else
                {
                        buf.put(16, 2, std::string(ic("\xef\x85\xaa ", "")) + "YTM> " + picker.filter + "  (" +
                                           std::to_string(picker.ytm_filtered.size()) + ")");
                        const std::size_t max_rows = 8;
                        if (picker.sel >= picker.ytm_filtered.size())
                                picker.sel = picker.ytm_filtered.empty() ? 0 : picker.ytm_filtered.size() - 1;
                        const std::size_t start =
                            picker.sel >= max_rows ? picker.sel - max_rows + 1 : 0;
                        for (std::size_t r = 0; r < max_rows && start + r < picker.ytm_filtered.size(); ++r)
                        {
                                const auto &t = picker.ytm_results[static_cast<std::size_t>(
                                    picker.ytm_filtered[start + r])];
                                std::string line = std::string(ic("\xef\x81\x94 ", "> ")) + t.title + "  --  " +
                                                   t.artist;
                                if (t.duration_seconds > 0)
                                        line += "  (" + mmss(std::chrono::seconds(t.duration_seconds)) + ")";
                                buf.put(17 + r, 2, line);
                        }
                }
                if (!picker.status.empty())
                        buf.put(26, 2, picker.status);
        }

        void draw_lyrics()
        {
                if (lyrics.loading)
                {
                        buf.put(16, 2, std::string(ic("\xef\x87\x8e ", "")) +
                                           "LRCLIB... / fetching / buscando / 获取中");
                        return;
                }
                if (lyrics.not_found)
                {
                        buf.put(16, 2, std::string(ic("\xef\x80\x8d ", "")) +
                                           "letra nao encontrada / lyrics not found / letra no encontrada / 未找到歌词");
                        return;
                }
                std::size_t row = 16;
                std::size_t count = 0;
                // PT: F9 alterna original <-> traduzida (quando há tradução).
                // EN: F9 toggles original <-> translated (when a translation exists).
                // ES: F9 alterna original <-> traducida. / ZH: F9 切换原文 <-> 译文。
                const std::string &src_text =
                    (lyrics.show_translation && lyrics.has_translation) ? lyrics.translated : lyrics.text;
                std::string_view rest(src_text);
                while (!rest.empty() && count < 10)
                {
                        const auto nl = rest.find('\n');
                        std::string_view line = rest.substr(0, nl == std::string_view::npos ? rest.size() : nl);
                        // PT: remove prefixo de timestamp LRC "[mm:ss.xx]".
                        // EN: strips LRC timestamp prefix "[mm:ss.xx]".
                        // ES: quita el prefijo de timestamp LRC "[mm:ss.xx]".
                        // ZH: 去除 LRC 时间戳前缀 "[mm:ss.xx]"。
                        while (!line.empty() && line.front() == '[')
                        {
                                const auto close = line.find(']');
                                if (close == std::string_view::npos)
                                        break;
                                line.remove_prefix(close + 1);
                        }
                        if (!line.empty())
                        {
                                buf.put(row, 2, std::string(line));
                                ++row;
                                ++count;
                        }
                        if (nl == std::string_view::npos)
                                break;
                        rest.remove_prefix(nl + 1);
                }
        }

        // PT: Garante a capa da faixa atual renderizada uma única vez por
        //     troca (cover_for faz I/O em disco cacheado; chafa é CPU-bound).
        // EN: Ensures the current track's cover rendered exactly once per
        //     change (cover_for does cached disk I/O; chafa is CPU-bound).
        // ZH: 保证当前曲目封面每首只渲染一次（cover_for 走磁盘缓存；chafa 为
        //     CPU 密集）。
        void ensure_cover(const audio::TrackInfo &cur)
        {
                if (cover.rendered_path == cur.path)
                        return;
                cover.rendered_path = cur.path;
                cover.image_path.clear();
                if (cur.path.empty())
                        return;
                if (auto img = library::cover_for(cur.path))
                {
                        cover.image_path = *img;
                        // PT: render falhou (imagem ruim): mostra "sem capa".
                        // EN: render failed (bad image): show "no cover".
                        // ZH: 渲染失败（图像损坏）：显示"无封面"。
                        if (!cover.canvas.render(*img))
                                cover.image_path.clear();
                }
        }

        // PT: Capa em células coloridas à direita (rows 13.., cols do fim).
        // EN: Cover as colored cells on the right (rows 13.., trailing cols).
        // ES: Carátula en celdas coloreadas a la derecha.
        // ZH: 右侧单元格彩色封面（第 13 行起、靠右列）。
        void draw_cover()
        {
                if (cover.image_path.empty())
                {
                        buf.put(16, 2,
                                std::string(ic("\xef\x80\xbe ", "")) +
                                    "sem capa / no cover / sin caratula / 无封面  (c: fechar / close / cerrar / 关闭)");
                        return;
                }
                const int x0 = std::max(0, buf.cols() - cover.canvas.cols() - 2);
                for (int y = 0; y < cover.canvas.rows(); ++y)
                {
                        for (int x = 0; x < cover.canvas.cols(); ++x)
                        {
                                const auto c = cover.canvas.at(x, y);
                                tui::Color fg;
                                fg.set = c.fg_set;
                                fg.rgb = {c.r, c.g, c.b};
                                tui::Color bg;
                                bg.set = c.bg_set;
                                bg.rgb = {c.br, c.bg, c.bb};
                                buf.put_cell(16 + y, x0 + x, utf8_of(c.ch), fg, bg);
                        }
                }
        }

        // PT: Playlist da fila: ">>" na faixa atual, ">" no item selecionado;
        //     Enter pula (jump_to), ESC fecha. Snapshot aberta na ativação.
        // EN: Queue playlist: ">>" marks the current track, ">" the selection;
        //     Enter jumps (jump_to), ESC closes. Snapshot taken on activation.
        // ZH: 队列播放列表：">>" 当前曲目，">" 选中项；Enter 跳转，ESC 关闭。
        void draw_playlist()
        {
                buf.put(13, 2,
                        std::string(ic("\xef\x83\x8a ", "")) + "QUEUE / playlist / cola / 播放列表  (" +
                            std::to_string(playlist.items.size()) +
                            ")  Enter: pular / jump / saltar / 跳转");
                const auto cur = engine.current();
                std::size_t row = 17;
                if (!cur.path.empty())
                {
                        buf.put(static_cast<int>(row), 2,
                                std::string(ic("\xef\x81\x8b ", ">> ")) + cur.title + "  (<)",
                                themed("nowplaying"));
                        ++row;
                }
                const std::size_t max_rows = 12;
                if (playlist.sel >= playlist.items.size())
                        playlist.sel = playlist.items.empty() ? 0 : playlist.items.size() - 1;
                const std::size_t start = playlist.sel >= max_rows ? playlist.sel - max_rows + 1 : 0;
                for (std::size_t r = 0; r < max_rows && start + r < playlist.items.size(); ++r)
                {
                        std::string line = std::string(ic("\xef\x81\x94 ", "> ")) +
                                           stem_title(playlist.items[start + r]);
                        buf.put(static_cast<int>(row + r), 2, line,
                                start + r == playlist.sel ? themed("playlist.playing") : tui::Color{});
                }
                if (playlist.items.empty())
                        buf.put(static_cast<int>(row), 2,
                                "(fila vazia / queue empty / cola vacia / 队列空)");
        }

        // PT: Detalhe da faixa: artista/título (gramática YTM), duração,
        //     posição, volume, path completo e tamanho da fila.
        // EN: Track detail: artist/title (YTM grammar), duration, position,
        //     volume, full path and queue size.
        // ES: Detalle de la pista: artista/título, duración, posición, path.
        // ZH: 曲目详情：艺人/标题、时长、位置、音量、完整路径、队列长度。
        void draw_trackinfo()
        {
                const auto cur = engine.current();
                if (cur.path.empty())
                {
                        buf.put(16, 2,
                                std::string(ic("\xef\x84\xa9 ", "")) +
                                    "nada tocando / nothing playing / nada sonando / 没有在播");
                        return;
                }
                const auto [artist, title] = split_artist_title(cur.title);
                buf.put(16, 2, std::string(ic("\xef\x84\xa9 ", "")) + "FAIXA / track / pista / 曲目");
                buf.put(17, 2, std::string(ic("\xef\x80\x81 ", "")) + "titulo / title / titulo / 标题: " + title);
                buf.put(18, 2, std::string(ic("\xef\x80\x87 ", "")) + "artista / artist / artista / 艺人: " +
                                    (artist.empty() ? "-" : artist));
                buf.put(19, 2,
                        std::string(ic("\xef\x80\x97 ", "")) + "dur / length / duracion / 时长: " +
                            mmss(cur.duration) + "   pos / position / posicion / 位置: " + mmss(engine.position()));
                buf.put(20, 2,
                        std::string(ic(engine.is_playing() ? "\xef\x81\x8b " : "\xef\x81\x8c ", "")) +
                            "estado / state / estado / 状态: " +
                            (engine.is_playing() ? "playing / 播放中" : "paused / 已暂停") + "   " +
                            ic("\xef\x80\xa8 ", "") +
                            std::to_string(static_cast<int>(engine.volume() * 100.0f + 0.5f)) + "%");
                buf.put(21, 2, std::string(ic("\xef\x83\x8a ", "")) + "fila / queue / cola / 队列: " +
                                    std::to_string(engine.queue_size()));
                buf.put(22, 2, std::string(ic("\xef\x84\x95 ", "")) + "path: " + cur.path);
        }
};

krt::Task<void> ui_clock(Ui &ui, std::atomic<bool> &alive)
{
        // PT: Relógio adaptativo: 50ms com visualizador ativo (movimento
        //     fluido), 250ms parado. Para quando alive baixa.
        // EN: Adaptive clock: 50ms with an active visualizer (fluid motion),
        //     250ms when idle. Stops when alive drops.
        // ES: Reloj adaptativo: 50ms con visualizador activo, 250ms en
        //     reposo. Para cuando alive baja.
        // ZH: 自适应时钟：可视化器激活时 50ms（流畅动画），空闲时 250ms。
        //     alive 置假后停止。
        while (alive.load(std::memory_order_acquire))
        {
                co_await krt::sleep(ui.viz.mode() != viz::Mode::Off ? 50ms : 250ms);
                ui.draw();
        }
}

// PT: Busca YTM em background (yt-dlp dentro de offload) e preenche o
//     picker. Download em segundo plano com o mesmo padrão.
// EN: Background YTM search (yt-dlp inside offload) filling the picker.
//     Download follows the same pattern.
// ES: Búsqueda YTM en background rellenando el picker. / ZH: 后台 YTM 搜索
//     （yt-dlp 置于 offload），填充选择器；下载同模式。
krt::Task<void> ytm_search_task(std::string query, Picker &picker)
{
        auto res = co_await krt::offload([q = std::move(query)] { return net::ytm_search(q, 10); });
        picker.ytm_busy = false;
        if (res.has_value() && !res->empty())
        {
                picker.ytm_results = std::move(*res);
                picker.ytm_prompt = false;
                picker.filter.clear();
                picker.sel = 0;
                picker.status.clear();
                picker.ytm_filtered.resize(picker.ytm_results.size());
                for (std::size_t i = 0; i < picker.ytm_results.size(); ++i)
                        picker.ytm_filtered[i] = static_cast<int>(i);
        }
        else
        {
                picker.status = "Nenhum resultado / no results / sin resultados / 无结果";
        }
}

// PT: Sanitiza nome de arquivo (mesmo jogo do sanitize do ytm: ASCII seguro),
//     mas PRESERVA bytes UTF-8 (acentos, CJK) — antes "Álbum" virava "_lbum".
// EN: Sanitizes a filename (same game as ytm's sanitize: safe ASCII) but
//     PRESERVES UTF-8 bytes (accents, CJK) — "Álbum" used to become "_lbum".
// ES: Sanitiza nombre de archivo, pero PRESERVA UTF-8.
// ZH: 文件名清理（与 ytm 相同：安全 ASCII），但保留 UTF-8 字节（重音、CJK）。
std::string sanitize_filename(const std::string &in)
{
        std::string out;
        for (const char c : in)
        {
                const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                                (c >= '0' && c <= '9') || c == ' ' || c == '-' || c == '_' || c == '(' || c == ')' ||
                                c == '[' || c == ']' || c == '.' || static_cast<unsigned char>(c) >= 0x80;
                out += ok ? c : '_';
        }
        return out;
}

krt::Task<void> ytm_download_task(net::YtmResult track, audio::Engine &engine, Picker &picker,
                                  app::Settings &settings, std::vector<library::Track> &tracks, Ui *ui_for_rescan)
{
        // PT: destino: library_path se houver, senão ~/.cache/kiutube/streams.
        // EN: destination: library_path if set, else ~/.cache/kiutube/streams.
        std::string dir = settings.library_path;
        if (dir.empty())
        {
                std::string base = std::getenv("XDG_CACHE_HOME") ? std::getenv("XDG_CACHE_HOME")
                                                                 : std::string(std::getenv("HOME")) + "/.cache";
                dir = base + "/kiutube/streams";
        }
        const std::string base = dir + "/" + sanitize_filename(track.artist) + " - " +
                                 sanitize_filename(track.title) + " [" + track.video_id + "]";
        auto path = co_await krt::offload([&] { return net::ytm_download(track.video_id, base); });
        picker.ytm_busy = false;
        if (path.has_value())
        {
                engine.enqueue(*path);
                engine.next();
                engine.play();
                picker.active = false;
                picker.status.clear();
                // PT: re-escaneia a biblioteca pra incluir o arquivo novo.
                // EN: rescans the library to include the new file.
                if (!settings.library_path.empty())
                {
                        tracks = co_await krt::offload([&] { return library::scan(settings.library_path); });
                        (void)ui_for_rescan;
                }
        }
        else
        {
                picker.status = "Falha no download / download failed / fallo / 下载失败";
        }
}

// PT: Busca a letra em background (rede síncrona dentro de offload) e
//     preenche o painel — o relógio de UI redesenha quando chegar.
// EN: Fetches lyrics in the background (sync network inside offload) and
//     fills the pane — the UI clock redraws when it arrives.
// ES: Busca la letra en background y llena el panel.
// ZH: 后台获取歌词（同步网络置于 offload 中）并填充面板 —— UI 时钟在到达后
//     重绘。
krt::Task<void> fetch_lyrics_task(std::string artist, std::string title, int duration_seconds,
                                  LyricsPane &pane, app::Settings &settings)
{
        auto lrc = co_await krt::offload([a = std::move(artist), t = std::move(title), duration_seconds] {
                return net::fetch_lyrics(a, t, duration_seconds);
        });
        if (lrc.has_value())
        {
                pane.text = std::move(*lrc);
                // PT: tradução em cadeia (Google -> MyMemory) quando habilitada.
                // EN: chained translation (Google -> MyMemory) when enabled.
                // ES: traducción en cadena cuando está habilitada.
                // ZH: 启用时进行链式翻译（Google -> MyMemory）。
                if (settings.translation)
                {
                        auto tr = co_await krt::offload([&] { return net::translate_to(pane.text, "pt"); });
                        if (tr.has_value())
                        {
                                pane.translated = std::move(*tr);
                                pane.has_translation = true;
                        }
                }
        }
        else
                pane.not_found = true;
        pane.loading = false;
}

krt::Task<void> app_main(audio::Engine &engine, Ui &ui, std::atomic<bool> &alive)
{
        krt::log::info("boot: kiutube M4");

        app::Settings settings;
        app::BindingMap map;
        map.load_defaults();
        load_kewrc(app::config_dir("kew") / "kewrc", settings, map);
        script::LuaState lua;
        lua.open_config((app::config_dir("kiutube") / "init.lua").string(), settings, map);
        ui.settings = settings;
        // PT: Visualizador da TUI ligado por padrão quando settings.visualizer
        //     (era preciso apertar 'v' antes). NÃO espelha na janela gráfica:
        //     vizwin.set_mode(Spectrum) ABRIRIA a janela Qt (show()) e a
        //     TUI suspenderia o próprio viz — área vazia. A janela continua
        //     abrindo só pelo ciclo da tecla 'v' (desenho M4).
        // EN: TUI visualizer on by default when settings.visualizer (it
        //     used to require 'v'). NOT mirrored to the GUI window:
        //     vizwin.set_mode(Spectrum) would OPEN the Qt window (show())
        //     and the TUI would suspend its own viz — an empty area. The
        //     window still opens only via the 'v' cycle (M4 design).
        // ZH: settings.visualizer 开启时默认启用 TUI 可视化器（原先需按
        //     'v'）。不同步到图形窗口：vizwin.set_mode(Spectrum) 会打开 Qt
        //     窗口（show()）导致 TUI 暂停自身 viz —— 区域空白。窗口仍仅由
        //     'v' 键循环打开（M4 设计）。
        if (settings.visualizer && ui.viz.mode() == viz::Mode::Off)
                ui.viz.set_mode(viz::Mode::Spectrum);

        // PT: Scan da biblioteca em offload (I/O) antes de entrar no loop.
        // EN: Library scan offloaded (I/O) before entering the loop.
        // ES: Scan de la biblioteca en offload (E/S) antes de entrar al bucle.
        // ZH: 在进入循环前经 offload 扫描曲库（I/O）。
        std::vector<library::Track> tracks;
        if (!settings.library_path.empty())
        {
                tracks = co_await krt::offload(
                    [&] { return library::scan(settings.library_path); });
                krt::log::info("library: {} faixas / tracks / pistas / 曲目", tracks.size());
        }
        ui.tracks = &tracks;

        std::vector<krt::Task<void>> bg_tasks; // PT: lyrics/buscas/downloads mantidos vivos / kept alive

        auto recompute = [&] {
                ui.picker.matches = library::fuzzy_filter(tracks, ui.picker.filter);
                if (ui.picker.sel >= ui.picker.matches.size())
                        ui.picker.sel = 0;
        };

        // PT: Filtro substring (case-insensitive) dos resultados YTM.
        // EN: Case-insensitive substring filter over YTM results.
        // ES: Filtro substring (case-insensitive) de los resultados YTM.
        // ZH: 对 YTM 结果做大小写不敏感的子串过滤。
        auto filter_ytm = [](Picker &p) {
                std::string lf;
                for (const char c : p.filter)
                        lf += static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
                p.ytm_filtered.clear();
                for (std::size_t i = 0; i < p.ytm_results.size(); ++i)
                {
                        std::string hay = p.ytm_results[i].title + " " + p.ytm_results[i].artist;
                        for (auto &c : hay)
                                c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
                        if (lf.empty() || hay.find(lf) != std::string::npos)
                                p.ytm_filtered.push_back(static_cast<int>(i));
                }
                p.sel = 0;
        };

        ui.draw();

        for (;;)
        {
                const auto ev = co_await ui.term.next_event();

                // PT: Modo picker: teclas crúas (sem BindingMap) editam o filtro.
                // EN: Picker mode: raw keys (no BindingMap) edit the filter.
                // ES: Modo picker: teclas crudas (sin BindingMap) editan el filtro.
                // ZH: 选择器模式：原始按键（不经 BindingMap）编辑过滤器。
                if (ui.picker.active)
                {
                        bool handled = true;
                        if (ev.key == tui::Key::Esc)
                                ui.picker = Picker{};
                        else if (ui.picker.source == PickerSource::Ytm)
                        {
                                if (ui.picker.ytm_prompt)
                                {
                                        // PT: fase de digitação da query YTM.
                                        // EN: YTM query typing phase.
                                        if (ev.key == tui::Key::Enter && !ui.picker.filter.empty() &&
                                            !ui.picker.ytm_busy)
                                        {
                                                ui.picker.ytm_busy = true;
                                                ui.picker.status = "Buscando / searching / buscando / 搜索中 ...";
                                                bg_tasks.push_back(
                                                    ytm_search_task(ui.picker.filter, ui.picker));
                                                bg_tasks.back().start();
                                        }
                                        else if (ev.key == tui::Key::Backspace && !ui.picker.filter.empty())
                                                ui.picker.filter.pop_back();
                                        else if (ev.key == tui::Key::Char && ev.ch >= 0x20 && ev.ch < 0x7f)
                                                ui.picker.filter += static_cast<char>(ev.ch);
                                        else
                                                handled = false;
                                }
                                else
                                {
                                        // PT: fase de resultados: filtra em
                                        //   tempo real, navega e Enter baixa+toca.
                                        // EN: results phase: live filtering,
                                        //   navigation, Enter downloads+plays.
                                        if (ev.key == tui::Key::Up && ui.picker.sel > 0)
                                                --ui.picker.sel;
                                        else if (ev.key == tui::Key::Down &&
                                                 ui.picker.sel + 1 < ui.picker.ytm_filtered.size())
                                                ++ui.picker.sel;
                                        else if (ev.key == tui::Key::Enter &&
                                                 !ui.picker.ytm_filtered.empty() && !ui.picker.ytm_busy)
                                        {
                                                const auto track = ui.picker.ytm_results[static_cast<std::size_t>(
                                                    ui.picker.ytm_filtered[ui.picker.sel])];
                                                ui.picker.ytm_busy = true;
                                                ui.picker.status =
                                                    "Baixando / downloading / descargando / 下载中 ...";
                                                bg_tasks.push_back(ytm_download_task(
                                                    track, engine, ui.picker, settings, tracks, &ui));
                                                bg_tasks.back().start();
                                        }
                                        else if (ev.key == tui::Key::Backspace && !ui.picker.filter.empty())
                                        {
                                                ui.picker.filter.pop_back();
                                                filter_ytm(ui.picker);
                                        }
                                        else if (ev.key == tui::Key::Char && ev.ch >= 0x20 && ev.ch < 0x7f)
                                        {
                                                ui.picker.filter += static_cast<char>(ev.ch);
                                                filter_ytm(ui.picker);
                                        }
                                        else
                                                handled = false;
                                }
                        }
                        else if (ev.key == tui::Key::Enter && !ui.picker.matches.empty() &&
                                 ui.picker.sel < ui.picker.matches.size())
                        {
                                const auto &t = tracks[static_cast<std::size_t>(
                                    ui.picker.matches[ui.picker.sel].index)];
                                engine.enqueue(t.path);
                                engine.next(); // PT: troca imediatamente / EN: switch immediately
                                engine.play();
                                ui.picker.active = false;
                        }
                        else if (ev.key == tui::Key::Up && ui.picker.sel > 0)
                                --ui.picker.sel;
                        else if (ev.key == tui::Key::Down &&
                                 ui.picker.sel + 1 < ui.picker.matches.size())
                                ++ui.picker.sel;
                        else if (ev.key == tui::Key::Backspace && !ui.picker.filter.empty())
                        {
                                ui.picker.filter.pop_back();
                                ui.picker.sel = 0;
                                recompute();
                        }
                        else if (ev.key == tui::Key::Char && ev.ch >= 0x20 && ev.ch < 0x7f)
                        {
                                ui.picker.filter += static_cast<char>(ev.ch);
                                ui.picker.sel = 0;
                                recompute();
                        }
                        else
                                handled = false;
                        if (handled)
                        {
                                ui.draw();
                                continue;
                        }
                }
                // PT: Modo playlist: teclas crúas navegam a snapshot; Enter pula
                //     para o item (jump_to), ESC fecha.
                // EN: Playlist mode: raw keys navigate the snapshot; Enter jumps
                //     to the item (jump_to), ESC closes.
                // ES: Modo playlist: teclas crudas navegan; Enter salta, ESC cierra.
                // ZH: 播放列表模式：原始按键导航快照；Enter 跳转，ESC 关闭。
                if (ui.playlist.active)
                {
                        bool handled = true;
                        if (ev.key == tui::Key::Esc)
                                ui.playlist = PlaylistPane{};
                        else if (ev.key == tui::Key::Up && ui.playlist.sel > 0)
                                --ui.playlist.sel;
                        else if (ev.key == tui::Key::Down && ui.playlist.sel + 1 < ui.playlist.items.size())
                                ++ui.playlist.sel;
                        else if (ev.key == tui::Key::Enter && !ui.playlist.items.empty() &&
                                 ui.playlist.sel < ui.playlist.items.size())
                        {
                                engine.jump_to(ui.playlist.sel);
                                ui.playlist = PlaylistPane{};
                        }
                        else
                                handled = false;
                        if (handled)
                        {
                                ui.draw();
                                continue;
                        }
                }
                if (ui.lyrics.active && ev.key == tui::Key::Esc)
                {
                        ui.lyrics.active = false;
                        ui.draw();
                        continue;
                }
                if (ui.trackinfo.active && ev.key == tui::Key::Esc)
                {
                        ui.trackinfo = TrackPane{};
                        ui.draw();
                        continue;
                }
                if (ui.cover.active && ev.key == tui::Key::Esc)
                {
                        ui.cover.active = false;
                        ui.draw();
                        continue;
                }

                if (const auto action = map.resolve(ev))
                {
                        switch (*action)
                        {
                        case app::Event::Quit:
                                alive.store(false, std::memory_order_release);
                                engine.shutdown();
                                engine.pause();
                                krt::Scheduler::instance().request_stop();
                                co_return;
                        case app::Event::TogglePause:
                                if (engine.is_playing())
                                        engine.pause();
                                else
                                        engine.play();
                                break;
                        case app::Event::Next:
                                engine.next();
                                break;
                        case app::Event::SeekBack:
                                engine.seek_by(std::chrono::seconds{-5});
                                break;
                        case app::Event::SeekForward:
                                engine.seek_by(std::chrono::seconds{5});
                                break;
                        case app::Event::VolUp:
                                engine.set_volume(engine.volume() + 0.05f);
                                break;
                        case app::Event::VolDown:
                                engine.set_volume(engine.volume() - 0.05f);
                                break;
                        case app::Event::Library:
                                ui.picker.active = true;
                                ui.picker.filter.clear();
                                ui.picker.sel = 0;
                                recompute();
                                break;
                        case app::Event::Lyrics:
                        {
                                const auto cur = engine.current();
                                if (!cur.path.empty())
                                {
                                        ui.lyrics = LyricsPane{};
                                        ui.lyrics.active = true;
                                        ui.lyrics.loading = true;
                                        const auto [artist, title] = split_artist_title(cur.title);
                                        const int dur = static_cast<int>(
                                            std::chrono::duration_cast<std::chrono::seconds>(cur.duration)
                                                .count());
                                        bg_tasks.push_back(fetch_lyrics_task(artist, title, dur, ui.lyrics, settings));
                                        bg_tasks.back().start();
                                }
                                break;
                        }
                        case app::Event::YtmSearch:
                                // PT: F8: prompt de busca YTM (rede em
                                //     background; download na seleção).
                                // EN: F8: YTM search prompt (background
                                //     network; download on selection).
                                // ES: F8: prompt de búsqueda YTM.
                                // ZH: F8：YTM 搜索输入框（后台网络；选中即下载）。
                                if (!ui.picker.ytm_busy)
                                {
                                        ui.picker = Picker{};
                                        ui.picker.active = true;
                                        ui.picker.source = PickerSource::Ytm;
                                        ui.picker.ytm_prompt = true;
                                }
                                break;
                        case app::Event::CycleViz:
                                // PT: cicla Off -> Spectrum -> Spectrogram -> Off e
                                //     espelha na janela gráfica (settings.visualizer
                                //     = off mantém tudo desligado).
                                // EN: cycles Off -> Spectrum -> Spectrogram -> Off
                                //     and mirrors on the GUI window (settings.
                                //     visualizer = off keeps everything off).
                                // ES: cicla Off -> Spectrum -> Spectrogram -> Off y
                                //     espeja en la ventana gráfica.
                                // ZH: 循环 Off -> Spectrum -> Spectrogram -> Off，并
                                //     同步到图形窗口（settings.visualizer=off 全关）。
                                if (settings.visualizer)
                                {
                                        switch (ui.viz.mode())
                                        {
                                        case viz::Mode::Off:
                                                ui.viz.set_mode(viz::Mode::Spectrum);
                                                break;
                                        case viz::Mode::Spectrum:
                                                ui.viz.set_mode(viz::Mode::Spectrogram);
                                                break;
                                        case viz::Mode::Spectrogram:
                                                ui.viz.set_mode(viz::Mode::Off);
                                                break;
                                        }
                                        ui.vizwin.set_mode(ui.viz.mode());
                                        krt::log::info("viz: modo / mode / modo / 模式: {}",
                                                       static_cast<int>(ui.viz.mode()));
                                }
                                break;
                        case app::Event::Playlist:
                                // PT: 'p': abre a playlist da fila (snapshot).
                                // EN: 'p': opens the queue playlist (snapshot).
                                // ES: 'p': abre la playlist de la cola.
                                // ZH: 'p'：打开队列播放列表（快照）。
                                ui.playlist.active = !ui.playlist.active;
                                if (ui.playlist.active)
                                {
                                        ui.playlist.items = engine.queue_snapshot();
                                        ui.playlist.sel = 0;
                                }
                                else
                                        ui.playlist = PlaylistPane{};
                                break;
                        case app::Event::TrackInfo:
                                // PT: 'i': detalhe da faixa atual.
                                // EN: 'i': current track detail.
                                // ES: 'i': detalle de la pista actual.
                                // ZH: 'i'：当前曲目详情。
                                ui.trackinfo.active = !ui.trackinfo.active;
                                break;
                        case app::Event::ToggleCover:
                                // PT: 'c': capa chafa na TUI (1 render por faixa).
                                // EN: 'c': chafa cover in the TUI (1 render per track).
                                // ES: 'c': carátula chafa en la TUI.
                                // ZH: 'c'：TUI chafa 封面（每曲目渲染一次）。
                                ui.cover.active = !ui.cover.active;
                                break;
                        case app::Event::CycleTheme:
                                // PT: 't': cicla temas ao vivo (nome aparece na
                                //     linha de config). --theme escolhe o inicial.
                                // EN: 't': cycles themes live (name shows on the
                                //     config line). --theme picks the initial one.
                                // ES: 't': cicla temas en vivo.
                                // ZH: 't'：实时循环主题（名称显示在配置行）。
                                ui.apply_theme(ui.next_theme_id());
                                krt::log::info("theme: / tema / tema / 主题: {}",
                                               ui.theme ? ui.theme->id : "default");
                                break;
                        case app::Event::ToggleTranslation:
                                // PT: F9 alterna letra original/traduzida na view
                                //     de letras (se a tradução chegou).
                                // EN: F9 toggles original/translated lyrics in the
                                //     lyrics view (if the translation arrived).
                                // ES: F9 alterna letra original/traducida.
                                // ZH: F9 在歌词视图中切换原文/译文（若译文已到）。
                                if (ui.lyrics.active && ui.lyrics.has_translation)
                                {
                                        ui.lyrics.show_translation = !ui.lyrics.show_translation;
                                        krt::log::info("lyrics: traducao / translation / traduccion / 译文: {}",
                                                       ui.lyrics.show_translation ? "on" : "off");
                                }
                                break;
                        default:
                                break;
                        }
                }
                else if (ev.key == tui::Key::Char && ev.ch > 0x20 && ev.ch < 0x7f)
                        ui.last_key = std::string(1, static_cast<char>(ev.ch));
                ui.draw();
        }
}

} // namespace

// PT: Idioma do mini-MAN: pt/en/es/zh a partir de LANG/LC_ALL/
//     LC_MESSAGES (prefixo, case-insensitive); pt = fallback.
// EN: Mini-MAN language: pt/en/es/zh from LANG/LC_ALL/LC_MESSAGES
//     (prefix, case-insensitive); pt = fallback.
// ZH: mini man 语言：取自 LANG/LC_ALL/LC_MESSAGES 前缀（pt/en/es/zh），
//     默认 pt。
std::string ui_lang()
{
        for (const char *var : {"LC_ALL", "LC_MESSAGES", "LANG"})
                if (const char *v = std::getenv(var))
                {
                        std::string s(v);
                        for (auto &c : s)
                                c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
                        for (const char *code : {"pt", "en", "es", "zh"})
                                if (s.rfind(code, 0) == 0)
                                        return code;
                }
        return "pt";
}

// PT: Prévia dos temas no terminal (--themes): nome, "Aa" no accent, mini
//     barra filled/empty e o gradiente da paleta do visualizador. Cores
//     truecolor só quando stdout é TTY; em pipe, só os nomes.
// EN: Terminal theme preview (--themes): name, "Aa" in accent, a mini
//     filled/empty bar and the visualizer palette gradient. Truecolor
//     only when stdout is a TTY; names only in pipes.
// ZH: 终端主题预览（--themes）：名称、accent 色 "Aa"、迷你进度条与可视化
//     调色板渐变。仅 stdout 为 TTY 时输出真彩色；管道只输出名称。
void print_themes_preview(const std::vector<app::Theme> &themes)
{
        const bool ansi = ::isatty(fileno(stdout));
        const auto pick = [&themes](const app::Theme &t, std::string_view key, std::string_view alt) {
                const auto it = t.colors.find(std::string(key));
                if (it != t.colors.end())
                        return it->second;
                const auto a = t.colors.find(std::string(alt));
                if (a != t.colors.end())
                        return a->second;
                return app::default_theme().get(key).rgb;
        };
        const auto lerp = [](const tui::Rgb &a, const tui::Rgb &b, double u) {
                return tui::Rgb{static_cast<std::uint8_t>(std::lround(a.r + (b.r - a.r) * u)),
                                static_cast<std::uint8_t>(std::lround(a.g + (b.g - a.g) * u)),
                                static_cast<std::uint8_t>(std::lround(a.b + (b.b - a.b) * u))};
        };
        const auto sgr_fg = [](const tui::Rgb &c) { return std::format("\x1b[38;2;{};{};{}m", c.r, c.g, c.b); };
        const auto sgr_bg = [](const tui::Rgb &c) { return std::format("\x1b[48;2;{};{};{}m", c.r, c.g, c.b); };
        std::printf("%-18s %-14s %s\n", "id", "title / título / 名称", "preview / prévia / 预览");
        for (const auto &t : themes)
        {
                const tui::Rgb accent = t.get("accent").set ? t.get("accent").rgb
                                                            : t.get("nowplaying").set ? t.get("nowplaying").rgb
                                                                                      : tui::Rgb{210, 210, 210};
                const tui::Rgb filled = t.get("progress.filled").set
                                            ? t.get("progress.filled").rgb
                                            : app::default_theme().get("progress.filled").rgb;
                const tui::Rgb empty = t.get("progress.empty").set
                                           ? t.get("progress.empty").rgb
                                           : app::default_theme().get("progress.empty").rgb;
                const tui::Rgb lo = pick(t, "viz.low", "trackview.visualizer");
                const tui::Rgb hi = pick(t, "viz.high", "accent");
                const tui::Rgb peak = pick(t, "viz.peak", "accent");
                std::string line = std::format("{:<18} {:<14} ", t.id, t.title);
                if (ansi)
                {
                        line += sgr_fg(accent) + "Aa\x1b[0m  ";
                        for (int i = 0; i < 6; ++i)
                                line += sgr_bg(i < 3 ? filled : empty) + " \x1b[0m";
                        line += "  ";
                        for (int i = 0; i < 8; ++i)
                                line += sgr_bg(lerp(lo, hi, i / 7.0)) + " \x1b[0m";
                        line += sgr_fg(peak) + "^\x1b[0m";
                }
                else
                        line += "(colors / cores / 彩色 — TTY only)";
                std::printf("%s\n", line.c_str());
        }
}

int main(int argc, char **argv)
{
        audio::Engine engine;
        app::Settings early_settings;
        // PT: Mini-MAN (--help) e a vitrine de temas (--themes) rodam sem
        //     TUI/MPRIS: puros printf no stdout, exit 0.
        // EN: Mini-MAN (--help) and the theme showcase (--themes) run
        //     without TUI/MPRIS: plain printf to stdout, exit 0.
        // ZH: mini man（--help）与主题橱窗（--themes）不经 TUI/MPRIS：
        //     直接打印到 stdout 后 exit 0。
        if (argc >= 2 && (std::string_view(argv[1]) == "--help" || std::string_view(argv[1]) == "-h"))
        {
                app::print_man(stdout, ui_lang());
                return 0;
        }
        if (argc >= 2 && std::string_view(argv[1]) == "--themes")
        {
                print_themes_preview(app::load_themes());
                return 0;
        }
        // PT: Parse flexível: flags e subcomandos em QUALQUER ordem
        //     (`kiutube --theme gruvbox play a.flac` == `play a.flac
        //     --theme gruvbox`). `--` separa flags de nomes de arquivo.
        // EN: Flexible parsing: flags and subcommands in ANY order
        //     (`kiutube --theme gruvbox play a.flac` == `play a.flac
        //     --theme gruvbox`). `--` separates flags from file names.
        // ZH: 灵活解析：标志与子命令可任意排列；`--` 分隔标志与文件名。
        bool want_play = false;
        bool after_dashdash = false;
        std::vector<std::string> play_files;
        for (int i = 1; i < argc; ++i)
        {
                const std::string_view a(argv[i]);
                if (after_dashdash)
                {
                        play_files.emplace_back(argv[i]);
                        continue;
                }
                if (a == "--")
                {
                        after_dashdash = true;
                        continue;
                }
                if (a == "--theme" && i + 1 < argc)
                        early_settings.theme = argv[++i];
                else if (a == "play")
                        want_play = true;
                else if (a == "--ytm" && i + 1 < argc)
                {
                        // PT: cli_ytm espera o argv COMPLETO (argv[1] ==
                        //     "--ytm", argv[2] = subcomando): repassa com
                        //     argv[0] real — argv+i-1 / argc-i+1.
                        // EN: cli_ytm expects the FULL argv (argv[1] ==
                        //     "--ytm", argv[2] = subcommand): pass it with
                        //     the real argv[0] — argv+i-1 / argc-i+1.
                        // ZH: cli_ytm 期望完整 argv（argv[1]=="--ytm"）：
                        //     用 argv+i-1 / argc-i+1 传真实 argv[0]。
                        const int rc = cli_ytm(argc - i + 1, argv + i - 1, engine);
                        if (rc >= 0)
                                return rc; // PT: search/erro: sai; play cai no TUI abaixo
                }
                else if (want_play)
                        play_files.emplace_back(argv[i]);
        }
        // PT: stderr é TTY (TUI a seguir): logs do krt E warnings de
        //     bibliotecas (Qt/gtk) vão para arquivo — escrever no stderr
        //     corromperia a tela alternativa. Em pipe/arquivo, mantém o
        //     stderr (amigável para CI e depuração).
        // EN: stderr is a TTY (TUI follows): krt logs AND library warnings
        //     (Qt/gtk) go to a file — writing to stderr would corrupt the
        //     alternate screen. On pipes/files, keep stderr (CI- and
        //     debug-friendly).
        // ZH: stderr 为 TTY（随后进入 TUI）：krt 日志与库警告（Qt/gtk）写入
        //     文件——写 stderr 会破坏备用屏幕。管道/文件时保持 stderr。
        if (::isatty(fileno(stderr)))
        {
                const auto log_path = app::cache_dir("kiutube") / "kiutube.log";
                if (krt::log::use_file(log_path.string()))
                {
                        std::fflush(stderr);
                        dup2(fileno(krt::log::sink_slot()), STDERR_FILENO);
                        krt::log::info("log: desviado p/ / redirected to / desviado a / 重定向到: {}",
                                       log_path.string());
                }
        }
        // PT: KIUTUBE_NERD_FONTS=1 liga os ícones sem mexer na config.
        // EN: KIUTUBE_NERD_FONTS=1 enables icons without touching config.
        // ZH: KIUTUBE_NERD_FONTS=1 无需改配置即可启用图标。
        if (std::getenv("KIUTUBE_NERD_FONTS"))
                early_settings.nerd_fonts = true;
        sys::Mpris mpris(engine); // PT: thread própria; dtor junta antes do engine / own thread; joins before the engine
        if (want_play)
        {
                for (const auto &f : play_files)
                        engine.enqueue(f);
                engine.play();
        }

        try
        {
                Ui ui(engine, early_settings);
                std::atomic<bool> alive{true};
                auto app = app_main(engine, ui, alive);
                auto clock = ui_clock(ui, alive);
                auto feeder = engine.feeder_task();
                app.start();
                clock.start();
                feeder.start();
                krt::Scheduler::instance().run();
                return 0;
        }
        catch (const std::exception &e)
        {
                std::fprintf(stderr, "kiutube: %s\n", e.what());
                return 1;
        }
}
