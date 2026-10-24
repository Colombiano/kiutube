// PT: Busca fuzzy com a métrica do Typesense (src/art.cpp, função
//     levenshtein_dist): distância de edição OSA (optimal string alignment)
//     com inserção/deleção/substituição/transposição. Semantics de match
//     também inspiradas no Typesense: tokens da query casam contra PREFIXOS
//     de tokens do candidato (0 typos) ou com tolerância de typos; todos os
//     tokens da query devem casar. Normalização igual à do Typesense
//     (lower_and_no_special_chars). Porta fiel da recorrência, com atribuição.
// EN: Fuzzy search using Typesense's metric (src/art.cpp, levenshtein_dist):
//     OSA edit distance (insert/delete/substitute/transpose). Match semantics
//     also inspired by Typesense: query tokens match candidate token PREFIXES
//     (0 typos) or within typo tolerance; ALL query tokens must match.
//     Typesense-style normalization (lower_and_no_special_chars). Faithful
//     port of the recurrence, with attribution.
// ES: Búsqueda fuzzy con la métrica de Typesense (src/art.cpp,
//     levenshtein_dist): distancia de edición OSA (inserción/deleción/
//     sustitución/transposición). Semántica de match también inspirada en
//     Typesense: tokens de la query casan contra PREFIJOS de tokens del
//     candidato (0 typos) o con tolerancia; TODOS los tokens deben casar.
// ZH: 使用 Typesense 度量（src/art.cpp，levenshtein_dist）的模糊搜索：
//     OSA 编辑距离（插入/删除/替换/转置）。匹配语义同样借鉴 Typesense：
//     查询 token 匹配候选 token 的前缀（0 typo）或在容错范围内匹配；
//     所有查询 token 都必须匹配。归一化与 Typesense 相同
//     （lower_and_no_special_chars）。忠实移植其递推关系并署名。
#pragma once

#include "library/library.hpp"

#include <algorithm>
#include <cctype>
#include <climits>
#include <string>
#include <string_view>
#include <vector>

namespace library
{

struct Scored
{
        int index = 0;
        int score = 0; // PT: maior = melhor (0 = match perfeito) / EN: higher is better / ZH: 越大越好（0 = 完美匹配）
};

// PT: Distância OSA (Typesense src/art.cpp): a[i] vs b[j]; irow=d[i-2],
//     jrow=d[i-1], krow=d[i]. Early-exit por banda: se a linha toda passar
//     de max_distance, devolve max_distance+1 (poda).
// EN: OSA distance (Typesense src/art.cpp): a[i] vs b[j]; irow=d[i-2],
//     jrow=d[i-1], krow=d[i]. Band early-exit: if the whole row exceeds
//     max_distance, returns max_distance+1 (pruned).
// ES: Distancia OSA (Typesense src/art.cpp): a[i] vs b[j]; irow=d[i-2],
//     jrow=d[i-1], krow=d[i]. Early-exit por banda.
// ZH: OSA 距离（Typesense src/art.cpp）：irow=d[i-2]、jrow=d[i-1]、krow=d[i]。
//     带状提前退出：整行超过 max_distance 时返回 max_distance+1（剪枝）。
inline int osa_distance(std::string_view a, std::string_view b, int max_distance = INT_MAX)
{
        const int n = static_cast<int>(a.size());
        const int m = static_cast<int>(b.size());
        if (n == 0)
                return m;
        if (m == 0)
                return n;

        std::vector<int> irow(m + 1), jrow(m + 1), krow(m + 1);
        for (int j = 0; j <= m; ++j)
                jrow[j] = j; // d[0][j] = j (linha da string vazia / empty-string row)

        for (int i = 1; i <= n; ++i)
        {
                krow[0] = jrow[0] + 1;
                int row_min = krow[0];
                const unsigned char c = static_cast<unsigned char>(a[i - 1]);
                const unsigned char p = i > 1 ? static_cast<unsigned char>(a[i - 2]) : 0;

                for (int column = 1; column <= m; ++column)
                {
                        const int cost = (c == static_cast<unsigned char>(b[column - 1])) ? 0 : 1;
                        const int delete_cost = jrow[column] + 1;
                        const int insert_cost = krow[column - 1] + 1;
                        const int substitution_cost = jrow[column - 1] + cost;
                        krow[column] = std::min({insert_cost, delete_cost, substitution_cost});
                        // PT: transposição OSA (troca adjacente) — mesma condição do art.cpp.
                        // EN: OSA transposition (adjacent swap) — same condition as art.cpp.
                        // ES: transposición OSA — misma condición del art.cpp.
                        // ZH: OSA 转置（相邻交换）—— 与 art.cpp 条件相同。
                        if (i > 1 && column > 1 && c == static_cast<unsigned char>(b[column - 2]) &&
                            p == static_cast<unsigned char>(b[column - 1]))
                                krow[column] = std::min(krow[column], irow[column - 2] + 1);
                        row_min = std::min(row_min, krow[column]);
                }

                if (row_min > max_distance)
                        return max_distance + 1; // PT: poda / EN: prune / ZH: 剪枝
                irow.swap(jrow);
                jrow.swap(krow);
        }
        return jrow[m];
}

// PT: Normalização estilo Typesense: minúsculas, só alnum + não-ASCII.
// EN: Typesense-style normalization: lowercase, alnum + non-ASCII only.
// ES: Normalización estilo Typesense: minúsculas, solo alnum + no-ASCII.
// ZH: Typesense 风格归一化：小写，仅保留 alnum 与非 ASCII。
inline std::string normalize(std::string_view s)
{
        std::string out;
        for (const char ch : s)
        {
                const auto c = static_cast<unsigned char>(ch);
                if (c >= 0x80 || std::isalnum(c))
                        out += static_cast<char>(std::tolower(c));
        }
        return out;
}

// PT: Tolerância de typos por tamanho do token (estilo Typesense):
//     1-3 chars: 0; 4-6: 1; 7+: 2.
// EN: Typo tolerance by token length (Typesense-style): 1-3: 0; 4-6: 1; 7+: 2.
// ES: Tolerancia de typos por tamaño del token (estilo Typesense).
// ZH: 按 token 长度的 typo 容错（Typesense 风格）：1-3：0；4-6：1；7+：2。
inline int max_typos(std::size_t token_len)
{
        if (token_len >= 7)
                return 2;
        if (token_len >= 4)
                return 1;
        return 0;
}

// PT: Um token de query contra um token candidato: prefixo casa com 0;
//     senão OSA dentro da tolerância; senão INF (não casa).
// EN: One query token against one candidate token: prefix matches at 0;
//     else OSA within tolerance; else INF (no match).
// ES: Un token de query contra uno candidato: prefijo casa a 0; si no, OSA
//     dentro de la tolerancia; si no, INF.
// ZH: 查询 token 对候选 token：前缀匹配计 0；否则在容错内取 OSA；否则 INF。
inline int token_match(std::string_view qtok, std::string_view ctok)
{
        if (ctok.size() >= qtok.size() && ctok.compare(0, qtok.size(), qtok) == 0)
                return 0;
        const int limit = max_typos(qtok.size());
        const int d = osa_distance(qtok, ctok, limit);
        return d <= limit ? d : INT_MAX;
}

// PT: Score da faixa: cada candidato (título, artista, "artista título") é
//     tokenizado; todos os tokens da query devem casar com algum token do
//     candidato; score = soma das distâncias (menor = melhor).
// EN: Track score: each candidate (title, artist, "artist title") is
//     tokenized; every query token must match some candidate token; score =
//     sum of distances (lower = better).
// ES: Score de la pista: cada candidato (título, artista, "artista título")
//     se tokeniza; todos los tokens de la query deben casar; score = suma.
// ZH: 曲目打分：每个候选（标题、艺人、"艺人 标题"）分词；查询的所有 token
//     都必须匹配某个候选 token；score = 距离之和（越小越好）。
inline int candidate_score(std::string_view query_norm, std::string_view cand_norm)
{
        if (query_norm.empty())
                return 0;
        // PT: split em espaços. / EN: split on spaces. / ES: split en espacios. / ZH: 按空格分词。
        std::vector<std::string_view> qt, ct;
        auto split = [](std::string_view s, std::vector<std::string_view> &out) {
                std::size_t i = 0;
                while (i < s.size())
                {
                        while (i < s.size() && s[i] == ' ')
                                ++i;
                        const auto start = i;
                        while (i < s.size() && s[i] != ' ')
                                ++i;
                        if (i > start)
                                out.emplace_back(s.substr(start, i - start));
                }
        };
        split(query_norm, qt);
        split(cand_norm, ct);
        if (qt.empty() || ct.empty())
                return INT_MAX;

        int total = 0;
        for (const auto qtok : qt)
        {
                int best = INT_MAX;
                for (const auto ctok : ct)
                        best = std::min(best, token_match(qtok, ctok));
                if (best == INT_MAX)
                        return INT_MAX; // PT: token sem match mata o candidato / EN: unmatched token kills the candidate / ZH: 有 token 匹配失败则候选出局
                total += best;
        }
        return total;
}

// PT: Filtra e ranqueia as faixas. Query vazia devolve tudo com score 0.
// EN: Filters and ranks the tracks. Empty query returns all with score 0.
// ES: Filtra y ranquea las pistas. Query vacía devuelve todas con score 0.
// ZH: 过滤并为曲目打分。空查询返回全部，score 0。
inline std::vector<Scored> fuzzy_filter(const std::vector<Track> &tracks, std::string_view query)
{
        const std::string qn = normalize(query);
        std::vector<Scored> out;
        for (std::size_t i = 0; i < tracks.size(); ++i)
        {
                if (qn.empty())
                {
                        out.push_back({static_cast<int>(i), 0});
                        continue;
                }
                int best = INT_MAX;
                best = std::min(best, candidate_score(qn, normalize(tracks[i].title)));
                best = std::min(best, candidate_score(qn, normalize(tracks[i].artist)));
                best = std::min(best,
                                candidate_score(qn, normalize(tracks[i].artist + " " + tracks[i].title)));
                if (best != INT_MAX)
                        out.push_back({static_cast<int>(i), -best}); // PT: menor distância → score maior / EN: lower distance → higher score / ZH: 距离越小分越高
        }
        std::stable_sort(out.begin(), out.end(), [](const Scored &a, const Scored &b) {
                return a.score > b.score;
        });
        return out;
}

} // namespace library
