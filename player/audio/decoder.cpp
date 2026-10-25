// PT: Implementação do MaDecoder sobre miniaudio (declarações; a
//     IMPLEMENTATION vive só em engine.cpp).
// EN: MaDecoder implementation over miniaudio (declarations only; the
//     IMPLEMENTATION lives solely in engine.cpp).
// ES: Implementación de MaDecoder sobre miniaudio (declaraciones; la
//     IMPLEMENTATION vive solo en engine.cpp).
// ZH: 基于 miniaudio 的 MaDecoder 实现（仅声明；IMPLEMENTATION 只存在于
//     engine.cpp）。
#include "audio/decoder.hpp"

#include "krt/log.hpp"

#include <miniaudio.h>

#include <algorithm>
#include <cstring>
#include <filesystem>

namespace audio
{

struct MaDecoder::Impl
{
        ma_decoder dec{};
        ma_decoder_config cfg{};
        TrackInfo info;
        int out_rate = 48000; // PT: rate efetivo da saída reamostrada / EN: effective resampled output rate
        bool eof = false;     // ES: rate efectivo de la salida / ZH: 重采样输出的有效采样率
};

MaDecoder::MaDecoder() : impl_(std::make_unique<Impl>())
{
}

MaDecoder::~MaDecoder() = default;
MaDecoder::MaDecoder(MaDecoder &&) noexcept = default;
MaDecoder &MaDecoder::operator=(MaDecoder &&) noexcept = default;

bool MaDecoder::open(const std::string &path, int target_sample_rate)
{
        impl_->cfg = ma_decoder_config_init(ma_format_f32, 2, static_cast<ma_uint32>(target_sample_rate));
        if (ma_decoder_init_file(path.c_str(), &impl_->cfg, &impl_->dec) != MA_SUCCESS)
        {
                krt::log::warn("audio: open falhou / failed / fallo / 打开失败: {}", path);
                return false;
        }
        impl_->eof = false;
        impl_->out_rate = target_sample_rate;
        impl_->info.path = path;
        impl_->info.title = std::filesystem::path(path).filename().string();
        impl_->info.channels = 2;
        impl_->info.sample_rate = target_sample_rate;

        ma_uint64 frames = 0;
        if (ma_decoder_get_length_in_pcm_frames(&impl_->dec, &frames) == MA_SUCCESS && frames > 0)
                impl_->info.duration = std::chrono::microseconds(
                    static_cast<long long>(frames) * 1'000'000 / target_sample_rate);
        return true;
}

std::size_t MaDecoder::read(std::span<float> out)
{
        if (impl_->eof || out.empty())
                return 0;
        const std::size_t frames_wanted = out.size() / 2;
        ma_uint64 frames_read = 0;
        if (ma_decoder_read_pcm_frames(&impl_->dec, out.data(), frames_wanted, &frames_read) != MA_SUCCESS)
        {
                impl_->eof = true;
                return 0;
        }
        // PT: leitura PARCIAL não é EOF — o miniaudio devolve menos frames
        //     que o pedido no meio da faixa (granularidade do resampler/
        //     backend). Só 0 frames marca fim; marcar parcial como EOF
        //     cortava a faixa a ~1,5s ("acelerada e ruim").
        // EN: a PARTIAL read is NOT EOF — miniaudio returns fewer frames
        //     than requested mid-stream (resampler/backend granularity).
        //     Only 0 frames marks the end; treating a partial read as EOF
        //     cut tracks short at ~1.5s ("fast and bad sounding").
        // ZH: 部分读取不是 EOF —— miniaudio 会按重采样器/后端粒度在曲目
        //     中途返回少于请求的帧数；只有 0 帧才标记结束。
        if (frames_read == 0)
                impl_->eof = true;
        return static_cast<std::size_t>(frames_read);
}

bool MaDecoder::seek(std::chrono::microseconds at)
{
        const auto frames = static_cast<ma_uint64>(at.count()) * static_cast<ma_uint64>(impl_->out_rate) / 1'000'000;
        if (ma_decoder_seek_to_pcm_frame(&impl_->dec, frames) != MA_SUCCESS)
                return false;
        impl_->eof = false;
        return true;
}

TrackInfo MaDecoder::info() const
{
        return impl_->info;
}

bool MaDecoder::at_end() const
{
        return impl_->eof;
}

} // namespace audio
