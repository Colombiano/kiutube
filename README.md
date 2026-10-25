# kiutube

![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=c%2B%2B&logoColor=white)
![CMake](https://img.shields.io/badge/CMake-064F8C?logo=cmake&logoColor=white)
![Lua 5.4](https://img.shields.io/badge/Lua-5.4-2C2D72?logo=lua&logoColor=white)
![Qt 6](https://img.shields.io/badge/Qt-6-41CD52?logo=qt&logoColor=white)
![YouTube Music](https://img.shields.io/badge/YouTube%20Music-yt--dlp-FF0033?logo=youtubemusic&logoColor=white)
![MPRIS](https://img.shields.io/badge/MPRIS-D--Bus-4C8BF5)
![Linux](https://img.shields.io/badge/Linux-FCC624?logo=linux&logoColor=black)
![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-blue.svg)
![CI](https://github.com/Colombiano/kiutube/actions/workflows/ubuntu.yml/badge.svg)

![Typesense](https://img.shields.io/badge/Typesense-fuzzy%20search-1035FB?logo=typesense&logoColor=white)
![miniaudio](https://img.shields.io/badge/miniaudio-audio%20engine-222222)
![PipeWire](https://img.shields.io/badge/PipeWire-audio%20server-262F3B)
![taglib](https://img.shields.io/badge/taglib-metadata-990000)
![libcurl](https://img.shields.io/badge/libcurl-HTTP-073551?logo=curl&logoColor=white)
![sdbus-c++](https://img.shields.io/badge/sdbus--c%2B%2B-D--Bus%20binding-00A4EF)
![chafa](https://img.shields.io/badge/chafa-TUI%20cover-8A2BE2)
![sol2](https://img.shields.io/badge/sol2-Lua%20binding-8F005E)
![pocketfft](https://img.shields.io/badge/pocketfft-FFT-336791)
![magic_enum](https://img.shields.io/badge/magic__enum-reflection-008080)
![LRCLIB](https://img.shields.io/badge/LRCLIB-lyrics-FF5733)

**[Português](#português) · [English](#english) · [Español](#español) · [中文](#中文)**

> Fork do [kew](https://github.com/ravachol/kew) (player de música para
> terminal, ravachol, GPL-2) com YouTube Music (yt-dlp), letras sincronizadas
> e tradução automática — **reescrito em C++20** (M0–M5 ✅). / Fork of kew
> with YouTube Music (yt-dlp), synced lyrics and auto-translation —
> **rewritten in C++20** (M0–M5 ✅). / Fork de kew con YouTube Music
> (yt-dlp), letras sincronizadas y traducción automática — **reescrito en
> C++20** (M0–M5 ✅). / 基于 kew 的 fork，带 YouTube Music（yt-dlp）、同步
> 歌词与自动翻译 —— **已用 C++20 重写**（M0–M5 ✅）。

---

## Português

### O que é

O kiutube é o player de terminal (fork do kew) reescrito em C++20 puro:
corrotinas próprias, metaprogramação, configuração em Lua e testes PTY.
Streaming do YouTube Music via yt-dlp, letras sincronizadas do LRCLIB com
tradução automática, busca fuzzy de biblioteca, visualizadores FFT,
janela gráfica (Qt) e MPRIS. A reescrita está completa
(**M0 ✅ … M5 ✅** — o C legado saiu do repositório). Roadmap em
[docs/CPP20-REWRITE.md](docs/CPP20-REWRITE.md); detalhes da camada ytm em
[README-YTM.md](README-YTM.md).

### Recursos (kiutube)

| Recurso | Como |
|---|---|
| Busca YouTube Music | `yt-dlp` (mesma estratégia do ytm-player) |
| Playback YTM | resolve a URL (`bestaudio`) e o próprio yt-dlp baixa para o cache |
| Letras sincronizadas | LRCLIB (grátis, sem auth) — também para faixas locais sem `.lrc` |
| Tradução PT | Google Translate → MyMemory (fallback), timestamps preservados |
| Busca YTM na TUI | **F8**: prompt → resultados → Enter baixa e toca |
| Letra original/traduzida | **F9** alterna na TUI |
| Config em Lua | `~/.config/kiutube/init.lua` |
| Temas (.theme do kew) | `kiutube --themes` prevê, `--theme NOME` aplica, **t** cicla ao vivo |
| Mini-MAN | `kiutube --help` (idioma do `LANG`) |
| CLI headless | `kiutube --ytm search|play|lyrics ...` |

### Instalar via GitHub

```bash
# 1. Clonar o repositório
git clone https://github.com/Colombiano/kiutube.git
cd kiutube

# 2. Dependências (Debian/Ubuntu; 'git' e rede são necessários —
#    sol2/pocketfft/magic_enum/sdbus-c++ vêm via FetchContent)
sudo apt install build-essential cmake git pkg-config \
                 liblua5.4-dev libtag1-dev libcurl4-openssl-dev \
                 libopus-dev libopusfile-dev libvorbis-dev libogg-dev \
                 libchafa-dev

# 3. Compilar e testar
cmake -B build && cmake --build build -j$(nproc)
ctest --test-dir build          # opcional: smoke tests

# 4. Usar (ou instalar no PATH + man page)
./build/kiutube play ~/musica/faixa.flac
sudo install -m 0755 build/kiutube /usr/local/bin/kiutube
sudo install -m 0644 docs/kiutube.1 /usr/local/share/man/man1/kiutube.1
# (ou tudo de uma vez: sudo cmake --install build)
```

Opcionais detectados no configure: Qt 6 Widgets (janela gráfica dos
visualizadores) e `yt-dlp` no PATH (busca/download do YouTube Music,
tecla F8 / CLI `--ytm`) — sem eles, o kiutube compila e roda igual, com
essas partes desabilitadas.

### Licença e créditos

Código do kew © ravachol e contribuidores (GPL-2). Camada ytm, reescrita
C++20 e este README © Colombiano. Obrigado: yt-dlp, LRCLIB, miniaudio, sol2,
pocketfft, magic_enum.

---

## English

### What it is

kiutube is the terminal music player (a kew fork) rewritten in pure C++20:
own coroutines, metaprogramming, Lua configuration and PTY tests. YouTube
Music streaming via yt-dlp, synced LRCLIB lyrics with automatic
translation, fuzzy library search, FFT visualizers, a GUI window (Qt) and
MPRIS. The rewrite is complete (**M0 ✅ … M5 ✅** — the legacy C left the
repository). Roadmap in
[docs/CPP20-REWRITE.md](docs/CPP20-REWRITE.md); ytm layer details in
[README-YTM.md](README-YTM.md).

### Features (kiutube)

| Feature | How |
|---|---|
| YouTube Music search | `yt-dlp` (same strategy as ytm-player) |
| YTM playback | resolves the URL (`bestaudio`) and yt-dlp itself downloads to cache |
| Synced lyrics | LRCLIB (free, no auth) — also for local tracks without `.lrc` |
| PT translation | Google Translate → MyMemory (fallback), timestamps preserved |
| In-TUI YTM search | **F8**: prompt → results → Enter downloads and plays |
| Original/translated lyrics | **F9** toggles in the TUI |
| Lua config | `~/.config/kiutube/init.lua` |
| Themes (kew .theme) | `kiutube --themes` previews, `--theme NAME` applies, **t** cycles live |
| Mini-MAN | `kiutube --help` (in your `LANG`) |
| Headless CLI | `kiutube --ytm search|play|lyrics ...` |

### Installing from GitHub

```bash
# 1. Clone the repository
git clone https://github.com/Colombiano/kiutube.git
cd kiutube

# 2. Dependencies (Debian/Ubuntu; 'git' and network access are needed —
#    sol2/pocketfft/magic_enum/sdbus-c++ come via FetchContent)
sudo apt install build-essential cmake git pkg-config \
                 liblua5.4-dev libtag1-dev libcurl4-openssl-dev \
                 libopus-dev libopusfile-dev libvorbis-dev libogg-dev \
                 libchafa-dev

# 3. Build and test
cmake -B build && cmake --build build -j$(nproc)
ctest --test-dir build          # optional: smoke tests

# 4. Run (or install to PATH + man page)
./build/kiutube play ~/music/track.flac
sudo install -m 0755 build/kiutube /usr/local/bin/kiutube
sudo install -m 0644 docs/kiutube.1 /usr/local/share/man/man1/kiutube.1
# (or everything at once: sudo cmake --install build)
```

Optional, detected at configure time: Qt 6 Widgets (the visualizer GUI
window) and `yt-dlp` on PATH (YouTube Music search/download, F8 key / `--ytm`
CLI) — without them kiutube builds and runs fine, with those parts
disabled.

### License and credits

kew code © ravachol and contributors (GPL-2). ytm layer, C++20 rewrite and
this README © Colombiano. Thanks: yt-dlp, LRCLIB, miniaudio, sol2, pocketfft,
magic_enum.

---

## Español

### Qué es

kiutube es el reproductor de terminal (fork de kew) reescrito en C++20
puro: corrutinas propias, metaprogramación, configuración en Lua y pruebas
PTY. Streaming de YouTube Music vía yt-dlp, letras sincronizadas de LRCLIB
con traducción automática, búsqueda fuzzy de biblioteca, visualizadores
FFT, ventana gráfica (Qt) y MPRIS. La reescritura está completa
(**M0 ✅ … M5 ✅** — el C legado salió del repositorio). Roadmap en
[docs/CPP20-REWRITE.md](docs/CPP20-REWRITE.md); detalles de la capa ytm en
[README-YTM.md](README-YTM.md).

### Características (kiutube)

| Característica | Cómo |
|---|---|
| Búsqueda YouTube Music | `yt-dlp` (misma estrategia que ytm-player) |
| Reproducción YTM | resuelve la URL (`bestaudio`) y el propio yt-dlp descarga a la caché |
| Letras sincronizadas | LRCLIB (gratis, sin auth) — también para pistas locales sin `.lrc` |
| Traducción al PT | Google Translate → MyMemory (fallback), timestamps preservados |
| Búsqueda YTM en la TUI | **F8**: prompt → resultados → Enter descarga y reproduce |
| Letra original/traducida | **F9** alterna en la TUI |
| Config en Lua | `~/.config/kiutube/init.lua` |
| Temas (.theme de kew) | `kiutube --themes` con vista previa, `--theme NOMBRE`, **t** cicla |
| Mini-MAN | `kiutube --help` (idioma del `LANG`) |
| CLI headless | `kiutube --ytm search|play|lyrics ...` |

### Instalar desde GitHub

```bash
# 1. Clonar el repositorio
git clone https://github.com/Colombiano/kiutube.git
cd kiutube

# 2. Dependencias (Debian/Ubuntu; hacen falta 'git' y red —
#    sol2/pocketfft/magic_enum/sdbus-c++ llegan vía FetchContent)
sudo apt install build-essential cmake git pkg-config \
                 liblua5.4-dev libtag1-dev libcurl4-openssl-dev \
                 libopus-dev libopusfile-dev libvorbis-dev libogg-dev \
                 libchafa-dev

# 3. Compilar y probar
cmake -B build && cmake --build build -j$(nproc)
ctest --test-dir build          # opcional: smoke tests

# 4. Usar (o instalar en el PATH + página man)
./build/kiutube play ~/musica/pista.flac
sudo install -m 0755 build/kiutube /usr/local/bin/kiutube
sudo install -m 0644 docs/kiutube.1 /usr/local/share/man/man1/kiutube.1
# (o todo junto: sudo cmake --install build)
```

Opcionales, detectados en el configure: Qt 6 Widgets (ventana gráfica de
los visualizadores) y `yt-dlp` en el PATH (búsqueda/descarga de YouTube
Music, tecla F8 / CLI `--ytm`) — sin ellos, kiutube compila y funciona
igual, con esas partes deshabilitadas.

### Licencia y créditos

Código de kew © ravachol y contribuidores (GPL-2). Capa ytm, reescritura
C++20 y este README © Colombiano. Gracias: yt-dlp, LRCLIB, miniaudio, sol2,
pocketfft, magic_enum.

---

## 中文

### 简介

kiutube 是用纯 C++20 重写的终端音乐播放器（kew 的 fork）：自研协程、
元编程、Lua 配置与 PTY 测试。通过 yt-dlp 实现 YouTube Music 流媒体、
LRCLIB 同步歌词与自动翻译、曲库模糊搜索、FFT 可视化器、图形窗口
（Qt）以及 MPRIS。重写已完成（**M0 ✅ … M5 ✅** —— 旧 C 代码已离开
仓库）。路线图见 [docs/CPP20-REWRITE.md](docs/CPP20-REWRITE.md)；ytm 层
详情见 [README-YTM.md](README-YTM.md)。

### 功能（kiutube）

| 功能 | 方式 |
|---|---|
| YouTube Music 搜索 | `yt-dlp`（与 ytm-player 相同策略） |
| YTM 播放 | 解析 URL（`bestaudio`），由 yt-dlp 下载到缓存 |
| 同步歌词 | LRCLIB（免费、无需认证）—— 也支持无 `.lrc` 的本地曲目 |
| 葡语翻译 | Google Translate → MyMemory（回退），时间戳保留 |
| TUI 内 YTM 搜索 | **F8**：输入 → 结果 → Enter 下载并播放 |
| 原文/译文切换 | **F9** 在 TUI 内切换 |
| Lua 配置 | `~/.config/kiutube/init.lua` |
| 主题（kew .theme） | `kiutube --themes` 预览，`--theme 名称` 应用，**t** 实时循环 |
| Mini man | `kiutube --help`（跟随 `LANG`） |
| 无头 CLI | `kiutube --ytm search|play|lyrics ...` |

### 从 GitHub 安装

```bash
# 1. 克隆仓库
git clone https://github.com/Colombiano/kiutube.git
cd kiutube

# 2. 依赖（Debian/Ubuntu；需要 git 与网络 —— sol2/pocketfft/
#    magic_enum/sdbus-c++ 通过 FetchContent 获取）
sudo apt install build-essential cmake git pkg-config \
                 liblua5.4-dev libtag1-dev libcurl4-openssl-dev \
                 libopus-dev libopusfile-dev libvorbis-dev libogg-dev \
                 libchafa-dev

# 3. 编译并测试
cmake -B build && cmake --build build -j$(nproc)
ctest --test-dir build          # 可选：冒烟测试

# 4. 运行（或安装到 PATH + man page）
./build/kiutube play ~/music/track.flac
sudo install -m 0755 build/kiutube /usr/local/bin/kiutube
sudo install -m 0644 docs/kiutube.1 /usr/local/share/man/man1/kiutube.1
#（或一次完成：sudo cmake --install build）
```

可选项（configure 时自动检测）：Qt 6 Widgets（可视化器图形窗口）与
PATH 中的 `yt-dlp`（YouTube Music 搜索/下载，F8 键 / `--ytm` CLI）——
没有它们 kiutube 照常编译运行，仅这些功能被禁用。

### 许可与致谢

kew 代码 © ravachol 及贡献者（GPL-2）。ytm 层、C++20 重写与本 README
© Colombiano。致谢：yt-dlp、LRCLIB、miniaudio、sol2、pocketfft、
magic_enum。
