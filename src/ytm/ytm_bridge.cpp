/**
 * @file ytm_bridge.cpp
 * @brief Implementação da ponte C: adapta Task<> coroutines para chamadas
 *        bloqueantes e adiciona cache em disco (letras/traduções/thumbs).
 */
#include "ytm_bridge.h"

#include "http.hpp"
#include "lrclib.hpp"
#include "lua_config.hpp"
#include "translate.hpp"
#include "ytm.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <sstream>
#include <string>

namespace
{

char *dup_cstr(const std::string &s)
{
        char *p = static_cast<char *>(std::malloc(s.size() + 1));
        if (!p)
                return nullptr;
        std::memcpy(p, s.c_str(), s.size() + 1);
        return p;
}

/** Hash estável p/ nome de arquivo de cache (FNV-1a 64). */
std::string hash64(const std::string &s)
{
        unsigned long long h = 1469598103934665603ULL;
        for (unsigned char c : s)
        {
                h ^= c;
                h *= 1099511628211ULL;
        }
        std::ostringstream oss;
        oss << std::hex << h;
        return oss.str();
}

std::string cache_path(const std::string &kind, const std::string &key)
{
        std::string dir = ytm::config().cache_dir + "/" + kind;
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        return dir + "/" + hash64(key);
}

std::optional<std::string> cache_read(const std::string &path)
{
        std::error_code ec;
        if (!std::filesystem::exists(path, ec))
                return std::nullopt;
        std::ifstream f(path, std::ios::binary);
        if (!f)
                return std::nullopt;
        std::ostringstream ss;
        ss << f.rdbuf();
        return ss.str();
}

void cache_write(const std::string &path, const std::string &data)
{
        std::ofstream f(path, std::ios::binary | std::ios::trunc);
        if (f)
                f << data;
}

/** Separa o LRC em linhas de texto puro (sem timestamps) p/ traduzir. */
std::string lrc_plain_text(const std::string &lrc)
{
        std::istringstream in(lrc);
        std::ostringstream out;
        std::string line;
        while (std::getline(in, line))
        {
                // remove tags de tempo [mm:ss.xx] e tags <mm:ss.xx> no início
                std::string clean;
                size_t i = 0;
                while (i < line.size() && line[i] == '[')
                {
                        size_t e = line.find(']', i);
                        if (e == std::string::npos)
                                break;
                        i = e + 1;
                }
                for (; i < line.size(); ++i)
                        clean += line[i];
                // também remove <..> karaoke inline simples
                std::string no_ang;
                bool in_ang = false;
                for (char c : clean)
                {
                        if (c == '<')
                                in_ang = true;
                        else if (c == '>')
                                in_ang = false;
                        else if (!in_ang)
                                no_ang += c;
                }
                out << no_ang << '\n';
        }
        return out.str();
}

/**
 * Reaplica timestamps do LRC original às linhas traduzidas.
 * Tenta alinhar 1:1; se a contagem divergir, devolve o texto traduzido puro.
 */
std::string reapply_timestamps(const std::string &orig_lrc, const std::string &translated_plain)
{
        std::vector<std::string> stamps;
        std::istringstream in(orig_lrc);
        std::string line;
        while (std::getline(in, line))
        {
                size_t b = line.find('[');
                size_t e = line.find(']', b);
                if (b != std::string::npos && e != std::string::npos && e - b >= 8)
                        stamps.push_back(line.substr(b, e - b + 1));
                else
                        stamps.push_back("");
        }

        std::vector<std::string> lines;
        std::istringstream tin(translated_plain);
        std::string tl;
        while (std::getline(tin, tl))
                lines.push_back(tl);

        std::ostringstream out;
        if (!stamps.empty() && stamps.size() == lines.size())
        {
                // caminho feliz: 1:1
                for (size_t i = 0; i < lines.size(); ++i)
                {
                        if (!stamps[i].empty())
                                out << stamps[i] << lines[i] << "\n";
                        else if (!lines[i].empty())
                                out << lines[i] << "\n";
                }
        }
        else
        {
                // tradução juntou/dividiu linhas: distribui os stamps não-vazios
                // em ordem entre as linhas traduzidas não-vazias.
                auto stamp_it = std::find_if(stamps.begin(), stamps.end(), [](const std::string &s) { return !s.empty(); });
                for (const auto &ln : lines)
                {
                        if (ln.empty())
                                continue;
                        if (stamp_it != stamps.end())
                        {
                                out << *stamp_it << ln << "\n";
                                stamp_it = std::find_if(std::next(stamp_it), stamps.end(),
                                                        [](const std::string &s) { return !s.empty(); });
                        }
                        else
                        {
                                out << ln << "\n";
                        }
                }
        }
        return out.str();
}

} // namespace

extern "C" {

int ytm_search_blocking(const char *query, int limit, YtmTrack **out)
{
        try
        {
                auto tracks = ytm::search(query ? query : "", limit).sync_wait();
                auto *arr = static_cast<YtmTrack *>(std::calloc(tracks.size(), sizeof(YtmTrack)));
                if (!arr)
                        return -1;
                for (size_t i = 0; i < tracks.size(); ++i)
                {
                        arr[i].video_id = dup_cstr(tracks[i].video_id);
                        arr[i].title = dup_cstr(tracks[i].title);
                        arr[i].artist = dup_cstr(tracks[i].artist);
                        arr[i].duration = tracks[i].duration;
                }
                *out = arr;
                return static_cast<int>(tracks.size());
        }
        catch (...)
        {
                return -1;
        }
}

char *ytm_resolve_stream_blocking(const char *video_id)
{
        try
        {
                return dup_cstr(ytm::resolve_stream(video_id ? video_id : "").sync_wait());
        }
        catch (...)
        {
                return nullptr;
        }
}

/* kiutube: download pode demorar minutos; o path é copiado p/ malloc antes de retornar */
char *ytm_download_track_blocking(const char *video_id, const char *dest_base)
{
        try
        {
                return dup_cstr(ytm::download_track(video_id ? video_id : "", dest_base ? dest_base : "").sync_wait());
        }
        catch (...)
        {
                return nullptr;
        }
}

/* kiutube: destino dos downloads TUI, combinando com ytm_cli.cpp (cache_streams_dir) */
char *ytm_default_cache_dir(void)
{
        try
        {
                std::string dir = ytm::config().cache_dir + "/streams";
                std::error_code ec;
                std::filesystem::create_directories(dir, ec);
                return dup_cstr(dir);
        }
        catch (...)
        {
                return nullptr;
        }
}

/** Limpa título p/ busca de letras: corta sufixos "(...)", "[...]", "| ...". */
std::string clean_title_for_lyrics(const char *title)
{
        std::string t = title ? title : "";
        size_t cut = t.find_first_of("([|");
        if (cut != std::string::npos)
                t.resize(cut);
        while (!t.empty() && (t.back() == ' ' || t.back() == '-' || t.back() == ':'))
                t.pop_back();
        return t;
}

char *ytm_fetch_lyrics_ex_blocking(const char *artist, const char *title, int duration_seconds)
{
        try
        {
                const std::string clean_title = clean_title_for_lyrics(title);
                const char *eff_title = clean_title.empty() ? (title ? title : "") : clean_title.c_str();

                const std::string key = std::string(artist ? artist : "") + "\n" + eff_title + "\n" +
                                        std::to_string(duration_seconds);
                const std::string cp = cache_path("lyrics", key);
                if (auto cached = cache_read(cp))
                        return dup_cstr(*cached);

                auto lrc = ytm::fetch_lrc(artist ? artist : "", eff_title, "", duration_seconds).sync_wait();
                if (!lrc || lrc->empty())
                        return nullptr;
                cache_write(cp, *lrc);
                return dup_cstr(*lrc);
        }
        catch (...)
        {
                return nullptr;
        }
}

char *ytm_fetch_lyrics_blocking(const char *artist, const char *title)
{
        return ytm_fetch_lyrics_ex_blocking(artist, title, 0);
}

char *ytm_translate_blocking(const char *text_lrc)
{
        try
        {
                const std::string &target = ytm::config().translation_target;
                const std::string key = target + "\n" + (text_lrc ? text_lrc : "");
                const std::string cp = cache_path("trans", key);
                if (auto cached = cache_read(cp))
                        return dup_cstr(*cached);

                // LRC entra como LRC: traduzimos o texto plano e devolvemos
                // com os timestamps originais reaplicados linha a linha.
                const bool is_lrc = std::strchr(text_lrc, '[') != nullptr;
                std::string input = is_lrc ? lrc_plain_text(text_lrc) : std::string(text_lrc ? text_lrc : "");
                if (input.empty())
                        return nullptr;

                std::optional<std::string> result = ytm::translate_to_engine(input, target,
                                                                           ytm::config().translation_engine,
                                                                           ytm::config().translation_source)
                                                        .sync_wait();
                if (!result || result->empty())
                        return nullptr;

                std::string final = is_lrc ? reapply_timestamps(text_lrc, *result) : *result;
                cache_write(cp, final);
                return dup_cstr(final);
        }
        catch (...)
        {
                return nullptr;
        }
}

char *ytm_fetch_thumb_blocking(const char *video_id, const char *thumb_url)
{
        try
        {
                const std::string cp = cache_path("thumbs", video_id ? video_id : thumb_url) + ".jpg";
                std::error_code ec;
                if (std::filesystem::exists(cp, ec))
                        return dup_cstr(cp);
                ytm::CurlHandle http;
                if (!http.download(thumb_url ? thumb_url : "", cp, 20))
                        return nullptr;
                return dup_cstr(cp);
        }
        catch (...)
        {
                return nullptr;
        }
}

int ytm_translation_enabled(void)
{
        return ytm::config().translation_enabled ? 1 : 0;
}

void ytm_free(void *ptr)
{
        std::free(ptr);
}

} // extern "C"
