# PT: CMake do kiutube2 (reescrita C++20). Convive com o Makefile antigo
#     (build do C) até o marco M5; ver docs/CPP20-REWRITE.md.
# EN: kiutube2 build (C++20 rewrite). Coexists with the legacy Makefile (C
#     build) until milestone M5; see docs/CPP20-REWRITE.md.
# ES: CMake de kiutube2 (reescritura C++20). Convive con el Makefile antiguo
#     (build de C) hasta el hito M5; ver docs/CPP20-REWRITE.md.
# ZH: kiutube2（C++20 重写）的 CMake。在 M5 之前与旧的 Makefile（C 构建）共存；
#     见 docs/CPP20-REWRITE.md。
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

# PT: Dependências header-only via FetchContent (pinadas; trocar GIT_TAG por hash
#     de commit após o primeiro configure — ver docs/CPP20-REWRITE.md).
# EN: Header-only dependencies via FetchContent (pinned; swap GIT_TAG for a commit
#     hash after the first configure — see docs/CPP20-REWRITE.md).
# ES: Dependencias header-only vía FetchContent (pinadas; cambiar GIT_TAG por hash
#     de commit tras el primer configure — ver docs/CPP20-REWRITE.md).
# ZH: 通过 FetchContent 引入 header-only 依赖（固定版本；首次 configure 后应将
#     GIT_TAG 换成 commit 哈希 —— 见 docs/CPP20-REWRITE.md）。
include(FetchContent)
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
FetchContent_MakeAvailable(sol2 pocketfft magic_enum)

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
# PT: includes SYSTEM: warnings de terceiros não quebram o -Werror.
# EN: SYSTEM includes: third-party warnings do not break -Werror.
# ES: includes SYSTEM: los warnings de terceros no rompen el -Werror.
# ZH: SYSTEM includes：第三方警告不会触发 -Werror。
add_library(kiutube2_deps INTERFACE)
target_include_directories(kiutube2_deps SYSTEM INTERFACE
  ${LUA_INCLUDE_DIR}
  ${sol2_SOURCE_DIR}/include
  ${magic_enum_SOURCE_DIR}/include
  ${pocketfft_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/include/miniaudio
  ${CMAKE_SOURCE_DIR}/src/sound)
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
  player/script/lua_state.cpp
  player/tui/term.cpp
  include/nestegg/nestegg.c)
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
