// PT: Wrapper sol2 do kiutube2 (pimpl: sol.hpp só entra no .cpp, protegendo
//     o tempo de compilação do resto). Expõe para o init.lua: bind(key,event),
//     log(msg) e a tabela `settings` — populada e relida genericamente a
//     partir dos descritores constexpr de app/settings.hpp.
// EN: kiutube2 sol2 wrapper (pimpl: sol.hpp only enters the .cpp, protecting
//     compile time for everything else). Exposes to init.lua: bind(key,event),
//     log(msg) and the `settings` table — populated and read back generically
//     from the constexpr descriptors in app/settings.hpp.
// ES: Wrapper sol2 de kiutube2 (pimpl: sol.hpp solo entra en el .cpp,
//     protegiendo el tiempo de compilación del resto). Expone al init.lua:
//     bind(key,event), log(msg) y la tabla `settings` — poblada y releída
//     genéricamente desde los descriptores constexpr de app/settings.hpp.
// ZH: kiutube2 的 sol2 包装（pimpl：sol.hpp 只在 .cpp 中引入，保护其余部分的
//     编译时间）。向 init.lua 暴露：bind(key,event)、log(msg) 以及 `settings`
//     表 —— 通过 app/settings.hpp 的 constexpr 描述符通用填充与回读。
#pragma once

#include "app/bindings.hpp"
#include "app/settings.hpp"

#include <memory>
#include <string>

namespace script
{

// PT: Estado Lua com sandbox mínimo (base/string/table/math; sem io/os/debug).
// EN: Lua state with minimal sandbox (base/string/table/math; no io/os/debug).
// ES: Estado Lua con sandbox mínimo (base/string/table/math; sin io/os/debug).
// ZH: 最小沙箱 Lua 状态（base/string/table/math；无 io/os/debug）。
class LuaState
{
      public:
        LuaState();
        ~LuaState();
        LuaState(LuaState &&) noexcept;
        LuaState &operator=(LuaState &&) noexcept;
        LuaState(const LuaState &) = delete;
        LuaState &operator=(const LuaState &) = delete;

        // PT: Roda o init.lua em `path` (se existir) aplicando binds e settings.
        //     Erros de Lua viram log warn/error — nunca derrubam o app.
        // EN: Runs the init.lua at `path` (if it exists) applying bindings and
        //     settings. Lua errors become warn/error logs — never crash the app.
        // ES: Ejecuta el init.lua en `path` (si existe) aplicando bindings y
        //     settings. Errores de Lua se vuelven logs warn/error — nunca caen.
        // ZH: 运行 `path` 处的 init.lua（若存在），应用绑定与 settings。
        //     Lua 错误只会变成 warn/error 日志 —— 绝不使应用崩溃。
        void open_config(const std::string &path, app::Settings &st, app::BindingMap &map);

        [[nodiscard]] const std::string &last_error() const;

      private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
};

} // namespace script
