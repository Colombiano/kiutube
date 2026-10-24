// PT: Implementação dos visualizadores. FFT: pocketfft (plano rfft C,
//     buffer n+2 doubles interleaved re,im). Mapeamentos log-freq como nos
//     referenciais: spectrum x=log(20Hz..16kHz), y=dB[-70..0]; spectrogram
//     y=log-freq (grave embaixo), cor=magnitude na rampa hot.
// EN: Visualizer implementation. FFT: pocketfft (C rfft plan, n+2 interleaved
//     re,im doubles). Log-freq mappings as in the references: spectrum
//     x=log(20Hz..16kHz), y=dB[-70..0]; spectrogram y=log-freq (bass at the
//     bottom), color=magnitude on the hot ramp.
// ES: Implementación de los visualizadores. FFT: pocketfft (plan rfft C).
// ZH: 可视化器实现。FFT：pocketfft（C rfft plan，n+2 doubles 交错 re,im）。
//     对数频率映射参照示例图：spectrum x=log(20Hz..16kHz)，y=dB[-70..0]；
//     spectrogram y=对数频率（低频在底部），颜色=幅度映射到 hot 色带。
#include "tui/visualizer.hpp"

#include "krt/log.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

extern "C"
{
#include "pocketfft.h"
}

namespace viz
{

namespace
{

// PT: Faixa de dB exibida (mapping 0..1). / EN: Displayed dB range. / ZH: 显示的 dB 范围。
constexpr double kDbMin = -70.0;
constexpr double kDbMax = 0.0;

// PT: frequências do eixo log. / EN: log axis frequencies. / ZH: 对数轴频率范围。
constexpr double kFmin = 20.0;
constexpr double kFmax = 16000.0;

[[nodiscard]] double gain_to_db(double g)
{
        return 20.0 * std::log10(g + 1e-9);
}

[[nodiscard]] int db_to_height(double db, int rows)
{
        const double t = std::clamp((db - kDbMin) / (kDbMax - kDbMin), 0.0, 1.0);
        return static_cast<int>(t * rows);
}

// PT: coluna x (0..cols-1) -> frequência (log). / EN: column x -> frequency (log).
// ES: columna x -> frecuencia (log). / ZH: 列 x -> 频率（对数）。
[[nodiscard]] double x_to_freq(double x01)
{
        return kFmin * std::pow(kFmax / kFmin, x01);
}

// PT: bin da FFT para uma frequência. / EN: FFT bin for a frequency.
// ES: bin de la FFT para una frecuencia. / ZH: 频率对应的 FFT bin。
[[nodiscard]] int freq_to_bin(double f)
{
        return std::clamp(static_cast<int>(f * Visualizer::kFftSize / Visualizer::kSampleRate), 1,
                          Visualizer::kFftSize / 2);
}

} // namespace

std::uint8_t hot_palette(double gain01) noexcept
{
        static constexpr std::uint8_t kRamp[] = {
            16,  16,  16,  232, 233, 234, 53,  53,  54,  55,  90,  90,  96,  96,  124, 124,
            160, 160, 166, 166, 202, 202, 208, 208, 214, 214, 220, 220, 226, 226, 230, 231,
        };
        const auto idx = static_cast<std::size_t>(
            std::clamp(gain01, 0.0, 1.0) * (sizeof(kRamp) / sizeof(kRamp[0]) - 1));
        return kRamp[idx];
}

Visualizer::Visualizer()
    : plan_(make_rfft_plan(kFftSize)), samples_(), window_(kFftSize), fft_buf_(kFftSize + 2),
      mags_(kFftSize / 2 + 1), peaks_(256, 0.0)
{
        if (!plan_)
                throw std::runtime_error("pocketfft: make_rfft_plan falhou / failed / fallo / 失败");
        // PT: janela Hann normalizada (coerente ganho 0.5 soma).
        // EN: Hann window (coherent 0.5 sum gain).
        // ES: ventana Hann. / ZH: Hann 窗。
        for (int i = 0; i < kFftSize; ++i)
                window_[i] = 0.5 * (1.0 - std::cos(2.0 * M_PI * i / (kFftSize - 1)));
}

Visualizer::~Visualizer()
{
        if (plan_)
                destroy_rfft_plan(plan_);
}

void Visualizer::set_mode(Mode m)
{
        if (mode_ == m)
                return;
        mode_ = m;
        samples_.clear();
        hist_.clear();
        std::fill(peaks_.begin(), peaks_.end(), 0.0);
}

Mode Visualizer::mode() const
{
        return mode_;
}

void Visualizer::feed(std::span<const float> stereo_frames)
{
        const std::size_t n = stereo_frames.size() / 2;
        samples_.reserve(kFftSize * 2);
        for (std::size_t i = 0; i < n; ++i)
                samples_.push_back(
                    (static_cast<double>(stereo_frames[i * 2]) + stereo_frames[i * 2 + 1]) * 0.5);
        // PT: mantém no máximo 1,5 janela (a FFT consome kFftSize).
        // EN: keeps at most 1.5 windows (the FFT consumes kFftSize).
        // ES: mantiene como máximo 1.5 ventanas. / ZH: 最多保留 1.5 个窗口。
        const std::size_t keep = kFftSize + kFftSize / 2;
        if (samples_.size() > keep)
                samples_.erase(samples_.begin(), samples_.begin() + (samples_.size() - keep));
}

void Visualizer::run_fft()
{
        if (samples_.size() < static_cast<std::size_t>(kFftSize))
                return;
        for (int i = 0; i < kFftSize; ++i)
                fft_buf_[i] = samples_[i] * window_[i];
        rfft_forward(plan_, fft_buf_.data(), 1.0);
        // PT: saída interleaved: c[2k]=re(k), c[2k+1]=im(k), k=0..n/2 (n+2 doubles).
        // EN: interleaved output: c[2k]=re(k), c[2k+1]=im(k), k=0..n/2.
        // ES: salida interleaved. / ZH: 交错输出：c[2k]=re(k)，c[2k+1]=im(k)。
        const double norm = 2.0 / kFftSize; // PT: compensa metade da energia / EN: coherent gain / ZH: 相干增益补偿
        for (int k = 0; k <= kFftSize / 2; ++k)
        {
                const double re = fft_buf_[2 * k];
                const double im = fft_buf_[2 * k + 1];
                mags_[k] = std::sqrt(re * re + im * im) * norm;
        }
        // PT: consome metade da janela (overlap 50%) para o próximo quadro.
        // EN: consumes half the window (50% overlap) for the next frame.
        // ES: consume media ventana (overlap 50%). / ZH: 消费半个窗口（50% 重叠）。
        samples_.erase(samples_.begin(), samples_.begin() + kFftSize / 2);
}

void Visualizer::draw(tui::CellBuffer &buf, int row, int col, int rows, int cols)
{
        if (mode_ == Mode::Off || rows <= 0 || cols <= 0)
                return;
        run_fft();
        if (mode_ == Mode::Spectrum)
                draw_spectrum(buf, row, col, rows, cols);
        else
                draw_spectrogram(buf, row, col, rows, cols);
}

void Visualizer::draw_spectrum(tui::CellBuffer &buf, int row, int col, int rows, int cols)
{
        if (static_cast<int>(peaks_.size()) < cols)
                peaks_.resize(cols, 0.0);

        for (int x = 0; x < cols; ++x)
        {
                const double f0 = x_to_freq(static_cast<double>(x) / cols);
                const double f1 = x_to_freq(static_cast<double>(x + 1) / cols);
                // PT: banda: maior magnitude entre os bins do intervalo log.
                // EN: band: max magnitude across the log interval's bins.
                // ES: banda: máxima magnitud entre los bins del intervalo.
                // ZH: 频段：对数区间内各 bin 的最大幅度。
                double g = 0.0;
                for (int b = freq_to_bin(f0); b <= freq_to_bin(f1) && b <= kFftSize / 2; ++b)
                        g = std::max(g, mags_[b]);
                const int h = db_to_height(gain_to_db(g), rows);

                // PT: peak-hold com decaimento lento (fica verde no referencial,
                //     cap rosa). / EN: slow-decay peak-hold (green fill, pink cap).
                peaks_[x] = std::max(peaks_[x] - 0.15, static_cast<double>(h));
                if (h >= peaks_[x])
                        peaks_[x] = h;
                const int peak_y = static_cast<int>(peaks_[x]);

                for (int y = 0; y < rows; ++y)
                {
                        const int from_bottom = rows - y; // PT: y de cima para baixo / top-down y
                        if (from_bottom <= h)
                        {
                                const double t = static_cast<double>(from_bottom) / rows;
                                const std::uint8_t color = t > 0.85 ? 196 : (t > 0.6 ? 220 : 34);
                                buf.put_cell(row + y, col + x, ' ', tui::kColorDefault, color);
                        }
                        else if (from_bottom == peak_y + 1 && peak_y > h)
                                buf.put_cell(row + y, col + x, '^', 211, tui::kColorDefault);
                        else if (from_bottom == 1)
                                buf.put_cell(row + y, col + x, '_', 240, tui::kColorDefault);
                }
        }
}

void Visualizer::draw_spectrogram(tui::CellBuffer &buf, int row, int col, int rows, int cols)
{
        // PT: nova coluna a partir da FFT atual (uma por draw; ~20 col/s).
        // EN: new column from the current FFT (one per draw; ~20 cols/s).
        // ES: nueva columna desde la FFT actual. / ZH: 由当前 FFT 生成新列（每次
        //     draw 一列；约 20 列/秒）。
        std::vector<std::uint8_t> column(static_cast<std::size_t>(rows));
        for (int y = 0; y < rows; ++y)
        {
                // PT: y=0 (topo) = agudo; y=rows-1 (base) = grave (log).
                // EN: y=0 (top) = treble; y=rows-1 (bottom) = bass (log).
                // ES: y=0 (arriba) = agudo; y=rows-1 (abajo) = grave (log).
                // ZH: y=0（顶部）= 高频；y=rows-1（底部）= 低频（对数）。
                const double frac = 1.0 - static_cast<double>(y) / rows; // 0=base 1=topo
                const double f = x_to_freq(std::clamp(frac, 0.0, 1.0));
                const int b = freq_to_bin(f);
                const double g = mags_[b];
                const double t = std::clamp((gain_to_db(g) - kDbMin) / (kDbMax - kDbMin), 0.0, 1.0);
                column[static_cast<std::size_t>(y)] = hot_palette(t);
        }
        hist_.push_back(std::move(column));
        while (static_cast<int>(hist_.size()) > cols)
                hist_.pop_front();

        for (std::size_t x = 0; x < hist_.size(); ++x)
        {
                const int cx = col + static_cast<int>(x) - static_cast<int>(hist_.size()) + cols;
                if (cx < col)
                        continue; // PT: nunca pintar fora da área / never paint outside the area
                for (int y = 0; y < rows; ++y)
                        buf.put_cell(row + y, cx, ' ', tui::kColorDefault,
                                     hist_[x][static_cast<std::size_t>(y)]);
        }
}

const std::vector<double> &Visualizer::mags() const
{
        return mags_;
}

} // namespace viz
