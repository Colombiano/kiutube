// PT: kiutube2 — marco M3b/M3c: biblioteca com busca fuzzy (métrica do
//     Typesense) via picker na TUI + letras do LRCLIB com view própria.
//     'l' abre a biblioteca (digite para filtrar, Enter toca, ESC fecha);
//     'g' busca a letra da faixa atual (rede em offload, nunca bloqueia).
// EN: kiutube2 — milestone M3b/M3c: library with Typesense-metric fuzzy
//     search through an in-TUI picker + LRCLIB lyrics with their own view.
//     'l' opens the library (type to filter, Enter plays, ESC closes); 'g'
//     fetches lyrics for the current track (network offloaded, never blocks).
// ES: kiutube2 — hito M3b/M3c: biblioteca con búsqueda fuzzy (métrica de
//     Typesense) vía picker en la TUI + letras de LRCLIB con vista propia.
//     'l' abre la biblioteca (teclea para filtrar, Enter reproduce, ESC
//     cierra); 'g' busca la letra de la pista actual (red en offload).
// ZH: kiutube2 —— 里程碑 M3b/M3c：带模糊搜索（Typesense 度量）的曲库
//     TUI 选择器 + LRCLIB 歌词视图。'l' 打开曲库（输入即过滤，Enter 播放，
//     ESC 关闭）；'g' 获取当前曲目歌词（网络经 offload，绝不阻塞）。
#include "app/bindings.hpp"
#include "app/paths.hpp"
#include "app/settings.hpp"
#include "audio/engine.hpp"
#include "gui/viz_window.hpp"
#include "krt/log.hpp"
#include "krt/scheduler.hpp"
#include "library/fuzzy.hpp"
#include "library/library.hpp"
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
#include <cstdio>
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
                std::fprintf(stderr, "uso / usage / uso / 用法: kiutube2 --ytm search|play \"<query>\"\n");
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
                                                 ? std::string(std::getenv("XDG_CACHE_HOME")) + "/kiutube2/streams"
                                                 : std::string(std::getenv("HOME")) + "/.cache/kiutube2/streams";
                const auto &t = (*r)[0];
                const auto path =
                        net::ytm_download(t.video_id, base_dir + "/" + sanitize_filename(t.artist) + " - " +
                                                          sanitize_filename(t.title) + " [" + t.video_id + "]");
                if (!path.has_value())
                        return 1;
                std::fprintf(stderr, "kiutube2: tocando / playing / reproduciendo / 播放: %s\n",
                             path->c_str());
                engine.enqueue(*path);
                engine.play();
                return -1; // PT: segue pro TUI / continue to the TUI / 进入 TUI
        }
        std::fprintf(stderr, "uso / usage / uso / 用法: kiutube2 --ytm search|play \"<query>\"\n");
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
        }

        ~Ui() = default; // PT: vizwin (membro último) é destruída primeiro, fechando a thread GUI. / vizwin (last member) is destroyed first, closing the GUI thread.

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
                buf.put(1, 2, "kiutube2 - M3+viz / PT / EN / ES / ZH");
                buf.put(3, 2, "PT: biblioteca fuzzy + letras + visualizadores");
                buf.put(4, 2, "EN: fuzzy library + lyrics + visualizers");
                buf.put(5, 2, "ES: biblioteca fuzzy + letras + visualizadores");
                buf.put(6, 2, "ZH: \xe6\xa8\xa1\xe7\xb3\x8a\xe6\x90\x9c\xe7\xb4\xa2 + \xe6\xad\x8c\xe8\xaf\x8d + \xe5\x8f\xaf\xe8\xa7\x86\xe5\x8c\x96");
                if (cur.path.empty())
                {
                        buf.put(8, 2, "kiutube2 play <arquivo / file / archivo / \xe6\x96\x87\xe4\xbb\xb6> ...   keys: l/g/v");
                        buf.put(9, 2, "key " + last_key);
                }
                else
                {
                        buf.put(8, 2, cur.title);
                        const auto shown_pos = std::min(pos, cur.duration);
                        const std::string line = mmss(shown_pos) + " / " + mmss(cur.duration) +
                                                 (engine.is_playing() ? "  |>" : "  ||") + "  vol " +
                                                 std::to_string(static_cast<int>(engine.volume() * 100.0f + 0.5f)) +
                                                 "%  queue " + std::to_string(engine.queue_size()) +
                                                 "  key " + last_key;
                        buf.put(9, 2, line);
                }
                std::string cfg = "volume=" + std::to_string(settings.volume) + " shuffle=" +
                                  (settings.shuffle ? "on" : "off") + " visualizer=" +
                                  (settings.visualizer ? "on" : "off") +
                                  (tracks ? " lib=" + std::to_string(tracks->size()) : "");
                buf.put(11, 2, cfg);

                // PT: Área do visualizador (rows 13..19). settings.visualizer
                //     = off força o modo Off.
                // EN: Visualizer area (rows 13..19). settings.visualizer = off
                //     forces Mode::Off.
                // ES: Área del visualizador (filas 13..19).
                // ZH: 可视化器区域（第 13..19 行）。settings.visualizer=off
                //     时强制关闭。
                if (!settings.visualizer && viz.mode() != viz::Mode::Off)
                        viz.set_mode(viz::Mode::Off);
                if (!gui_open && viz.mode() != viz::Mode::Off)
                        viz.draw(buf, 13, 2, 7, buf.cols() - 4);

                if (picker.active && (picker.source == PickerSource::Ytm || tracks))
                        draw_picker();
                if (lyrics.active)
                        draw_lyrics();
                term.write_segments(buf.flush_diff());
        }

        void draw_picker()
        {
                if (picker.source == PickerSource::Ytm)
                {
                        draw_ytm_picker();
                        return;
                }
                buf.put(13, 2, "LIB> " + picker.filter + "_");
                const std::size_t max_rows = 8;
                if (picker.sel >= picker.matches.size())
                        picker.sel = picker.matches.empty() ? 0 : picker.matches.size() - 1;
                const std::size_t start = picker.sel >= max_rows ? picker.sel - max_rows + 1 : 0;
                for (std::size_t r = 0; r < max_rows && start + r < picker.matches.size(); ++r)
                {
                        const auto &m = picker.matches[start + r];
                        const auto &t = (*tracks)[static_cast<std::size_t>(m.index)];
                        std::string line = (start + r == picker.sel ? "> " : "  ") + t.title;
                        if (!t.artist.empty())
                                line += "  --  " + t.artist;
                        buf.put(14 + r, 2, line);
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
                        buf.put(13, 2, "YTM> " + picker.filter + "_");
                        buf.put(14, 2, "Enter: buscar / search / buscar / 搜索   ESC: "
                                       "cancelar / cancel / cancelar / 取消");
                }
                else
                {
                        buf.put(13, 2, "YTM> " + picker.filter + "  (" +
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
                                std::string line = (start + r == picker.sel ? "> " : "  ") + t.title + "  --  " +
                                                   t.artist;
                                if (t.duration_seconds > 0)
                                        line += "  (" + mmss(std::chrono::seconds(t.duration_seconds)) + ")";
                                buf.put(14 + r, 2, line);
                        }
                }
                if (!picker.status.empty())
                        buf.put(23, 2, picker.status);
        }

        void draw_lyrics()
        {
                if (lyrics.loading)
                {
                        buf.put(13, 2, "LRCLIB... / fetching / buscando / 获取中");
                        return;
                }
                if (lyrics.not_found)
                {
                        buf.put(13, 2, "letra nao encontrada / lyrics not found / letra no encontrada / 未找到歌词");
                        return;
                }
                std::size_t row = 13;
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

// PT: Sanitiza nome de arquivo (mesmo jogo do sanitize do ytm: ASCII seguro).
// EN: Sanitizes a filename (same game as ytm's sanitize: safe ASCII).
// ES: Sanitiza nombre de archivo (mismo juego que el sanitize de ytm).
// ZH: 文件名清理（与 ytm 的 sanitize 相同：安全 ASCII）。
std::string sanitize_filename(const std::string &in)
{
        std::string out;
        for (const char c : in)
        {
                const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                                (c >= '0' && c <= '9') || c == ' ' || c == '-' || c == '_' || c == '(' || c == ')' ||
                                c == '[' || c == ']' || c == '.';
                out += ok ? c : '_';
        }
        return out;
}

krt::Task<void> ytm_download_task(net::YtmResult track, audio::Engine &engine, Picker &picker,
                                  app::Settings &settings, std::vector<library::Track> &tracks, Ui *ui_for_rescan)
{
        // PT: destino: library_path se houver, senão ~/.cache/kiutube2/streams.
        // EN: destination: library_path if set, else ~/.cache/kiutube2/streams.
        std::string dir = settings.library_path;
        if (dir.empty())
        {
                std::string base = std::getenv("XDG_CACHE_HOME") ? std::getenv("XDG_CACHE_HOME")
                                                                 : std::string(std::getenv("HOME")) + "/.cache";
                dir = base + "/kiutube2/streams";
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
        krt::log::info("boot: kiutube2 M3");

        app::Settings settings;
        app::BindingMap map;
        map.load_defaults();
        load_kewrc(app::config_dir("kew") / "kewrc", settings, map);
        script::LuaState lua;
        lua.open_config((app::config_dir("kiutube2") / "init.lua").string(), settings, map);
        ui.settings = settings;

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
                if (ui.lyrics.active && ev.key == tui::Key::Esc)
                {
                        ui.lyrics.active = false;
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

int main(int argc, char **argv)
{
        audio::Engine engine;
        sys::Mpris mpris(engine); // PT: thread própria; dtor junta antes do engine / own thread; joins before the engine
        app::Settings early_settings;
        if (argc >= 2 && std::string_view(argv[1]) == "--ytm")
        {
                const int rc = cli_ytm(argc, argv, engine);
                if (rc >= 0)
                        return rc; // PT: search/erro: sai; play cai no TUI abaixo
        }
        if (argc >= 3 && std::string_view(argv[1]) == "play")
        {
                for (int i = 2; i < argc; ++i)
                        engine.enqueue(argv[i]);
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
                std::fprintf(stderr, "kiutube2: %s\n", e.what());
                return 1;
        }
}
