/**
 * @file translate.cpp
 * @brief Google (gtx) + MyMemory, com extração de resposta sem lib JSON.
 *
 * Google devolve JSON aninhado: [[["segmento traduzido","original",...]]]
 * Varremos o primeiro grupo coletando os literais das posições pares.
 */
#include "translate.hpp"

#include "http.hpp"

#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace ytm
{

namespace
{

/** Extrai todos os literais de string JSON de s, na ordem. */
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
                        if (n == 'u')
                        {
                                // \uXXXX -> UTF-8 (com surrogate pairs \uD83D\uDE00)
                                auto hex4 = [&](size_t p) { return std::stoul(s.substr(p, 4), nullptr, 16); };
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
                                out += n; // cobre ", \\, /
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

/** Parser do formato [[["t","o",...],[...]],...] do gtx. */
std::optional<std::string> parse_gtx(const std::string &json)
{
        size_t p = json.find("[[[");
        if (p == std::string::npos)
                return std::nullopt;
        p += 3;
        std::string out;
        bool first = true;
        while (p < json.size())
        {
                auto seg = read_json_string(json, p);
                if (!seg)
                        break;
                // pula o segmento original (par) e possíveis extras até a próxima tradução
                size_t save = p;
                // formato: [["trad","orig",null,null,n], ...] — após "orig" vem ',' ou ']'
                while (p < json.size() && json[p] != ',' && json[p] != ']')
                        ++p;
                // decide se o próximo literal é tradução (posição par) ou origem
                if (!first)
                {
                        // alterna: se o último literal lido era "origem", descarta
                        // Heurística: na primeira passada lemos "trad"; agora lemos "orig".
                        // Então: descartar o que acabamos de ler e ler o próximo como trad.
                        first = true;
                        // reposiciona: o próximo literal é a próxima tradução
                        if (p < json.size() && json[p] == ']')
                                break;
                        ++p; // pula ','
                        continue;
                }
                if (!out.empty())
                        out += ' ';
                out += *seg;
                first = false;
                if (p < json.size() && json[p] == ']')
                        break;
                ++p;
                // lê "orig"
                auto orig = read_json_string(json, p);
                if (!orig)
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

Task<std::optional<std::string>> google_translate(std::shared_ptr<const std::string> text, std::string target)
{
        co_return co_await offload([txt = std::move(text), tgt = std::move(target)] {
                CurlHandle http;
                std::string url = "https://translate.googleapis.com/translate_a/single?client=gtx&sl=auto&tl=" +
                                  tgt + "&dt=t&q=" + http.escape(*txt);
                std::string body = http.get(url, {}, 15);
                return parse_gtx(body);
        });
}

Task<std::optional<std::string>> mymemory_translate(std::shared_ptr<const std::string> text, std::string target,
                                                    std::string source)
{
        co_return co_await offload([txt = std::move(text), tgt = std::move(target), src = std::move(source)] {
                std::string source_lang = (src.empty() || src == "auto") ? "en" : src; // MyMemory não aceita auto
                CurlHandle http;
                std::string url = "https://api.mymemory.translated.net/get?q=" + http.escape(*txt) +
                                  "&langpair=" + source_lang + "|" + tgt;
                std::string body = http.get(url, {}, 15);
                return json_field(body, "translatedText");
        });
}

} // namespace

Task<std::optional<std::string>> GoogleTranslator::translate(TranslationRequest req) const
{
        return google_translate(std::move(req.text), std::move(req.target_lang));
}

Task<std::optional<std::string>> MyMemoryTranslator::translate(TranslationRequest req) const
{
        return mymemory_translate(std::move(req.text), std::move(req.target_lang), std::move(req.source_hint));
}

namespace
{

constexpr std::size_t kChunkLimit = 400; ///< limite seguro abaixo dos 500 dos providers

/** Divide em blocos de linhas completas, cada um <= kChunkLimit chars. */
std::vector<std::string> split_chunks(const std::string &text)
{
        std::vector<std::string> chunks;
        std::istringstream in(text);
        std::string line, cur;
        while (std::getline(in, line))
        {
                std::size_t extra = cur.empty() ? line.size() : line.size() + 1;
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

/** Traduz chunkado com a cadeia de providers dada (mesma ordem p/ todo chunk). */
template <ytm::Translator... Ts>
ytm::Task<std::optional<std::string>> translate_chunked(const std::string &text, const std::string &target_lang,
                                                        std::string source, Ts... providers)
{
        auto chunks = split_chunks(text);
        if (chunks.empty())
                co_return std::optional<std::string>{};

        std::string out;
        for (auto &chunk : chunks)
        {
                auto r = co_await ytm::translate_text_from(chunk, target_lang, source, providers...);
                if (!r || r->empty())
                        co_return std::optional<std::string>{}; // falha total
                if (!out.empty())
                        out += '\n';
                out += std::move(*r);
        }
        co_return std::optional<std::string>(std::move(out));
}

Task<std::optional<std::string>> translate_to(const std::string &text, const std::string &target_lang)
{
        co_return co_await translate_chunked(text, target_lang, "auto", GoogleTranslator{}, MyMemoryTranslator{});
}

Task<std::optional<std::string>> translate_to_engine(const std::string &text, const std::string &target_lang,
                                                     const std::string &engine, std::string source)
{
        if (engine == "mymemory")
                co_return co_await translate_chunked(text, target_lang, std::move(source), MyMemoryTranslator{});
        co_return co_await translate_chunked(text, target_lang, std::move(source), GoogleTranslator{},
                                             MyMemoryTranslator{});
}

} // namespace ytm
