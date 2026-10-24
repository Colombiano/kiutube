// PT: YouTube Music via yt-dlp (port síncrono e enxuto de src/ytm/ytm.cpp —
//     mesma estratégia do ytm-player: templates --print, sem parser de JSON).
//     SÍNCRONO: chamar via krt::offload. Download pelo próprio yt-dlp
//     (resolve+download consistentes dentro de um processo — URLs do
//     googlevideo são assinadas por IP).
// EN: YouTube Music via yt-dlp (lean synchronous port of src/ytm/ytm.cpp —
//     same ytm-player strategy: --print templates, no JSON parser).
//     SYNCHRONOUS: call through krt::offload. Download runs through yt-dlp
//     itself (consistent resolve+download in one process — googlevideo URLs
//     are IP-signed).
// ES: YouTube Music vía yt-dlp (port síncrono y enxuto de src/ytm/ytm.cpp).
//     SÍNCRONO: llamar vía krt::offload. El download lo hace el propio
//     yt-dlp (resolve+download consistentes en un proceso).
// ZH: 经 yt-dlp 的 YouTube Music（精简同步移植自 src/ytm/ytm.cpp —— 与
//     ytm-player 相同策略：--print 模板，无 JSON 解析）。同步：须经
//     krt::offload 调用。下载由 yt-dlp 自身完成（单进程内
//     resolve+download 一致 —— googlevideo URL 按 IP 签名）。
#pragma once

#include <optional>
#include <string>
#include <vector>

namespace net
{

// PT: Um resultado de busca no YouTube Music.
// EN: One YouTube Music search result.
// ES: Un resultado de búsqueda en YouTube Music.
// ZH: 一条 YouTube Music 搜索结果。
struct YtmResult
{
        std::string video_id;
        std::string title;
        std::string artist;
        int duration_seconds = 0; // PT: 0 = desconhecida (flat playlist vem "NA") / 0 = unknown / 0 = 未知
};

// PT: Localiza um yt-dlp funcional: $YTDLP → venv do ytm-player → PATH.
// EN: Locates a working yt-dlp: $YTDLP -> ytm-player venv -> PATH.
// ES: Localiza un yt-dlp funcional: $YTDLP → venv de ytm-player → PATH.
// ZH: 定位可用的 yt-dlp：$YTDLP → ytm-player venv → PATH。
[[nodiscard]] std::string ytdlp_path();

// PT: Busca `limit` resultados. nullopt em falha (yt-dlp ausente/erro).
// EN: Searches `limit` results. nullopt on failure.
// ES: Busca `limit` resultados. nullopt en caso de fallo.
// ZH: 搜索 `limit` 条结果；失败返回 nullopt。
[[nodiscard]] std::optional<std::vector<YtmResult>> ytm_search(const std::string &query, int limit = 10);

// PT: Baixa o bestaudio de `video_id` para dest_base + ext (yt-dlp escolhe).
//     Retorna o caminho final ou nullopt. Cria o diretório do destino.
// EN: Downloads bestaudio of `video_id` to dest_base + ext (yt-dlp picks).
//     Returns the final path or nullopt. Creates the destination dir.
// ES: Descarga el bestaudio de `video_id` a dest_base + ext. Devuelve el
//     camino final o nullopt. Crea el directorio del destino.
// ZH: 下载 `video_id` 的 bestaudio 到 dest_base + 扩展名（由 yt-dlp 决定）。
//     返回最终路径或 nullopt。会创建目标目录。
[[nodiscard]] std::optional<std::string> ytm_download(const std::string &video_id, const std::string &dest_base);

} // namespace net
