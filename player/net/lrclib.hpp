// PT: LRCLIB (letras sincronizadas, grátis/sem auth): busca por
//     artista+título e escolhe o melhor resultado (syncedLyrics preferido).
//     Extração JSON manual (sem dependência): o mesmo approach do
//     src/ytm/lrclib.cpp. Síncrono — usar via krt::offload.
// EN: LRCLIB (synced lyrics, free/no auth): search by artist+title and pick
//     the best result (syncedLyrics preferred). Manual JSON extraction (no
//     dependency): same approach as src/ytm/lrclib.cpp. Synchronous — use
//     through krt::offload.
// ES: LRCLIB (letras sincronizadas, gratis/sin auth): búsqueda por
//     artista+título y mejor resultado (syncedLyrics preferido). Extracción
//     JSON manual (sin dependencia): el mismo approach de
//     src/ytm/lrclib.cpp. Síncrono — usar vía krt::offload.
// ZH: LRCLIB（同步歌词，免费/无需认证）：按艺人+标题搜索并选取最佳结果
//     （优先 syncedLyrics）。手工 JSON 提取（无依赖）：与
//     src/ytm/lrclib.cpp 相同思路。同步 —— 须经 krt::offload 使用。
#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace net
{

// PT: Letra (LRC com timestamps, ou texto puro) da faixa; nullopt se não achou.
// EN: Lyrics (LRC with timestamps, or plain text) for the track; nullopt if not found.
// ES: Letra (LRC con timestamps, o texto plano) de la pista; nullopt si no aparece.
// ZH: 歌词（带时间戳的 LRC 或纯文本）；未找到返回 nullopt。
[[nodiscard]] std::optional<std::string> fetch_lyrics(std::string_view artist, std::string_view title,
                                                      int duration_seconds = 0);

} // namespace net
