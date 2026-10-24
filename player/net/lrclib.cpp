// PT: Implementação LRCLIB: busca na API, unescape \n \" \\, preferência
//     por syncedLyrics. Extrata JSON por busca de chaves — LRCLIB devolve
//     arrays simples e este parser é suficiente (mesma aposta do ytm).
// EN: LRCLIB implementation: API search, unescape \n \" \\, syncedLyrics
//     preferred. JSON by key scanning — LRCLIB returns simple arrays and
//     this parser suffices (same bet as ytm).
// ES: Implementación LRCLIB: búsqueda en la API, unescape \n \" \\,
//     syncedLyrics preferido. JSON por búsqueda de claves — LRCLIB devuelve
//     arrays simples y este parser basta (misma apuesta de ytm).
// ZH: LRCLIB 实现：API 搜索、反转义 \n \" \\、优先 syncedLyrics。按键名
//     扫描提取 JSON —— LRCLIB 返回简单数组，此解析器足够（与 ytm 相同
//     策略）。
#include "net/lrclib.hpp"

#include "krt/log.hpp"
#include "net/http.hpp"

#include <cctype>
#include <chrono>
#include <thread>

namespace net
{
namespace
{

// PT: Decodifica \uXXXX -> UTF-8 (mesma técnica do ytm).
// EN: Decodes \uXXXX -> UTF-8 (same technique as ytm).
// ES: Decodifica \uXXXX -> UTF-8 (misma técnica de ytm).
// ZH: 解码 \uXXXX -> UTF-8（与 ytm 相同技术）。
void append_utf8(std::string &out, unsigned cp)
{
        if (cp < 0x80)
                out += static_cast<char>(cp);
        else if (cp < 0x800)
        {
                out += static_cast<char>(0xc0 | (cp >> 6));
                out += static_cast<char>(0x80 | (cp & 0x3f));
        }
        else if (cp < 0x10000)
        {
                out += static_cast<char>(0xe0 | (cp >> 12));
                out += static_cast<char>(0x80 | ((cp >> 6) & 0x3f));
                out += static_cast<char>(0x80 | (cp & 0x3f));
        }
        else
        {
                out += static_cast<char>(0xf0 | (cp >> 18));
                out += static_cast<char>(0x80 | ((cp >> 12) & 0x3f));
                out += static_cast<char>(0x80 | ((cp >> 6) & 0x3f));
                out += static_cast<char>(0x80 | (cp & 0x3f));
        }
}

// PT: Remove escapes JSON de uma string extraída.
// EN: Strips JSON escapes from an extracted string.
// ES: Quita escapes JSON de una cadena extraída.
// ZH: 去除提取字符串中的 JSON 转义。
std::string unescape(std::string_view s)
{
        std::string out;
        out.reserve(s.size());
        for (std::size_t i = 0; i < s.size(); ++i)
        {
                if (s[i] == '\\' && i + 1 < s.size())
                {
                        const char n = s[++i];
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
                        case 'u':
                                if (i + 4 < s.size())
                                {
                                        unsigned cp = 0;
                                        for (int k = 0; k < 4; ++k)
                                        {
                                                const char h = s[++i];
                                                cp <<= 4;
                                                if (h >= '0' && h <= '9')
                                                        cp |= static_cast<unsigned>(h - '0');
                                                else if (h >= 'a' && h <= 'f')
                                                        cp |= static_cast<unsigned>(h - 'a' + 10);
                                                else if (h >= 'A' && h <= 'F')
                                                        cp |= static_cast<unsigned>(h - 'A' + 10);
                                        }
                                        append_utf8(out, cp);
                                }
                                break;
                        default:
                                out += n; // \\, \", \' etc.
                                break;
                        }
                }
                else
                        out += s[i];
        }
        return out;
}

// PT: Extrai o valor string de "key":"value" a partir de pos.
// EN: Extracts the string value of "key":"value" starting at pos.
// ES: Extrae el valor string de "key":"value" desde pos.
// ZH: 从 pos 提取 "key":"value" 的字符串值。
std::optional<std::string> extract_string(std::string_view body, std::string_view key, std::size_t from = 0)
{
        const std::string needle = "\"" + std::string(key) + "\":\"";
        const auto pos = body.find(needle, from);
        if (pos == std::string_view::npos)
                return std::nullopt;
        const auto start = pos + needle.size();
        // PT: procura o fim respeitando escapes.
        // EN: finds the end respecting escapes. / ES: busca el fin respetando escapes. / ZH: 查找尊重转义的结尾。
        for (std::size_t i = start; i < body.size(); ++i)
        {
                if (body[i] == '\\')
                {
                        ++i;
                        continue;
                }
                if (body[i] == '"')
                        return unescape(body.substr(start, i - start));
        }
        return std::nullopt;
}

} // namespace

// PT: Limpa título para busca de letra: corta sufixos "(...)", "[...]",
//     "| ..." (mesma estratégia de clean_title_for_lyrics do ytm).
// EN: Cleans title for lyrics search: cuts "(...)", "[...]", "| ..." suffixes
//     (same strategy as ytm's clean_title_for_lyrics).
// ES: Limpia el título para búsqueda de letra: corta sufijos "(...)",
//     "[...]", "| ..." (misma estrategia que clean_title_for_lyrics de ytm).
// ZH: 清理搜索用标题：截去 "(...)"、"[...]"、"| ..." 后缀（与 ytm 的
//     clean_title_for_lyrics 相同策略）。
inline std::string clean_title(std::string_view t)
{
        std::string out(t);
        const auto cut_at = [&](char open) {
                bool again = true;
                while (again)
                {
                        again = false;
                        if (const auto p = out.find(open); p != std::string::npos)
                        {
                                const char close = open == '(' ? ')' : ']';
                                if (const auto c = out.find(close, p); c != std::string::npos)
                                {
                                        out.erase(p, c - p + 1);
                                        again = true;
                                }
                        }
                }
        };
        cut_at('(');
        cut_at('[');
        if (const auto bar = out.find('|'); bar != std::string::npos)
                out.erase(bar);
        // PT: '-' é operador de EXCLUSÃO na sintaxe de busca do LRCLIB
        //     (typesense): 'A - B' busca A sem B. Para letras, vira espaço.
        // EN: '-' is an EXCLUSION operator in LRCLIB's search syntax
        //     (typesense): 'A - B' searches A without B. For lyrics it
        //     becomes a space.
        // ES: '-' es operador de EXCLUSIÓN en la sintaxis de LRCLIB
        //     (typesense): 'A - B' busca A sin B. Para letras, se vuelve
        //     espacio.
        // ZH: '-' 在 LRCLIB（typesense 风格）搜索语法中是排除运算符：
        //     'A - B' 表示搜 A 且不含 B。歌词搜索中替换为空格。
        for (auto &c : out)
                if (c == '-')
                        c = ' ';
        while (!out.empty() && (out.back() == ' '))
                out.pop_back();
        return out;
}

std::optional<std::string> fetch_lyrics(std::string_view artist, std::string_view title, int duration_seconds)
{
        const std::string title_clean = clean_title(title);
        // PT: URL: espaços e caracteres especiais codificados de forma mínima.
        // EN: URL: minimal encoding of spaces and special chars.
        // ES: URL: codificación mínima de espacios y caracteres especiales.
        // ZH: URL：对空格与特殊字符做最小编码。
        auto enc = [](std::string_view s) {
                std::string out;
                for (const unsigned char c : s)
                {
                        if (std::isalnum(c) || c == '-' || c == '_' || c == '.')
                                out += static_cast<char>(c);
                        else
                        {
                                const char *hex = "0123456789ABCDEF";
                                out += '%';
                                out += hex[c >> 4];
                                out += hex[c & 15];
                        }
                }
                return out;
        };

        std::string url = "https://lrclib.net/api/search?track_name=" + enc(title_clean) +
                          "&artist_name=" + enc(artist);
        if (duration_seconds > 0)
                url += "&duration=" + std::to_string(duration_seconds);

        // PT: LRCLIB é grátis e às vezes sobrecarregado (503): 3 tentativas
        //     com backoff de 2s. / EN: LRCLIB is free and sometimes
        //     overloaded (503): 3 attempts with 2s backoff.
        // ES: LRCLIB es gratis y a veces está saturado (503): 3 intentos con
        //     backoff de 2s. / ZH: LRCLIB 免费且偶尔过载（503）：3 次重试，
        //     退避 2 秒。
        std::optional<std::string> body;
        for (int attempt = 0; attempt < 3 && !body.has_value(); ++attempt)
        {
                if (attempt > 0)
                {
                        krt::log::info("lrclib: retry {}/3 / retry / reintento / 重试", attempt + 1);
                        std::this_thread::sleep_for(std::chrono::seconds(2));
                }
                body = get(url);
        }
        if (!body || body->empty())
        {
                krt::log::warn("lrclib: sem resposta / no response / sin respuesta / 无响应");
                return std::nullopt;
        }

        // PT: Preferência: primeiro resultado com syncedLyrics; senão plainLyrics.
        // EN: Preference: first result with syncedLyrics; else plainLyrics.
        // ES: Preferencia: primer resultado con syncedLyrics; si no, plainLyrics.
        // ZH: 优先：首个含 syncedLyrics 的结果；否则 plainLyrics。
        for (std::size_t pos = 0;;)
        {
                const auto obj = body->find('{', pos);
                if (obj == std::string::npos)
                        break;
                pos = obj + 1;
                if (auto synced = extract_string(*body, "syncedLyrics", obj);
                    synced && !synced->empty())
                        return synced;
                if (auto plain = extract_string(*body, "plainLyrics", obj); plain && !plain->empty())
                        return plain;
        }
        krt::log::info("lrclib: letra nao encontrada / lyrics not found / letra no encontrada / 未找到歌词");
        return std::nullopt;
}

} // namespace net
