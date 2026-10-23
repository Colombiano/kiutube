/**
 * @file lua_config.hpp
 * @brief Configuração do kiutube via Lua 5.4 embarcada.
 *
 * Lê ~/.config/kiutube/init.lua se existir; variáveis ausentes caem nos
 * defaults C++. A máquina Lua é RAII move-only (LuaState).
 */
#pragma once

#include <memory>
#include <string>

namespace ytm
{

struct Config
{
        bool translation_enabled = true;      ///< traduzir letras para PT
        std::string translation_target = "pt";///< idioma alvo (ISO 639-1)
        std::string translation_engine = "google"; ///< google | mymemory
        std::string translation_source = "auto"; ///< idioma-fonte ("auto"->onde suportado)
        std::string search_key = "F8";        ///< tecla da busca YTM na TUI
        std::string toggle_translation_key = "F9"; ///< alterna original/tradução
        std::string cache_dir;                ///< cache de streams/letras/traduções
        bool verbose = false;                 ///< log da camada ytm no stderr
};

/** Config efetiva (carregada uma vez, lazy). */
const Config &config();

/** Caminho do init.lua (~/.config/kiutube/init.lua). */
std::string config_file_path();

} // namespace ytm
