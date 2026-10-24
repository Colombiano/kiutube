# PT: CMake do kiutube2 (reescrita C++20). Com o M5 o C legado (src/) saiu
#     do repositório — este é o único build; ver docs/CPP20-REWRITE.md.
# EN: kiutube2 build (C++20 rewrite). With M5 the legacy C (src/) left the
#     repository — this is the only build; see docs/CPP20-REWRITE.md.
# ES: CMake de kiutube2 (reescritura C++20). Con el M5 el C legado (src/)
#     salió del repositorio — este es el único build.
# ZH: kiutube2（C++20 重写）的 CMake。M5 起旧 C 代码（src/）已离开仓库
#     —— 这是唯一的构建方式；见 docs/CPP20-REWRITE.md。
cmake_minimum_required(VERSION 3.28)
project(kiutube2 VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# PT: -D_DEFAULT_SOURCE expõe POSIX (pwritev, TIOCGWINSZ) sem gnu++ extensions.
# EN: -D_DEFAULT_SOURCE exposes POSIX (pwritev, TIOCGWINSZ) without gnu++ extensions.
# ES: -D_DEFAULT_SOURCE expone POSIX (pwritev, TIOCGWINSZ) sin gnu++ extensions.
# ZH: -D_DEFAULT_SOURCE 在不使用 gnu++ 扩展的情况下提供 POSIX 接口（pwritev、TIOCGWINSZ）。
add_compile_definitions(_DEFAULT_SOURCE)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# PT: warnings como erros: o código C++ novo nasce limpo (o build C legado não usa).
# EN: warnings as errors: the new C++ code is born clean (the legacy C build does not).
# ES: warnings como errores: el código C++ nuevo nace limpio (el build C legado no).
# ZH: 警告视为错误：新的 C++ 代码从一开始就保持干净（旧的 C 构建并非如此）。
add_compile_options(-Wall -Wextra -Werror)

find_package(Threads REQUIRED)

# PT: Qt 6 opcional: liga a janela gráfica do visualizador (gui/viz_window).
#     Sem Qt, o kiutube2 compila com um stub que ignora (mesma API).
# EN: Optional Qt 6: enables the visualizer GUI window (gui/viz_window).
#     Without Qt, kiutube2 builds with a no-op stub (same API).
# ES: Qt 6 opcional: habilita la ventana gráfica del visualizador.
#     Sin Qt, kiutube2 compila con un stub que ignora (misma API).
# ZH: 可选 Qt 6：启用可视化器图形窗口（gui/viz_window）。无 Qt 时以同名
#     no-op stub 编译。
find_package(Qt6 COMPONENTS Widgets PrintSupport QUIET)
if(Qt6Widgets_FOUND)
  message(STATUS "Qt6 Widgets encontrado / found: janela grafica habilitada / GUI enabled")
  set(CMAKE_AUTOMOC ON)
else()
  message(STATUS "Qt6 Widgets NAO encontrado / not found: GUI desabilitada (stub) / disabled")
endif()

# PT: chafa opcional: capa de álbum na TUI (library/canvas). Sem chafa, o
#     kiutube2 compila com um stub que sempre falha em render() (mesma API).
#     Via pkg-config: os headers do chafa exigem glib (Requires: glib-2.0).
# EN: Optional chafa: album art in the TUI (library/canvas). Without chafa,
#     kiutube2 builds with a stub whose render() always fails (same API).
#     Via pkg-config: chafa headers require glib (Requires: glib-2.0).
# ES: chafa opcional: carátula en la TUI. Sin chafa, compila con stub.
# ZH: 可选 chafa：TUI 专辑封面（library/canvas）。无 chafa 时编译同名 stub。
#     通过 pkg-config：chafa 头文件依赖 glib（Requires: glib-2.0）。
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(CHafa QUIET IMPORTED_TARGET chafa)
endif()
if(CHafa_FOUND)
  message(STATUS "chafa encontrado / found: capa TUI habilitada / TUI cover enabled")
else()
  message(STATUS "chafa NAO encontrado / not found: capa TUI desabilitada (stub) / disabled")
endif()

# PT: Dependências header-only via FetchContent (pinadas; trocar GIT_TAG por hash
#     de commit após o primeiro configure — ver docs/CPP20-REWRITE.md).
# EN: Header-only dependencies via FetchContent (pinned; swap GIT_TAG for a commit
#     hash after the first configure — see docs/CPP20-REWRITE.md).
# ES: Dependencias header-only vía FetchContent (pinadas; cambiar GIT_TAG por hash
#     de commit tras el primer configure — ver docs/CPP20-REWRITE.md).
# ZH: 通过 FetchContent 引入 header-only 依赖（固定版本；首次 configure 后应将
#     GIT_TAG 换成 commit 哈希 —— 见 docs/CPP20-REWRITE.md）。
include(FetchContent)
# PT: sdbus-c++ (KDE, LGPL) pro MPRIS — estático, sem codegen/tests/exemplos.
# EN: sdbus-c++ (KDE, LGPL) for MPRIS — static, no codegen/tests/examples.
# ES: sdbus-c++ (KDE, LGPL) para el MPRIS — estático, sin codegen/tests.
# ZH: 用于 MPRIS 的 sdbus-c++（KDE，LGPL）—— 静态，无 codegen/tests/示例。
FetchContent_Declare(sdbus-cpp
  GIT_REPOSITORY https://github.com/Kistler-Group/sdbus-cpp.git
  GIT_TAG v2.1.0
  GIT_SHALLOW TRUE)
set(SDBUSCPP_BUILD_CODEGEN OFF CACHE BOOL "" FORCE)
set(SDBUSCPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SDBUSCPP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDBUSCPP_BUILD_SHARED OFF CACHE BOOL "" FORCE)
FetchContent_Declare(sol2
  GIT_REPOSITORY https://github.com/ThePhD/sol2.git
  GIT_TAG v3.3.0
  GIT_SHALLOW TRUE)
FetchContent_Declare(pocketfft
  GIT_REPOSITORY https://github.com/mreineck/pocketfft.git
  GIT_TAG 81d171a6d5562e3aaa2c73489b70f564c633ff81
  GIT_SHALLOW FALSE)
FetchContent_Declare(magic_enum
  GIT_REPOSITORY https://github.com/Neargye/magic_enum.git
  GIT_TAG v0.9.7
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(sdbus-cpp sol2 pocketfft magic_enum)

# PT: sol2 precisa dos headers do Lua 5.4 (PUC Lua permanece C — decisão arquitetural).
# EN: sol2 needs the Lua 5.4 headers (PUC Lua stays C — architectural decision).
# ES: sol2 necesita los headers de Lua 5.4 (PUC Lua permanece C — decisión arquitectónica).
# ZH: sol2 需要 Lua 5.4 的头文件（PUC Lua 保持为 C —— 架构决策）。
find_path(LUA_INCLUDE_DIR lua.hpp PATH_SUFFIXES lua5.4 lua)
find_library(LUA_LIBRARY NAMES lua5.4 lua-5.4 lua)
if(NOT LUA_INCLUDE_DIR OR NOT LUA_LIBRARY)
  message(FATAL_ERROR "Lua 5.4 not found (install liblua5.4-dev)")
endif()

# PT: pocketfft traz pocketfft_hdronly.h na raiz do repositório.
# EN: pocketfft ships pocketfft_hdronly.h at the repository root.
# ES: pocketfft trae pocketfft_hdronly.h en la raíz del repositorio.
# ZH: pocketfft 的 pocketfft_hdronly.h 位于仓库根目录。
# PT: Includes SYSTEM: warnings de terceiros não quebram o -Werror.
#     compat/webm: demux webm do kew (TU C único em decoder_impl.c — o
#     corpo compila ao ver MINIAUDIO_IMPLEMENTATION), mantido como spec
#     implementada; nestegg/miniaudio continuam vendored em include/.
# EN: SYSTEM includes: third-party warnings do not break -Werror.
#     compat/webm: kew's webm demuxer (single C TU in decoder_impl.c — the
#     body compiles on MINIAUDIO_IMPLEMENTATION), kept as the implemented
#     spec; nestegg/miniaudio stay vendored under include/.
# ES: includes SYSTEM: los warnings de terceros no rompen el -Werror.
#     compat/webm: demuxer webm del kew (TU C único), mantenido como spec.
# ZH: SYSTEM includes：第三方警告不会触发 -Werror。compat/webm：kew 的 webm
#     解封装器（decoder_impl.c 中唯一 C TU —— 看到 MINIAUDIO_IMPLEMENTATION
#     时编译本体），作为已实现的规范保留；nestegg/miniaudio 仍在 include/。
add_library(kiutube2_deps INTERFACE)
target_include_directories(kiutube2_deps SYSTEM INTERFACE
  ${LUA_INCLUDE_DIR}
  ${sol2_SOURCE_DIR}/include
  ${magic_enum_SOURCE_DIR}/include
  ${pocketfft_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/include/miniaudio)
target_link_libraries(kiutube2_deps INTERFACE Threads::Threads dl ${LUA_LIBRARY})

# PT: miniaudio no Linux fala ALSA; se não houver, cai em null backend (sem som).
# EN: miniaudio talks ALSA on Linux; without it, falls back to null (silent).
# ES: miniaudio habla ALSA en Linux; sin ella, cae en null backend (sin sonido).
# ZH: Linux 上 miniaudio 使用 ALSA；没有则回退到 null 后端（无声）。
find_library(ASOUND_LIBRARY asound)
if(ASOUND_LIBRARY)
  target_link_libraries(kiutube2_deps INTERFACE ${ASOUND_LIBRARY})
endif()

# PT: Demux/decodificação webm: nestegg (C) + opus/opusfile + vorbis.
# EN: Webm demux/decoding: nestegg (C) + opus/opusfile + vorbis.
# ES: Demux/decodificación webm: nestegg (C) + opus/opusfile + vorbis.
# ZH: webm 解封装/解码：nestegg（C）+ opus/opusfile + vorbis。
find_library(OPUSFILE_LIBRARY opusfile)
find_library(OPUS_LIBRARY opus)
find_library(VORBISFILE_LIBRARY vorbisfile)
find_library(VORBIS_LIBRARY vorbis)
find_library(OGG_LIBRARY ogg)
find_library(TAGLIB_LIBRARY tag)
find_library(CURL_LIBRARY curl)
# PT: headers de opus/opusfile em subdir (Debian: /usr/include/opus).
# EN: opus/opusfile headers live in a subdir (Debian: /usr/include/opus).
# ES: los headers de opus/opusfile están en un subdirectorio (Debian).
# ZH: opus/opusfile 头文件位于子目录（Debian：/usr/include/opus）。
find_path(OPUS_INCLUDE_DIR opusfile.h PATH_SUFFIXES opus)
find_path(VORBIS_INCLUDE_DIR vorbisfile.h PATH_SUFFIXES vorbis)
foreach(lib OPUSFILE_LIBRARY OPUS_LIBRARY VORBISFILE_LIBRARY VORBIS_LIBRARY OGG_LIBRARY TAGLIB_LIBRARY CURL_LIBRARY)
  if(${lib})
    target_link_libraries(kiutube2_deps INTERFACE ${${lib}})
  endif()
endforeach()
foreach(inc OPUS_INCLUDE_DIR VORBIS_INCLUDE_DIR)
  if(${inc})
    target_include_directories(kiutube2_deps SYSTEM INTERFACE ${${inc}})
  endif()
endforeach()

# PT: Binário M3a: AnyDecoder com fallback webm (nestegg + opus/vorbis).
# EN: M3a binary: AnyDecoder with webm fallback (nestegg + opus/vorbis).
# ES: Binario M3a: AnyDecoder con fallback webm (nestegg + opus/vorbis).
# ZH: M3a 二进制：带 webm 回退的 AnyDecoder（nestegg + opus/vorbis）。
add_executable(kiutube2
  player/main.cpp
  player/audio/decoder.cpp
  player/audio/engine.cpp
  player/audio/decoder_impl.c
  player/audio/webm_decoder.cpp
  player/krt/scheduler.cpp
  player/library/library.cpp
  player/net/http.cpp
  player/net/lrclib.cpp
  player/net/translate.cpp
  player/net/ytm.cpp
  player/script/lua_state.cpp
  player/sys/mpris.cpp
  player/tui/term.cpp
  player/tui/visualizer.cpp
  player/gui/viz_window.cpp
  player/library/cover.cpp
  player/library/canvas.cpp
  include/nestegg/nestegg.c
  ${pocketfft_SOURCE_DIR}/pocketfft.c)

# PT: QCustomPlot (vendored, GPL-3) entra apenas com Qt.
# EN: QCustomPlot (vendored, GPL-3) only joins with Qt.
# ES: QCustomPlot (vendored, GPL-3) solo entra con Qt.
# ZH: QCustomPlot（vendored，GPL-3）仅在启用 Qt 时加入。
if(Qt6Widgets_FOUND)
  target_sources(kiutube2 PRIVATE third_party/qcustomplot/qcustomplot.cpp)
  target_include_directories(kiutube2 PRIVATE third_party/qcustomplot)
  target_compile_definitions(kiutube2 PRIVATE KIUTUBE2_WITH_QT)
  target_link_libraries(kiutube2 PRIVATE Qt6::Widgets Qt6::PrintSupport)
  set_source_files_properties(third_party/qcustomplot/qcustomplot.cpp
    PROPERTIES COMPILE_FLAGS "-w")
endif()

# PT: sdbus-c++ presente (FetchContent) -> MPRIS real; senão, stub.
# EN: sdbus-c++ present (FetchContent) -> real MPRIS; otherwise, stub.
# ES: sdbus-c++ presente -> MPRIS real; si no, stub.
# ZH: sdbus-c++ 存在（FetchContent）-> 真实 MPRIS；否则 stub。
if(TARGET sdbus-c++)
  target_compile_definitions(kiutube2 PRIVATE KIUTUBE2_WITH_MPRIS)
  target_link_libraries(kiutube2 PRIVATE sdbus-c++)
  find_library(SYSTEMD_LIBRARY systemd)
  if(SYSTEMD_LIBRARY)
    target_link_libraries(kiutube2 PRIVATE ${SYSTEMD_LIBRARY})
  endif()
endif()

# PT: chafa presente -> capa TUI real; senão, stub. Includes SYSTEM: warnings
#     de terceiros não quebram o -Werror.
# EN: chafa present -> real TUI cover; otherwise, stub. SYSTEM includes:
#     third-party warnings do not break -Werror.
# ES: chafa presente -> carátula TUI real; si no, stub.
# ZH: chafa 存在 -> 真实 TUI 封面；否则 stub。SYSTEM includes：第三方警告
#     不会触发 -Werror。
if(CHafa_FOUND)
  target_compile_definitions(kiutube2 PRIVATE KIUTUBE2_WITH_CHAFA)
  target_link_libraries(kiutube2 PRIVATE PkgConfig::CHafa)
endif()
# PT: Código legado (webm.h do kew, nestegg): warnings suprimidos por não
#     serem nossos para polir; a fronteira C fica no shim.
# EN: Legacy code (kew's webm.h, nestegg): warnings suppressed — not ours to
#     polish; the C boundary lives in the shim.
# ES: Código legado (webm.h de kew, nestegg): warnings suprimidos — la
#     frontera C vive en el shim.
# ZH: 遗留代码（kew 的 webm.h、nestegg）：抑制警告 —— C 边界位于 shim。
set_source_files_properties(player/audio/webm_decoder.cpp player/audio/decoder_impl.c include/nestegg/nestegg.c
  PROPERTIES COMPILE_FLAGS "-w")
target_include_directories(kiutube2 PRIVATE player)
target_link_libraries(kiutube2 PRIVATE kiutube2_deps)

# PT: Smoke test do scheduler (gate de regressão de todos os marcos).
# EN: Scheduler smoke test (regression gate for every milestone).
# ES: Smoke test del scheduler (gate de regresión de todos los hitos).
# ZH: 调度器冒烟测试（每个里程碑的回归门槛）。
add_executable(scheduler_smoke player/tests/scheduler_smoke.cpp player/krt/scheduler.cpp)
target_include_directories(scheduler_smoke PRIVATE player)
target_link_libraries(scheduler_smoke PRIVATE kiutube2_deps)

# PT: Smoke test de settings/bindings (parse genérico + magic_enum + override).
# EN: Settings/bindings smoke test (generic parsing + magic_enum + override).
# ES: Smoke test de settings/bindings (parse genérico + magic_enum + override).
# ZH: settings/bindings 冒烟测试（通用解析 + magic_enum + 覆盖）。
add_executable(settings_smoke player/tests/settings_smoke.cpp)
target_include_directories(settings_smoke PRIVATE player)
target_link_libraries(settings_smoke PRIVATE kiutube2_deps)

# PT: Smoke test do decoder (WAV gerado em C++, reamostragem, seek, EOF).
# EN: Decoder smoke test (C++-generated WAV, resampling, seek, EOF).
# ES: Smoke test del decoder (WAV generado en C++, remuestreo, seek, EOF).
# ZH: 解码器冒烟测试（C++ 生成 WAV、重采样、seek、EOF）。
add_executable(audio_smoke player/tests/audio_smoke.cpp player/audio/decoder.cpp player/audio/decoder_impl.c include/nestegg/nestegg.c)
target_include_directories(audio_smoke PRIVATE player)
target_link_libraries(audio_smoke PRIVATE kiutube2_deps)

enable_testing()
add_test(NAME scheduler_smoke COMMAND scheduler_smoke)
add_test(NAME settings_smoke COMMAND settings_smoke)
add_test(NAME audio_smoke COMMAND audio_smoke)

# PT: Smoke test de library + fuzzy (OSA typesense, scan taglib).
# EN: Library + fuzzy smoke test (typesense OSA, taglib scan).
# ES: Smoke test de library + fuzzy (OSA de typesense, scan taglib).
# ZH: library + fuzzy 冒烟测试（typesense OSA、taglib 扫描）。
add_executable(library_smoke player/tests/library_smoke.cpp player/library/library.cpp)
target_include_directories(library_smoke PRIVATE player)
target_link_libraries(library_smoke PRIVATE kiutube2_deps)
add_test(NAME library_smoke COMMAND library_smoke)

# PT: Smoke test do visualizador (FFT 440Hz ±3%, paleta, SGR colorido).
# EN: Visualizer smoke test (440Hz FFT ±3%, palette, colored SGR).
# ES: Smoke test del visualizador (FFT 440Hz ±3%, paleta, SGR coloreado).
# ZH: 可视化器冒烟测试（440Hz FFT ±3%、调色板、彩色 SGR）。
add_executable(viz_smoke player/tests/viz_smoke.cpp player/tui/visualizer.cpp ${pocketfft_SOURCE_DIR}/pocketfft.c)
target_include_directories(viz_smoke PRIVATE player ${pocketfft_SOURCE_DIR})
target_link_libraries(viz_smoke PRIVATE kiutube2_deps)
add_test(NAME viz_smoke COMMAND viz_smoke)

# PT: Smoke test de capa (sidecar de thumb, negativos).
# EN: Cover smoke test (thumbnail sidecar, negatives).
# ES: Smoke test de carátula. / ZH: 封面冒烟测试（sidecar 缩略图、负路径）。
add_executable(cover_smoke player/tests/cover_smoke.cpp player/library/cover.cpp)
target_include_directories(cover_smoke PRIVATE player)
target_link_libraries(cover_smoke PRIVATE kiutube2_deps)
add_test(NAME cover_smoke COMMAND cover_smoke)

# PT: Smoke test do canvas chafa (BMP de 4 quadrantes, cores, stub).
# EN: chafa canvas smoke test (4-quadrant BMP, colors, stub).
# ES: Smoke test del canvas chafa. / ZH: chafa 画布冒烟测试（四象限 BMP）。
add_executable(canvas_smoke player/tests/canvas_smoke.cpp player/library/canvas.cpp)
target_include_directories(canvas_smoke PRIVATE player)
target_link_libraries(canvas_smoke PRIVATE kiutube2_deps)
if(CHafa_FOUND)
  target_compile_definitions(canvas_smoke PRIVATE KIUTUBE2_WITH_CHAFA)
  target_link_libraries(canvas_smoke PRIVATE PkgConfig::CHafa)
endif()
add_test(NAME canvas_smoke COMMAND canvas_smoke)
