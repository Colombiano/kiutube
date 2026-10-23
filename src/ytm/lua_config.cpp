/**
 * @file lua_config.cpp
 * @brief Lua 5.4 embarcada: init.lua sobrescreve defaults; LuaState RAII.
 */
#include "lua_config.hpp"

#include <cstdlib>
#include <filesystem>
#include <lua.hpp>
#include <stdexcept>
#include <utility>

namespace ytm
{

namespace
{

/** Máquina Lua em propriedade exclusiva (move-only, deleter custom). */
class LuaState
{
      public:
        LuaState() : state_(luaL_newstate())
        {
                if (!state_)
                        throw std::runtime_error("luaL_newstate falhou");
        }
        ~LuaState()
        {
                if (state_)
                        lua_close(state_);
        }
        LuaState(LuaState &&other) noexcept : state_(std::exchange(other.state_, nullptr))
        {
        }
        LuaState &operator=(LuaState &&other) noexcept
        {
                if (this != &other)
                {
                        if (state_)
                                lua_close(state_);
                        state_ = std::exchange(other.state_, nullptr);
                }
                return *this;
        }
        LuaState(const LuaState &) = delete;
        LuaState &operator=(const LuaState &) = delete;

        lua_State *get() const
        {
                return state_;
        }

      private:
        lua_State *state_;
};

std::string env_or(const char *name, const char *fallback)
{
        const char *v = std::getenv(name);
        return (v && *v) ? v : fallback;
}

std::string default_cache_dir()
{
        return env_or("XDG_CACHE_HOME", (std::string(std::getenv("HOME") ? std::getenv("HOME") : ".") + "/.cache").c_str()) +
               "/kiutube";
}

void set_global_string(lua_State *L, const char *name, const std::string &value)
{
        lua_pushstring(L, value.c_str());
        lua_setglobal(L, name);
}

void set_global_bool(lua_State *L, const char *name, bool value)
{
        lua_pushboolean(L, value ? 1 : 0);
        lua_setglobal(L, name);
}

/** getenv exposto ao init.lua (sem abrir a lib os inteira). */
int lua_getenv(lua_State *L)
{
        const char *name = luaL_checkstring(L, 1);
        const char *value = std::getenv(name);
        if (value)
                lua_pushstring(L, value);
        else
                lua_pushnil(L);
        return 1;
}

bool get_global_bool(lua_State *L, const char *name, bool fallback)
{
        lua_getglobal(L, name);
        bool v = lua_isboolean(L, -1) ? (lua_toboolean(L, -1) != 0) : fallback;
        lua_pop(L, 1);
        return v;
}

std::string get_global_string(lua_State *L, const char *name, const std::string &fallback)
{
        lua_getglobal(L, name);
        const char *v = lua_isstring(L, -1) ? lua_tostring(L, -1) : nullptr;
        std::string out = v ? v : fallback;
        lua_pop(L, 1);
        return out;
}

Config load_config()
{
        Config cfg;
        cfg.cache_dir = default_cache_dir();

        LuaState lua;
        lua_State *L = lua.get();

        // bibliotecas seguras para config
        luaL_requiref(L, "_G", luaopen_base, 1);
        lua_pop(L, 1);
        luaL_requiref(L, LUA_STRLIBNAME, luaopen_string, 1);
        lua_pop(L, 1);
        luaL_requiref(L, LUA_TABLIBNAME, luaopen_table, 1);
        lua_pop(L, 1);

        // globals com defaults (init.lua pode sobrescrever)
        lua_pushcfunction(L, lua_getenv);
        lua_setglobal(L, "getenv");
        set_global_bool(L, "translation_enabled", cfg.translation_enabled);
        set_global_string(L, "translation_target", cfg.translation_target);
        set_global_string(L, "translation_engine", cfg.translation_engine);
        set_global_string(L, "translation_source", cfg.translation_source);
        set_global_string(L, "search_key", cfg.search_key);
        set_global_string(L, "toggle_translation_key", cfg.toggle_translation_key);
        set_global_string(L, "cache_dir", cfg.cache_dir);
        set_global_bool(L, "verbose", cfg.verbose);

        const std::string path = config_file_path();
        std::error_code ec;
        if (std::filesystem::exists(path, ec))
        {
                if (luaL_dofile(L, path.c_str()) != LUA_OK)
                {
                        std::string err = lua_tostring(L, -1);
                        lua_pop(L, 1);
                        throw std::runtime_error("init.lua: " + err);
                }
                cfg.translation_enabled = get_global_bool(L, "translation_enabled", cfg.translation_enabled);
                cfg.translation_target = get_global_string(L, "translation_target", cfg.translation_target);
                cfg.translation_engine = get_global_string(L, "translation_engine", cfg.translation_engine);
                cfg.translation_source = get_global_string(L, "translation_source", cfg.translation_source);
                cfg.search_key = get_global_string(L, "search_key", cfg.search_key);
                cfg.toggle_translation_key =
                    get_global_string(L, "toggle_translation_key", cfg.toggle_translation_key);
                cfg.cache_dir = get_global_string(L, "cache_dir", cfg.cache_dir);
                cfg.verbose = get_global_bool(L, "verbose", cfg.verbose);
        }
        return cfg;
}

} // namespace

std::string config_file_path()
{
        std::string home = std::getenv("HOME") ? std::getenv("HOME") : ".";
        return env_or("XDG_CONFIG_HOME", (home + "/.config").c_str()) + "/kiutube/init.lua";
}

const Config &config()
{
        static const Config cfg = load_config();
        return cfg;
}

} // namespace ytm
