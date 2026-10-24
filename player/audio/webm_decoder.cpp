// PT: Shim sobre include/compat/webm/webm.h (C, GPL-2 do kew): a única unidade que
//     inclui o header legado. Compilada com -w (CMake): não é nosso código
//     para polir, é infraestrutura encapsulada — a fronteira C fica aqui.
// EN: Shim over include/compat/webm/webm.h (C, kew's GPL-2): the only unit including
//     the legacy header. Compiled with -w (CMake): not our code to polish,
//     it is encapsulated infrastructure — the C boundary lives here.
// ES: Shim sobre include/compat/webm/webm.h (C, GPL-2 de kew): la única unidad que
//     incluye el header legado. Compilada con -w (CMake): no es nuestro
//     código para pulir — la frontera C vive aquí.
// ZH: 基于 include/compat/webm/webm.h（C，kew 的 GPL-2）的 shim：唯一引入该遗留头文件
//     的编译单元。以 -w 编译（CMake）：不是我们打磨的代码，而是被封装的
//     基础设施 —— C 边界就在此处。
#include "audio/webm_decoder.hpp"

#include "krt/log.hpp"

#include <filesystem>

// PT: webm.h exige miniaudio neste TU e traz a implementação ma_webm inline.
// EN: webm.h requires miniaudio in this TU and brings the ma_webm
//     implementation inline.
// ES: webm.h requiere miniaudio en este TU y trae la implementación ma_webm
//     inline.
// ZH: webm.h 要求本 TU 引入 miniaudio，并内联携带 ma_webm 实现。
#include "miniaudio.h"
#include "compat/webm/webm.h"

namespace audio
{

struct WebmDecoder::Impl
{
        ma_webm webm{};
        TrackInfo info;
        bool eof = false;
};

WebmDecoder::WebmDecoder() : impl_(std::make_unique<Impl>())
{
}

WebmDecoder::~WebmDecoder()
{
        if (impl_)
                ma_webm_uninit(&impl_->webm, nullptr);
}

WebmDecoder::WebmDecoder(WebmDecoder &&other) noexcept = default;
WebmDecoder &WebmDecoder::operator=(WebmDecoder &&other) noexcept = default;

bool WebmDecoder::open(const std::string &path, int target_sample_rate)
{
        // PT: backend config do kew: só preferredFormat (f32). O opus sai
        //     sempre a 48kHz; se o device pedir outro rate, M3 falha aqui
        //     (reamostragem do shim fica para o M4 — TODO).
        // EN: kew's backend config: preferredFormat only (f32). Opus always
        //     decodes at 48kHz; if the device asks another rate, M3 fails
        //     here (shim resampling lands in M4 — TODO).
        // ES: backend config de kew: solo preferredFormat (f32). El opus
        //     siempre sale a 48kHz; si el device pide otro rate, M3 falla
        //     aquí (remuestreo del shim va al M4 — TODO).
        // ZH: kew 的 backend config：仅 preferredFormat（f32）。opus 始终以
        //     48kHz 解码；若设备要求其他采样率，M3 在此失败（shim 重采样留到
        //     M4 —— TODO）。
        ma_decoding_backend_config cfg = ma_decoding_backend_config_init(ma_format_f32, 0);
        if (ma_webm_init_file(path.c_str(), &cfg, nullptr, &impl_->webm) != MA_SUCCESS)
        {
                krt::log::warn("audio: webm open falhou / failed / fallo / 打开失败: {}", path);
                return false;
        }

        ma_format fmt{};
        ma_uint32 ch = 0, rate = 0;
        if (ma_webm_get_data_format(&impl_->webm, &fmt, &ch, &rate, nullptr, 0) != MA_SUCCESS || fmt != ma_format_f32 ||
            static_cast<int>(rate) != target_sample_rate || ch != 2)
        {
                krt::log::warn("audio: webm fmt inesperado / unexpected format / formato inesperado / 格式不符: "
                               "{}ch {}Hz (esperado / expected / esperado / 期望: 2ch {}Hz)",
                               ch, rate, target_sample_rate);
                ma_webm_uninit(&impl_->webm, nullptr);
                return false;
        }

        impl_->eof = false;
        impl_->info.path = path;
        impl_->info.title = std::filesystem::path(path).filename().string();
        impl_->info.channels = 2;
        impl_->info.sample_rate = target_sample_rate;

        ma_uint64 length = 0;
        if (ma_webm_get_length_in_pcm_frames(&impl_->webm, &length) == MA_SUCCESS && length > 0)
                impl_->info.duration =
                    std::chrono::microseconds(static_cast<long long>(length) * 1'000'000 / target_sample_rate);
        return true;
}

std::size_t WebmDecoder::read(std::span<float> out)
{
        if (impl_->eof || out.empty())
                return 0;
        const std::size_t frames_wanted = out.size() / 2;
        ma_uint64 frames_read = 0;
        if (ma_webm_read_pcm_frames(&impl_->webm, out.data(), frames_wanted, &frames_read) != MA_SUCCESS)
        {
                impl_->eof = true;
                return 0;
        }
        if (frames_read < frames_wanted)
                impl_->eof = true;
        return static_cast<std::size_t>(frames_read);
}

bool WebmDecoder::seek(std::chrono::microseconds at)
{
        const auto frames = static_cast<ma_uint64>(at.count()) * impl_->info.sample_rate / 1'000'000;
        if (ma_webm_seek_to_pcm_frame(&impl_->webm, frames) != MA_SUCCESS)
                return false;
        impl_->eof = false;
        return true;
}

TrackInfo WebmDecoder::info() const
{
        return impl_->info;
}

bool WebmDecoder::at_end() const
{
        return impl_->eof;
}

} // namespace audio
