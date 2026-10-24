// PT: Tradução automática de letras (port síncrono de src/ytm/translate.cpp):
//     Google gtx (não-oficial) → MyMemory (fallback), chunkado por linhas
//     completas ≤400 chars (timestamps LRC preservados porque viajam nas
//     linhas). Parser JSON próprio — sem dependência. SÍNCRONO: usar via
//     krt::offload.
// EN: Automatic lyric translation (synchronous port of src/ytm/translate.cpp):
//     Google gtx (unofficial) -> MyMemory (fallback), chunked by complete
//     lines <=400 chars (LRC timestamps survive because they ride the
//     lines). Hand-rolled JSON parsing — no dependency. SYNCHRONOUS: use
//     through krt::offload.
// ES: Traducción automática de letras (port síncrono de
//     src/ytm/translate.cpp): Google gtx -> MyMemory (fallback), chunked
//     por líneas completas <=400 chars. Parser JSON propio. SÍNCRONO.
// ZH: 歌词自动翻译（src/ytm/translate.cpp 的同步移植）：Google gtx（非官方）
//     → MyMemory（回退），按完整行分块 ≤400 字符（LRC 时间戳随行保留）。
//     手写 JSON 解析 —— 无依赖。同步：须经 krt::offload 使用。
#pragma once

#include <optional>
#include <string>

namespace net
{

// PT: Traduz `text` para `target_lang` (ISO 639-1, ex. "pt"). Cadeia
//     Google -> MyMemory; falha total se um chunk falhar em ambos.
// EN: Translates `text` to `target_lang` (ISO 639-1, e.g. "pt"). Google ->
//     MyMemory chain; total failure if a chunk fails on both.
// ES: Traduce `text` a `target_lang`. Cadena Google -> MyMemory.
// ZH: 将 `text` 翻译为 `target_lang`（ISO 639-1，如 "pt"）。Google ->
//     MyMemory 链；某块在两者都失败则整体失败。
[[nodiscard]] std::optional<std::string> translate_to(const std::string &text, const std::string &target_lang);

} // namespace net
