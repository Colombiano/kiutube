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

### Estado atual: M0 ✅

Fundação provada: scheduler de corrotinas (epoll + timers + eventfd),
terminal layer própria em C++ (raw mode, tabela de escapes compatível com
termbox2, input awaitable), harness PTY de smoke-test, build CMake com
`-Wall -Wextra -Werror`. O binário `kiutube2` abre uma TUI vazia, ecoa teclas
e sai em `q`/ESC/F10. O build C legado (`Makefile` → `kiutube`) segue
intocado e funcional.

### Decisões arquiteturais

1. **Greenfield com gates de paridade** — árvore `player/`; C legado como spec.
2. **CMake** com `FetchContent` (sol2 v3.3.0, pocketfft `81d171a6`), convivendo
   com o Makefile antigo até o M5.
3. **Terminal layer própria** (~800 linhas C++): termios RAII + escapes do
   termbox2 (MIT) + `co_await` de eventos. Sem termbox2/FTXUI no caminho do
   produto.
4. **Scheduler de corrotinas próprio** (`krt`) substituindo o loop GLib:
   epoll + heap de timers + eventfd; mensagens viram chamadas diretas.
5. **GLib quarentenado no MPRIS** (thread própria) até o M5, quando
   **sdbus-c++** o remove do link.
6. **pocketfft** (visualizador) e **sol2** (binding Lua) substituem fftw3f e o
   binding manual; **magic_enum** para enum↔string (M1).
7. **Convenção 4 idiomas** (PT/EN/ES/ZH) em todo texto do projeto.

### Arquitetura-alvo

```
player/
  main.cpp            # bootstrap: scheduler, terminal, app
  krt/                # runtime: scheduler, task, thread_pool (move da ytm)
  audio/              # engine (miniaudio), Decoder concept, demux, pipeline
  library/            # directory tree, metadata (taglib), fuzzy search, cache
  tui/                # term, cell_buffer, components (CRTP), views, visualizer
  app/                # player state machine, queue, constexpr bindings, settings
  net/                # http (curl), lrclib, translate, ytm (migra de src/ytm/)
  script/             # sol2: config Lua, keybindings, temas, hooks
  sys/                # MPRIS (sdbus-c++), notificações
```

| Wrapper | Sobre C | Nota |
|---|---|---|
| `audio::Engine` | miniaudio | device + callback; feed via ring lock-free |
| `Decoder` concept | opusfile/vorbis/faad/dr_mp3 | dispatch estático |
| `Demuxer` adapters | nestegg/libmp4 | parsers isolados |
| `net::Http` | libcurl | `CurlHandle` RAII (move da ytm) |
| `tui::Term` | termios + tabelas | único lugar com escapes |
| `script::State` | Lua 5.4 via sol2 | |
| `cover::Canvas` | chafa | |
| `sys::Mpris` | sd-bus (M5) | glib sai do link |

### Scheduler

Run-loop única com três fontes de eventos: fds epoll (one-shot), heap de
timers e fila thread-safe via eventfd. Invariante central: **nenhuma corrotina
retoma fora da thread do loop** — `krt::complete()` roteia toda continuação
pelo hook do scheduler. Awaitables: `sleep(ms)`, `fd_readable(fd)`,
`offload(fn)` (pool existente, conclusão no loop). `sync_wait()` é proibido na
thread do scheduler (deadlock por construção) — serve de ponte C em M2+.

### Metaprogramação

Concepts (`Decoder`, `LyricsProvider`, `Component`), tabela `constexpr` de
keybindings com validação de `kewrc` em compile-time, CRTP em componentes de
UI, policies para o visualizador, `std::format` com format strings de
compile-time.

### Lua (onde couber)

Config (`init.lua`), keybindings, temas e hooks (`bar_color`,
`on_track_change`). Custo documentado: µs/chamada — ok por frame, proibido por
amostra de áudio.

### Convenção 4 idiomas

Todo comentário, documento, string de UI e commit segue PT → EN → ES → ZH.

### Marcos

| Marco | Entrega |
|---|---|
| M0 ✅ | Doc + CMake + krt + TUI vazia + harness PTY |
| M1 | Settings tipadas, sol2 Lua config, bindings constexpr, logging |
| M2 | audio::Engine + Decoders + play/pause/seek/gapless local |
| M3 | library/playlist/search/lyrics/capa; migra ytm para player/net |
| M4 | Views completas, visualizador pocketfft, F8 YTM no scheduler, CLI --ytm |
| M5 | MPRIS sdbus-c++, apaga src/ C, renomeia binário |

Cada marco fecha com: build verde (`-Werror`), `scheduler_smoke`, smoke PTY
(`tools/tui_smoke.py`) e paridade verificada contra o binário C.

### Como buildar e testar (M0)

```bash
cmake -B build && cmake --build build -j16
./build/scheduler_smoke
python3 tools/tui_smoke.py ./build/kiutube2 --expect "kiutube2" \
    --send "x" --expect "[x]" --send "q" --expect-exit 3
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

### Current state: M0 ✅

Foundation proven: coroutine scheduler (epoll + timers + eventfd), own C++
terminal layer (raw mode, termbox2-compatible escape table, awaitable input),
PTY smoke-test harness, CMake build with `-Wall -Wextra -Werror`. The
`kiutube2` binary opens an empty TUI, echoes keys and exits on `q`/ESC/F10.
The legacy C build (`Makefile` → `kiutube`) remains untouched and functional.

### Architectural decisions

1. **Greenfield with parity gates** — `player/` tree; legacy C as spec.
2. **CMake** with `FetchContent` (sol2 v3.3.0, pocketfft `81d171a6`),
   coexisting with the legacy Makefile until M5.
3. **Own terminal layer** (~800 lines C++): termios RAII + termbox2 (MIT)
   escapes + `co_await` events. No termbox2/FTXUI in the product path.
4. **Own coroutine scheduler** (`krt`) replacing the GLib loop: epoll + timer
   heap + eventfd; messages become direct calls.
5. **GLib quarantined in MPRIS** (own thread) until M5, when **sdbus-c++**
   removes it from the link.
6. **pocketfft** (visualizer) and **sol2** (Lua binding) replace fftw3f and the
   manual binding; **magic_enum** for enum↔string (M1).
7. **Four-language convention** (PT/EN/ES/ZH) for every project text.

### Target architecture

```
player/
  main.cpp            # bootstrap: scheduler, terminal, app
  krt/                # runtime: scheduler, task, thread_pool (moved from ytm)
  audio/              # engine (miniaudio), Decoder concept, demux, pipeline
  library/            # directory tree, metadata (taglib), fuzzy search, cache
  tui/                # term, cell_buffer, components (CRTP), views, visualizer
  app/                # player state machine, queue, constexpr bindings, settings
  net/                # http (curl), lrclib, translate, ytm (migrates from src/ytm/)
  script/             # sol2: Lua config, keybindings, themes, hooks
  sys/                # MPRIS (sdbus-c++), notifications
```

| Wrapper | Over C | Note |
|---|---|---|
| `audio::Engine` | miniaudio | device + callback; lock-free ring feed |
| `Decoder` concept | opusfile/vorbis/faad/dr_mp3 | static dispatch |
| `Demuxer` adapters | nestegg/libmp4 | isolated parsers |
| `net::Http` | libcurl | `CurlHandle` RAII (moved from ytm) |
| `tui::Term` | termios + tables | the only place with escapes |
| `script::State` | Lua 5.4 via sol2 | |
| `cover::Canvas` | chafa | |
| `sys::Mpris` | sd-bus (M5) | glib leaves the link |

### Scheduler

Single run-loop with three event sources: epoll fds (one-shot), timer heap and
a thread-safe queue via eventfd. Central invariant: **no coroutine ever
resumes outside the loop thread** — `krt::complete()` routes every
continuation through the scheduler hook. Awaitables: `sleep(ms)`,
`fd_readable(fd)`, `offload(fn)` (existing pool, completion on the loop).
`sync_wait()` is forbidden on the scheduler thread (deadlock by construction)
— it serves as the M2+ C bridge.

### Metaprogramming

Concepts (`Decoder`, `LyricsProvider`, `Component`), `constexpr` keybinding
table with compile-time `kewrc` validation, CRTP UI components, policies for
the visualizer, `std::format` with compile-time format strings.

### Lua (where it fits)

Config (`init.lua`), keybindings, themes and hooks (`bar_color`,
`on_track_change`). Documented cost: µs per call — fine per frame, forbidden
per audio sample.

### Four-language convention

Every comment, document, UI string and commit follows PT → EN → ES → ZH.

### Milestones

| Milestone | Deliverable |
|---|---|
| M0 ✅ | Doc + CMake + krt + empty TUI + PTY harness |
| M1 | Typed settings, sol2 Lua config, constexpr bindings, logging |
| M2 | audio::Engine + Decoders + play/pause/seek/gapless local |
| M3 | library/playlist/search/lyrics/cover; ytm migrates to player/net |
| M4 | Full views, pocketfft visualizer, F8 YTM on the scheduler, --ytm CLI |
| M5 | sdbus-c++ MPRIS, delete legacy src/ C, rename binary |

Every milestone closes with: green build (`-Werror`), `scheduler_smoke`, PTY
smoke (`tools/tui_smoke.py`) and verified parity against the C binary.

### Build and test (M0)

```bash
cmake -B build && cmake --build build -j16
./build/scheduler_smoke
python3 tools/tui_smoke.py ./build/kiutube2 --expect "kiutube2" \
    --send "x" --expect "[x]" --send "q" --expect-exit 3
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

### Estado actual: M0 ✅

Fundación probada: scheduler de corrutinas (epoll + timers + eventfd),
terminal layer propia en C++ (raw mode, tabla de escapes compatible con
termbox2, input awaitable), harness PTY de smoke-test, build CMake con
`-Wall -Wextra -Werror`. El binario `kiutube2` abre una TUI vacía, hace eco
de las teclas y sale con `q`/ESC/F10. El build C legado (`Makefile` →
`kiutube`) sigue intacto y funcional.

### Decisiones arquitectónicas

1. **Greenfield con gates de paridad** — árbol `player/`; C legado como spec.
2. **CMake** con `FetchContent` (sol2 v3.3.0, pocketfft `81d171a6`), conviviendo
   con el Makefile antiguo hasta el M5.
3. **Terminal layer propia** (~800 líneas C++): termios RAII + escapes de
   termbox2 (MIT) + `co_await` de eventos. Sin termbox2/FTXUI en la ruta del
   producto.
4. **Scheduler de corrutinas propio** (`krt`) en lugar del loop GLib: epoll +
   heap de timers + eventfd; los mensajes pasan a ser llamadas directas.
5. **GLib en cuarentena en MPRIS** (hilo propio) hasta el M5, cuando
   **sdbus-c++** lo saca del link.
6. **pocketfft** (visualizador) y **sol2** (binding Lua) reemplazan a fftw3f y
   al binding manual; **magic_enum** para enum↔string (M1).
7. **Convención de 4 idiomas** (PT/EN/ES/ZH) en todo texto del proyecto.

### Arquitectura objetivo

```
player/
  main.cpp            # bootstrap: scheduler, terminal, app
  krt/                # runtime: scheduler, task, thread_pool (movido de ytm)
  audio/              # engine (miniaudio), Decoder concept, demux, pipeline
  library/            # directory tree, metadata (taglib), fuzzy search, cache
  tui/                # term, cell_buffer, components (CRTP), views, visualizer
  app/                # player state machine, queue, constexpr bindings, settings
  net/                # http (curl), lrclib, translate, ytm (migra de src/ytm/)
  script/             # sol2: config Lua, keybindings, temas, hooks
  sys/                # MPRIS (sdbus-c++), notificaciones
```

| Wrapper | Sobre C | Nota |
|---|---|---|
| `audio::Engine` | miniaudio | device + callback; feed por ring lock-free |
| `Decoder` concept | opusfile/vorbis/faad/dr_mp3 | dispatch estático |
| `Demuxer` adapters | nestegg/libmp4 | parsers aislados |
| `net::Http` | libcurl | `CurlHandle` RAII (movido de ytm) |
| `tui::Term` | termios + tablas | único lugar con escapes |
| `script::State` | Lua 5.4 vía sol2 | |
| `cover::Canvas` | chafa | |
| `sys::Mpris` | sd-bus (M5) | glib sale del link |

### Scheduler

Run-loop único con tres fuentes de eventos: fds epoll (one-shot), heap de
timers y cola thread-safe vía eventfd. Invariante central: **ninguna corrutina
retoma fuera del hilo del loop** — `krt::complete()` enruta toda continuación
por el hook del scheduler. Awaitables: `sleep(ms)`, `fd_readable(fd)`,
`offload(fn)` (pool existente, conclusión en el loop). `sync_wait()` está
prohibido en el hilo del scheduler (deadlock por construcción) — sirve de
puente C en M2+.

### Metaprogramación

Concepts (`Decoder`, `LyricsProvider`, `Component`), tabla `constexpr` de
keybindings con validación de `kewrc` en compile-time, CRTP en componentes de
UI, policies para el visualizador, `std::format` con format strings de
compile-time.

### Lua (donde quepa)

Config (`init.lua`), keybindings, temas y hooks (`bar_color`,
`on_track_change`). Coste documentado: µs/llamada — ok por frame, prohibido
por muestra de audio.

### Convención de 4 idiomas

Todo comentario, documento, string de UI y commit sigue PT → EN → ES → ZH.

### Hitos

| Hito | Entrega |
|---|---|
| M0 ✅ | Doc + CMake + krt + TUI vacía + harness PTY |
| M1 | Settings tipadas, sol2 Lua config, bindings constexpr, logging |
| M2 | audio::Engine + Decoders + play/pause/seek/gapless local |
| M3 | library/playlist/search/lyrics/capa; ytm migra a player/net |
| M4 | Views completas, visualizador pocketfft, F8 YTM en el scheduler, CLI --ytm |
| M5 | MPRIS sdbus-c++, se borra src/ C, renombra binario |

Cada hito cierra con: build verde (`-Werror`), `scheduler_smoke`, smoke PTY
(`tools/tui_smoke.py`) y paridad verificada contra el binario C.

### Cómo compilar y probar (M0)

```bash
cmake -B build && cmake --build build -j16
./build/scheduler_smoke
python3 tools/tui_smoke.py ./build/kiutube2 --expect "kiutube2" \
    --send "x" --expect "[x]" --send "q" --expect-exit 3
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

### 当前状态：M0 ✅

基础已验证：协程调度器（epoll + 定时器 + eventfd）、自研 C++ 终端层
（raw mode、兼容 termbox2 的转义表、可 await 的输入）、PTY 冒烟测试
harness、带 `-Wall -Wextra -Werror` 的 CMake 构建。`kiutube2` 二进制可
打开空白 TUI、回显按键，并在 `q`/ESC/F10 时退出。旧的 C 构建
（`Makefile` → `kiutube`）保持原样且可用。

### 架构决策

1. **Greenfield + 对齐门槛** —— `player/` 新目录；旧 C 代码作为规范。
2. **CMake** + `FetchContent`（sol2 v3.3.0、pocketfft `81d171a6`），在
   M5 之前与旧 Makefile 共存。
3. **自研终端层**（约 800 行 C++）：termios RAII + termbox2（MIT）转义
   表 + `co_await` 事件。产品路径上不再使用 termbox2/FTXUI。
4. **自研协程调度器**（`krt`）替代 GLib 主循环：epoll + 定时器堆 +
   eventfd；消息传递变为直接函数调用。
5. **GLib 隔离在 MPRIS 线程**中直到 M5，届时用 **sdbus-c++** 将其从链
   接中移除。
6. **pocketfft**（可视化器）与 **sol2**（Lua 绑定）替代 fftw3f 与手动绑
   定；**magic_enum** 用于 enum↔string（M1）。
7. **四种语言约定**（PT/EN/ES/ZH）覆盖项目中的所有文本。

### 目标架构

```
player/
  main.cpp            # bootstrap: scheduler, terminal, app
  krt/                # runtime: scheduler, task, thread_pool（从 ytm 迁移）
  audio/              # engine (miniaudio)、Decoder concept、demux、pipeline
  library/            # directory tree、metadata (taglib)、fuzzy search、cache
  tui/                # term、cell_buffer、components (CRTP)、views、visualizer
  app/                # player 状态机、queue、constexpr bindings、settings
  net/                # http (curl)、lrclib、translate、ytm（从 src/ytm/ 迁移）
  script/             # sol2：Lua 配置、键绑定、主题、hooks
  sys/                # MPRIS (sdbus-c++)、通知
```

| 包装层 | 基于 C | 说明 |
|---|---|---|
| `audio::Engine` | miniaudio | device + 回调；lock-free 环形缓冲喂数据 |
| `Decoder` concept | opusfile/vorbis/faad/dr_mp3 | 静态分发 |
| `Demuxer` adapters | nestegg/libmp4 | 隔离的解析器 |
| `net::Http` | libcurl | `CurlHandle` RAII（从 ytm 迁移） |
| `tui::Term` | termios + 转义表 | 唯一包含转义序列的地方 |
| `script::State` | Lua 5.4（经 sol2） | |
| `cover::Canvas` | chafa | |
| `sys::Mpris` | sd-bus（M5） | glib 退出链接 |

### 调度器

单 run-loop，三种事件源：epoll fd（one-shot）、定时器堆、经由 eventfd
的线程安全队列。核心不变式：**任何协程都不会在 loop 线程之外恢复**
—— 所有续体都经 `krt::complete()` 路由到调度器 hook。Awaitable：
`sleep(ms)`、`fd_readable(fd)`、`offload(fn)`（既有线程池，完成事件交
付到 loop）。`sync_wait()` 禁止在调度器线程上调用（按构造会死锁）
—— M2+ 起用作 C 桥接。

### 元编程

Concepts（`Decoder`、`LyricsProvider`、`Component`）、`constexpr` 键绑定
表（编译期校验 `kewrc`）、UI 组件的 CRTP、可视化器的 policy、编译期格
式字符串的 `std::format`。

### Lua（适可而止之处）

配置（`init.lua`）、键绑定、主题与 hooks（`bar_color`、
`on_track_change`）。已记录的代价：µs/次调用 —— 每帧调用可以，每个音
频采样禁止。

### 四种语言约定

所有注释、文档、UI 字符串与 commit 均遵循 PT → EN → ES → ZH。

### 里程碑

| 里程碑 | 交付物 |
|---|---|
| M0 ✅ | 文档 + CMake + krt + 空白 TUI + PTY harness |
| M1 | 类型化 settings、sol2 Lua 配置、constexpr 键绑定、logging |
| M2 | audio::Engine + 解码器 + 本地 play/pause/seek/gapless |
| M3 | library/playlist/search/歌词/封面；ytm 迁移到 player/net |
| M4 | 完整视图、pocketfft 可视化器、调度器上的 F8 YTM、--ytm CLI |
| M5 | sdbus-c++ MPRIS、删除旧 src/ C、重命名二进制 |

每个里程碑的关闭条件：构建通过（`-Werror`）、`scheduler_smoke`、PTY
冒烟（`tools/tui_smoke.py`）、与 C 二进制对齐验证。

### 构建与测试（M0）

```bash
cmake -B build && cmake --build build -j16
./build/scheduler_smoke
python3 tools/tui_smoke.py ./build/kiutube2 --expect "kiutube2" \
    --send "x" --expect "[x]" --send "q" --expect-exit 3
```

### 不在范围内

重写 miniaudio/nestegg/libmp4/stb_image/libcurl/chafa/解码器；用 C++ 实
现 Lua 解释器；Windows/SMTC 支持（另行通知前）。
