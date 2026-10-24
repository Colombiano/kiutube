# Kiutube — YouTube Music, letras e tradução (camada ytm)

**[Português](#português) · [English](#english) · [Español](#español) · [中文](#中文)**

> **Nota histórica / Historical note / Nota histórica / 历史说明:** este doc
> descreve a camada ytm **atual** da reescrita C++20 (`player/net/`), pós-M5.
> A ponte C antiga (`src/ytm/`) foi removida no marco M5.

---

## Português

### O que é

A camada ytm do kiutube agrega, sobre o player C++20: busca e download do
YouTube Music via `yt-dlp`, letras sincronizadas do LRCLIB (também para
faixas locais) e tradução automática das letras. Implementação síncrona e
enxuta em `player/net/ytm.{hpp,cpp}` — toda chamada de rede roda dentro de
`krt::offload`, nunca bloqueando a UI.

| Recurso | Como funciona |
|---|---|
| Busca YouTube Music | `yt-dlp` com templates `--print` (mesma estratégia do ytm-player; sem parser de JSON) |
| Playback YTM | o próprio yt-dlp resolve e baixa (`bestaudio`) — URLs do googlevideo são assinadas por IP |
| Capa | `--write-thumbnail` grava o sidecar `base.<ext>`, usado pela capa da TUI (chafa) e da janela gráfica |
| Letras sincronizadas | LRCLIB (grátis, sem auth), com retry e normalização do título |
| Tradução | Google Translate → MyMemory (fallback), timestamps preservados |
| Busca YTM na TUI | **F8**: prompt → resultados com filtro ao vivo → Enter baixa e toca |
| Letra original/traduzida | **F9** alterna na view de letras ('g') |

### Uso

```bash
# CLI (headless)
kiutube --ytm search "songbird bee gees"   # lista e sai
kiutube --ytm play  "songbird bee gees"    # baixa o top 1 e abre o TUI tocando

# Na TUI
F8    # prompt de busca YTM; Enter no resultado baixa e toca
g     # letra da faixa atual (LRCLIB + tradução quando habilitada)
F9    # alterna original <-> traduzida (quando há tradução)
```

Downloads vão para `library_path` (se configurado) ou
`~/.cache/kiutube/streams/`, e a biblioteca é re-escaneada em seguida.

### Configuração

Em camadas: defaults → `kewrc` (compat: `~/.config/kew/kewrc`) →
`~/.config/kiutube/init.lua`:

```lua
settings.path = "/home/voce/musica"   -- biblioteca local
settings.translation = true           -- traduzir letras p/ PT
bind("F8", "ytm_search")              -- teclas são eventos redefiníveis
bind("F9", "toggle_translation")
```

### Arquitetura (`player/net/`)

- **`ytm.cpp`** — search/download/info via subprocesso yt-dlp. Atenção ao
  escape: `\t` no literal C++ é um TAB real no comando; `\\t` quebra o parse.
- **`lrclib.cpp`** — GET síncrono com retry; normalização do título.
- **`translate.cpp`** — tradução chunkada (limite dos providers), cadeia
  Google → MyMemory.
- **`http.cpp`** — `CurlHandle` RAII move-only sobre libcurl.
- **Sincronia** — a API é bloqueante por design; o scheduler a isola em
  `krt::offload` (thread pool), então a corrotina da UI só continua quando o
  resultado chega.

### Notas e limitações

- **yt-dlp** procurado nesta ordem: `$YTDLP` → venv do ytm-player
  (`~/.local/share/pipx/venvs/ytm-player/bin/yt-dlp`) → `yt-dlp` no PATH.
  O do apt pode ficar velho demais e o YouTube passa a recusar.
- **IP-signed URLs**: resolver a URL e baixar em processos diferentes dá
  403 em IPv6 rotacionado — por isso o download é um único yt-dlp.
- **Google Translate** pode rate-limitar por IP; o MyMemory assume como
  fallback.
- Diferente da era C: o picker F8 **filtra os resultados em tempo real**
  (substring, case-insensitive) enquanto você digita.

---

## English

### What it is

kiutube's ytm layer adds, on top of the C++20 player: YouTube Music search
and download via `yt-dlp`, synced LRCLIB lyrics (also for local tracks) and
automatic lyric translation. A lean synchronous implementation in
`player/net/ytm.{hpp,cpp}` — every network call runs inside
`krt::offload`, never blocking the UI.

| Feature | How it works |
|---|---|
| YouTube Music search | `yt-dlp` with `--print` templates (ytm-player strategy; no JSON parser) |
| YTM playback | yt-dlp itself resolves and downloads (`bestaudio`) — googlevideo URLs are IP-signed |
| Cover art | `--write-thumbnail` writes the `base.<ext>` sidecar, used by the TUI (chafa) and GUI cover |
| Synced lyrics | LRCLIB (free, no auth), with retry and title normalization |
| Translation | Google Translate → MyMemory (fallback), timestamps preserved |
| In-TUI YTM search | **F8**: prompt → live-filtered results → Enter downloads and plays |
| Original/translated lyrics | **F9** toggles in the lyrics view ('g') |

### Usage

```bash
# CLI (headless)
kiutube --ytm search "songbird bee gees"   # list and exit
kiutube --ytm play  "songbird bee gees"    # download top 1, open the TUI playing

# In the TUI
F8    # YTM search prompt; Enter on a result downloads and plays
g     # lyrics for the current track (LRCLIB + translation when enabled)
F9    # toggle original <-> translated (when a translation exists)
```

Downloads go to `library_path` (if set) or `~/.cache/kiutube/streams/`,
then the library is rescanned.

### Configuration

Layered: defaults → `kewrc` (compat: `~/.config/kew/kewrc`) →
`~/.config/kiutube/init.lua`:

```lua
settings.path = "/home/you/music"     -- local library
settings.translation = true           -- translate lyrics to PT
bind("F8", "ytm_search")              -- keys are redefinable events
bind("F9", "toggle_translation")
```

### Architecture (`player/net/`)

- **`ytm.cpp`** — search/download/info through a yt-dlp subprocess. Mind the
  escape: `\t` in the C++ literal is a REAL TAB in the command; `\\t`
  breaks the parse.
- **`lrclib.cpp`** — synchronous GET with retry; title normalization.
- **`translate.cpp`** — chunked translation (provider limits), Google →
  MyMemory chain.
- **`http.cpp`** — move-only RAII `CurlHandle` over libcurl.
- **Sync by design** — the API is blocking on purpose; the scheduler
  isolates it in `krt::offload` (thread pool), so the UI coroutine only
  resumes when the result is there.

### Notes and limitations

- **yt-dlp** is looked up in this order: `$YTDLP` → ytm-player venv
  (`~/.local/share/pipx/venvs/ytm-player/bin/yt-dlp`) → `yt-dlp` on PATH.
  The apt one can age out and YouTube starts refusing.
- **IP-signed URLs**: resolving the URL and downloading in different
  processes gives 403 on rotating IPv6 — hence a single yt-dlp does both.
- **Google Translate** may rate-limit per IP; MyMemory takes over as the
  fallback.
- Unlike the C era: the F8 picker **filters results live** (case-insensitive
  substring) as you type.

---

## Español

### Qué es

La capa ytm de kiutube agrega, sobre el reproductor C++20: búsqueda y
descarga de YouTube Music vía `yt-dlp`, letras sincronizadas de LRCLIB
(también para pistas locales) y traducción automática de letras.
Implementación síncrona y enxuta en `player/net/ytm.{hpp,cpp}` — toda
llamada de red corre dentro de `krt::offload`, sin bloquear la UI.

| Característica | Cómo funciona |
|---|---|
| Búsqueda YouTube Music | `yt-dlp` con plantillas `--print` (estrategia de ytm-player; sin parser de JSON) |
| Reproducción YTM | el propio yt-dlp resuelve y descarga (`bestaudio`) — las URLs de googlevideo van firmadas por IP |
| Carátula | `--write-thumbnail` escribe el sidecar `base.<ext>`, usado por la carátula TUI (chafa) y gráfica |
| Letras sincronizadas | LRCLIB (gratis, sin auth), con retry y normalización del título |
| Traducción | Google Translate → MyMemory (fallback), timestamps preservados |
| Búsqueda YTM en la TUI | **F8**: prompt → resultados con filtro en vivo → Enter descarga y reproduce |
| Letra original/traducida | **F9** alterna en la vista de letras ('g') |

### Uso

```bash
# CLI (headless)
kiutube --ytm search "songbird bee gees"   # lista y sale
kiutube --ytm play  "songbird bee gees"    # descarga el top 1 y abre el TUI

# En la TUI
F8    # prompt de búsqueda YTM; Enter en un resultado descarga y reproduce
g     # letra de la pista actual (LRCLIB + traducción si está habilitada)
F9    # alterna original <-> traducida (cuando hay traducción)
```

Las descargas van a `library_path` (si está configurado) o a
`~/.cache/kiutube/streams/`, y la biblioteca se re-escanea después.

### Configuración

En capas: defaults → `kewrc` (compat: `~/.config/kew/kewrc`) →
`~/.config/kiutube/init.lua`:

```lua
settings.path = "/home/tu/musica"     -- biblioteca local
settings.translation = true           -- traducir letras al PT
bind("F8", "ytm_search")              -- las teclas son eventos redefinibles
bind("F9", "toggle_translation")
```

### Arquitectura (`player/net/`)

- **`ytm.cpp`** — búsqueda/descarga/info vía subproceso yt-dlp. Ojo al
  escape: `\t` en el literal C++ es un TAB real en el comando; `\\t`
  rompe el parse.
- **`lrclib.cpp`** — GET síncrono con retry; normalización del título.
- **`translate.cpp`** — traducción en chunks (límite de los providers),
  cadena Google → MyMemory.
- **`http.cpp`** — `CurlHandle` RAII move-only sobre libcurl.
- **Sincronía por diseño** — la API bloquea a propósito; el scheduler la
  aísla en `krt::offload` (thread pool), así que la corrutina de la UI
  solo continúa cuando el resultado llega.

### Notas y limitaciones

- **yt-dlp** se busca en este orden: `$YTDLP` → venv de ytm-player →
  `yt-dlp` en el PATH. El del apt puede quedarse viejo y YouTube empieza
  a rechazarlo.
- **URLs firmadas por IP**: resolver la URL y descargar en procesos
  distintos da 403 con IPv6 rotativo — por eso un único yt-dlp hace ambas.
- **Google Translate** puede rate-limitar por IP; MyMemory entra como
  fallback.
- A diferencia de la era C: el picker F8 **filtra los resultados en vivo**
  (substring, case-insensitive) mientras tecleas.

---

## 中文

### 简介

kiutube 的 ytm 层在 C++20 播放器之上提供：经 `yt-dlp` 的 YouTube Music
搜索与下载、LRCLIB 同步歌词（也支持本地曲目）以及歌词自动翻译。实现在
`player/net/ytm.{hpp,cpp}` 中，保持精简同步——所有网络调用都经
`krt::offload` 执行，绝不阻塞 UI。

| 功能 | 方式 |
|---|---|
| YouTube Music 搜索 | `yt-dlp` 使用 `--print` 模板（ytm-player 策略；无 JSON 解析） |
| YTM 播放 | 由 yt-dlp 自身解析并下载（`bestaudio`）—— googlevideo URL 按 IP 签名 |
| 封面 | `--write-thumbnail` 写出 `base.<ext>` sidecar，供 TUI（chafa）与图形封面使用 |
| 同步歌词 | LRCLIB（免费、无需认证），带重试与标题归一化 |
| 翻译 | Google Translate → MyMemory（回退），时间戳保留 |
| TUI 内 YTM 搜索 | **F8**：输入框 → 实时过滤结果 → Enter 下载并播放 |
| 原文/译文切换 | **F9** 在歌词视图（'g'）中切换 |

### 用法

```bash
# CLI（无头）
kiutube --ytm search "songbird bee gees"   # 列出后退出
kiutube --ytm play  "songbird bee gees"    # 下载第 1 个结果并进入 TUI 播放

# TUI 中
F8    # YTM 搜索输入框；在结果上 Enter 即下载并播放
g     # 当前曲目歌词（LRCLIB；启用时含翻译）
F9    # 切换原文 <-> 译文（存在译文时）
```

下载目标为 `library_path`（若已配置）或 `~/.cache/kiutube/streams/`，
随后自动重扫曲库。

### 配置

分层：默认值 → `kewrc`（兼容：`~/.config/kew/kewrc`）→
`~/.config/kiutube/init.lua`：

```lua
settings.path = "/home/you/music"     -- 本地曲库
settings.translation = true           -- 将歌词翻译为葡语
bind("F8", "ytm_search")              -- 按键是可重定义的事件
bind("F9", "toggle_translation")
```

### 架构（`player/net/`）

- **`ytm.cpp`** — 经 yt-dlp 子进程搜索/下载/信息。注意转义：C++ 字面
  量中的 `\t` 是命令中的真实 TAB；`\\t` 会使解析断裂。
- **`lrclib.cpp`** — 同步 GET，带重试；标题归一化。
- **`translate.cpp`** — 分块翻译（provider 限制），Google → MyMemory
  链式回退。
- **`http.cpp`** — 基于 libcurl 的 move-only RAII `CurlHandle`。
- **刻意同步** — API 设计上就是阻塞的；调度器将其隔离在
  `krt::offload`（线程池）中，UI 协程只在结果就绪后才恢复。

### 说明与限制

- **yt-dlp** 按以下顺序查找：`$YTDLP` → ytm-player 的 venv → PATH 中
  的 `yt-dlp`。apt 版本可能过旧而被 YouTube 拒绝。
- **按 IP 签名的 URL**：在不同进程中解析与下载会在轮换 IPv6 下
  返回 403 —— 因此由同一个 yt-dlp 完成两步。
- **Google Translate** 可能按 IP 限流；MyMemory 作为回退接手。
- 与 C 时代不同：F8 选择器在你输入时**实时过滤结果**（大小写不敏感
  子串）。
