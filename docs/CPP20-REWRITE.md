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

### Estado atual: M0 ✅ M1 ✅

**M0 — fundação**: scheduler de corrotinas (epoll + timers + eventfd),
terminal layer própria em C++ (raw mode, tabela de escapes compatível com
termbox2, input awaitable), harness PTY de smoke-test, build CMake com
`-Wall -Wextra -Werror`.

**M1 — configuração real**: settings tipadas com parse genérico alimentado por
tabela constexpr de descritores (a mesma tabela expõe os campos ao Lua),
bindings com eventos via magic_enum (case-insensitive + snake_case), kewrc
legado em camadas (defaults → kewrc → `init.lua` sol2) e logging. Gate M1
verificado: tecla custom definida no `init.lua` dispara ação, e `bind=` do
kewrc funciona.

### Decisões arquiteturais

1. **Greenfield com gates de paridade** — árvore `player/`; C legado como spec.
2. **CMake** com `FetchContent` (sol2 v3.3.0, pocketfft `81d171a6`,
   magic_enum v0.9.7), convivendo com o Makefile antigo até o M5.
3. **Terminal layer própria** (~800 linhas C++): termios RAII + escapes do
   termbox2 (MIT) + `co_await` de eventos. Sem termbox2/FTXUI no caminho do
   produto.
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
  audio/              # engine (miniaudio), Decoder concept, demux, pipeline
  library/            # directory tree, metadata (taglib), fuzzy search, cache
  tui/                # term, cell_buffer, components (CRTP), views, visualizer
  app/                # settings, bindings, player state machine, queue
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
| `script::State` | Lua 5.4 via sol2 | tabela `settings` genérica |
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

Concepts (`Decoder`, `LyricsProvider`, `Component`), tupla constexpr de
descritores de settings alimentando parse kewrc E exposição Lua, bindings
default `constexpr`, eventos via magic_enum (sem strings sincronizadas à
mão), CRTP em componentes de UI, `std::format` com format strings de
compile-time.

### Lua (onde couber)

Config (`init.lua`) com `bind(tecla, evento)`, `log(msg)` e tabela `settings`
type-safe; temas e hooks (`bar_color`, `on_track_change`) no M4. Custo
documentado: µs/chamada — ok por frame, proibido por amostra de áudio.

### Convenção 4 idiomas

Todo comentário, documento, string de UI e commit segue PT → EN → ES → ZH.

### Marcos

| Marco | Entrega |
|---|---|
| M0 ✅ | Doc + CMake + krt + TUI vazia + harness PTY |
| M1 ✅ | Settings tipadas (descritores constexpr), sol2 Lua config, bindings magic_enum, logging |
| M2 | audio::Engine + Decoders + play/pause/seek/gapless local |
| M3 | library/playlist/search/lyrics/capa; migra ytm para player/net |
| M4 | Views completas, visualizador pocketfft, F8 YTM no scheduler, CLI --ytm |
| M5 | MPRIS sdbus-c++, apaga src/ C, renomeia binário |

Cada marco fecha com: build verde (`-Werror`), `scheduler_smoke`,
`settings_smoke`, smoke PTY (`tools/tui_smoke.py`) e paridade verificada
contra o binário C.

### Como buildar e testar (M1)

```bash
cmake -B build && cmake --build build -j16
ctest --test-dir build                      # scheduler_smoke + settings_smoke
python3 tools/tui_smoke.py ./build/kiutube2 --expect "kiutube2" \
    --send "x" --expect "[x]" --send "q" --expect-exit 3
# PT: gate M1: tecla custom do init.lua sai do app.
H=$(mktemp -d); mkdir -p $H/.config/kiutube2
echo 'bind("p", "Quit")' > $H/.config/kiutube2/init.lua
python3 tools/tui_smoke.py ./build/kiutube2 --home $H \
    --expect "kiutube2" --send "p" --expect-exit 3
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

### Current state: M0 ✅ M1 ✅

**M0 — foundation**: coroutine scheduler (epoll + timers + eventfd), own C++
terminal layer (raw mode, termbox2-compatible escape table, awaitable input),
PTY smoke-test harness, CMake build with `-Wall -Wextra -Werror`.

**M1 — real configuration**: typed settings with generic parsing fed by a
constexpr descriptor table (the same table exposes fields to Lua), bindings
with magic_enum events (case-insensitive + snake_case), layered legacy kewrc
(defaults → kewrc → sol2 `init.lua`) and logging. M1 gate verified: a custom
key defined in `init.lua` triggers an action, and kewrc `bind=` works.

### Architectural decisions

1. **Greenfield with parity gates** — `player/` tree; legacy C as spec.
2. **CMake** with `FetchContent` (sol2 v3.3.0, pocketfft `81d171a6`,
   magic_enum v0.9.7), coexisting with the legacy Makefile until M5.
3. **Own terminal layer** (~800 lines C++): termios RAII + termbox2 (MIT)
   escapes + `co_await` events. No termbox2/FTXUI in the product path.
4. **Own coroutine scheduler** (`krt`) replacing the GLib loop: epoll + timer
   heap + eventfd; messages become direct calls.
5. **GLib quarantined in MPRIS** (own thread) until M5, when **sdbus-c++**
   removes it from the link.
6. **pocketfft** (visualizer) and **sol2** (Lua binding) replace fftw3f and the
   manual binding; **magic_enum** for event/settings enum↔string.
7. **Four-language convention** (PT/EN/ES/ZH) for every project text.

### Target architecture

```
player/
  main.cpp            # bootstrap: scheduler, terminal, app
  krt/                # runtime: scheduler, task, thread_pool, log
  audio/              # engine (miniaudio), Decoder concept, demux, pipeline
  library/            # directory tree, metadata (taglib), fuzzy search, cache
  tui/                # term, cell_buffer, components (CRTP), views, visualizer
  app/                # settings, bindings, player state machine, queue
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
| `script::State` | Lua 5.4 via sol2 | generic `settings` table |
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

Concepts (`Decoder`, `LyricsProvider`, `Component`), constexpr settings
descriptor tuple feeding both kewrc parsing AND Lua exposure, `constexpr`
default bindings, magic_enum events (no hand-synced strings), CRTP UI
components, `std::format` with compile-time format strings.

### Lua (where it fits)

Config (`init.lua`) with `bind(key, event)`, `log(msg)` and a type-safe
`settings` table; themes and hooks (`bar_color`, `on_track_change`) in M4.
Documented cost: µs per call — fine per frame, forbidden per audio sample.

### Four-language convention

Every comment, document, UI string and commit follows PT → EN → ES → ZH.

### Milestones

| Milestone | Deliverable |
|---|---|
| M0 ✅ | Doc + CMake + krt + empty TUI + PTY harness |
| M1 ✅ | Typed settings (constexpr descriptors), sol2 Lua config, magic_enum bindings, logging |
| M2 | audio::Engine + Decoders + play/pause/seek/gapless local |
| M3 | library/playlist/search/lyrics/cover; ytm migrates to player/net |
| M4 | Full views, pocketfft visualizer, F8 YTM on the scheduler, --ytm CLI |
| M5 | sdbus-c++ MPRIS, delete legacy src/ C, rename binary |

Every milestone closes with: green build (`-Werror`), `scheduler_smoke`,
`settings_smoke`, PTY smoke (`tools/tui_smoke.py`) and verified parity against
the C binary.

### Build and test (M1)

```bash
cmake -B build && cmake --build build -j16
ctest --test-dir build                      # scheduler_smoke + settings_smoke
python3 tools/tui_smoke.py ./build/kiutube2 --expect "kiutube2" \
    --send "x" --expect "[x]" --send "q" --expect-exit 3
# EN: M1 gate: a custom init.lua key quits the app.
H=$(mktemp -d); mkdir -p $H/.config/kiutube2
echo 'bind("p", "Quit")' > $H/.config/kiutube2/init.lua
python3 tools/tui_smoke.py ./build/kiutube2 --home $H \
    --expect "kiutube2" --send "p" --expect-exit 3
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

### Estado actual: M0 ✅ M1 ✅

**M0 — fundación**: scheduler de corrutinas (epoll + timers + eventfd),
terminal layer propia en C++ (raw mode, tabla de escapes compatible con
termbox2, input awaitable), harness PTY de smoke-test, build CMake con
`-Wall -Wextra -Werror`.

**M1 — configuración real**: settings tipadas con parse genérico alimentado
por una tabla constexpr de descriptores (la misma tabla expone los campos a
Lua), bindings con eventos vía magic_enum (case-insensitive + snake_case),
kewrc legado en capas (defaults → kewrc → `init.lua` sol2) y logging. Gate M1
verificado: una tecla custom definida en el `init.lua` dispara acción, y el
`bind=` del kewrc funciona.

### Decisiones arquitectónicas

1. **Greenfield con gates de paridad** — árbol `player/`; C legado como spec.
2. **CMake** con `FetchContent` (sol2 v3.3.0, pocketfft `81d171a6`,
   magic_enum v0.9.7), conviviendo con el Makefile antiguo hasta el M5.
3. **Terminal layer propia** (~800 líneas C++): termios RAII + escapes de
   termbox2 (MIT) + `co_await` de eventos. Sin termbox2/FTXUI en la ruta del
   producto.
4. **Scheduler de corrutinas propio** (`krt`) en lugar del loop GLib: epoll +
   heap de timers + eventfd; los mensajes pasan a ser llamadas directas.
5. **GLib en cuarentena en MPRIS** (hilo propio) hasta el M5, cuando
   **sdbus-c++** lo saca del link.
6. **pocketfft** (visualizador) y **sol2** (binding Lua) reemplazan a fftw3f y
   al binding manual; **magic_enum** para enum↔string de eventos/settings.
7. **Convención de 4 idiomas** (PT/EN/ES/ZH) en todo texto del proyecto.

### Arquitectura objetivo

```
player/
  main.cpp            # bootstrap: scheduler, terminal, app
  krt/                # runtime: scheduler, task, thread_pool, log
  audio/              # engine (miniaudio), Decoder concept, demux, pipeline
  library/            # directory tree, metadata (taglib), fuzzy search, cache
  tui/                # term, cell_buffer, components (CRTP), views, visualizer
  app/                # settings, bindings, player state machine, queue
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
| `script::State` | Lua 5.4 vía sol2 | tabla `settings` genérica |
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

Concepts (`Decoder`, `LyricsProvider`, `Component`), tupla constexpr de
descriptores de settings alimentando el parse kewrc Y la exposición a Lua,
bindings default `constexpr`, eventos vía magic_enum (sin cadenas
sincronizadas a mano), CRTP en componentes de UI, `std::format` con format
strings de compile-time.

### Lua (donde quepa)

Config (`init.lua`) con `bind(tecla, evento)`, `log(msg)` y tabla `settings`
type-safe; temas y hooks (`bar_color`, `on_track_change`) en el M4. Coste
documentado: µs/llamada — ok por frame, prohibido por muestra de audio.

### Convención de 4 idiomas

Todo comentario, documento, string de UI y commit sigue PT → EN → ES → ZH.

### Hitos

| Hito | Entrega |
|---|---|
| M0 ✅ | Doc + CMake + krt + TUI vacía + harness PTY |
| M1 ✅ | Settings tipadas (descriptores constexpr), sol2 Lua config, bindings magic_enum, logging |
| M2 | audio::Engine + Decoders + play/pause/seek/gapless local |
| M3 | library/playlist/search/lyrics/capa; ytm migra a player/net |
| M4 | Views completas, visualizador pocketfft, F8 YTM en el scheduler, CLI --ytm |
| M5 | MPRIS sdbus-c++, se borra src/ C, renombra binario |

Cada hito cierra con: build verde (`-Werror`), `scheduler_smoke`,
`settings_smoke`, smoke PTY (`tools/tui_smoke.py`) y paridad verificada contra
el binario C.

### Cómo compilar y probar (M1)

```bash
cmake -B build && cmake --build build -j16
ctest --test-dir build                      # scheduler_smoke + settings_smoke
python3 tools/tui_smoke.py ./build/kiutube2 --expect "kiutube2" \
    --send "x" --expect "[x]" --send "q" --expect-exit 3
# ES: gate M1: una tecla custom del init.lua sale de la app.
H=$(mktemp -d); mkdir -p $H/.config/kiutube2
echo 'bind("p", "Quit")' > $H/.config/kiutube2/init.lua
python3 tools/tui_smoke.py ./build/kiutube2 --home $H \
    --expect "kiutube2" --send "p" --expect-exit 3
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

### 当前状态：M0 ✅ M1 ✅

**M0 —— 基础**：协程调度器（epoll + 定时器 + eventfd）、自研 C++ 终端层
（raw mode、兼容 termbox2 的转义表、可 await 的输入）、PTY 冒烟测试
harness、带 `-Wall -Wextra -Werror` 的 CMake 构建。

**M1 —— 真实配置**：类型化 settings，由 constexpr 描述符表驱动的通用
解析（同一张表向 Lua 暴露字段）、基于 magic_enum 的事件绑定（大小写
不敏感 + snake_case）、分层的旧 kewrc（默认值 → kewrc → sol2
`init.lua`）以及日志。M1 门槛已验证：`init.lua` 中定义的自定义键可触
发动作，kewrc 的 `bind=` 亦生效。

### 架构决策

1. **Greenfield + 对齐门槛** —— `player/` 新目录；旧 C 代码作为规范。
2. **CMake** + `FetchContent`（sol2 v3.3.0、pocketfft `81d171a6`、
   magic_enum v0.9.7），在 M5 之前与旧 Makefile 共存。
3. **自研终端层**（约 800 行 C++）：termios RAII + termbox2（MIT）转义
   表 + `co_await` 事件。产品路径上不再使用 termbox2/FTXUI。
4. **自研协程调度器**（`krt`）替代 GLib 主循环：epoll + 定时器堆 +
   eventfd；消息传递变为直接函数调用。
5. **GLib 隔离在 MPRIS 线程**中直到 M5，届时用 **sdbus-c++** 将其从链
   接中移除。
6. **pocketfft**（可视化器）与 **sol2**（Lua 绑定）替代 fftw3f 与手动绑
   定；**magic_enum** 用于事件/settings 的 enum↔string。
7. **四种语言约定**（PT/EN/ES/ZH）覆盖项目中的所有文本。

### 目标架构

```
player/
  main.cpp            # bootstrap: scheduler, terminal, app
  krt/                # runtime: scheduler, task, thread_pool, log
  audio/              # engine (miniaudio)、Decoder concept、demux、pipeline
  library/            # directory tree、metadata (taglib)、fuzzy search、cache
  tui/                # term、cell_buffer、components (CRTP)、views、visualizer
  app/                # settings、bindings、player 状态机、queue
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
| `script::State` | Lua 5.4（经 sol2） | 通用 `settings` 表 |
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

Concepts（`Decoder`、`LyricsProvider`、`Component`）、constexpr settings
描述符元组同时驱动 kewrc 解析与 Lua 暴露、`constexpr` 默认绑定、
magic_enum 事件（无手工同步的字符串）、UI 组件的 CRTP、编译期格式字
符串的 `std::format`。

### Lua（适可而止之处）

配置（`init.lua`）提供 `bind(键, 事件)`、`log(msg)` 与类型安全的
`settings` 表；主题与 hooks（`bar_color`、`on_track_change`）在 M4 加
入。已记录的代价：µs/次调用 —— 每帧调用可以，每个音频采样禁止。

### 四种语言约定

所有注释、文档、UI 字符串与 commit 均遵循 PT → EN → ES → ZH。

### 里程碑

| 里程碑 | 交付物 |
|---|---|
| M0 ✅ | 文档 + CMake + krt + 空白 TUI + PTY harness |
| M1 ✅ | 类型化 settings（constexpr 描述符）、sol2 Lua 配置、magic_enum 绑定、日志 |
| M2 | audio::Engine + 解码器 + 本地 play/pause/seek/gapless |
| M3 | library/playlist/search/歌词/封面；ytm 迁移到 player/net |
| M4 | 完整视图、pocketfft 可视化器、调度器上的 F8 YTM、--ytm CLI |
| M5 | sdbus-c++ MPRIS、删除旧 src/ C、重命名二进制 |

每个里程碑的关闭条件：构建通过（`-Werror`）、`scheduler_smoke`、
`settings_smoke`、PTY 冒烟（`tools/tui_smoke.py`）、与 C 二进制对齐
验证。

### 构建与测试（M1）

```bash
cmake -B build && cmake --build build -j16
ctest --test-dir build                      # scheduler_smoke + settings_smoke
python3 tools/tui_smoke.py ./build/kiutube2 --expect "kiutube2" \
    --send "x" --expect "[x]" --send "q" --expect-exit 3
# ZH: M1 门槛：init.lua 的自定义键使应用退出。
H=$(mktemp -d); mkdir -p $H/.config/kiutube2
echo 'bind("p", "Quit")' > $H/.config/kiutube2/init.lua
python3 tools/tui_smoke.py ./build/kiutube2 --home $H \
    --expect "kiutube2" --send "p" --expect-exit 3
```

### 不在范围内

重写 miniaudio/nestegg/libmp4/stb_image/libcurl/chafa/解码器；用 C++ 实
现 Lua 解释器；Windows/SMTC 支持（另行通知前）。
