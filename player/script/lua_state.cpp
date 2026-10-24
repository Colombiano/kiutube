// PT: Implementação do wrapper Lua (sol2). A tabela `settings` é exposta de
//     forma genérica: o mesmo for_each_setting de app/settings.hpp instala os
//     valores e depois relê o que o script mudou (type-safe via std::optional).
// EN: Lua wrapper implementation (sol2). The `settings` table is exposed
//     generically: the same for_each_setting from app/settings.hpp installs
//     the values and reads back what the script changed (type-safe via
//     std::optional).
// ES: Implementación del wrapper Lua (sol2). La tabla `settings` se expone de
//     forma genérica: el mismo for_each_setting de app/settings.hpp instala
//     los valores y relee lo que el script cambió (type-safe vía std::optional).
// ZH: Lua 包装实现（sol2）。`settings` 表以通用方式暴露：app/settings.hpp 中的
//     for_each_setting 负责写入初值并回读脚本修改（通过 std::optional 保证
//     类型安全）。
#include "script/lua_state.hpp"

#include "krt/log.hpp"

#include <sol/sol.hpp>

#include <fstream>
#include <type_traits>
#include <utility>

namespace script
{

struct LuaState::Impl
{
        sol::state lua;
        std::string error;
};

LuaState::LuaState() : impl_(std::make_unique<Impl>())
{
        // PT: Sandbox deliberada: sem io/os/debug — config não toca o sistema.
        // EN: Deliberate sandbox: no io/os/debug — config cannot touch the system.
        // ES: Sandbox deliberada: sin io/os/debug — la config no toca el sistema.
        // ZH: 刻意的沙箱：不提供 io/os/debug —— 配置无法触及系统。
        impl_->lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::table, sol::lib::math);
}

LuaState::~LuaState() = default;
LuaState::LuaState(LuaState &&) noexcept = default;
LuaState &LuaState::operator=(LuaState &&) noexcept = default;

void LuaState::open_config(const std::string &path, app::Settings &st, app::BindingMap &map)
{
        auto &lua = impl_->lua;

        // PT: bind("F8", "ytm_search"): parse de tecla + evento; inválido = warn.
        // EN: bind("F8", "ytm_search"): key + event parsing; invalid = warn.
        // ES: bind("F8", "ytm_search"): parse de tecla + evento; inválido = warn.
        // ZH: bind("F8", "ytm_search")：解析键与事件；无效则 warn。
        lua.set_function("bind", [&map](const std::string &key, const std::string &ev) {
                const auto spec = app::parse_key(key);
                const auto event = app::event_from_name(ev);
                if (!spec || !event)
                {
                        krt::log::warn("lua: bind invalido / invalid binding / binding inválido / 无效绑定: {} -> {}", key, ev);
                        return;
                }
                map.add_or_replace({*spec, *event});
                krt::log::info("lua: bind {} -> {}", key, std::string(app::event_name(*event)));
        });

        // PT: log(msg) repassa para o logger krt (nível info). / EN: log(msg) forwards to the krt logger (info level).
        // ES: log(msg) repasa al logger krt (nivel info). / ZH: log(msg) 转发到 krt 日志（info 级别）。
        lua.set_function("log", [](const std::string &msg) { krt::log::info("lua: {}", msg); });

        // PT: Popula `settings` com os valores atuais (genérico, compile-time).
        // EN: Populates `settings` with current values (generic, compile-time).
        // ES: Puebla `settings` con los valores actuales (genérico, compile-time).
        // ZH: 用当前值填充 `settings`（通用，编译期驱动）。
        sol::table t = lua.create_named_table("settings");
        app::for_each_setting([&](auto d) { t[std::string(d.name)] = st.*(d.member); });

        std::ifstream f(path);
        if (!f)
        {
                krt::log::info("lua: {} nao encontrado, defaults / not found, defaults / no encontrado, defaults / 未找到，使用默认值", path);
                return;
        }

        sol::load_result loaded = lua.load_file(path);
        if (!loaded.valid())
        {
                impl_->error = sol::error(loaded).what();
                krt::log::error("lua: load {}: {}", path, impl_->error);
                return;
        }
        const sol::protected_function_result run = loaded();
        if (!run.valid())
        {
                impl_->error = sol::error(run).what();
                krt::log::error("lua: run {}: {}", path, impl_->error);
                return;
        }

        // PT: Relê genericamente o que o script mudou; tipo errado = warn e ignora.
        // EN: Reads back generically what the script changed; wrong type = warn and ignore.
        // ES: Relee genéricamente lo que el script cambió; tipo erróneo = warn e ignora.
        // ZH: 通用回读脚本的修改；类型错误则 warn 并忽略。
        app::for_each_setting([&](auto d) {
                using M = std::remove_cv_t<std::remove_reference_t<decltype(st.*(d.member))>>;
                const auto v = t.get<std::optional<M>>(std::string(d.name));
                if (v.has_value())
                        st.*(d.member) = std::move(*v);
                else if (!t[std::string(d.name)].valid())
                        krt::log::warn("lua: settings.{} com tipo inesperado / unexpected type / tipo inesperado / 类型不符", d.name);
        });

        krt::log::info("lua: config carregada / config loaded / config cargada / 配置已加载: {}", path);
}

const std::string &LuaState::last_error() const
{
        return impl_->error;
}

} // namespace script
