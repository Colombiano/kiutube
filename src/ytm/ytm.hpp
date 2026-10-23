/**
 * @file ytm.hpp
 * @brief Provider YouTube Music: busca e resolução de stream via yt-dlp
 *        (subprocesso, templates --print — sem parser de JSON).
 */
#pragma once

#include "async.hpp"

#include <string>
#include <utility>
#include <vector>

namespace ytm
{

struct Track
{
        std::string video_id;
        std::string title;
        std::string artist;
        std::string album;
        std::string thumb_url;
        double duration = 0.0; ///< segundos; 0 = desconhecida
};

class YtmError : public std::runtime_error
{
      public:
        explicit YtmError(const std::string &what) : std::runtime_error(what)
        {
        }
};

/** Busca no YouTube Music (ytsearchN:). Roda em thread do pool. */
Task<std::vector<Track>> search(std::string query, int limit = 10);

/** Metadados de um videoId (título/artista/album limpos do YTM). */
Task<Track> info(std::string video_id);

/** Metadados de vários videoIds numa única chamada yt-dlp. */
Task<std::vector<Track>> info_multi(const std::vector<std::string> &video_ids);

/** Resolve a URL direta do stream de áudio (bestaudio) de um videoId. */
Task<std::string> resolve_stream(std::string video_id);

/** Resolve stream e retorna {url, ext} (ext p/ nomear o arquivo baixado). */
Task<std::pair<std::string, std::string>> resolve_stream_ex(std::string video_id);

/** Resolve e baixa o áudio para dest_base + "." + ext. Retorna o caminho final. */
Task<std::string> download_track(std::string video_id, std::string dest_base);

/** URL da página watch (helper). */
inline std::string watch_url(const std::string &video_id)
{
        return "https://music.youtube.com/watch?v=" + video_id;
}

} // namespace ytm
