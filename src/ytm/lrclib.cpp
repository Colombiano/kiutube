/**
 * @file lrclib.cpp
 * @brief LRCLIB: GET /api/get, fallback GET /api/search. Extração do campo
 *        "syncedLyrics" com unescape JSON mínimo (sem dependência externa).
 */
#include "lrclib.hpp"

#include "http.hpp"

#include <optional>
#include <string>

namespace ytm
{

namespace
{

/** Lê um literal JSON ("...") em *pos (apontando para a abre-aspas) e avança. */
std::optional<std::string> read_json_string(const std::string &s, size_t &pos)
{
        if (pos >= s.size() || s[pos] != '"')
                return std::nullopt;
        std::string out;
        ++pos;
        while (pos < s.size())
        {
                char c = s[pos];
                if (c == '"')
                {
                        ++pos;
                        return out;
                }
                if (c == '\\' && pos + 1 < s.size())
                {
                        char n = s[pos + 1];
                        switch (n)
                        {
                        case 'n':
                                out += '\n';
                                break;
                        case 't':
                                out += '\t';
                                break;
                        case 'r':
                                out += '\r';
                                break;
                        case '"':
                                out += '"';
                                break;
                        case '\\':
                                out += '\\';
                                break;
                        case '/':
                                out += '/';
                                break;
                        case 'u':
                        {
                                // \uXXXX -> UTF-8 (BMP + surrogate pairs)
                                auto hex4 = [&](size_t p) { return std::stoul(s.substr(p, 4), nullptr, 16); };
                                if (pos + 6 > s.size())
                                        break;
                                unsigned cp = 0;
                                try
                                {
                                        cp = hex4(pos + 2);
                                }
                                catch (...)
                                {
                                        break;
                                }
                                pos += 6;
                                if (cp >= 0xD800 && cp <= 0xDBFF && pos + 6 <= s.size() && s[pos] == '\\' &&
                                    s[pos + 1] == 'u')
                                {
                                        unsigned lo = hex4(pos + 2);
                                        if (lo >= 0xDC00 && lo <= 0xDFFF)
                                        {
                                                cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                                                pos += 6;
                                        }
                                }
                                if (cp < 0x80)
                                        out += static_cast<char>(cp);
                                else if (cp < 0x800)
                                {
                                        out += static_cast<char>(0xC0 | (cp >> 6));
                                        out += static_cast<char>(0x80 | (cp & 0x3F));
                                }
                                else if (cp < 0x10000)
                                {
                                        out += static_cast<char>(0xE0 | (cp >> 12));
                                        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                                        out += static_cast<char>(0x80 | (cp & 0x3F));
                                }
                                else
                                {
                                        out += static_cast<char>(0xF0 | (cp >> 18));
                                        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
                                        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                                        out += static_cast<char>(0x80 | (cp & 0x3F));
                                }
                                continue;
                        }
                        default:
                                out += n;
                                break;
                        }
                        pos += 2;
                        continue;
                }
                out += c;
                ++pos;
        }
        return std::nullopt;
}

/** Extrai o valor string de "campo" no JSON (primeira ocorrência). */
std::optional<std::string> json_field(const std::string &json, const std::string &field)
{
        std::string needle = "\"" + field + "\"";
        size_t p = json.find(needle);
        if (p == std::string::npos)
                return std::nullopt;
        p = json.find(':', p + needle.size());
        if (p == std::string::npos)
                return std::nullopt;
        ++p;
        while (p < json.size() && (json[p] == ' ' || json[p] == '\t'))
                ++p;
        if (p >= json.size() || json[p] != '"')
                return std::nullopt;
        return read_json_string(json, p);
}

/** Procura syncedLyrics no primeiro objeto de um array JSON de resultados. */
std::optional<std::string> first_synced_from_search(const std::string &json)
{
        size_t p = 0;
        while ((p = json.find("\"syncedLyrics\"", p)) != std::string::npos)
        {
                std::string v = json_field(json.substr(p), "syncedLyrics").value_or("");
                if (!v.empty())
                        return v;
                p += 14;
        }
        return std::nullopt;
}

Task<std::optional<std::string>> fetch_lrc_impl(std::string artist, std::string title, std::string album,
                                                int duration_seconds)
{
        co_return co_await offload([a = std::move(artist), t = std::move(title), al = std::move(album),
                                    dur = duration_seconds] {
                CurlHandle http;
                std::string dur_q = dur > 0 ? "&duration=" + std::to_string(dur) : "";
                std::string url = "https://lrclib.net/api/get?artist_name=" + http.escape(a) +
                                  "&track_name=" + http.escape(t) + dur_q;
                if (!al.empty())
                        url += "&album_name=" + http.escape(al);

                try
                {
                        std::string body = http.get(url, {}, 10);
                        auto lrc = json_field(body, "syncedLyrics");
                        if (lrc && !lrc->empty())
                                return lrc;
                }
                catch (...)
                {
                }

                // fallback: busca fuzzy com artista+título
                try
                {
                        std::string url2 = "https://lrclib.net/api/search?artist_name=" + http.escape(a) +
                                           "&track_name=" + http.escape(t) + dur_q;
                        std::string body = http.get(url2, {}, 10);
                        auto lrc = first_synced_from_search(body);
                        if (lrc && !lrc->empty())
                                return lrc;
                }
                catch (...)
                {
                }

                // último recurso: busca só pelo título (artista do YouTube costuma
                // vir sujo: "beegees", "B.G's" etc.)
                try
                {
                        std::string url3 = "https://lrclib.net/api/search?track_name=" + http.escape(t) + dur_q;
                        std::string body = http.get(url3, {}, 10);
                        return first_synced_from_search(body);
                }
                catch (...)
                {
                        return std::optional<std::string>{};
                }
        });
}

} // namespace

Task<std::optional<std::string>> fetch_lrc(const std::string &artist, const std::string &title,
                                           const std::string &album, int duration_seconds)
{
        return fetch_lrc_impl(artist, title, album, duration_seconds);
}

} // namespace ytm
