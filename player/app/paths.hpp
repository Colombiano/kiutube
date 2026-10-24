// PT: Resolução de diretórios de config (XDG). Usado para ler o kewrc legado
//     (~/.config/kew) e o init.lua do kiutube2 (~/.config/kiutube2).
// EN: Config directory resolution (XDG). Used to read the legacy kewrc
//     (~/.config/kew) and kiutube2's init.lua (~/.config/kiutube2).
// ES: Resolución de directorios de config (XDG). Usado para leer el kewrc
//     legado (~/.config/kew) y el init.lua de kiutube2 (~/.config/kiutube2).
// ZH: 配置目录解析（XDG）。用于读取旧的 kewrc（~/.config/kew）与 kiutube2 的
//     init.lua（~/.config/kiutube2）。
#pragma once

#include <cstdlib>
#include <filesystem>
#include <string>

namespace app
{

// PT: <xdg config>/<app>/, criando se preciso. / EN: <xdg config>/<app>/, creating if needed.
// ES: <xdg config>/<app>/, creando si hace falta. / ZH: <xdg config>/<app>/，必要时创建。
inline std::filesystem::path config_dir(const char *app_name)
{
        std::filesystem::path base;
        if (const char *xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg)
                base = xdg;
        else if (const char *home = std::getenv("HOME"); home && *home)
                base = std::filesystem::path(home) / ".config";
        else
                base = ".";
        const auto dir = base / app_name;
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        return dir;
}

} // namespace app
