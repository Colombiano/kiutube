# kiutube

**[Português](#português) · [English](#english) · [Español](#español) · [中文](#中文)**

> Fork do [kew](https://github.com/ravachol/kew) (player de música para
> terminal, ravachol, GPL-2) com camada própria de YouTube Music, letras
> sincronizadas e tradução automática — e uma **reescrita C++20 em
> andamento**. / Fork of kew with a YouTube Music layer, synced lyrics and
> auto-translation — plus an **ongoing C++20 rewrite**. / Fork de kew con
> capa de YouTube Music, letras sincronizadas y traducción automática — más
> una **reescritura C++20 en marcha**. / 基于 kew 的 fork，带 YouTube Music
> 层、同步歌词与自动翻译 —— 外加**进行中的 C++20 重写**。

---

## Português

### O que é

O kiutube tem duas linhas de código convivendo:

- **`kiutube`** (binário C, build via `Makefile`) — o player completo hoje:
  fork do kew com a camada ytm (`src/ytm/`, C++20) que adiciona streaming do
  YouTube Music via yt-dlp, letras sincronizadas do LRCLIB (também para
  faixas locais) e tradução automática das letras para português. Detalhes em
  [README-YTM.md](README-YTM.md).
- **`kiutube`** (binário C++20, build via `CMake`) — a reescrita greenfield
  do player em C++20 puro: corrotinas próprias, metaprogramação, Lua e
  testes PTY. Progresso: **M0 ✅ M1 ✅ M2 ✅** (scheduler, configuração,
  engine de áudio com gapless). Roadmap em
  [docs/CPP20-REWRITE.md](docs/CPP20-REWRITE.md).

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
| CLI headless | `kiutube --ytm search|play|lyrics ...` |

### Compilar

```bash
# kiutube (C + camada ytm)
sudo apt install libopusfile-dev libvorbis-dev libogg-dev libtag1-dev \
                 libchafa-dev libgdk-pixbuf-2.0-dev libcurl4-openssl-dev \
                 libfaad-dev liblua5.4-dev
make -j$(nproc)

# kiutube (reescrita C++20)
cmake -B build && cmake --build build -j16
ctest --test-dir build
```

### Licença e créditos

Código do kew © ravachol e contribuidores (GPL-2). Camada ytm, reescrita
C++20 e este README © Colombiano. Obrigado: yt-dlp, LRCLIB, miniaudio, sol2,
pocketfft, magic_enum.

---

## English

### What it is

kiutube has two code lines coexisting:

- **`kiutube`** (C binary, built via `Makefile`) — the complete player today:
  a kew fork with the ytm layer (`src/ytm/`, C++20) adding YouTube Music
  streaming via yt-dlp, synced lyrics from LRCLIB (also for local tracks) and
  automatic lyric translation to Portuguese. See
  [README-YTM.md](README-YTM.md).
- **`kiutube`** (C++20 binary, built via `CMake`) — the greenfield rewrite
  in pure C++20: own coroutines, metaprogramming, Lua and PTY tests.
  Progress: **M0 ✅ M1 ✅ M2 ✅** (scheduler, configuration, gapless audio
  engine). Roadmap in [docs/CPP20-REWRITE.md](docs/CPP20-REWRITE.md).

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
| Headless CLI | `kiutube --ytm search|play|lyrics ...` |

### Building

```bash
# kiutube (C + ytm layer)
sudo apt install libopusfile-dev libvorbis-dev libogg-dev libtag1-dev \
                 libchafa-dev libgdk-pixbuf-2.0-dev libcurl4-openssl-dev \
                 libfaad-dev liblua5.4-dev
make -j$(nproc)

# kiutube (C++20 rewrite)
cmake -B build && cmake --build build -j16
ctest --test-dir build
```

### License and credits

kew code © ravachol and contributors (GPL-2). ytm layer, C++20 rewrite and
this README © Colombiano. Thanks: yt-dlp, LRCLIB, miniaudio, sol2, pocketfft,
magic_enum.

---

## Español

### Qué es

kiutube tiene dos líneas de código conviviendo:

- **`kiutube`** (binario C, build vía `Makefile`) — el player completo hoy:
  fork de kew con la capa ytm (`src/ytm/`, C++20) que añade streaming de
  YouTube Music vía yt-dlp, letras sincronizadas de LRCLIB (también para
  pistas locales) y traducción automática de letras al portugués. Detalles en
  [README-YTM.md](README-YTM.md).
- **`kiutube`** (binario C++20, build vía `CMake`) — la reescritura
  greenfield en C++20 puro: corrutinas propias, metaprogramación, Lua y
  pruebas PTY. Progreso: **M0 ✅ M1 ✅ M2 ✅** (scheduler, configuración,
  motor de audio gapless). Roadmap en
  [docs/CPP20-REWRITE.md](docs/CPP20-REWRITE.md).

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
| CLI headless | `kiutube --ytm search|play|lyrics ...` |

### Compilar

```bash
# kiutube (C + capa ytm)
sudo apt install libopusfile-dev libvorbis-dev libogg-dev libtag1-dev \
                 libchafa-dev libgdk-pixbuf-2.0-dev libcurl4-openssl-dev \
                 libfaad-dev liblua5.4-dev
make -j$(nproc)

# kiutube (reescritura C++20)
cmake -B build && cmake --build build -j16
ctest --test-dir build
```

### Licencia y créditos

Código de kew © ravachol y contribuidores (GPL-2). Capa ytm, reescritura
C++20 y este README © Colombiano. Gracias: yt-dlp, LRCLIB, miniaudio, sol2,
pocketfft, magic_enum.

---

## 中文

### 简介

kiutube 目前有两条并存的代码线：

- **`kiutube`**（C 二进制，`Makefile` 构建）—— 今天可用的完整播放器：
  kew 的 fork，带 ytm 层（`src/ytm/`，C++20），通过 yt-dlp 实现
  YouTube Music 流媒体、LRCLIB 同步歌词（也支持本地曲目）以及歌词的
  葡萄牙语自动翻译。详见 [README-YTM.md](README-YTM.md)。
- **`kiutube`**（C++20 二进制，`CMake` 构建）—— 纯 C++20 的 greenfield
  重写：自研协程、元编程、Lua 与 PTY 测试。进度：**M0 ✅ M1 ✅ M2 ✅**
  （调度器、配置、gapless 音频引擎）。路线图见
  [docs/CPP20-REWRITE.md](docs/CPP20-REWRITE.md)。

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
| 无头 CLI | `kiutube --ytm search|play|lyrics ...` |

### 构建

```bash
# kiutube（C + ytm 层）
sudo apt install libopusfile-dev libvorbis-dev libogg-dev libtag1-dev \
                 libchafa-dev libgdk-pixbuf-2.0-dev libcurl4-openssl-dev \
                 libfaad-dev liblua5.4-dev
make -j$(nproc)

# kiutube（C++20 重写）
cmake -B build && cmake --build build -j16
ctest --test-dir build
```

### 许可与致谢

kew 代码 © ravachol 及贡献者（GPL-2）。ytm 层、C++20 重写与本 README
© Colombiano。致谢：yt-dlp、LRCLIB、miniaudio、sol2、pocketfft、
magic_enum。
