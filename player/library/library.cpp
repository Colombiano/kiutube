// PT: Implementação do scan com taglib (C++) — FileRef RAII lê tags de
//     wav/mp3/flac/ogg/opus/m4a sem codecs próprios (só metadados).
// EN: Scan implementation with taglib (C++) — FileRef RAII reads tags from
//     wav/mp3/flac/ogg/opus/m4a without own codecs (metadata only).
// ES: Implementación del scan con taglib (C++) — FileRef RAII lee tags de
//     wav/mp3/flac/ogg/opus/m4a sin codecs propios (solo metadatos).
// ZH: 基于 taglib（C++）的扫描实现 —— FileRef RAII 读取 wav/mp3/flac/ogg/
//     opus/m4a 的标签（仅元数据，无需自有解码器）。
#include "library/library.hpp"

#include <taglib/fileref.h>
#include <taglib/tag.h>

#include <algorithm>
#include <filesystem>
#include <system_error>

namespace library
{

namespace
{

// PT: Uma faixa a partir de um arquivo: taglib primeiro, filename fallback.
// EN: One track from a file: taglib first, filename fallback.
// ES: Una pista a partir de un archivo: taglib primero, filename fallback.
// ZH: 从文件构造曲目：优先 taglib，回退文件名。
Track make_track(const std::filesystem::path &p)
{
        Track t;
        t.path = p.string();
        t.title = p.filename().string();
        t.artist.clear();

        TagLib::FileRef f(p.c_str());
        if (!f.isNull() && f.tag())
        {
                const TagLib::Tag *tag = f.tag();
                const std::string title = tag->title().to8Bit(true);
                const std::string artist = tag->artist().to8Bit(true);
                if (!title.empty())
                        t.title = title;
                t.artist = artist;
                t.album = tag->album().to8Bit(true);
        }
        if (f.audioProperties())
                t.duration_seconds = static_cast<int>(f.audioProperties()->lengthInSeconds());
        return t;
}

} // namespace

std::vector<Track> scan(const std::string &root)
{
        std::vector<Track> out;
        std::error_code ec;
        if (!std::filesystem::exists(root, ec) || ec)
                return out;

        std::filesystem::recursive_directory_iterator it(root, std::filesystem::directory_options::skip_permission_denied, ec);
        const std::filesystem::recursive_directory_iterator end;
        for (; !ec && it != end; it.increment(ec))
        {
                if (!it->is_regular_file(ec))
                        continue;
                if (is_audio_file(it->path().string()))
                        out.push_back(make_track(it->path()));
        }

        // PT: Ordenação estável: artista, depois título (minúsculas).
        // EN: Stable sort: artist, then title (lowercase).
        // ES: Ordenación estable: artista, después título (minúsculas).
        // ZH: 稳定排序：艺人，再标题（小写）。
        std::sort(out.begin(), out.end(), [](const Track &a, const Track &b) {
                const auto lc = [](const std::string &s) {
                        std::string r(s);
                        for (auto &c : r)
                                if (c >= 'A' && c <= 'Z')
                                        c = static_cast<char>(c - 'A' + 'a');
                        return r;
                };
                const auto la = lc(a.artist), lb = lc(b.artist);
                if (la != lb)
                        return la < lb;
                return lc(a.title) < lc(b.title);
        });
        return out;
}

} // namespace library
