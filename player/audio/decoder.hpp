// PT: Decoder concept + adapter MaDecoder (miniaudio: wav/mp3/flac). Cada
//     adapter implementa o MESMO contrato estático — a engine dispõe em
//     compile-time, sem virtual. Adapters opus/vorbis/faad/nestegg chegam no
//     M3 (cache YTM). miniaudio só aparece no .cpp (pimpl no engine).
// EN: Decoder concept + MaDecoder adapter (miniaudio: wav/mp3/flac). Every
//     adapter implements the SAME static contract — the engine uses them at
//     compile time, no virtual. opus/vorbis/faad/nestegg adapters land in M3
//     (YTM cache). miniaudio only appears in the .cpp (pimpl in the engine).
// ES: Decoder concept + adapter MaDecoder (miniaudio: wav/mp3/flac). Cada
//     adapter implementa el MISMO contrato estático — la engine los dispone
//     en compile-time, sin virtual. Los adapters opus/vorbis/faad/nestegg
//     llegan en M3 (caché YTM). miniaudio solo aparece en el .cpp (pimpl en
//     la engine).
// ZH: Decoder concept + MaDecoder 适配器（miniaudio：wav/mp3/flac）。每个适配器
//     实现相同的静态契约 —— 引擎在编译期使用它们，无 virtual。opus/vorbis/
//     faad/nestegg 适配器在 M3（YTM 缓存）加入。miniaudio 只出现在 .cpp
//     （引擎内 pimpl）。
#pragma once

#include <chrono>
#include <concepts>
#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <utility>

namespace audio
{

// PT: Metadados mínimos de uma faixa. / EN: Minimal track metadata.
// ES: Metadatos mínimos de una pista. / ZH: 音轨的最小元数据。
struct TrackInfo
{
        std::string path;
        std::string title;                              // M2: nome do arquivo; M3: taglib
        std::chrono::microseconds duration{0};
        int channels = 2;
        int sample_rate = 48000;
};

// PT: O contrato estático de todo decoder: open(path, sample_rate_alvo),
//     read(out)->frames, seek(pos), info(), at_end(). nada de virtual.
// EN: The static contract for every decoder: open(path, target_rate),
//     read(out)->frames, seek(pos), info(), at_end(). No virtuals.
// ES: El contrato estático de todo decoder: open(path, sample_rate_objetivo),
//     read(out)->frames, seek(pos), info(), at_end(). Sin virtual.
// ZH: 所有解码器的静态契约：open(path, 目标采样率)、read(out)->frames、
//     seek(pos)、info()、at_end()。无 virtual。
template <typename D>
concept Decoder = std::movable<D> && requires(D d, std::span<float> out, std::chrono::microseconds at,
                                              const std::string &path, int rate) {
        { d.open(path, rate) } -> std::same_as<bool>;
        { d.read(out) } -> std::same_as<std::size_t>;
        { d.seek(at) } -> std::same_as<bool>;
        { d.info() } -> std::same_as<TrackInfo>;
        { d.at_end() } -> std::same_as<bool>;
};

// PT: Adapter sobre ma_decoder: decodifica E reamostra para o formato do
//     device (float stereo no sample rate alvo) numa chamada só.
// EN: Adapter over ma_decoder: decodes AND resamples to the device format
//     (float stereo at the target rate) in a single call.
// ES: Adapter sobre ma_decoder: decodifica Y remuestrea al formato del device
//     (float stereo en el sample rate objetivo) en una sola llamada.
// ZH: 基于 ma_decoder 的适配器：一次调用即完成解码并重采样到设备格式
//     （目标采样率的 float 立体声）。
class MaDecoder
{
      public:
        MaDecoder();
        ~MaDecoder();
        MaDecoder(MaDecoder &&) noexcept;
        MaDecoder &operator=(MaDecoder &&) noexcept;
        MaDecoder(const MaDecoder &) = delete;
        MaDecoder &operator=(const MaDecoder &) = delete;

        // PT: Abre e configura reamostragem. / EN: Opens and configures resampling.
        // ES: Abre y configura remuestreo. / ZH: 打开并配置重采样。
        [[nodiscard]] bool open(const std::string &path, int target_sample_rate);
        // PT: Lê frames interleaved (out.size()/channels). / EN: Reads interleaved frames.
        // ES: Lee frames interleaved (out.size()/canales). / ZH: 读取交错帧（out.size()/声道数）。
        [[nodiscard]] std::size_t read(std::span<float> out);
        [[nodiscard]] bool seek(std::chrono::microseconds at);
        [[nodiscard]] TrackInfo info() const;
        [[nodiscard]] bool at_end() const;

      private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
};

static_assert(Decoder<MaDecoder>, "MaDecoder must satisfy the Decoder concept");

} // namespace audio
