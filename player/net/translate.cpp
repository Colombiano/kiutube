// PT: Implementação net/translate: parser JSON próprio (read_json_string
//     com \uXXXX e surrogate pairs, parse do formato [[["t","o",...]]] do
//     gtx, json_field para o MyMemory), chunking por linhas. Port fiel do
//     src/ytm/translate.cpp em forma síncrona sobre net::http.
// EN: net/translate implementation: hand-rolled JSON parsing
//     (read_json_string with \uXXXX and surrogate pairs, parsing of the
//     gtx [[["t","o",...]]] format, json_field for MyMemory), line-based
//     chunking. Faithful port of src/ytm/translate.cpp in synchronous form
//     over net::http.
// ES: Implementación net/translate: parser JSON propio, chunking por
//     líneas. Port fiel de src/ytm/translate.cpp en forma síncrona.
// ZH: net/translate 实现：手写 JSON 解析（read_json_string 支持
//     \uXXXX 与代理对、解析 gtx 的 [[["t","o",...]]] 格式、
//     json_field 用于 MyMemory），按行分块。忠实移植
//     src/ytm/translate.cpp，基于 net::http 的同步形态。
#include "net/translate.hpp"

#include "krt/log.hpp"
#include "net/http.hpp"

#include <curl/curl.h>

#include <sstream>
#include <vector>

namespace net
{
namespace
{

// PT: Percent-encoding via curl (igual ao CurlHandle::escape do ytm).
// EN: Percent-encoding via curl (same as ytm's CurlHandle::escape).
// ES: Percent-encoding vía curl. / ZH: 经 curl 的百分号编码。
std::string url_escape(const std::string &s)
{
        CURL *h = curl_easy_init();
        if (!h)
                return s;
        char *esc = curl_easy_escape(h, s.c_str(), static_cast<int>(s.size()));
        std::string out = esc ? esc : s;
        curl_free(esc);
        curl_easy_cleanup(h);
        return out;
}

// PT: Extrai um literal JSON de s em pos (com \uXXXX e surrogate pairs).
// EN: Extracts one JSON string literal from s at pos (with \uXXXX and
//     surrogate pairs).
// ES: Extrae un literal JSON de s en pos (con \uXXXX y surrogate pairs).
// ZH: 从 s 的 pos 提取一个 JSON 字符串字面量（支持 \uXXXX 与代理对）。
std::optional<std::string> read_json_string(const std::string &s, std::size_t &pos)
{
        if (pos >= s.size() || s[pos] != '"')
                return std::nullopt;
        std::string out;
        ++pos;
        while (pos < s.size())
        {
                const char c = s[pos];
                if (c == '"')
                {
                        ++pos;
                        return out;
                }
                if (c == '\\' && pos + 1 < s.size())
                {
                        const char n = s[pos + 1];
                        if (n == 'u')
                        {
                                auto hex4 = [&](std::size_t p) { return std::stoul(s.substr(p, 4), nullptr, 16); };
                                unsigned cp = 0;
                                bool ok = pos + 6 <= s.size();
                                if (ok)
                                {
                                        try
                                        {
                                                cp = hex4(pos + 2);
                                        }
                                        catch (...)
                                        {
                                                ok = false;
                                        }
                                }
                                if (!ok)
                                {
                                        out += n;
                                        pos += 2;
                                        continue;
                                }
                                pos += 6;
                                if (cp >= 0xD800 && cp <= 0xDBFF && pos + 6 <= s.size() && s[pos] == '\\' &&
                                    s[pos + 1] == 'u')
                                {
                                        const unsigned lo = hex4(pos + 2);
                                        if (lo >= 0xDC00 && lo <= 0xDFFF)
                                        {
                                                cp = 0x10000u + ((cp - 0xD800u) << 10) + (lo - 0xDC00u);
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
                        default:
                                out += n; // PT: cobre ", \\, / / covers ", \\, /
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

std::optional<std::string> json_field(const std::string &json, const std::string &field)
{
        const std::string needle = "\"" + field + "\"";
        std::size_t p = json.find(needle);
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

// PT: Parser do formato [[["trad","orig",...]],...] do gtx: coleta os
//     literais das posições de tradução.
// EN: Parser for the gtx [[["tr","orig",...]],...] format: collects the
//     translated-position literals.
// ES: Parser del formato [[["trad","orig",...]],...] del gtx.
// ZH: 解析 gtx 的 [[["trad","orig",...]],...] 格式：收集译文位置的字面量。
std::optional<std::string> parse_gtx(const std::string &json)
{
        std::size_t p = json.find("[[[");
        if (p == std::string::npos)
                return std::nullopt;
        p += 3;
        std::string out;
        bool first = true;
        while (p < json.size())
        {
                const auto seg = read_json_string(json, p);
                if (!seg.has_value())
                        break;
                while (p < json.size() && json[p] != ',' && json[p] != ']')
                        ++p;
                if (!first)
                {
                        first = true;
                        if (p < json.size() && json[p] == ']')
                                break;
                        ++p;
                        continue;
                }
                if (!out.empty())
                        out += ' ';
                out += *seg;
                first = false;
                if (p < json.size() && json[p] == ']')
                        break;
                ++p;
                const auto orig = read_json_string(json, p); // PT: lê e descarta o original / read+drop original
                if (!orig.has_value())
                        break;
                first = true;
                if (p < json.size() && json[p] == ']')
                        break;
                ++p;
        }
        if (out.empty())
                return std::nullopt;
        return out;
}

// PT: Google gtx (não-oficial). / EN: Google gtx (unofficial).
// ES: Google gtx (no oficial). / ZH: Google gtx（非官方）。
std::optional<std::string> google_translate(const std::string &text, const std::string &target)
{
        const std::string url = "https://translate.googleapis.com/translate_a/single?client=gtx&sl=auto&tl=" +
                                target + "&dt=t&q=" + url_escape(text);
        const auto body = get(url, 15s);
        if (!body.has_value())
                return std::nullopt;
        return parse_gtx(*body);
}

// PT: MyMemory (fallback; não aceita "auto" — assume "en").
// EN: MyMemory (fallback; no "auto" — assumes "en").
// ES: MyMemory (fallback; no acepta "auto" — asume "en").
// ZH: MyMemory（回退；不接受 "auto" —— 按 "en"）。
std::optional<std::string> mymemory_translate(const std::string &text, const std::string &target,
                                              const std::string &source)
{
        const std::string source_lang = (source.empty() || source == "auto") ? "en" : source;
        const std::string url = "https://api.mymemory.translated.net/get?q=" + url_escape(text) +
                                "&langpair=" + source_lang + "|" + target;
        const auto body = get(url, 15s);
        if (!body.has_value())
                return std::nullopt;
        return json_field(*body, "translatedText");
}

constexpr std::size_t kChunkLimit = 400; ///< PT: abaixo dos 500 dos providers / below the providers' 500

// PT: Divide em blocos de linhas completas <= kChunkLimit (timestamps LRC
//     preservados nas linhas). / EN: Splits into complete-line chunks <=
//     kChunkLimit (LRC timestamps preserved in lines).
// ES: Divide en bloques de líneas completas <= kChunkLimit.
// ZH: 按完整行分块 ≤ kChunkLimit（LRC 时间戳保留在行内）。
std::vector<std::string> split_chunks(const std::string &text)
{
        std::vector<std::string> chunks;
        std::istringstream in(text);
        std::string line, cur;
        while (std::getline(in, line))
        {
                const std::size_t extra = cur.empty() ? line.size() : line.size() + 1;
                if (!cur.empty() && cur.size() + extra > kChunkLimit)
                {
                        chunks.push_back(std::move(cur));
                        cur.clear();
                }
                if (!cur.empty())
                        cur += '\n';
                cur += line;
        }
        if (!cur.empty())
                chunks.push_back(std::move(cur));
        return chunks;
}

} // namespace

std::optional<std::string> translate_to(const std::string &text, const std::string &target_lang)
{
        const auto chunks = split_chunks(text);
        if (chunks.empty())
                return std::nullopt;

        std::string out;
        for (const auto &chunk : chunks)
        {
                auto r = google_translate(chunk, target_lang);
                if (!r.has_value() || r->empty())
                        r = mymemory_translate(chunk, target_lang, "auto");
                if (!r.has_value() || r->empty())
                {
                        krt::log::warn("translate: chunk falhou / chunk failed / chunk falló / 块失败");
                        return std::nullopt; // PT: falha total / total failure / 整体失败
                }
                if (!out.empty())
                        out += '\n';
                out += std::move(*r);
        }
        return out;
}

} // namespace net
