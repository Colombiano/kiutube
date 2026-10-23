/**
 * @file lrclib.hpp
 * @brief Letras sincronizadas (LRC) via LRCLIB — API gratuita, sem auth.
 */
#pragma once

#include "async.hpp"

#include <optional>
#include <string>

namespace ytm
{

/**
 * Busca letra sincronizada. Retorna o texto LRC cru ([mm:ss.xx] linha)
 * ou nullopt. Tenta /api/get exato; sem resultado, /api/search e pega a
 * melhor candidata com syncedLyrics; por fim busca só pelo título.
 * duration_seconds (>0) filtra por duração — evita pegar letra de outra
 * música com o mesmo título.
 */
Task<std::optional<std::string>> fetch_lrc(const std::string &artist, const std::string &title,
                                           const std::string &album = "", int duration_seconds = 0);

} // namespace ytm
