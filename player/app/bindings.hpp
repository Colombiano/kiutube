// PT: Bindings de teclado: tabela default constexpr + parse de nome de tecla
//     ("F8", "ctrl-x", "shift-tab", "enter") + resolução KeyEvent -> Event.
//     Nomes de evento via magic_enum (enum_cast/enum_name) — sem strings
//     hardcoded sincronizadas à mão.
// EN: Key bindings: constexpr default table + key name parsing
//     ("F8", "ctrl-x", "shift-tab", "enter") + KeyEvent -> Event resolution.
//     Event names via magic_enum (enum_cast/enum_name) — no hand-synced
//     hardcoded strings.
// ES: Bindings de teclado: tabla default constexpr + parse de nombre de tecla
//     ("F8", "ctrl-x", "shift-tab", "enter") + resolución KeyEvent -> Event.
//     Nombres de evento vía magic_enum (enum_cast/enum_name) — sin cadenas
//     hardcoded sincronizadas a mano.
// ZH: 键盘绑定：constexpr 默认表 + 键名解析（"F8"、"ctrl-x"、"shift-tab"、
//     "enter"）+ KeyEvent -> Event 解析。事件名使用 magic_enum
//     （enum_cast/enum_name）—— 无需手工同步硬编码字符串。
#pragma once

#include "app/settings.hpp"
#include "tui/term.hpp"

#include <magic_enum/magic_enum.hpp>

#include <cctype>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace app
{

// PT: Uma tecla desejada: Char+codepoint ou especial+mods (match exato de mods).
// EN: One desired key: Char+codepoint or special+mods (exact mods match).
// ES: Una tecla deseada: Char+codepoint o especial+mods (match exacto de mods).
// ZH: 目标键：Char+码点 或 特殊键+修饰键（修饰键精确匹配）。
struct KeySpec
{
        tui::Key key = tui::Key::Char;
        std::uint32_t ch = 0;
        std::uint8_t mods = tui::MOD_NONE;
};

struct Binding
{
        KeySpec spec;
        Event event;
};

// PT: Defaults do M1 (subconjunto compatível com o kew: q/esc/F10 saem etc.).
// EN: M1 defaults (kew-compatible subset: q/esc/F10 quit etc.).
// ES: Defaults de M1 (subconjunto compatible con kew: q/esc/F10 salen etc.).
// ZH: M1 默认值（兼容 kew 的子集：q/esc/F10 退出等）。
inline constexpr Binding kDefaultBindings[] = {
    {{tui::Key::Char, 'q', tui::MOD_NONE}, Event::Quit},
    {{tui::Key::Char, 'Q', tui::MOD_NONE}, Event::Quit},
    {{tui::Key::Esc, 0, tui::MOD_NONE}, Event::Quit},
    {{tui::Key::F10, 0, tui::MOD_NONE}, Event::Quit},
    {{tui::Key::Up, 0, tui::MOD_NONE}, Event::ScrollUp},
    {{tui::Key::Down, 0, tui::MOD_NONE}, Event::ScrollDown},
    {{tui::Key::F8, 0, tui::MOD_NONE}, Event::YtmSearch},
    {{tui::Key::F9, 0, tui::MOD_NONE}, Event::ToggleTranslation},
    // PT: Transporte M2 (subset do kew: space toca/pausa, n próxima etc.).
    // EN: M2 transport (kew subset: space toggles pause, n next, etc.).
    // ES: Transporte M2 (subset de kew: space toca/pausa, n siguiente, etc.).
    // ZH: M2 传输控制（kew 子集：space 播放/暂停、n 下一首等）。
    {{tui::Key::Char, ' ', tui::MOD_NONE}, Event::TogglePause},
    {{tui::Key::Char, 'n', tui::MOD_NONE}, Event::Next},
    {{tui::Key::Left, 0, tui::MOD_NONE}, Event::SeekBack},
    {{tui::Key::Right, 0, tui::MOD_NONE}, Event::SeekForward},
    {{tui::Key::Char, '+', tui::MOD_NONE}, Event::VolUp},
    {{tui::Key::Char, '=', tui::MOD_NONE}, Event::VolUp},
    {{tui::Key::Char, '-', tui::MOD_NONE}, Event::VolDown},
};

// PT: Parse de nome de tecla (case-insensitive): "x", "F8", "enter", "ctrl-x",
//     "shift-tab", "ctrl-alt-left". Formato: [mod-][mod-]tecla.
// EN: Key name parsing (case-insensitive): "x", "F8", "enter", "ctrl-x",
//     "shift-tab", "ctrl-alt-left". Format: [mod-][mod-]key.
// ES: Parse de nombre de tecla (case-insensitive): "x", "F8", "enter",
//     "ctrl-x", "shift-tab", "ctrl-alt-left". Formato: [mod-][mod-]tecla.
// ZH: 键名解析（大小写不敏感）："x"、"F8"、"enter"、"ctrl-x"、"shift-tab"、
//     "ctrl-alt-left"。格式：[mod-][mod-]key。
inline std::optional<KeySpec> parse_key(std::string_view name)
{
        std::string lower(name);
        for (auto &c : lower)
                if (c >= 'A' && c <= 'Z')
                        c = static_cast<char>(c - 'A' + 'a');

        KeySpec spec{};
        for (;;)
        {
                if (lower.rfind("ctrl-", 0) == 0)
                {
                        spec.mods |= tui::MOD_CTRL;
                        lower.erase(0, 5);
                }
                else if (lower.rfind("shift-", 0) == 0)
                {
                        spec.mods |= tui::MOD_SHIFT;
                        lower.erase(0, 6);
                }
                else if (lower.rfind("alt-", 0) == 0)
                {
                        spec.mods |= tui::MOD_ALT;
                        lower.erase(0, 4);
                }
                else
                        break;
        }
        if (lower.empty())
                return std::nullopt;

        if (lower.size() == 1)
        {
                spec.key = tui::Key::Char;
                spec.ch = static_cast<unsigned char>(lower[0]);
                return spec;
        }

        static const struct
        {
                const char *name;
                tui::Key key;
        } kNamed[] = {
            {"enter", tui::Key::Enter},     {"return", tui::Key::Enter}, {"tab", tui::Key::Tab},
            {"backspace", tui::Key::Backspace}, {"esc", tui::Key::Esc},  {"up", tui::Key::Up},
            {"down", tui::Key::Down},       {"left", tui::Key::Left},   {"right", tui::Key::Right},
            {"home", tui::Key::Home},       {"end", tui::Key::End},     {"insert", tui::Key::Insert},
            {"delete", tui::Key::Delete},   {"pageup", tui::Key::PageUp}, {"pagedown", tui::Key::PageDown},
            {"f1", tui::Key::F1}, {"f2", tui::Key::F2}, {"f3", tui::Key::F3}, {"f4", tui::Key::F4},
            {"f5", tui::Key::F5}, {"f6", tui::Key::F6}, {"f7", tui::Key::F7}, {"f8", tui::Key::F8},
            {"f9", tui::Key::F9}, {"f10", tui::Key::F10}, {"f11", tui::Key::F11}, {"f12", tui::Key::F12},
        };
        for (const auto &n : kNamed)
                if (lower == n.name)
                {
                        spec.key = n.key;
                        return spec;
                }
        return std::nullopt;
}

// PT: Match: Char compara só o codepoint (shift de letras se perde no raw
//     mode); especiais exigem mods exatos.
// EN: Match: Char compares only the codepoint (letter shift is lost in raw
//     mode); special keys require exact mods.
// ES: Match: Char compara solo el codepoint (el shift de letras se pierde en
//     raw mode); especiales exigen mods exactos.
// ZH: 匹配：Char 只比较码点（raw mode 下字母的 shift 信息丢失）；特殊键要求
//     修饰键精确匹配。
inline bool matches(const KeySpec &spec, const tui::KeyEvent &ev)
{
        if (spec.key == tui::Key::Char)
                return ev.key == tui::Key::Char && ev.ch == spec.ch;
        return ev.key == spec.key && ev.mods == spec.mods;
}

// PT: Mapa de bindings com override (kewrc e Lua): substitui evento de uma
//     tecla já ligada; adiciona se nova.
// EN: Binding map with override (kewrc and Lua): replaces the event of an
//     already-bound key; adds if new.
// ES: Mapa de bindings con override (kewrc y Lua): sustituye el evento de una
//     tecla ya ligada; añade si es nueva.
// ZH: 带覆盖能力的绑定表（kewrc 与 Lua）：替换已绑定键的事件；新键则添加。
class BindingMap
{
      public:
        void load_defaults()
        {
                for (const auto &b : kDefaultBindings)
                        add_or_replace(b);
        }

        void add_or_replace(const Binding &b)
        {
                for (auto &existing : bindings_)
                        if (existing.spec.key == b.spec.key && existing.spec.ch == b.spec.ch &&
                            existing.spec.mods == b.spec.mods)
                        {
                                existing.event = b.event;
                                return;
                        }
                bindings_.push_back(b);
        }

        [[nodiscard]] std::optional<Event> resolve(const tui::KeyEvent &ev) const
        {
                for (const auto &b : bindings_)
                        if (matches(b.spec, ev))
                                return b.event;
                return std::nullopt;
        }

        [[nodiscard]] std::size_t size() const
        {
                return bindings_.size();
        }

      private:
        std::vector<Binding> bindings_;
};

// PT: Nome do evento via magic_enum (identificador, estável). / EN: Event name via magic_enum.
// ES: Nombre del evento vía magic_enum. / ZH: 事件名（magic_enum，稳定标识符）。
inline std::string_view event_name(Event e)
{
        return magic_enum::enum_name(e);
}

// PT: Evento a partir do nome ("Quit", "ytm_search" em snake também aceito).
// EN: Event from name ("Quit"; snake_case like "ytm_search" also accepted).
// ES: Evento a partir del nombre ("Quit"; snake_case como "ytm_search" aceptado).
// ZH: 由名称得到事件（"Quit"；也接受 snake_case，如 "ytm_search"）。
inline std::optional<Event> event_from_name(std::string_view name)
{
        // PT: Case-insensitive direto ("quit", "YTMSEARCH", "ytmSearch").
        // EN: Direct case-insensitive ("quit", "YTMSEARCH", "ytmSearch").
        // ES: Case-insensitive directo ("quit", "YTMSEARCH", "ytmSearch").
        // ZH: 直接大小写不敏感匹配（"quit"、"YTMSEARCH"、"ytmSearch"）。
        const auto ci = [](char l, char r) {
                return std::tolower(static_cast<unsigned char>(l)) == std::tolower(static_cast<unsigned char>(r));
        };
        if (auto e = magic_enum::enum_cast<Event>(name, ci); e.has_value())
                return *e;
        // PT: snake_case -> PascalCase ("ytm_search" -> "YtmSearch").
        // EN: snake_case -> PascalCase ("ytm_search" -> "YtmSearch").
        // ES: snake_case -> PascalCase ("ytm_search" -> "YtmSearch").
        // ZH: snake_case -> PascalCase（"ytm_search" -> "YtmSearch"）。
        std::string pascal;
        pascal.reserve(name.size());
        bool upper_next = true;
        for (const char c : name)
        {
                if (c == '_')
                {
                        upper_next = true; // PT: remove o '_' e capitaliza a próxima.
                        continue;          // EN: drops '_' and capitalizes the next.
                }                          // ES: quita el '_' y capitaliza la siguiente.
                pascal += upper_next && c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A') : c;
                upper_next = false;
        }
        return magic_enum::enum_cast<Event>(pascal, ci);
}

} // namespace app
