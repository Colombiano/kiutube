# kiutube2 — Reescrita C++20 / C++20 Rewrite / Reescritura C++20 / C++20 重写

**[Português](#português) · [English](#english) · [Español](#español) · [中文](#中文)**

---

## Português

### Visão

Reescrita do kiutube (fork do [kew](https://github.com/ravachol/kew), GPL-2) em
C++20 puro na camada do produto, com corrotinas, metaprogramação e scripting
Lua. O código C legado (~35,5k linhas) fica intacto como spec de referência
até a paridade funcional, e é removido no marco M5. Decisão fundamental:
**reescrita greenfield** — o god-struct `Model` (908 linhas) acopla todas as
views do kew, tornando a migração incremental mais cara que a reescrita.

Filosofia de fronteira: **own your code, wrap what you don't need to own**.
Bibliotecas C maduras (miniaudio, decoders, demuxers, libcurl, chafa, PUC Lua)
não são reescritas — são encapsuladas atrás de wrappers RAII com
`extern "C"`.

### Estado atual: M0 ✅ M1 ✅ M2 ✅ M3 ✅

**M3a — caches YTM**: `AnyDecoder` (apagamento de tipo sobre o concept, vtable
estática) com cadeia de adapters na engine (miniaudio → webm); demux
nestegg+opus/vorbis via shim RAII sobre a implementação C do kew
(`src/sound/webm.h`, TU único em C). Gate: .webm real tocando com sink input
pactl.

**M3b — biblioteca fuzzy**: scan recursivo com taglib, busca fuzzy com a
métrica OSA do Typesense (`src/art.cpp`), picker na TUI ('l': filtra
digitando, Enter toca, ESC fecha).

**M3c — letras LRCLIB**: `net/` (http síncrono + LRCLIB com retry e
normalização de título), view de letras ('g'), fetch em offload sem
bloquear a UI.

**M0 — fundação**: scheduler de corrotinas (epoll + timers + eventfd),
terminal layer própria em C++, harness PTY, build CMake `-Werror`.

**M1 — configuração real**: settings tipadas com parse genérico por tabela
constexpr de descritores (a mesma tabela expõe os campos ao Lua), bindings
com eventos magic_enum, kewrc legado em camadas (defaults → kewrc →
`init.lua` sol2), logging.

**M2 — engine de áudio**: `kiutube2 play <arq>` toca wav/mp3/flac via
miniaudio: device RAII + ring lock-free SPSC (~2,7s) + feeder corrotina no
scheduler. Gapless por construção (o ring nunca é drenado entre faixas;
`playing_info` separa "o que está tocando" do "decoder aberto", que é
liberado no EOF). Transporte completo na TUI: space toca/pausa, `n` próxima,
setas seek ±5s, `+`/`-` volume. Gate M2: sink input ativo no PipeWire
(pactl) com posição avançando, verificado por smoke PTY.

### Decisões arquiteturais

1. **Greenfield com gates de paridade** — árvore `player/`; C legado como spec.
2. **CMake** com `FetchContent` (sol2 v3.3.0, pocketfft `81d171a6`,
   magic_enum v0.9.7), convivendo com o Makefile antigo até o M5.
3. **Terminal layer própria** (~800 linhas C++): termios RAII + escapes do
   termbox2 (MIT) + `co_await` de eventos.
4. **Scheduler de corrotinas próprio** (`krt`) substituindo o loop GLib:
   epoll + heap de timers + eventfd; mensagens viram chamadas diretas.
5. **GLib quarentenado no MPRIS** (thread própria) até o M5, quando
   **sdbus-c++** o remove do link.
6. **pocketfft** (visualizador) e **sol2** (binding Lua) substituem fftw3f e o
   binding manual; **magic_enum** para enum↔string de eventos/settings.
7. **Convenção 4 idiomas** (PT/EN/ES/ZH) em todo texto do projeto.

### Arquitetura-alvo

```
player/
  main.cpp            # bootstrap: scheduler, terminal, app
  krt/                # runtime: scheduler, task, thread_pool, log
  audio/              # engine (miniaudio), Decoder concept (MaDecoder), ring SPSC
  library/            # taglib scan, fuzzy search (Typesense OSA) — real M3
  tui/                # term, cell_buffer, components (CRTP), views, visualizer
  app/                # settings, bindings, player state machine, queue
  net/                # http (curl) + lrclib — real M3; ytm/translate migram no M4
  script/             # sol2: config Lua, keybindings, temas, hooks
  sys/                # MPRIS (sdbus-c++), notificações
```

| Wrapper | Sobre C | Nota |
|---|---|---|
| `audio::Engine` | miniaudio | device + ring SPSC + feeder corrotina |
| `Decoder` concept | opusfile/vorbis/faad/dr_mp3 | MaDecoder cobre wav/mp3/flac (M2); demuxers no M3 |
| `net::Http` | libcurl | `CurlHandle` RAII (move da ytm) |
| `tui::Term` | termios + tabelas | único lugar com escapes |
| `script::State` | Lua 5.4 via sol2 | tabela `settings` genérica |
| `cover::Canvas` | chafa | |
| `sys::Mpris` | sd-bus (M5) | glib sai do link |

### Scheduler

Run-loop única com três fontes de eventos: fds epoll (one-shot), heap de
timers e fila thread-safe via eventfd. Invariante central: **nenhuma corrotina
retoma fora da thread do loop** — `krt::complete()` roteia toda continuação
pelo hook do scheduler. Awaitables: `sleep(ms)`, `fd_readable(fd)`,
`offload(fn)`. Corrotinas de longa duração (feeder, relógio de UI) checam
flag de término — senão seus timers impediriam o fim do run-loop.

### Metaprogramação

Concepts (`Decoder`, `Component`), tupla constexpr de descritores de settings
alimentando parse kewrc E exposição Lua, bindings default `constexpr`,
eventos via magic_enum, CRTP em componentes de UI, `std::format`.

### Lua (onde couber)

Config (`init.lua`) com `bind(tecla, evento)`, `log(msg)` e tabela
`settings` type-safe; temas e hooks no M4. µs/chamada — ok por frame, proibido
por amostra de áudio.

### Convenção 4 idiomas

Todo comentário, documento, string de UI e commit segue PT → EN → ES → ZH.

### Marcos

| Marco | Entrega |
|---|---|
| M0 ✅ | Doc + CMake + krt + TUI vazia + harness PTY |
| M1 ✅ | Settings tipadas (descritores constexpr), sol2 Lua config, bindings magic_enum, logging |
| M2 ✅ | audio::Engine + Decoder concept + play/pause/seek/gapless + gate pactl |
| M3 ✅ | WebmDecoder (caches YTM), library taglib + fuzzy Typesense + picker, letras LRCLIB |
| M4 | Views completas (playlist/faixa), visualizador pocketfft, capa chafa, F8 YTM, CLI --ytm, migra ytm p/ net |
| M5 | MPRIS sdbus-c++, apaga src/ C, renomeia binário |

### Como buildar e testar

```bash
cmake -B build && cmake --build build -j16
ctest --test-dir build
# gate M2 (áudio): toca um WAV e afirma sink input no PipeWire
python3 tools/tui_smoke.py "./build/kiutube2 play /tmp/kiutube2_gate.wav" \
  --expect "kiutube2" \
  --sh "sleep 1.2; pactl list sink-inputs | grep -q 'application.process.binary = \"kiutube2\"'" \
  --expect "00:01" --send " " --expect "||" --send "q" --expect-exit 3
```

### Fora de escopo

Reescrita de miniaudio/nestegg/libmp4/stb_image/libcurl/chafa/decoders;
interpretador Lua em C++; suporte Windows/SMTC até novo aviso.

---

## English

### Vision

A from-scratch rewrite of kiutube (a [kew](https://github.com/ravachol/kew)
fork, GPL-2) in pure C++20 at the product layer, with coroutines,
metaprogramming and Lua scripting. The legacy C code (~35.5k lines) stays
untouched as the reference spec until functional parity, and is removed at
milestone M5. Key decision: **greenfield rewrite** — the kew `Model`
god-struct (908 lines) couples every view, making incremental migration more
expensive than rewriting.

Boundary philosophy: **own your code, wrap what you don't need to own**.
Mature C libraries (miniaudio, decoders, demuxers, libcurl, chafa, PUC Lua)
are not rewritten — they are wrapped behind RAII `extern "C"` interfaces.

### Current state: M0 ✅ M1 ✅ M2 ✅ M3 ✅

**M3a — YTM caches**: `AnyDecoder` (type erasure over the concept, static
vtable) with an adapter chain in the engine (miniaudio → webm); nestegg+opus/
vorbis demux through a RAII shim over kew's C implementation
(`src/sound/webm.h`, single C TU). Gate: real .webm playing with a pactl
sink input.

**M3b — fuzzy library**: taglib recursive scan, fuzzy search with Typesense's
OSA metric (`src/art.cpp`), in-TUI picker ('l': type to filter, Enter plays,
ESC closes).

**M3c — LRCLIB lyrics**: `net/` (sync http + LRCLIB with retry and title
normalization), lyrics view ('g'), offloaded fetch that never blocks the UI.

**M0 — foundation**: coroutine scheduler (epoll + timers + eventfd), own C++
terminal layer, PTY smoke harness, CMake `-Werror` build.

**M1 — real configuration**: typed settings with generic parsing fed by a
constexpr descriptor table (the same table exposes fields to Lua), magic_enum
bindings, layered legacy kewrc (defaults → kewrc → sol2 `init.lua`), logging.

**M2 — audio engine**: `kiutube2 play <file>` plays wav/mp3/flac via
miniaudio: RAII device + lock-free SPSC ring (~2.7s) + scheduler coroutine
feeder. Gapless by construction (the ring is never drained between tracks;
`playing_info` separates "what is playing" from the "open decoder", which is
freed at EOF). Full TUI transport: space toggles pause, `n` next, arrows seek
±5s, `+`/`-` volume. M2 gate: active sink input on PipeWire (pactl) with the
position advancing, verified by PTY smoke.

### Architectural decisions

1. **Greenfield with parity gates** — `player/` tree; legacy C as spec.
2. **CMake** with `FetchContent` (sol2 v3.3.0, pocketfft `81d171a6`,
   magic_enum v0.9.7), coexisting with the legacy Makefile until M5.
3. **Own terminal layer** (~800 lines C++): termios RAII + termbox2 (MIT)
   escapes + `co_await` events.
4. **Own coroutine scheduler** (`krt`) replacing the GLib loop: epoll + timer
   heap + eventfd; messages become direct calls.
5. **GLib quarantined in MPRIS** (own thread) until M5, when **sdbus-c++**
   removes it from the link.
6. **pocketfft** (visualizer) and **sol2** (Lua binding) replace fftw3f and the
   manual binding; **magic_enum** for enum↔string.
7. **Four-language convention** (PT/EN/ES/ZH) for every project text.

### Target architecture

```
player/
  main.cpp            # bootstrap: scheduler, terminal, app
  krt/                # runtime: scheduler, task, thread_pool, log
  audio/              # engine (miniaudio), Decoder concept (MaDecoder), SPSC ring
  library/            # taglib scan, fuzzy search (Typesense OSA) — real M3
  tui/                # term, cell_buffer, components (CRTP), views, visualizer
  app/                # settings, bindings, player state machine, queue
  net/                # http (curl) + lrclib — real M3; ytm/translate migrate M4
  script/             # sol2: Lua config, keybindings, themes, hooks
  sys/                # MPRIS (sdbus-c++), notifications
```

| Wrapper | Over C | Note |
|---|---|---|
| `audio::Engine` | miniaudio | device + SPSC ring + coroutine feeder |
| `Decoder` concept | opusfile/vorbis/faad/dr_mp3 | MaDecoder covers wav/mp3/flac (M2); demuxers in M3 |
| `net::Http` | libcurl | `CurlHandle` RAII (moved from ytm) |
| `tui::Term` | termios + tables | the only place with escapes |
| `script::State` | Lua 5.4 via sol2 | generic `settings` table |
| `cover::Canvas` | chafa | |
| `sys::Mpris` | sd-bus (M5) | glib leaves the link |

### Scheduler

Single run-loop with three event sources: epoll fds (one-shot), timer heap and
a thread-safe queue via eventfd. Central invariant: **no coroutine ever
resumes outside the loop thread** — `krt::complete()` routes every
continuation through the scheduler hook. Awaitables: `sleep(ms)`,
`fd_readable(fd)`, `offload(fn)`. Long-lived coroutines (feeder, UI clock)
check a termination flag — otherwise their timers would keep the run-loop
alive forever.

### Metaprogramming

Concepts (`Decoder`, `Component`), constexpr settings descriptor tuple feeding
both kewrc parsing AND Lua exposure, `constexpr` default bindings,
magic_enum events, CRTP UI components, `std::format`.

### Lua (where it fits)

Config (`init.lua`) with `bind(key, event)`, `log(msg)` and a type-safe
`settings` table; themes and hooks in M4. µs per call — fine per frame,
forbidden per audio sample.

### Four-language convention

Every comment, document, UI string and commit follows PT → EN → ES → ZH.

### Milestones

| Milestone | Deliverable |
|---|---|
| M0 ✅ | Doc + CMake + krt + empty TUI + PTY harness |
| M1 ✅ | Typed settings (constexpr descriptors), sol2 Lua config, magic_enum bindings, logging |
| M2 ✅ | audio::Engine + Decoder concept + play/pause/seek/gapless + pactl gate |
| M3 ✅ | WebmDecoder (YTM caches), taglib library + Typesense fuzzy + picker, LRCLIB lyrics |
| M4 | Full views (playlist/track), pocketfft visualizer, chafa cover, F8 YTM, --ytm CLI, ytm migration to net |
| M5 | sdbus-c++ MPRIS, delete legacy src/ C, rename binary |

### Build and test

```bash
cmake -B build && cmake --build build -j16
ctest --test-dir build
# M2 gate (audio): plays a WAV and asserts a PipeWire sink input
python3 tools/tui_smoke.py "./build/kiutube2 play /tmp/kiutube2_gate.wav" \
  --expect "kiutube2" \
  --sh "sleep 1.2; pactl list sink-inputs | grep -q 'application.process.binary = \"kiutube2\"'" \
  --expect "00:01" --send " " --expect "||" --send "q" --expect-exit 3
```

### Out of scope

Rewriting miniaudio/nestegg/libmp4/stb_image/libcurl/chafa/decoders; a C++
Lua interpreter; Windows/SMTC support until further notice.

---

## Español

### Visión

Reescritura desde cero de kiutube (fork de [kew](https://github.com/ravachol/kew),
GPL-2) en C++20 puro en la capa del producto, con corrutinas,
metaprogramación y scripting Lua. El código C legado (~35,5k líneas) queda
intacto como spec de referencia hasta la paridad funcional, y se elimina en el
hito M5. Decisión clave: **reescritura greenfield** — el god-struct `Model`
(908 líneas) acopla todas las vistas del kew, lo que hace la migración
incremental más cara que reescribir.

Filosofía de frontera: **own your code, wrap what you don't need to own**.
Las bibliotecas C maduras (miniaudio, decoders, demuxers, libcurl, chafa, PUC
Lua) no se reescriben — se encapsulan tras wrappers RAII con `extern "C"`.

### Estado actual: M0 ✅ M1 ✅ M2 ✅ M3 ✅

**M3a — cachés YTM**: `AnyDecoder` (borrado de tipo sobre el concept, vtable
estática) con cadena de adapters en la engine (miniaudio → webm); demux
nestegg+opus/vorbis vía shim RAII sobre la implementación C de kew
(`src/sound/webm.h`, TU único en C). Gate: .webm real sonando con sink input
pactl.

**M3b — biblioteca fuzzy**: scan recursivo con taglib, búsqueda fuzzy con la
métrica OSA de Typesense (`src/art.cpp`), picker en la TUI ('l': filtra
tecleando, Enter reproduce, ESC cierra).

**M3c — letras LRCLIB**: `net/` (http síncrono + LRCLIB con retry y
normalización del título), vista de letras ('g'), fetch en offload sin
bloquear la UI.

**M0 — fundación**: scheduler de corrutinas (epoll + timers + eventfd),
terminal layer propia en C++, harness PTY, build CMake `-Werror`.

**M1 — configuración real**: settings tipadas con parse genérico por tabla
constexpr de descriptores (la misma tabla expone los campos a Lua), bindings
con eventos magic_enum, kewrc legado en capas (defaults → kewrc →
`init.lua` sol2), logging.

**M2 — motor de audio**: `kiutube2 play <archivo>` reproduce wav/mp3/flac vía
miniaudio: device RAII + ring lock-free SPSC (~2,7s) + feeder corrutina en el
scheduler. Gapless por construcción (el ring nunca se drena entre pistas;
`playing_info` separa "lo que suena" del "decoder abierto", que se libera en
el EOF). Transporte completo en la TUI: space toca/pausa, `n` siguiente,
flechas seek ±5s, `+`/`-` volumen. Gate M2: sink input activo en PipeWire
(pactl) con la posición avanzando, verificado por smoke PTY.

### Decisiones arquitectónicas

1. **Greenfield con gates de paridad** — árbol `player/`; C legado como spec.
2. **CMake** con `FetchContent` (sol2 v3.3.0, pocketfft `81d171a6`,
   magic_enum v0.9.7), conviviendo con el Makefile antiguo hasta el M5.
3. **Terminal layer propia** (~800 líneas C++): termios RAII + escapes de
   termbox2 (MIT) + `co_await` de eventos.
4. **Scheduler de corrutinas propio** (`krt`) en lugar del loop GLib: epoll +
   heap de timers + eventfd; los mensajes pasan a ser llamadas directas.
5. **GLib en cuarentena en MPRIS** (hilo propio) hasta el M5, cuando
   **sdbus-c++** lo saca del link.
6. **pocketfft** (visualizador) y **sol2** (binding Lua) reemplazan a fftw3f y
   al binding manual; **magic_enum** para enum↔string.
7. **Convención de 4 idiomas** (PT/EN/ES/ZH) en todo texto del proyecto.

### Arquitectura objetivo

```
player/
  main.cpp            # bootstrap: scheduler, terminal, app
  krt/                # runtime: scheduler, task, thread_pool, log
  audio/              # engine (miniaudio), Decoder concept (MaDecoder), ring SPSC
  library/            # taglib scan, fuzzy search (Typesense OSA) — real M3
  tui/                # term, cell_buffer, components (CRTP), views, visualizer
  app/                # settings, bindings, player state machine, queue
  net/                # http (curl) + lrclib — real M3; ytm/translate migram no M4
  script/             # sol2: config Lua, keybindings, temas, hooks
  sys/                # MPRIS (sdbus-c++), notificaciones
```

| Wrapper | Sobre C | Nota |
|---|---|---|
| `audio::Engine` | miniaudio | device + ring SPSC + feeder corrutina |
| `Decoder` concept | opusfile/vorbis/faad/dr_mp3 | MaDecoder cubre wav/mp3/flac (M2); demuxers en M3 |
| `net::Http` | libcurl | `CurlHandle` RAII (movido de ytm) |
| `tui::Term` | termios + tablas | único lugar con escapes |
| `script::State` | Lua 5.4 vía sol2 | tabla `settings` genérica |
| `cover::Canvas` | chafa | |
| `sys::Mpris` | sd-bus (M5) | glib sale del link |

### Scheduler

Run-loop único con tres fuentes de eventos: fds epoll (one-shot), heap de
timers y cola thread-safe vía eventfd. Invariante central: **ninguna corrutina
retoma fuera del hilo del loop** — `krt::complete()` enruta toda continuación
por el hook del scheduler. Awaitables: `sleep(ms)`, `fd_readable(fd)`,
`offload(fn)`. Las corrutinas de larga duración (feeder, reloj de UI) checan
un flag de término — si no, sus timers impedirían que el run-loop termine.

### Metaprogramación

Concepts (`Decoder`, `Component`), tupla constexpr de descriptores de settings
alimentando el parse kewrc Y la exposición a Lua, bindings default
`constexpr`, eventos vía magic_enum, CRTP en componentes de UI,
`std::format`.

### Lua (donde quepa)

Config (`init.lua`) con `bind(tecla, evento)`, `log(msg)` y tabla `settings`
type-safe; temas y hooks en el M4. µs/llamada — ok por frame, prohibido por
muestra de audio.

### Convención de 4 idiomas

Todo comentario, documento, string de UI y commit sigue PT → EN → ES → ZH.

### Hitos

| Hito | Entrega |
|---|---|
| M0 ✅ | Doc + CMake + krt + TUI vacía + harness PTY |
| M1 ✅ | Settings tipadas (descriptores constexpr), sol2 Lua config, bindings magic_enum, logging |
| M2 ✅ | audio::Engine + Decoder concept + play/pause/seek/gapless + gate pactl |
| M3 ✅ | WebmDecoder (cachés YTM), library taglib + fuzzy Typesense + picker, letras LRCLIB |
| M4 | Views completas, visualizador pocketfft, capa chafa, F8 YTM, CLI --ytm, migración ytm a net |
| M5 | MPRIS sdbus-c++, se borra src/ C, renombra binario |

### Cómo compilar y probar

```bash
cmake -B build && cmake --build build -j16
ctest --test-dir build
# gate M2 (audio): reproduce un WAV y asegura sink input en PipeWire
python3 tools/tui_smoke.py "./build/kiutube2 play /tmp/kiutube2_gate.wav" \
  --expect "kiutube2" \
  --sh "sleep 1.2; pactl list sink-inputs | grep -q 'application.process.binary = \"kiutube2\"'" \
  --expect "00:01" --send " " --expect "||" --send "q" --expect-exit 3
```

### Fuera de alcance

Reescribir miniaudio/nestegg/libmp4/stb_image/libcurl/chafa/decoders;
intérprete Lua en C++; soporte Windows/SMTC hasta nuevo aviso.

---

## 中文

### 愿景

将 kiutube（[kew](https://github.com/ravachol/kew) 的 fork，GPL-2）在产品
层用纯 C++20 彻底重写，引入协程、元编程与 Lua 脚本。在功能对齐之前，
原有 C 代码（约 35,500 行）保持原样作为参照规范，并在里程碑 M5 中移除。
关键决策：**greenfield 重写** —— kew 的 `Model` god-struct（908 行）耦
合了所有视图，增量迁移比重写更昂贵。

边界哲学：**own your code, wrap what you don't need to own**。成熟的 C
库（miniaudio、解码器、解封装器、libcurl、chafa、PUC Lua）不做重写
—— 而是通过 RAII + `extern "C"` 包装。

### 当前状态：M0 ✅ M1 ✅ M2 ✅ M3 ✅

**M3a —— YTM 缓存**：`AnyDecoder`（基于 concept 的类型擦除，静态 vtable），
引擎按链式尝试适配器（miniaudio → webm）；nestegg+opus/vorbis 解封装经
RAII shim 复用 kew 的 C 实现（`src/sound/webm.h`，单一 C TU）。门槛：
真实 .webm 播放并断言 pactl sink input。

**M3b —— 模糊曲库**：taglib 递归扫描，采用 Typesense 的 OSA 度量
（`src/art.cpp`）的模糊搜索，TUI 选择器（'l'：输入即过滤，Enter 播放，
ESC 关闭）。

**M3c —— LRCLIB 歌词**：`net/`（同步 http + 带重试与标题归一化的
LRCLIB），歌词视图（'g'），经 offload 的不阻塞获取。

**M0 —— 基础**：协程调度器（epoll + 定时器 + eventfd）、自研 C++ 终端
层、PTY 冒烟 harness、CMake `-Werror` 构建。

**M1 —— 真实配置**：类型化 settings，由 constexpr 描述符表驱动的通用
解析（同一张表向 Lua 暴露字段）、magic_enum 事件绑定、分层的旧 kewrc
（默认值 → kewrc → sol2 `init.lua`）、日志。

**M2 —— 音频引擎**：`kiutube2 play <文件>` 播放 wav/mp3/flac（经
miniaudio）：RAII 设备 + lock-free SPSC 环形缓冲（约 2.7 秒）+ 调度器
协程 feeder。按构造实现 gapless（曲目之间环形缓冲不被排空；
`playing_info` 将"正在播放的曲目"与"打开的解码器"分离，解码器在 EOF
时释放）。TUI 完整传输控制：space 播放/暂停、`n` 下一首、方向键
seek ±5 秒、`+`/`-` 音量。M2 门槛：PipeWire 上存在活跃 sink input
（pactl）且位置前进，由 PTY 冒烟验证。

### 架构决策

1. **Greenfield + 对齐门槛** —— `player/` 新目录；旧 C 代码作为规范。
2. **CMake** + `FetchContent`（sol2 v3.3.0、pocketfft `81d171a6`、
   magic_enum v0.9.7），在 M5 之前与旧 Makefile 共存。
3. **自研终端层**（约 800 行 C++）：termios RAII + termbox2（MIT）转义
   表 + `co_await` 事件。
4. **自研协程调度器**（`krt`）替代 GLib 主循环：epoll + 定时器堆 +
   eventfd；消息传递变为直接函数调用。
5. **GLib 隔离在 MPRIS 线程**中直到 M5，届时用 **sdbus-c++** 将其从链
   接中移除。
6. **pocketfft**（可视化器）与 **sol2**（Lua 绑定）替代 fftw3f 与手动
   绑定；**magic_enum** 用于 enum↔string。
7. **四种语言约定**（PT/EN/ES/ZH）覆盖项目中的所有文本。

### 目标架构

```
player/
  main.cpp            # bootstrap: scheduler, terminal, app
  krt/                # runtime: scheduler, task, thread_pool, log
  audio/              # engine (miniaudio)、Decoder concept (MaDecoder)、SPSC ring
  library/            # directory tree、metadata (taglib)、fuzzy search、cache
  tui/                # term、cell_buffer、components (CRTP)、views、visualizer
  app/                # settings、bindings、player 状态机、queue
  net/                # http (curl) + lrclib —— M3 已实现；ytm/translate 在 M4
  script/             # sol2：Lua 配置、键绑定、主题、hooks
  sys/                # MPRIS (sdbus-c++)、通知
```

| 包装层 | 基于 C | 说明 |
|---|---|---|
| `audio::Engine` | miniaudio | device + SPSC 环形缓冲 + 协程 feeder |
| `Decoder` concept | opusfile/vorbis/faad/dr_mp3 | MaDecoder 覆盖 wav/mp3/flac（M2）；demuxer 在 M3 |
| `net::Http` | libcurl | `CurlHandle` RAII（从 ytm 迁移） |
| `tui::Term` | termios + 转义表 | 唯一包含转义序列的地方 |
| `script::State` | Lua 5.4（经 sol2） | 通用 `settings` 表 |
| `cover::Canvas` | chafa | |
| `sys::Mpris` | sd-bus（M5） | glib 退出链接 |

### 调度器

单 run-loop，三种事件源：epoll fd（one-shot）、定时器堆、经由 eventfd
的线程安全队列。核心不变式：**任何协程都不会在 loop 线程之外恢复**
—— 所有续体都经 `krt::complete()` 路由到调度器 hook。Awaitable：
`sleep(ms)`、`fd_readable(fd)`、`offload(fn)`。长生命周期协程
（feeder、UI 时钟）检查终止标志 —— 否则它们的定时器会让 run-loop
永远无法结束。

### 元编程

Concepts（`Decoder`、`Component`）、constexpr settings 描述符元组同时
驱动 kewrc 解析与 Lua 暴露、`constexpr` 默认绑定、magic_enum 事件、
UI 组件的 CRTP、`std::format`。

### Lua（适可而止之处）

配置（`init.lua`）提供 `bind(键, 事件)`、`log(msg)` 与类型安全的
`settings` 表；主题与 hooks 在 M4 加入。µs/次调用 —— 每帧调用可以，
每个音频采样禁止。

### 四种语言约定

所有注释、文档、UI 字符串与 commit 均遵循 PT → EN → ES → ZH。

### 里程碑

| 里程碑 | 交付物 |
|---|---|
| M0 ✅ | 文档 + CMake + krt + 空白 TUI + PTY harness |
| M1 ✅ | 类型化 settings（constexpr 描述符）、sol2 Lua 配置、magic_enum 绑定、日志 |
| M2 ✅ | audio::Engine + Decoder concept + play/pause/seek/gapless + pactl 门槛 |
| M3 ✅ | WebmDecoder（YTM 缓存）、taglib 曲库 + Typesense 模糊搜索 + 选择器、LRCLIB 歌词 |
| M4 | 完整视图、pocketfft 可视化器、chafa 封面、F8 YTM、--ytm CLI、ytm 迁移到 net |
| M5 | sdbus-c++ MPRIS、删除旧 src/ C、重命名二进制 |

### 构建与测试

```bash
cmake -B build && cmake --build build -j16
ctest --test-dir build
# M2 门槛（音频）：播放 WAV 并断言 PipeWire sink input
python3 tools/tui_smoke.py "./build/kiutube2 play /tmp/kiutube2_gate.wav" \
  --expect "kiutube2" \
  --sh "sleep 1.2; pactl list sink-inputs | grep -q 'application.process.binary = \"kiutube2\"'" \
  --expect "00:01" --send " " --expect "||" --send "q" --expect-exit 3
```

### 不在范围内

重写 miniaudio/nestegg/libmp4/stb_image/libcurl/chafa/解码器；用 C++ 实
现 Lua 解释器；Windows/SMTC 支持（另行通知前）。
