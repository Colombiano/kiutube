// PT: kiutube2 — marco M2: engine de áudio real. `kiutube2 play <arq...>`
//     enfileira e toca (wav/mp3/flac via miniaudio); a TUI mostra faixa,
//     posição, estado e volume; space/n/setas/+/- controlam o transporte.
//     Gate M2: sink input ativo no PipeWire (pactl) com o processo tocando.
// EN: kiutube2 — milestone M2: real audio engine. `kiutube2 play <file...>`
//     enqueues and plays (wav/mp3/flac via miniaudio); the TUI shows track,
//     position, state and volume; space/n/arrows/+/- control transport.
//     M2 gate: active sink input on PipeWire (pactl) while the process plays.
// ES: kiutube2 — hito M2: engine de audio real. `kiutube2 play <archivo...>`
//     encola y reproduce (wav/mp3/flac vía miniaudio); la TUI muestra pista,
//     posición, estado y volumen; space/n/flechas/+/- controlan el transporte.
//     Gate M2: sink input activo en PipeWire (pactl) con el proceso sonando.
// ZH: kiutube2 —— 里程碑 M2：真实音频引擎。`kiutube2 play <文件...>` 入队并播放
//     （经 miniaudio 的 wav/mp3/flac）；TUI 显示曲目、位置、状态与音量；
//     space/n/方向键/+/- 控制传输。M2 门槛：进程播放时 PipeWire 上存在活跃
//     sink input（pactl 验证）。
#include "app/bindings.hpp"
#include "app/paths.hpp"
#include "app/settings.hpp"
#include "audio/engine.hpp"
#include "krt/log.hpp"
#include "krt/scheduler.hpp"
#include "script/lua_state.hpp"
#include "tui/cell_buffer.hpp"
#include "tui/term.hpp"

#include <chrono>
#include <atomic>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <format>
#include <string>
#include <string_view>
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

// PT: kewrc legado (read-only) — igual ao M1. / EN: legacy kewrc (read-only) — same as M1.
// ES: kewrc legado (read-only) — igual al M1. / ZH: 旧 kewrc（只读）—— 与 M1 相同。
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

// PT: mm:ss neutro (sem i18n). / EN: neutral mm:ss (no i18n).
// ES: mm:ss neutro (sin i18n). / ZH: 中性 mm:ss（无 i18n）。
std::string mmss(std::chrono::microseconds us)
{
        const auto s = std::chrono::duration_cast<std::chrono::seconds>(us).count();
        return std::format("{:02}:{:02}", s / 60, s % 60);
}

// PT: Tela M2 compartilhada entre o loop de teclas e o relógio de UI — as
//     duas corrotinas rodam na MESMA thread (scheduler), sem race.
// EN: M2 screen shared by the key loop and the UI clock — both coroutines
//     run on the SAME thread (scheduler), race-free.
// ES: Pantalla M2 compartida entre el bucle de teclas y el reloj de UI — las
//     dos corrutinas corren en el MISMO hilo (scheduler), sin race.
// ZH: M2 屏幕由按键循环与 UI 时钟共享 —— 两个协程运行在相同线程（调度器）上，
//     无竞争。
struct Ui
{
        tui::Term term;
        tui::CellBuffer buf;
        audio::Engine &engine;
        app::Settings &settings;
        std::string last_key = "-";

        Ui(audio::Engine &e, app::Settings &s, int rows_hint = 0) : buf(24, 100), engine(e), settings(s)
        {
                (void)rows_hint;
                buf = tui::CellBuffer(term.rows(), term.cols());
        }

        void draw()
        {
                const auto cur = engine.current();
                const auto pos = engine.position();
                // PT: Telemetria do Heisenbug: valores brutos a cada redraw.
                // EN: Heisenbug telemetry: raw values at every redraw.
                // ES: Telemetría del Heisenbug: valores brutos en cada redraw.
                // ZH: Heisenbug 遥测：每次重绘的原始值。
                krt::log::debug("ui: pos={}us playing={} cur={} queue={}", pos.count(), engine.is_playing(),
                                cur.path.empty() ? "none" : "set", engine.queue_size());
                buf.clear();
                buf.put(1, 2, "kiutube2 - M2 / PT / EN / ES / ZH");
                buf.put(3, 2, "PT: engine de audio (miniaudio) / EN: audio engine / ES: motor de audio / ZH: \xe9\x9f\xb3\xe9\xa2\x91\xe5\xbc\x95\xe6\x93\x8e");
                if (cur.path.empty())
                {
                        buf.put(5, 2, "kiutube2 play <arquivo / file / archivo / \xe6\x96\x87\xe4\xbb\xb6> ...");
                        buf.put(6, 2, "key " + last_key); // PT: eco de tecla também sem faixa. EN: key echo also without a track.
                                                          // ES: eco de tecla también sin pista. / ZH: 无曲目时同样回显按键。
                }
                else
                {
                        buf.put(5, 2, cur.title);
                        // PT: pos exibida limitada à duração: o ring pode continuar
                        //     tocando após o EOF do decoder (gapless/dreno).
                        // EN: displayed position clamped to duration: the ring may
                        //     keep playing past the decoder EOF (gapless/drain).
                        // ES: pos mostrada limitada a la duración: el ring puede
                        //     seguir sonando tras el EOF del decoder (gapless/dreno).
                        // ZH: 显示位置限制在时长内：解码器 EOF 后环形缓冲可能继续
                        //     播放（gapless/排空）。
                        const auto shown_pos = std::min(pos, cur.duration);
                        const std::string line = mmss(shown_pos) + " / " + mmss(cur.duration) +
                                                 (engine.is_playing() ? "  |>" : "  ||") + "  vol " +
                                                 std::to_string(static_cast<int>(engine.volume() * 100.0f + 0.5f)) +
                                                 "%  queue " + std::to_string(engine.queue_size()) +
                                                 "  key " + last_key;
                        buf.put(6, 2, line);
                }
                // PT: Linha de config SEMPRE visível (gates M1 dependem dela).
                // EN: Config line always visible (M1 gates depend on it).
                // ES: Línea de config SIEMPRE visible (los gates M1 dependen de ella).
                // ZH: 配置行始终可见（M1 门槛依赖它）。
                std::string cfg = "volume=" + std::to_string(settings.volume) + " shuffle=" +
                                  (settings.shuffle ? "on" : "off") + " visualizer=" +
                                  (settings.visualizer ? "on" : "off") + " bindings ok";
                buf.put(8, 2, cfg);
                if (!settings.library_path.empty())
                        buf.put(9, 2, "path=" + settings.library_path);
                term.write_segments(buf.flush_diff());
        }
};

krt::Task<void> ui_clock(Ui &ui, std::atomic<bool> &alive)
{
        // PT: Relógio: redesenha 4x/s para a posição avançar na tela; para
        //     quando alive baixa (senão seus timers segurariam o run-loop).
        // EN: Clock: redraws 4x/s so the position advances on screen; stops
        //     when alive drops (otherwise its timers would hold the run-loop).
        // ES: Reloj: redibuja 4x/s para que la posición avance en pantalla;
        //     para cuando alive baja (si no, sus timers retendrían el run-loop).
        // ZH: 时钟：每秒 4 次重绘，使位置在屏幕上前进；alive 置假后停止
        //     （否则它的定时器会拖住 run-loop）。
        while (alive.load(std::memory_order_acquire))
        {
                co_await krt::sleep(250ms);
                ui.draw();
        }
}

krt::Task<void> app_main(audio::Engine &engine, Ui &ui, std::atomic<bool> &alive)
{
        krt::log::info("boot: kiutube2 M2");

        app::Settings settings;
        app::BindingMap map;
        map.load_defaults();
        load_kewrc(app::config_dir("kew") / "kewrc", settings, map);
        script::LuaState lua;
        lua.open_config((app::config_dir("kiutube2") / "init.lua").string(), settings, map);
        ui.settings = settings;
        ui.draw();

        for (;;)
        {
                const auto ev = co_await ui.term.next_event();
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

// PT: main carrega o engine ANTES das tasks: destruição em ordem reversa
//     garante que feeder/app morram antes do engine (elas o referenciam).
// EN: main declares the engine BEFORE the tasks: reverse destruction order
//     guarantees feeder/app die before the engine (they reference it).
// ES: main declara el engine ANTES de las tasks: la destrucción en orden
//     inverso garantiza que feeder/app mueran antes del engine (lo referencian).
// ZH: main 在 tasks 之前声明 engine：逆序销毁保证 feeder/app 先于 engine
//     销毁（它们引用 engine）。
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
