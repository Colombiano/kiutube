# PT: CMake do kiutube2 (reescrita C++20). Convive com o Makefile antigo
#     (build do C) até o marco M5; ver docs/CPP20-REWRITE.md.
# EN: kiutube2 build (C++20 rewrite). Coexists with the legacy Makefile (C
#     build) until milestone M5; see docs/CPP20-REWRITE.md.
# ES: CMake de kiutube2 (reescritura C++20). Convive con el Makefile antiguo
#     (build de C) hasta el hito M5; ver docs/CPP20-REWRITE.md.
# ZH: kiutube2（C++20 重写）的 CMake。在 M5 之前与旧的 Makefile（C 构建）共存；
#     见 docs/CPP20-REWRITE.md。
cmake_minimum_required(VERSION 3.28)
project(kiutube2 VERSION 0.1.0 LANGUAGES CXX)

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
FetchContent_MakeAvailable(sol2 pocketfft)

# PT: sol2 precisa dos headers do Lua 5.4 (PUC Lua permanece C — decisão arquitetural).
# EN: sol2 needs the Lua 5.4 headers (PUC Lua stays C — architectural decision).
# ES: sol2 necesita los headers de Lua 5.4 (PUC Lua permanece C — decisión arquitectónica).
# ZH: sol2 需要 Lua 5.4 的头文件（PUC Lua 保持为 C —— 架构决策）。
find_path(LUA_INCLUDE_DIR lua.hpp PATH_SUFFIXES lua5.4 lua)
if(NOT LUA_INCLUDE_DIR)
  message(FATAL_ERROR "Lua 5.4 headers not found (install liblua5.4-dev)")
endif()

# PT: pocketfft traz pocketfft_hdronly.h na raiz do repositório.
# EN: pocketfft ships pocketfft_hdronly.h at the repository root.
# ES: pocketfft trae pocketfft_hdronly.h en la raíz del repositorio.
# ZH: pocketfft 的 pocketfft_hdronhdronly.h 位于仓库根目录。
add_library(kiutube2_deps INTERFACE)
target_include_directories(kiutube2_deps INTERFACE
  ${LUA_INCLUDE_DIR}
  ${pocketfft_SOURCE_DIR})
target_link_libraries(kiutube2_deps INTERFACE Threads::Threads dl)

# PT: Binário M0: abre terminal, renderiza 1 frame, ecoa teclas, sai.
# EN: M0 binary: opens the terminal, renders 1 frame, echoes keys, exits.
# ES: Binario M0: abre la terminal, renderiza 1 frame, tecla eco, sale.
# ZH: M0 二进制：打开终端，渲染 1 帧，回显按键，退出。
add_executable(kiutube2
  player/main.cpp
  player/krt/scheduler.cpp
  player/tui/term.cpp)
target_include_directories(kiutube2 PRIVATE player)
target_link_libraries(kiutube2 PRIVATE kiutube2_deps)

# PT: Smoke test do scheduler (gate de regressão de todos os marcos).
# EN: Scheduler smoke test (regression gate for every milestone).
# ES: Smoke test del scheduler (gate de regresión de todos los hitos).
# ZH: 调度器冒烟测试（每个里程碑的回归门槛）。
add_executable(scheduler_smoke player/tests/scheduler_smoke.cpp player/krt/scheduler.cpp)
target_include_directories(scheduler_smoke PRIVATE player)
target_link_libraries(scheduler_smoke PRIVATE kiutube2_deps)

enable_testing()
add_test(NAME scheduler_smoke COMMAND scheduler_smoke)
