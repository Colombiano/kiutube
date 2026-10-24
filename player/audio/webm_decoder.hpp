// PT: Decoder de WebM/Opus (e WebM/Vorbis) adaptando a implementação C
//     battle-tested do kew (include/compat/webm/webm.h, miniaudio-compatible) para o
//     nosso concept Decoder. Wrapper RAII: nada de lógica de demux aqui.
// EN: WebM/Opus (and WebM/Vorbis) decoder adapting kew's battle-tested C
//     implementation (include/compat/webm/webm.h, miniaudio-compatible) to our Decoder
//     concept. RAII wrapper: no demux logic lives here.
// ES: Decoder de WebM/Opus (y WebM/Vorbis) adaptando la implementación C
//     probada de kew (include/compat/webm/webm.h, compatible con miniaudio) a nuestro
//     concept Decoder. Wrapper RAII: sin lógica de demux aquí.
// ZH: WebM/Opus（以及 WebM/Vorbis）解码器：将 kew 经过实战检验的 C 实现
//     （include/compat/webm/webm.h，兼容 miniaudio）适配到我们的 Decoder concept。
//     RAII 包装：此处不包含任何 demux 逻辑。
#pragma once

#include "audio/decoder.hpp"

#include <chrono>
#include <cstddef>
#include <memory>
#include <span>
#include <string>

// PT: ma_webm vem do header C do kew — incluído só no .cpp (shim). Aqui só
//     usamos pimpl próprio; nada de declarar o tipo C no header.
// EN: ma_webm comes from kew's C header — included only in the .cpp (shim).
//     Only our own pimpl lives here; no C type declared in this header.
// ES: ma_webm viene del header C de kew — incluido solo en el .cpp (shim).
//     Aquí solo vive nuestro pimpl; sin tipos C en el header.
// ZH: ma_webm 来自 kew 的 C 头文件 —— 仅在 .cpp（shim）中引入。此处只有我们
//     自己的 pimpl；头文件中不声明任何 C 类型。

namespace audio
{

class WebmDecoder
{
      public:
        WebmDecoder();
        ~WebmDecoder();
        WebmDecoder(WebmDecoder &&) noexcept;
        WebmDecoder &operator=(WebmDecoder &&) noexcept;
        WebmDecoder(const WebmDecoder &) = delete;
        WebmDecoder &operator=(const WebmDecoder &) = delete;

        [[nodiscard]] bool open(const std::string &path, int target_sample_rate);
        [[nodiscard]] std::size_t read(std::span<float> out);
        [[nodiscard]] bool seek(std::chrono::microseconds at);
        [[nodiscard]] TrackInfo info() const;
        [[nodiscard]] bool at_end() const;

      private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
};

} // namespace audio
