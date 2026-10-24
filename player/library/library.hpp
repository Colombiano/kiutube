// PT: Biblioteca de faixas: scan recursivo do diretório configurado com
//     metadados via taglib (título/artista/álbum/duração), fallback para o
//     nome do arquivo. I/O puro — o caller faz scan via krt::offload.
// EN: Track library: recursive scan of the configured directory with taglib
//     metadata (title/artist/album/duration), filename fallback. Pure I/O —
//     the caller runs scan through krt::offload.
// ES: Biblioteca de pistas: scan recursivo del directorio configurado con
//     metadatos vía taglib (título/artista/álbum/duración), fallback al
//     nombre del archivo. E/S pura — el caller hace scan vía krt::offload.
// ZH: 曲库模块：递归扫描配置目录，经 taglib 读取元数据（标题/艺人/专辑/
//     时长），失败回退到文件名。纯 I/O —— 调用方应经 krt::offload 执行。
#pragma once

#include <string>
#include <vector>

namespace library
{

struct Track
{
        std::string path;
        std::string title;  // PT: tag taglib ou stem do arquivo / EN: taglib tag or file stem / ZH: taglib 标签或文件名主干
        std::string artist; // PT: tag taglib ou vazio / EN: taglib tag or empty / ZH: taglib 标签或空
        std::string album;
        int duration_seconds = 0;
};

// PT: Extensões reconhecidas (miniaudio nativo + webm do shim).
// EN: Recognized extensions (native miniaudio + shim webm).
// ES: Extensiones reconocidas (miniaudio nativo + webm del shim).
// ZH: 支持的扩展名（原生 miniaudio + shim webm）。
inline bool is_audio_file(const std::string &path)
{
        const std::string lower = [](std::string s) {
                for (auto &c : s)
                        if (c >= 'A' && c <= 'Z')
                                c = static_cast<char>(c - 'A' + 'a');
                return s;
        }(path);
        for (const char *ext : {".wav", ".mp3", ".flac", ".ogg", ".opus", ".webm", ".m4a", ".aac"})
                if (lower.size() >= std::char_traits<char>::length(ext) &&
                    lower.compare(lower.size() - std::char_traits<char>::length(ext),
                                  std::char_traits<char>::length(ext), ext) == 0)
                        return true;
        return false;
}

// PT: Escaneia `root` recursivo e devolve as faixas ordenadas por artista+título.
// EN: Recursively scans `root` and returns tracks sorted by artist+title.
// ES: Escanea `root` recursivamente y devuelve las pistas ordenadas por artista+título.
// ZH: 递归扫描 `root`，按艺人+标题排序返回曲目。
[[nodiscard]] std::vector<Track> scan(const std::string &root);

} // namespace library
