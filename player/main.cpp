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
#include "krt/log.hpp"
#include "krt/scheduler.hpp"
#include "library/fuzzy.hpp"
#include "library/library.hpp"
#include "net/lrclib.hpp"
#include "script/lua_state.hpp"
#include "tui/cell_buffer.hpp"
#include "tui/term.hpp"

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

// PT: Estado do picker de biblioteca (compartilhado com o relógio de UI).
// EN: Library picker state (shared with the UI clock).
// ES: Estado del picker de biblioteca (compartido con el reloj de UI).
// ZH: 曲库选择器状态（与 UI 时钟共享）。
struct Picker
{
        bool active = false;
        std::string filter;
        std::vector<library::Scored> matches;
        std::size_t sel = 0;
};

// PT: Estado da view de letras. / EN: Lyrics view state.
// ES: Estado de la vista de letras. / ZH: 歌词视图状态。
struct LyricsPane
{
        bool active = false;
        std::string text;
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

        Ui(audio::Engine &e, app::Settings &s) : buf(24, 100), engine(e), settings(s)
        {
                buf = tui::CellBuffer(term.rows(), term.cols());
        }

        void draw()
        {
                const auto cur = engine.current();
                const auto pos = engine.position();
                buf.clear();
                buf.put(1, 2, "kiutube2 - M3 / PT / EN / ES / ZH");
                buf.put(3, 2, "PT: biblioteca fuzzy (Typesense) + letras LRCLIB");
                buf.put(4, 2, "EN: fuzzy library (Typesense) + LRCLIB lyrics");
                buf.put(5, 2, "ES: biblioteca fuzzy (Typesense) + letras LRCLIB");
                buf.put(6, 2, "ZH: Typesense \xe6\xa8\xa1\xe7\xb3\x8a\xe6\x90\x9c\xe7\xb4\xa2\xe6\x9b\xb2\xe5\xba\x93 + LRCLIB \xe6\xad\x8c\xe8\xaf\x8d");
                if (cur.path.empty())
                {
                        buf.put(8, 2, "kiutube2 play <arquivo / file / archivo / \xe6\x96\x87\xe4\xbb\xb6> ...   keys: l/g");
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

                if (picker.active && tracks)
                        draw_picker();
                if (lyrics.active)
                        draw_lyrics();
                term.write_segments(buf.flush_diff());
        }

        void draw_picker()
        {
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
                std::string_view rest(lyrics.text);
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
        while (alive.load(std::memory_order_acquire))
        {
                co_await krt::sleep(250ms);
                ui.draw();
        }
}

// PT: Busca a letra em background (rede síncrona dentro de offload) e
//     preenche o painel — o relógio de UI redesenha quando chegar.
// EN: Fetches lyrics in the background (sync network inside offload) and
//     fills the pane — the UI clock redraws when it arrives.
// ES: Busca la letra en background (red síncrona dentro de offload) y llena
//     el panel — el reloj de UI redibuja cuando llegue.
// ZH: 后台获取歌词（同步网络置于 offload 中）并填充面板 —— UI 时钟在到达后
//     重绘。
krt::Task<void> fetch_lyrics_task(std::string artist, std::string title, int duration_seconds,
                                  LyricsPane &pane)
{
        auto lrc = co_await krt::offload([a = std::move(artist), t = std::move(title), duration_seconds] {
                return net::fetch_lyrics(a, t, duration_seconds);
        });
        pane.loading = false;
        if (lrc.has_value())
                pane.text = std::move(*lrc);
        else
                pane.not_found = true;
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

        std::vector<krt::Task<void>> bg_tasks; // PT: lyrics tasks mantidas vivas / kept alive

        auto recompute = [&] {
                ui.picker.matches = library::fuzzy_filter(tracks, ui.picker.filter);
                if (ui.picker.sel >= ui.picker.matches.size())
                        ui.picker.sel = 0;
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
                                ui.picker.active = false;
                        else if (ev.key == tui::Key::Enter && !ui.picker.matches.empty() &&
                                 ui.picker.sel < ui.picker.matches.size())
                        {
                                const auto &t = tracks[static_cast<std::size_t>(
                                    ui.picker.matches[ui.picker.sel].index)];
                                engine.enqueue(t.path);
                                engine.next(); // PT: troca imediatamente / EN: switch immediately / ZH: 立即切换
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
                                        bg_tasks.push_back(fetch_lyrics_task(artist, title, dur, ui.lyrics));
                                        bg_tasks.back().start();
                                }
                                break;
                        }
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
        app::Settings early_settings;
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
