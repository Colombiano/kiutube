// PT: kiutube2 — marco M1: configuração real. Lê o kewrc legado (compat),
//     sobrepõe o init.lua (~/.config/kiutube2/init.lua) com binds e settings,
//     e despacha teclas via BindingMap (defaults constexpr + overrides).
//     Prova o gate M1: tecla custom definida no Lua dispara ação.
// EN: kiutube2 — milestone M1: real configuration. Reads the legacy kewrc
//     (compat), overlays init.lua (~/.config/kiutube2/init.lua) with bindings
//     and settings, and dispatches keys through BindingMap (constexpr
//     defaults + overrides). Proves the M1 gate: a Lua-defined custom key
//     triggers an action.
// ES: kiutube2 — hito M1: configuración real. Lee el kewrc legado (compat),
//     superpone el init.lua (~/.config/kiutube2/init.lua) con bindings y
//     settings, y despacha teclas vía BindingMap (defaults constexpr +
//     overrides). Prueba el gate M1: una tecla custom del Lua dispara acción.
// ZH: kiutube2 —— 里程碑 M1：真实配置。读取旧的 kewrc（兼容），并用 init.lua
//     （~/.config/kiutube2/init.lua）覆盖绑定与 settings，再通过 BindingMap
//     分发按键（constexpr 默认值 + 覆盖）。验证 M1 门槛：Lua 定义的自定义
//     键触发动作。
#include "app/bindings.hpp"
#include "app/paths.hpp"
#include "app/settings.hpp"
#include "krt/log.hpp"
#include "krt/scheduler.hpp"
#include "script/lua_state.hpp"
#include "tui/cell_buffer.hpp"
#include "tui/term.hpp"

#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

namespace
{

// PT: trim de espaços para parse de linha key=value. / EN: whitespace trim for key=value line parsing.
// ES: trim de espacios para parse de línea key=value. / ZH: key=value 行解析用的空白裁剪。
std::string_view trim(std::string_view s)
{
        while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
                s.remove_prefix(1);
        while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r'))
                s.remove_suffix(1);
        return s;
}

// PT: Carrega o kewrc legado (read-only): key=value conhecidas viram settings;
//     bind=TECLA,EVENTO entra no BindingMap; resto é ignorado com log debug.
// EN: Loads the legacy kewrc (read-only): known key=value lines become
//     settings; bind=KEY,EVENT enters the BindingMap; the rest is ignored
//     with a debug log.
// ES: Carga el kewrc legado (read-only): key=value conocidas se vuelven
//     settings; bind=TECLA,EVENTO entra en el BindingMap; el resto se ignora
//     con log debug.
// ZH: 加载旧的 kewrc（只读）：已知的 key=value 变为 settings；
//     bind=键,事件 进入 BindingMap；其余记录 debug 日志后忽略。
void load_kewrc(const std::filesystem::path &path, app::Settings &st, app::BindingMap &map)
{
        std::ifstream f(path);
        if (!f)
        {
                krt::log::info("kewrc: nao encontrado / not found / no encontrado / 未找到: {}", path.string());
                return;
        }
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
                        else
                                krt::log::warn("kewrc: bind invalido / invalid / inválido / 无效: {}", std::string(value));
                        continue;
                }
                if (!app::apply_line(st, key, value))
                        krt::log::debug("kewrc: chave ignorada / ignored key / clave ignorada / 忽略键: {}", std::string(key));
        }
        krt::log::info("kewrc: carregado / loaded / cargado / 已加载: {}", path.string());
}

// PT: Identificador neutro da tecla (sem palavras traduzíveis; i18n das
//     strings de UI chega com catálogos no M4).
// EN: Neutral key identifier (no translatable words; UI string i18n lands
//     with catalogs in M4).
// ES: Identificador neutro de la tecla (sin palabras traducibles; el i18n de
//     cadenas de UI llega con catálogos en M4).
// ZH: 键的中性标识（不含可翻译词汇；UI 字符串 i18n 随 M4 的目录加入）。
std::string key_id(const tui::KeyEvent &ev)
{
        if (ev.key == tui::Key::Char)
                return std::string(1, static_cast<char>(ev.ch));
        switch (ev.key)
        {
        case tui::Key::Enter: return "Enter"; case tui::Key::Tab: return "Tab";
        case tui::Key::Backspace: return "Backspace"; case tui::Key::Esc: return "Esc";
        case tui::Key::Up: return "Up"; case tui::Key::Down: return "Down";
        case tui::Key::Left: return "Left"; case tui::Key::Right: return "Right";
        case tui::Key::Home: return "Home"; case tui::Key::End: return "End";
        case tui::Key::Insert: return "Insert"; case tui::Key::Delete: return "Delete";
        case tui::Key::PageUp: return "PageUp"; case tui::Key::PageDown: return "PageDown";
        case tui::Key::F1: return "F1"; case tui::Key::F2: return "F2";
        case tui::Key::F3: return "F3"; case tui::Key::F4: return "F4";
        case tui::Key::F5: return "F5"; case tui::Key::F6: return "F6";
        case tui::Key::F7: return "F7"; case tui::Key::F8: return "F8";
        case tui::Key::F9: return "F9"; case tui::Key::F10: return "F10";
        case tui::Key::F11: return "F11"; case tui::Key::F12: return "F12";
        default: return "?";
        }
}

krt::Task<void> app_main()
{
        krt::log::info("boot: kiutube2 M1");

        // PT: Config em camadas: defaults -> kewrc legado -> init.lua.
        // EN: Layered config: defaults -> legacy kewrc -> init.lua.
        // ES: Config en capas: defaults -> kewrc legado -> init.lua.
        // ZH: 分层配置：默认值 -> 旧 kewrc -> init.lua。
        app::Settings settings;
        app::BindingMap map;
        map.load_defaults();
        load_kewrc(app::config_dir("kew") / "kewrc", settings, map);

        script::LuaState lua;
        lua.open_config((app::config_dir("kiutube2") / "init.lua").string(), settings, map);

        krt::log::info("config: volume={} shuffle={} visualizer={} bindings={}", settings.volume,
                       settings.shuffle ? "on" : "off", settings.visualizer ? "on" : "off", map.size());

        tui::Term term;
        tui::CellBuffer buf(term.rows(), term.cols());
        int scroll = 0;
        std::string last = "-";

        auto draw = [&] {
                buf.clear();
                buf.put(1, 2, "kiutube2 - M1 / PT / EN / ES / ZH");
                buf.put(3, 2, "PT: config em camadas (kewrc + init.lua)");
                buf.put(4, 2, "EN: layered config (kewrc + init.lua)");
                buf.put(5, 2, "ES: config en capas (kewrc + init.lua)");
                buf.put(6, 2, "ZH: \xe5\x88\x86\xe5\xb1\x82\xe9\x85\x8d\xe7\xbd\xae\xef\xbc\x88kewrc + init.lua\xef\xbc\x89");
                std::string status = "volume=" + std::to_string(settings.volume) + " shuffle=" +
                                     (settings.shuffle ? "on" : "off") + " visualizer=" +
                                     (settings.visualizer ? "on" : "off") + " scroll=" + std::to_string(scroll) +
                                     " last=" + last + " bindings=" + std::to_string(map.size());
                buf.put(8, 2, status);
                if (!settings.library_path.empty())
                        buf.put(9, 2, "path=" + settings.library_path);
                term.write_segments(buf.flush_diff());
        };
        draw();

        for (;;)
        {
                const auto ev = co_await term.next_event();
                if (const auto action = map.resolve(ev))
                {
                        last = std::string(app::event_name(*action));
                        switch (*action)
                        {
                        case app::Event::Quit:
                                draw();
                                krt::Scheduler::instance().request_stop();
                                co_return;
                        case app::Event::ScrollUp:
                                ++scroll;
                                break;
                        case app::Event::ScrollDown:
                                --scroll;
                                break;
                        default:
                                // PT: YtmSearch/ToggleTranslation: placeholders M2+.
                                // EN: YtmSearch/ToggleTranslation: M2+ placeholders.
                                // ES: YtmSearch/ToggleTranslation: placeholders de M2+.
                                // ZH: YtmSearch/ToggleTranslation：M2+ 占位。
                                break;
                        }
                }
                else
                        last = "[" + key_id(ev) + "]";
                draw();
        }
}

} // namespace

int main()
{
        try
        {
                auto app = app_main();
                app.start();
                krt::Scheduler::instance().run();
                return 0;
        }
        catch (const std::exception &e)
        {
                std::fprintf(stderr, "kiutube2: %s\n", e.what());
                return 1;
        }
}
