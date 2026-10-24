// PT: Settings tipadas do kiutube2 — showcase de metaprogramação: a mesma
//     tabela constexpr de descritores (nome + ponteiro-para-membro) alimenta
//     o parse genérico do kewrc (apply_line) E a exposição genérica para Lua
//     (script/lua_state.cpp). Adicionar um campo = adicionar uma linha na tupla.
// EN: kiutube2 typed settings — metaprogramming showcase: the same constexpr
//     descriptor table (name + pointer-to-member) feeds both the generic
//     kewrc parser (apply_line) AND the generic Lua exposure
//     (script/lua_state.cpp). Adding a field = adding one tuple row.
// ES: Settings tipadas de kiutube2 — showcase de metaprogramación: la misma
//     tabla constexpr de descriptores (nombre + puntero-a-miembro) alimenta
//     el parse genérico del kewrc (apply_line) Y la exposición genérica a Lua
//     (script/lua_state.cpp). Añadir un campo = añadir una fila a la tupla.
// ZH: kiutube2 类型化 settings —— 元编程展示：同一份 constexpr 描述符表
//     （名称 + 成员指针）同时驱动 kewrc 的通用解析（apply_line）与 Lua 的
//     通用暴露（script/lua_state.cpp）。新增字段 = 在元组中加一行。
#pragma once

#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <tuple>

namespace app
{

// PT: Eventos da aplicação (M1). Nomes vêm de magic_enum para bindings.
// EN: Application events (M1). Names come from magic_enum for bindings.
// ES: Eventos de la aplicación (M1). Los nombres vienen de magic_enum.
// ZH: 应用事件（M1）。名称由 magic_enum 提供，用于键绑定。
enum class Event : std::uint16_t
{
        Quit,
        ScrollUp,
        ScrollDown,
        YtmSearch,
        ToggleTranslation,
        TogglePause,
        Next,
        SeekBack,
        SeekForward,
        VolUp,
        VolDown
};

struct Settings
{
        std::string library_path;   // kewrc: path=
        int volume = 100;           // kewrc: volume=
        bool shuffle = false;       // kewrc: shuffle=
        bool visualizer = true;     // kewrc: visualizer=
};

// PT: Descritor tipado: nome de config + ponteiro-para-membro.
// EN: Typed descriptor: config name + pointer-to-member.
// ES: Descriptor tipado: nombre de config + puntero-a-miembro.
// ZH: 类型化描述符：配置名 + 成员指针。
template <typename M>
struct SettingDesc
{
        std::string_view name;
        M Settings::*member;
};

// PT: A ÚNICA fonte da verdade dos campos. Tupla heterogênea -> iteração em
//     compile-time via std::apply (parse, Lua, futura serialização).
// EN: The SINGLE source of truth for fields. Heterogeneous tuple -> compile
//     -time iteration via std::apply (parsing, Lua, future serialization).
// ES: La ÚNICA fuente de verdad de los campos. Tupla heterogénea -> iteración
//     en compile-time vía std::apply (parse, Lua, futura serialización).
// ZH: 字段的唯一事实来源。异构元组 -> 通过 std::apply 进行编译期迭代
//     （解析、Lua、未来序列化）。
inline constexpr auto kSettingsDescs = std::tuple{
    SettingDesc<std::string>{.name = "path", .member = &Settings::library_path},
    SettingDesc<int>{.name = "volume", .member = &Settings::volume},
    SettingDesc<bool>{.name = "shuffle", .member = &Settings::shuffle},
    SettingDesc<bool>{.name = "visualizer", .member = &Settings::visualizer},
};

// PT: Aplica F(desc) para cada descritor em compile-time.
// EN: Applies F(desc) for each descriptor at compile time.
// ES: Aplica F(desc) para cada descriptor en compile-time.
// ZH: 在编译期对每个描述符应用 F(desc)。
template <typename F>
constexpr void for_each_setting(F &&f)
{
        std::apply([&](auto... d) { (f(d), ...); }, kSettingsDescs);
}

namespace detail
{

// PT: Converte string para o tipo do membro (if-constexpr: string/int/bool).
// EN: Converts a string to the member type (if-constexpr: string/int/bool).
// ES: Convierte string al tipo del miembro (if-constexpr: string/int/bool).
// ZH: 将字符串转换为成员类型（if-constexpr：string/int/bool）。
template <typename M>
bool parse_value(M &out, std::string_view s)
{
        if constexpr (std::is_same_v<M, std::string>)
        {
                out.assign(s);
                return true;
        }
        else if constexpr (std::is_same_v<M, bool>)
        {
                out = s == "true" || s == "1" || s == "yes" || s == "on";
                return s == "false" || s == "0" || s == "no" || s == "off" || out;
        }
        else if constexpr (std::is_integral_v<M>)
        {
                const auto r = std::from_chars(s.data(), s.data() + s.size(), out);
                return r.ec == std::errc{};
        }
        else
                return false;
}

} // namespace detail

// PT: Aplica uma linha kewrc (key=value) ao struct, se o campo existir.
// EN: Applies one kewrc line (key=value) to the struct, if the field exists.
// ES: Aplica una línea kewrc (key=value) al struct, si el campo existe.
// ZH: 将一行 kewrc（key=value）应用到结构体（若字段存在）。
// PT: Retorna true se o campo é conhecido. / EN: Returns true if the field is known.
// ES: Devuelve true si el campo es conocido. / ZH: 字段已知则返回 true。
inline bool apply_line(Settings &st, std::string_view key, std::string_view value)
{
        bool known = false;
        for_each_setting([&](auto d) {
                if (key == d.name)
                {
                        known = true;
                        detail::parse_value(st.*(d.member), value);
                }
        });
        return known;
}

} // namespace app
