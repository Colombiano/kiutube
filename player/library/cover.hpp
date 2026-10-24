// PT: Capa do álbum para a janela gráfica. Duas fontes, nessa ordem:
//     1) sidecar de thumbnail do yt-dlp (base.<ext> ao lado da faixa —
//        escrito no download YTM desde o M4h);
//     2) imagem embutida extraída via taglib (mp3/ID3v2 APIC, flac, m4a)
//        para cache (~/.cache/kiutube2/covers/), reutilizável.
//     Faixas YTM em cache antigo (sem thumb) simplesmente ficam sem capa.
// EN: Album art for the GUI window. Two sources, in this order:
//     1) yt-dlp thumbnail sidecar (base.<ext> next to the track — written
//        at YTM download since M4h);
//     2) embedded image extracted via taglib (mp3/ID3v2 APIC, flac, m4a)
//        into a cache (~/.cache/kiutube2/covers/), reusable.
//     Old YTM caches (no thumb) simply have no cover.
// ES: Carátula para la ventana gráfica. Dos fuentes: sidecar de miniatura
//     del yt-dlp e imagen embebida vía taglib (mp3/flac/m4a) en caché.
// ZH: 图形窗口的专辑封面。两个来源，按序：1) yt-dlp 缩略图 sidecar
//     （曲目旁的 base.<ext> —— M4h 起随 YTM 下载写入）；2) 经 taglib
//     提取的内嵌图片（mp3/ID3v2 APIC、flac、m4a）到缓存
//     （~/.cache/kiutube2/covers/），可复用。旧 YTM 缓存（无缩略图）
//     仅无封面。
#pragma once

#include <optional>
#include <string>

namespace library
{

// PT: Resolve a capa de uma faixa: sidecar de thumb primeiro, embutida via
//     taglib depois (cacheada em disco). nullopt = sem capa.
// EN: Resolves a track's cover: thumbnail sidecar first, embedded via
//     taglib after (disk-cached). nullopt = no cover.
// ES: Resuelve la carátula de una pista: sidecar primero, embebida vía
//     taglib después (cacheada). nullopt = sin carátula.
// ZH: 解析曲目封面：先 sidecar 缩略图，后 taglib 内嵌（磁盘缓存）。
//     nullopt = 无封面。
[[nodiscard]] std::optional<std::string> cover_for(const std::string &track_path);

} // namespace library
