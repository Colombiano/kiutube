// PT: Smoke test de settings/bindings (M1): parse genérico de kewrc via
//     descritores constexpr, parse de nomes de tecla, eventos via magic_enum
//     e override no BindingMap. Gate de regressão do M1 (parte C++ pura).
// EN: Settings/bindings smoke test (M1): generic kewrc parsing via constexpr
//     descriptors, key name parsing, magic_enum events and BindingMap
//     override. M1 regression gate (pure C++ part).
// ES: Smoke test de settings/bindings (M1): parse genérico de kewrc vía
//     descriptores constexpr, parse de nombres de tecla, eventos vía
//     magic_enum y override en BindingMap. Gate de regresión M1 (parte C++ pura).
// ZH: settings/bindings 冒烟测试（M1）：经 constexpr 描述符的通用 kewrc 解析、
//     键名解析、magic_enum 事件与 BindingMap 覆盖。M1 回归门槛（纯 C++ 部分）。
#include "app/bindings.hpp"
#include "app/settings.hpp"

#include <cstdio>

namespace
{

int failures = 0;

void check(bool cond, const char *what)
{
        if (!cond)
        {
                std::fprintf(stderr, "settings_smoke: FAIL: %s\n", what);
                ++failures;
        }
}

} // namespace

int main()
{
        // PT: apply_line genérico alimentado pela tabela de descritores.
        // EN: Generic apply_line fed by the descriptor table.
        // ES: apply_line genérico alimentado por la tabla de descriptores.
        // ZH: 由描述符表驱动的通用 apply_line。
        app::Settings st;
        check(app::apply_line(st, "path", "/tmp/music"), "path conhecido");
        check(app::apply_line(st, "volume", "42"), "volume conhecido");
        check(app::apply_line(st, "shuffle", "true"), "shuffle conhecido");
        check(!app::apply_line(st, "desconhecido", "x"), "chave ignorada");
        check(st.library_path == "/tmp/music" && st.volume == 42 && st.shuffle, "valores aplicados");

        // PT: parse de nomes de tecla. / EN: Key name parsing.
        // ES: Parse de nombres de tecla. / ZH: 键名解析。
        const auto k_f8 = app::parse_key("F8");
        const auto k_cx = app::parse_key("ctrl-x");
        const auto k_st = app::parse_key("shift-tab");
        const auto k_en = app::parse_key("enter");
        check(k_f8 && k_f8->key == tui::Key::F8, "F8");
        check(k_cx && k_cx->key == tui::Key::Char && k_cx->ch == 'x' && (k_cx->mods & tui::MOD_CTRL),
              "ctrl-x");
        check(k_st && k_st->key == tui::Key::Tab && (k_st->mods & tui::MOD_SHIFT), "shift-tab");
        check(k_en && k_en->key == tui::Key::Enter, "enter");
        check(!app::parse_key("ctrl-").has_value(), "tecla invalida");

        // PT: eventos: nomes estáveis + case-insensitive + snake_case.
        // EN: Events: stable names + case-insensitive + snake_case.
        // ES: Eventos: nombres estables + case-insensitive + snake_case.
        // ZH: 事件：稳定名称 + 大小写不敏感 + snake_case。
        check(app::event_name(app::Event::Quit) == "Quit", "enum_name Quit");
        check(app::event_from_name("quit") == app::Event::Quit, "quit ci");
        check(app::event_from_name("YTMSEARCH") == app::Event::YtmSearch, "YTMSEARCH ci");
        check(app::event_from_name("toggle_translation") == app::Event::ToggleTranslation, "snake");
        check(!app::event_from_name("nope").has_value(), "evento invalido");

        // PT: BindingMap: defaults + override de tecla já ligada.
        // EN: BindingMap: defaults + override of an already-bound key.
        // ES: BindingMap: defaults + override de una tecla ya ligada.
        // ZH: BindingMap：默认值 + 覆盖已绑定键。
        app::BindingMap map;
        map.load_defaults();
        check(map.resolve(tui::KeyEvent{tui::Key::Char, 'q', tui::MOD_NONE}) == app::Event::Quit, "q=Quit");
        check(map.resolve(tui::KeyEvent{tui::Key::F8, 0, tui::MOD_NONE}) == app::Event::YtmSearch, "F8");
        const auto p = app::parse_key("p");
        check(p.has_value(), "parse p");
        map.add_or_replace({*p, app::Event::Quit});
        check(map.resolve(tui::KeyEvent{tui::Key::Char, 'p', tui::MOD_NONE}) == app::Event::Quit, "p=Quit");
        map.add_or_replace({app::KeySpec{tui::Key::Char, 'q', tui::MOD_NONE}, app::Event::ScrollUp});
        check(map.resolve(tui::KeyEvent{tui::Key::Char, 'q', tui::MOD_NONE}) == app::Event::ScrollUp,
              "q override");

        if (failures > 0)
                return 1;
        std::printf("settings_smoke: PASS\n");
        return 0;
}
