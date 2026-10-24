// PT: Implementação hi-fi dos visualizadores. Spectrum: bandas 1/3-octava
//     (20Hz-16kHz), preenchimento verde + contorno de pico rosa que decai,
//     ticks de dB no eixo Y. Spectrogram: meio-bloco '▀' (fg=pixel de cima,
//     bg=de baixo), rampa inferno exata, eixo log-freq com labels e colorbar
//     de dB [-120..-30]. FFT pocketfft 2048, Hann, hop 512 (75%).
// EN: Hi-fi visualizer implementation. Spectrum: 1/3-octave bands (20Hz-
//     16kHz), green fill + decaying pink peak outline, dB y-ticks.
//     Spectrogram: half-block '▀' (fg=upper pixel, bg=lower), exact inferno
//     ramp, log-freq axis with labels and dB colorbar [-120..-30]. pocketfft
//     2048, Hann, hop 512 (75%).
// ES: Implementación hi-fi. Spectrum: bandas 1/3-octava, relleno verde +
//     contorno de pico rosa. Spectrogram: semibloque '▀', rampa inferno
//     exacta, eje log-freq con labels y colorbar de dB.
// ZH: 高保真实现。Spectrum：1/3 倍频程（20Hz-16kHz），绿色填充 + 衰减粉色
//     峰值轮廓，dB Y 轴刻度。Spectrogram：半块 '▀'（fg=上像素、bg=下像素）、
//     精确 inferno 色带、带标签的对数频率轴与 dB 色条 [-120..-30]。
//     pocketfft 2048 点、Hann 窗、hop 512（75% 重叠）。
#include "tui/visualizer.hpp"

#include "krt/log.hpp"
#include "tui/color.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <stdexcept>

extern "C"
{
#include "pocketfft.h"
}

namespace viz
{

namespace
{

// PT: dB por modo (faixas dos referenciais). / EN: dB ranges per mode.
// ES: dB por modo. / ZH: 各模式的 dB 范围。
constexpr double kSpecDbMin = -100.0, kSpecDbMax = -20.0;     // PT: spectrum (fft.png)
constexpr double kSpecGramDbMin = -120.0, kSpecGramDbMax = -30.0; // PT: spectrogram (matplotlib psd)

// PT: eixo de frequência agora mora na classe (compartilhado com a GUI).
// EN: the frequency axis now lives on the class (shared with the GUI).
// ES: el eje de frecuencia ahora vive en la clase (compartido con la GUI).
// ZH: 频率轴现位于类上（与 GUI 共享）。
constexpr double kFmin = Visualizer::kFreqMin;
constexpr double kFmax = Visualizer::kFreqMax;

[[nodiscard]] double gain_to_db(double g)
{
        return 20.0 * std::log10(g + 1e-9);
}

[[nodiscard]] double x_to_freq(double frac)
{
        return kFmin * std::pow(kFmax / kFmin, std::clamp(frac, 0.0, 1.0));
}

[[nodiscard]] int freq_to_bin(double f)
{
        return std::clamp(static_cast<int>(f * Visualizer::kFftSize / Visualizer::kSampleRate), 1,
                          Visualizer::kFftSize / 2);
}

[[nodiscard]] int db_to_rows(double db, double db_min, double db_max, int rows)
{
        const double t = std::clamp((db - db_min) / (db_max - db_min), 0.0, 1.0);
        return static_cast<int>(t * rows);
}

// PT: cor do preenchimento por altura (verde->amarelo->vermelho).
// EN: fill color by height (green->yellow->red).
// ES: color de relleno por altura. / ZH: 按高度的填充色（绿->黄->红）。
[[nodiscard]] tui::Rgb height_color(double t)
{
        if (t > 0.85)
                return {230, 40, 40};
        if (t > 0.6)
                return {240, 200, 40};
        return {40, 200, 70};
}

} // namespace

Visualizer::Visualizer()
    : plan_(make_rfft_plan(kFftSize)), samples_(), window_(kFftSize), fft_buf_(kFftSize + 2),
      mags_(kFftSize / 2 + 1), peaks_(kBandCount, 0.0)
{
        if (!plan_)
                throw std::runtime_error("pocketfft: make_rfft_plan falhou / failed / fallo / 失败");
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

double Visualizer::band_edge(int k)
{
        return kFreqMin * std::pow(2.0, (static_cast<double>(k) - 0.5) / 3.0);
}

void Visualizer::analyze()
{
        run_fft();
}

void Visualizer::feed(std::span<const float> stereo_frames)
{
        const std::size_t n = stereo_frames.size() / 2;
        samples_.reserve(kFftSize * 2);
        for (std::size_t i = 0; i < n; ++i)
                samples_.push_back(
                    (static_cast<double>(stereo_frames[i * 2]) + stereo_frames[i * 2 + 1]) * 0.5);
        const std::size_t keep = kFftSize + kHop;
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
        const double norm = 2.0 / kFftSize;
        for (int k = 0; k <= kFftSize / 2; ++k)
        {
                const double re = fft_buf_[2 * k];
                const double im = fft_buf_[2 * k + 1];
                mags_[k] = std::sqrt(re * re + im * im) * norm;
        }
        samples_.erase(samples_.begin(), samples_.begin() + kHop);
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
        const int label_w = 5;
        const int bar_col = col + label_w;
        const int bar_cols = cols - label_w;
        const tui::Rgb kLabel{150, 150, 150};
        const tui::Rgb kFloor{90, 90, 90};
        const tui::Rgb kPeak{255, 110, 150};

        // PT: ticks de dB no eixo Y (fft.png: -20 a -100).
        // EN: dB ticks on the Y axis (fft.png: -20 to -100).
        for (int tick : {-20, -40, -60, -80, -100})
        {
                const int y = row + rows - 1 - db_to_rows(tick, kSpecDbMin, kSpecDbMax, rows - 1);
                buf.put(y, col, std::format("{:>4}", tick), tui::Color{kLabel});
        }

        const int band_cols = std::max(1, bar_cols / kBandCount);
        for (int b = 0; b < kBandCount; ++b)
        {
                const int x0 = bar_col + b * band_cols;
                const int x1 = (b == kBandCount - 1) ? bar_col + bar_cols : x0 + band_cols;
                double g = 0.0;
                for (int bin = freq_to_bin(Visualizer::band_edge(b)); bin <= freq_to_bin(Visualizer::band_edge(b + 1)) && bin <= kFftSize / 2;
                     ++bin)
                        g = std::max(g, mags_[bin]);
                const int h = db_to_rows(gain_to_db(g), kSpecDbMin, kSpecDbMax, rows - 1);

                // PT: peak-hold por banda (altura em linhas), decaimento lento.
                // EN: per-band peak-hold (height in rows), slow decay.
                // ES: peak-hold por banda (altura en filas), decaimiento lento.
                // ZH: 每带峰值保持（以行为单位的高度），缓慢衰减。
                peaks_[b] = std::max(peaks_[b] - 0.25, static_cast<double>(h));
                const int peak_h = static_cast<int>(peaks_[b]);

                for (int x = x0; x < x1 && x < col + cols; ++x)
                {
                        for (int y = 0; y < rows - 1; ++y)
                        {
                                const int from_bottom = rows - 1 - y;
                                if (from_bottom <= h)
                                        buf.put_cell(row + y, x, " ", tui::Color{},
                                                     tui::Color{height_color(static_cast<double>(from_bottom) /
                                                                             (rows - 1))});
                                else if (from_bottom == peak_h && peak_h > h)
                                        buf.put_cell(row + y, x, "^", tui::Color{kPeak}, tui::Color{});
                        }
                        buf.put_cell(row + rows - 1, x, "_", tui::Color{kFloor}, tui::Color{});
                }
        }
}

void Visualizer::draw_spectrogram(tui::CellBuffer &buf, int row, int col, int rows, int cols)
{
        const int label_w = 5;
        const int cb_w = 8; // PT: colorbar + labels / colorbar + labels / 色条 + 标签
        const int plot_cols = cols - label_w - cb_w;
        if (plot_cols <= 0)
                return;

        // PT: nova coluna de pixels (2*rows por coluna) da FFT atual.
        // EN: new pixel column (2*rows per column) from the current FFT.
        // ES: nueva columna de píxeles desde la FFT actual.
        // ZH: 由当前 FFT 生成新像素列（每列 2*rows 个像素）。
        std::vector<tui::Rgb> column(static_cast<std::size_t>(rows) * 2);
        for (int py = 0; py < rows * 2; ++py)
        {
                const double frac = static_cast<double>(py) / (rows * 2); // 0=base / bottom
                const int b = freq_to_bin(x_to_freq(frac));
                const double t = std::clamp(
                    (gain_to_db(mags_[b]) - kSpecGramDbMin) / (kSpecGramDbMax - kSpecGramDbMin), 0.0, 1.0);
                column[static_cast<std::size_t>(py)] = tui::inferno(t);
        }
        hist_.push_back(std::move(column));
        while (static_cast<int>(hist_.size()) > plot_cols)
                hist_.pop_front();

        const tui::Rgb kLabel{150, 150, 150};
        // PT: labels de frequência à esquerda (como no matplotlib).
        // EN: frequency labels on the left (as in matplotlib).
        // ES: labels de frecuencia a la izquierda. / ZH: 左侧频率标签。
        for (const auto &[label, f] : {std::pair{"10k", 10000.0}, {"1k", 1000.0}, {"100", 100.0}})
        {
                const int py = static_cast<int>(std::log(f / kFmin) / std::log(kFmax / kFmin) * rows * 2);
                const int y = row + rows - 1 - (py / 2);
                buf.put(y, col, label, tui::Color{kLabel});
        }

        // PT: pinta o histórico: cada célula é um meio-bloco (fg=pixel de
        //     cima=agudo, bg=pixel de baixo=grave) — resolução vertical ×2.
        // EN: paints the history: each cell is a half-block (fg=upper=treble,
        //     bg=lower=bass) — 2x vertical resolution.
        // ES: pinta el histórico: cada celda es un semibloque (fg=agudo,
        //     bg=grave) — resolución vertical ×2.
        // ZH: 绘制历史：每格为半块（fg=上=高频，bg=下=低频）——垂直分辨率 ×2。
        for (std::size_t x = 0; x < hist_.size(); ++x)
        {
                const int cx = col + label_w + static_cast<int>(x);
                if (cx >= col + label_w + plot_cols)
                        break;
                for (int y = 0; y < rows; ++y)
                {
                        const auto &col_px = hist_[x];
                        const tui::Rgb upper = col_px[static_cast<std::size_t>(y) * 2 + 1];
                        const tui::Rgb lower = col_px[static_cast<std::size_t>(y) * 2];
                        buf.put_cell(row + y, cx, "\xE2\x96\x80" /* ▀ U+2580 */, tui::Color{upper},
                                     tui::Color{lower});
                }
        }

        // PT: colorbar à direita: rampa inferno [-30 topo .. -120 base] + ticks.
        // EN: colorbar on the right: inferno ramp [-30 top .. -120 bottom] + ticks.
        // ES: colorbar a la derecha: rampa inferno con ticks. / ZH: 右侧色条：
        //     inferno 渐变 [-30 顶部 .. -120 底部] + 刻度。
        const int cb_col = col + label_w + plot_cols + 1;
        for (int y = 0; y < rows; ++y)
        {
                const double t = 1.0 - static_cast<double>(y) / rows;
                buf.put_cell(row + y, cb_col, " ", tui::Color{}, tui::Color{tui::inferno(t)});
        }
        for (const int tick : {-30, -60, -90})
        {
                const int y = row + db_to_rows(tick, kSpecGramDbMin, kSpecGramDbMax, rows - 1);
                buf.put(y, cb_col + 2, std::to_string(tick), tui::Color{kLabel});
        }
}

const std::vector<double> &Visualizer::mags() const
{
        return mags_;
}

} // namespace viz
