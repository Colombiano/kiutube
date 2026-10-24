// PT: Implementação library/cover: sidecar de thumb (barato) e extração
//     taglib por formato (mp3 APIC, FLAC picture list, MP4 covr) com cache
//     em disco. Síncrono — a janela gráfica chama na troca de faixa.
// EN: library/cover implementation: thumbnail sidecar (cheap) and per-format
//     taglib extraction (mp3 APIC, FLAC picture list, MP4 covr) with a disk
//     cache. Synchronous — the GUI window calls it on track change.
// ES: Implementación library/cover: sidecar de miniatura y extracción
//     taglib por formato (mp3 APIC, FLAC pictures, MP4 covr) con caché.
// ZH: library/cover 实现：sidecar 缩略图（廉价）与按格式的 taglib 提取
//     （mp3 APIC、FLAC picture list、MP4 covr），带磁盘缓存。同步 ——
//     图形窗口在曲目切换时调用。
#include "library/cover.hpp"

#include "krt/log.hpp"

#include <taglib/attachedpictureframe.h>
#include <taglib/flacfile.h>
#include <taglib/flacpicture.h>
#include <taglib/id3v2tag.h>
#include <taglib/mp4coverart.h>
#include <taglib/mp4file.h>
#include <taglib/mp4tag.h>
#include <taglib/mpegfile.h>
#include <taglib/tbytevector.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <system_error>

namespace library
{
namespace
{

// PT: Sidecar de thumb: base.<ext> ao lado da faixa (yt-dlp --write-thumbnail).
// EN: Thumbnail sidecar: base.<ext> next to the track (yt-dlp --write-thumbnail).
// ES: Sidecar de miniatura: base.<ext> junto a la pista.
// ZH: 缩略图 sidecar：曲目旁的 base.<ext>（yt-dlp --write-thumbnail）。
std::optional<std::string> sidecar_thumb(const std::string &track_path)
{
        const std::filesystem::path p(track_path);
        const std::string base = p.parent_path().string() + "/" + p.stem().string();
        for (const char *ext : {".webp", ".jpg", ".jpeg", ".png"})
        {
                std::error_code ec;
                if (std::filesystem::exists(base + ext, ec) && !ec)
                        return base + ext;
        }
        return std::nullopt;
}

// PT: Cache de capas extraídas: <xdg cache>/kiutube/covers/<hash>.<ext>.
// EN: Extracted cover cache: <xdg cache>/kiutube/covers/<hash>.<ext>.
// ES: Caché de carátulas extraídas. / ZH: 提取封面的缓存目录。
std::filesystem::path covers_dir()
{
        std::string base;
        if (const char *xdg = std::getenv("XDG_CACHE_HOME"); xdg && *xdg)
                base = xdg;
        else if (const char *home = std::getenv("HOME"); home && *home)
                base = std::string(home) + "/.cache";
        else
                base = ".";
        return std::filesystem::path(base) / "kiutube" / "covers";
}

std::string ext_from_mime(const std::string &mime)
{
        if (mime.find("png") != std::string::npos)
                return ".png";
        if (mime.find("jpeg") != std::string::npos || mime.find("jpg") != std::string::npos)
                return ".jpg";
        return ".img";
}

bool write_cache(const std::string &key, const std::string &ext, const char *data, std::size_t size,
                 std::string &out_path)
{
        std::error_code ec;
        std::filesystem::create_directories(covers_dir(), ec);
        const auto path = covers_dir() / (std::to_string(std::hash<std::string>{}(key)) + ext);
        std::ofstream f(path, std::ios::binary | std::ios::trunc);
        if (!f)
                return false;
        f.write(data, static_cast<std::streamsize>(size));
        if (!f)
                return false;
        out_path = path.string();
        return true;
}

// PT: Extrai imagem embutida por formato. TagLib lê de graça o que cada
//     container guarda. / EN: Extracts the embedded image per format.
// ES: Extrae la imagen embebida por formato. / ZH: 按格式提取内嵌图片。
std::optional<std::string> extract_embedded(const std::string &track_path)
{
        const auto lower = [](std::string s) {
                for (auto &c : s)
                        if (c >= 'A' && c <= 'Z')
                                c = static_cast<char>(c - 'A' + 'a');
                return s;
        }(track_path);
        auto save = [&](const TagLib::ByteVector &data, const std::string &mime) -> std::optional<std::string> {
                if (data.isEmpty())
                        return std::nullopt;
                std::string out;
                if (!write_cache(track_path, ext_from_mime(mime), data.data(), data.size(), out))
                        return std::nullopt;
                return out;
        };

        if (lower.size() >= 4 && lower.compare(lower.size() - 4, 4, ".mp3") == 0)
        {
                TagLib::MPEG::File f(track_path.c_str());
                if (f.isValid() && f.ID3v2Tag())
                {
                        const auto frames = f.ID3v2Tag()->frameListMap()["APIC"];
                        if (!frames.isEmpty())
                        {
                                const auto *pic = static_cast<TagLib::ID3v2::AttachedPictureFrame *>(frames.front());
                                return save(pic->picture(), pic->mimeType().to8Bit(true));
                        }
                }
                return std::nullopt;
        }
        if (lower.size() >= 5 && lower.compare(lower.size() - 5, 5, ".flac") == 0)
        {
                TagLib::FLAC::File f(track_path.c_str());
                if (f.isValid())
                {
                        const auto pics = f.pictureList();
                        if (!pics.isEmpty())
                                return save(pics.front()->data(), pics.front()->mimeType().to8Bit(true));
                }
                return std::nullopt;
        }
        if ((lower.size() >= 4 && lower.compare(lower.size() - 4, 4, ".m4a") == 0) ||
            (lower.size() >= 4 && lower.compare(lower.size() - 4, 4, ".mp4") == 0))
        {
                TagLib::MP4::File f(track_path.c_str());
                if (f.isValid() && f.tag())
                {
                        const auto it = f.tag()->itemMap().find("covr"); // PT: taglib 2.x: itemMap (era itemListMap) / taglib 2.x renamed it
                        if (it != f.tag()->itemMap().end())
                        {
                                const auto arts = it->second.toCoverArtList();
                                if (!arts.isEmpty())
                                {
                                        // PT: taglib 2.x: CoverArt perdeu
                                        //     mimeType(); format() diz o tipo.
                                        // EN: taglib 2.x: CoverArt lost
                                        //     mimeType(); format() tells it.
                                        // ZH: taglib 2.x：CoverArt 移除了
                                        //     mimeType()；用 format() 判断。
                                        const auto &art = arts.front();
                                        const std::string ext =
                                            art.format() == TagLib::MP4::CoverArt::PNG ? ".png" : ".jpg";
                                        std::string out;
                                        if (!art.data().isEmpty() &&
                                            write_cache(track_path, ext, art.data().data(), art.data().size(), out))
                                                return out;
                                }
                        }
                }
                return std::nullopt;
        }
        return std::nullopt;
}

} // namespace

std::optional<std::string> cover_for(const std::string &track_path)
{
        if (auto sidecar = sidecar_thumb(track_path); sidecar.has_value())
                return sidecar;
        std::error_code ec;
        if (!std::filesystem::exists(track_path, ec) || ec)
                return std::nullopt;
        if (auto embedded = extract_embedded(track_path); embedded.has_value())
        {
                krt::log::info("cover: extraida / extracted / extraída / 已提取: {}", *embedded);
                return embedded;
        }
        return std::nullopt;
}

} // namespace library
