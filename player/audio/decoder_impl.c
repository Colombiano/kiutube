/* PT: Único TU com as implementações, em C (como o kew compila): miniaudio
 *     (MINIAUDIO_IMPLEMENTATION) e ma_webm (webm.h compila o corpo ao ver o
 *     mesmo macro). C permite o goto do demux; C++ não. As demais unidades
 *     C++ só veem declarações.
 * EN: Sole TU with the implementations, in C (as kew compiles): miniaudio
 *     (MINIAUDIO_IMPLEMENTATION) and ma_webm (webm.h compiles its body when
 *     it sees the same macro). C allows the demux goto; C++ does not. The
 *     other C++ units only see declarations.
 * ES: Único TU con las implementaciones, en C (como kew compila): miniaudio
 *     (MINIAUDIO_IMPLEMENTATION) y ma_webm (webm.h compila su cuerpo al ver
 *     el mismo macro). C permite el goto del demux; C++ no. Las demás
 *     unidades C++ solo ven declaraciones.
 * ZH: 唯一的实现编译单元，使用 C（与 kew 相同）：miniaudio
 *     （MINIAUDIO_IMPLEMENTATION）与 ma_webm（webm.h 看到相同宏时编译本体）。
 *     C 允许 demux 中的 goto；C++ 不允许。其余 C++ 单元只看到声明。 */
#define MA_NO_ENGINE
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

/* PT: webm.h do kew (C): implementação ma_webm ativa pelo macro acima.
 * EN: kew's webm.h (C): ma_webm implementation enabled by the macro above.
 * ES: webm.h de kew (C): implementación ma_webm activada por el macro.
 * ZH: kew 的 webm.h（C）：由上述宏启用 ma_webm 实现。 */
#include "webm.h"
