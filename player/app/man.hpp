// PT: Mini-MAN do kiutube: `kiutube --help` imprime uma página estilo
//     man(1) no terminal — NAME, SYNOPSIS, DESCRIPTION, OPTIONS, KEYS,
//     FILES, ENVIRONMENT, EXAMPLES. Seções em negrito (ANSI só quando
//     stdout é TTY), texto quebrado na largura do terminal (TIOCGWINSZ,
//     fallback 80). Idioma segue o LANG (pt/en/es/zh; pt = fallback).
// EN: kiutube's mini man page: `kiutube --help` prints a man(1)-style
//     page to the terminal — NAME, SYNOPSIS, DESCRIPTION, OPTIONS, KEYS,
//     FILES, ENVIRONMENT, EXAMPLES. Bold section headers (ANSI only when
//     stdout is a TTY), text wrapped to the terminal width (TIOCGWINSZ,
//     fallback 80). Language follows LANG (pt/en/es/zh; pt = fallback).
// ES: Mini-MAN de kiutube: `kiutube --help` imprime una página estilo
//     man(1). ZH: kiutube 的内置 mini man：`kiutube --help` 在终端输出
//     man(1) 风格页面。
#pragma once

#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <unistd.h>
#include <vector>

namespace app
{

// PT: Imprime o MAN no stream (stdout para --help). Idioma: "pt"/"en"/"es"/"zh".
// EN: Prints the MAN to the stream (stdout for --help). Lang: "pt"/"en"/"es"/"zh".
// ZH: 将 MAN 打印到流（--help 用 stdout）。语言："pt"/"en"/"es"/"zh"。
inline void print_man(std::FILE *out, const std::string_view lang)
{
        // PT: seções: título + linhas (strings já quebradas manualmente;
        //     wrap só aplica a linhas longas demais para a largura).
        struct Sec
        {
                const char *title;
                std::vector<const char *> lines;
        };
        const bool pt = lang != "en" && lang != "es" && lang != "zh";
        const std::vector<Sec> secs =
            pt ? std::vector<Sec>{
                     {"NAME", {"kiutube - player de musica para terminal (reescrita C++20 do kew)"}},
                     {"SYNOPSIS",
                      {"kiutube [--help] [--themes] [--theme NOME]",
                       "kiutube [--ytm search|play \"<busca>\"]",
                       "kiutube play <arquivo ...>"}},
                     {"DESCRIPTION",
                      {"Toca musica local (wav/mp3/flac/webm) com engine gapless, biblioteca",
                       "fuzzy, letras LRCLIB com traducao automatica, YouTube Music via yt-dlp,",
                       "visualizadores FFT (TUI e janela Qt), capa de album (chafa/Qt) e MPRIS."}},
                     {"OPTIONS",
                      {"--help, -h        esta pagina",
                       "--themes          lista os temas com previa colorida",
                       "--theme NOME      abre usando o tema (.theme do kew)",
                       "--ytm search Q    busca no YouTube Music e lista",
                       "--ytm play Q      baixa o top 1 e abre o TUI tocando",
                       "play arq...       toca os arquivos (sem argumentos: TUI vazia)"}},
                     {"KEYS",
                      {"espaco pausa/toca   n proxima   setas seek +/-5s   +/- volume",
                       "l biblioteca (picker)   g letra (LRCLIB)   F8 busca YTM   F9 traducao",
                       "v visualizador (TUI/Qt)   p fila   i faixa   c capa   t tema   q sai"}},
                     {"FILES",
                      {"~/.config/kiutube/init.lua     configuracao Lua (settings + bind)",
                       "~/.config/kew/kewrc            compat: path=/volume=/visualizer=/theme=",
                       "~/.config/kiutube/themes/      temas proprios (*.theme)",
                       "~/.config/kew/themes/          temas do kew reaproveitados",
                       "~/.cache/kiutube/streams/      downloads do YouTube Music"}},
                     {"ENVIRONMENT",
                      {"KIUTUBE2_LOG=debug|info|warn|error   nivel de log (stderr)",
                       "KIUTUBE_NO_GUI=1                       desativa a janela Qt",
                       "KIUTUBE_NERD_FONTS=1                   icones Nerd Font na TUI"}},
                     {"EXAMPLES",
                      {"kiutube play ~/musica/album/*.flac",
                       "kiutube --ytm play \"songbird bee gees\"",
                       "kiutube --theme gruvbox"}},
                     {"SEE ALSO",
                      {"docs/CPP20-REWRITE.md (roadmap/arquitetura), README-YTM.md (camada ytm)",
                       "Repositorio: https://github.com/Colombiano/kiutube"}},
                 }
                 : lang == "en" ? std::vector<Sec>{
                     {"NAME", {"kiutube - terminal music player (C++20 rewrite of kew)"}},
                     {"SYNOPSIS",
                      {"kiutube [--help] [--themes] [--theme NAME]",
                       "kiutube [--ytm search|play \"<query>\"]",
                       "kiutube play <file ...>"}},
                     {"DESCRIPTION",
                      {"Plays local music (wav/mp3/flac/webm) with a gapless engine, fuzzy",
                       "library, LRCLIB lyrics with auto-translation, YouTube Music via",
                       "yt-dlp, FFT visualizers (TUI and Qt window), album art (chafa/Qt), MPRIS."}},
                     {"OPTIONS",
                      {"--help, -h        this page",
                       "--themes          list themes with a colored preview",
                       "--theme NAME      start using a theme (kew .theme file)",
                       "--ytm search Q    YouTube Music search and list",
                       "--ytm play Q      download top 1 and open the TUI playing",
                       "play files...     play files (no args: empty TUI)"}},
                     {"KEYS",
                      {"space pause/play   n next   arrows seek +/-5s   +/- volume",
                       "l library (picker)   g lyrics (LRCLIB)   F8 YTM search   F9 translation",
                       "v visualizer (TUI/Qt)   p queue   i track   c cover   t theme   q quit"}},
                     {"FILES",
                      {"~/.config/kiutube/init.lua     Lua config (settings + bind)",
                       "~/.config/kew/kewrc            compat: path=/volume=/visualizer=/theme=",
                       "~/.config/kiutube/themes/      your own themes (*.theme)",
                       "~/.config/kew/themes/          reused kew themes",
                       "~/.cache/kiutube/streams/      YouTube Music downloads"}},
                     {"ENVIRONMENT",
                      {"KIUTUBE2_LOG=debug|info|warn|error   log level (stderr)",
                       "KIUTUBE_NO_GUI=1                       disable the Qt window",
                       "KIUTUBE_NERD_FONTS=1                   Nerd Font icons in the TUI"}},
                     {"EXAMPLES",
                      {"kiutube play ~/music/album/*.flac",
                       "kiutube --ytm play \"songbird bee gees\"",
                       "kiutube --theme gruvbox"}},
                     {"SEE ALSO",
                      {"docs/CPP20-REWRITE.md (roadmap/architecture), README-YTM.md (ytm layer)",
                       "Repository: https://github.com/Colombiano/kiutube"}},
                 }
                 : lang == "es" ? std::vector<Sec>{
                     {"NAME", {"kiutube - reproductor de musica para terminal (reescritura C++20 de kew)"}},
                     {"SYNOPSIS",
                      {"kiutube [--help] [--themes] [--theme NOMBRE]",
                       "kiutube [--ytm search|play \"<busqueda>\"]",
                       "kiutube play <archivo ...>"}},
                     {"DESCRIPTION",
                      {"Reproduce musica local (wav/mp3/flac/webm) con motor gapless,",
                       "biblioteca fuzzy, letras LRCLIB con traduccion automatica, YouTube",
                       "Music via yt-dlp, visualizadores FFT (TUI y ventana Qt) y MPRIS."}},
                     {"OPTIONS",
                      {"--help, -h        esta pagina",
                       "--themes          lista los temas con vista previa en color",
                       "--theme NOMBRE    abre usando el tema (.theme de kew)",
                       "--ytm search Q    busca en YouTube Music y lista",
                       "--ytm play Q      descarga el top 1 y abre el TUI sonando",
                       "play archivos...  reproduce archivos (sin args: TUI vacia)"}},
                     {"KEYS",
                      {"espacio pausa/toca   n siguiente   flechas seek +/-5s   +/- volumen",
                       "l biblioteca (picker)   g letra (LRCLIB)   F8 busca YTM   F9 traduccion",
                       "v visualizador (TUI/Qt)   p cola   i pista   c caratula   t tema   q sale"}},
                     {"FILES",
                      {"~/.config/kiutube/init.lua     configuracion Lua (settings + bind)",
                       "~/.config/kew/kewrc            compat: path=/volume=/visualizer=/theme=",
                       "~/.config/kiutube/themes/      temas propios (*.theme)",
                       "~/.config/kew/themes/          temas de kew reaprovechados",
                       "~/.cache/kiutube/streams/      descargas de YouTube Music"}},
                     {"ENVIRONMENT",
                      {"KIUTUBE2_LOG=debug|info|warn|error   nivel de log (stderr)",
                       "KIUTUBE_NO_GUI=1                       desactiva la ventana Qt",
                       "KIUTUBE_NERD_FONTS=1                   iconos Nerd Font en la TUI"}},
                     {"EXAMPLES",
                      {"kiutube play ~/musica/album/*.flac",
                       "kiutube --ytm play \"songbird bee gees\"",
                       "kiutube --theme gruvbox"}},
                     {"SEE ALSO",
                      {"docs/CPP20-REWRITE.md (roadmap/arquitectura), README-YTM.md (capa ytm)",
                       "Repositorio: https://github.com/Colombiano/kiutube"}},
                 }
                 : std::vector<Sec>{
                       {"NAME", {"kiutube - 终端音乐播放器（kew 的 C++20 重写）"}},
                       {"SYNOPSIS",
                        {"kiutube [--help] [--themes] [--theme 名称]",
                         "kiutube [--ytm search|play \"<搜索>\"]",
                         "kiutube play <文件 ...>"}},
                       {"DESCRIPTION",
                        {"播放本地音乐（wav/mp3/flac/webm），支持 gapless 引擎、模糊曲库、",
                         "LRCLIB 歌词与自动翻译、经 yt-dlp 的 YouTube Music、FFT 可视化",
                         "（TUI 与 Qt 窗口）、专辑封面（chafa/Qt）以及 MPRIS。"}},
                       {"OPTIONS",
                        {"--help, -h        本页面",
                         "--themes          列出全部主题并彩色预览",
                         "--theme 名称      使用指定主题启动（kew 的 .theme 文件）",
                         "--ytm search Q    YouTube Music 搜索并列出",
                         "--ytm play Q      下载第 1 个结果并进入 TUI 播放",
                         "play 文件...      播放文件（无参数：空白 TUI）"}},
                       {"KEYS",
                        {"space 暂停/播放   n 下一首   方向键 seek ±5s   +/- 音量",
                         "l 曲库（选择器）   g 歌词（LRCLIB）   F8 YTM 搜索   F9 翻译",
                         "v 可视化器（TUI/Qt）   p 队列   i 曲目   c 封面   t 主题   q 退出"}},
                       {"FILES",
                        {"~/.config/kiutube/init.lua     Lua 配置（settings + bind）",
                         "~/.config/kew/kewrc            兼容：path=/volume=/visualizer=/theme=",
                         "~/.config/kiutube/themes/      自有主题（*.theme）",
                         "~/.config/kew/themes/          复用 kew 主题",
                         "~/.cache/kiutube/streams/      YouTube Music 下载"}},
                       {"ENVIRONMENT",
                        {"KIUTUBE2_LOG=debug|info|warn|error   日志级别（stderr）",
                         "KIUTUBE_NO_GUI=1                       禁用 Qt 窗口",
                         "KIUTUBE_NERD_FONTS=1                   TUI 使用 Nerd Font 图标"}},
                       {"EXAMPLES",
                        {"kiutube play ~/music/album/*.flac",
                         "kiutube --ytm play \"songbird bee gees\"",
                         "kiutube --theme gruvbox"}},
                       {"SEE ALSO",
                        {"docs/CPP20-REWRITE.md（路线图/架构）、README-YTM.md（ytm 层）",
                         "仓库：https://github.com/Colombiano/kiutube"}},
                   };

        const bool ansi = ::isatty(fileno(out));
        const auto bold = [&](const char *s) {
                if (ansi)
                        std::fprintf(out, "\x1b[1m%s\x1b[0m\n", s);
                else
                        std::fprintf(out, "%s\n", s);
        };
        bold("kiutube(1)                     Manual do kiutube                     kiutube(1)");
        for (const auto &sec : secs)
        {
                std::fputc('\n', out);
                bold(sec.title);
                for (const char *line : sec.lines)
                        std::fprintf(out, "       %s\n", line);
        }
}

} // namespace app
