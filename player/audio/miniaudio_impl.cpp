// PT: Único TU com MINIAUDIO_IMPLEMENTATION: as demais unidades (decoder,
//     engine, testes) só veem as declarações — linking sem duplicatas.
// EN: Sole TU with MINIAUDIO_IMPLEMENTATION: the other units (decoder,
//     engine, tests) only see declarations — no duplicate linking.
// ES: Único TU con MINIAUDIO_IMPLEMENTATION: las demás unidades (decoder,
//     engine, tests) solo ven declaraciones — sin duplicados al enlazar.
// ZH: 唯一的 MINIAUDIO_IMPLEMENTATION 编译单元：其余单元（decoder、engine、
//     测试）只看到声明 —— 链接无重复定义。
#define MA_NO_ENGINE
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>
