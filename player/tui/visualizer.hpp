// PT: Visualizadores de áudio hi-fi: FFT Spectrum com bandas de 1/3 de
//     oitava (preenchimento verde + contorno de pico rosa, estilo fft.png)
//     e Spectrograma 2D scroll com meio-bloco (resolução vertical ×2), LUT
//     inferno exata do matplotlib, labels de frequência e colorbar de dB
//     (estilo espectrograma2d.png). FFT pocketfft 2048, Hann 75% overlap.
// EN: Hi-fi audio visualizers: FFT Spectrum with 1/3-octave bands (green
//     fill + pink peak outline, like fft.png) and a scrolling 2D
//     spectrogram with half-block rendering (2x vertical resolution), the
//     exact matplotlib inferno LUT, frequency labels and a dB colorbar
//     (like espectrograma2d.png). pocketfft 2048, Hann 75% overlap.
// ES: Visualizadores de audio hi-fi: FFT Spectrum con bandas de 1/3 de
//     octava y espectrograma 2D con semibloque, LUT inferno exacta.
// ZH: 高保真音频可视化器：1/3 倍频程频谱（绿色填充 + 粉色峰值轮廓，类似
//     fft.png）与半块渲染滚动 2D 频谱图（垂直分辨率 ×2）、matplotlib 精确
//     inferno LUT、频率标签与 dB 色条（类似 espectrograma2d.png）。
//     pocketfft 2048 点、Hann 窗 75% 重叠。
#pragma once

#include "tui/cell_buffer.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <span>
#include <vector>

struct rfft_plan_i; // PT: plano C do pocketfft / pocketfft C plan

namespace viz
{

enum class Mode : std::uint8_t
{
        Off,
        Spectrum,   // PT: barras FFT 1/3-oct + peak-hold / 1/3-oct FFT bars + peak-hold
        Spectrogram // PT: scroll 2D / scrolling 2D
};

class Visualizer
{
      public:
        static constexpr int kSampleRate = 48000;
        static constexpr int kFftSize = 2048;
        static constexpr int kHop = kFftSize / 4; // PT: overlap 75% / 75% overlap / 75% 重叠

        Visualizer();
        ~Visualizer();
        Visualizer(Visualizer &&) = delete;
        Visualizer &operator=(Visualizer &&) = delete;

        void set_mode(Mode m);
        [[nodiscard]] Mode mode() const;

        void feed(std::span<const float> stereo_frames);

        void draw(tui::CellBuffer &buf, int row, int col, int rows, int cols);

        // PT: Magnitudes da última FFT (testes: pico em 440Hz etc.).
        // EN: Last FFT magnitudes (tests: 440Hz peak etc.).
        // ZH: 最近一次 FFT 的各 bin 幅度（测试用）。
        [[nodiscard]] const std::vector<double> &mags() const;

      private:
        void run_fft();
        void draw_spectrum(tui::CellBuffer &, int row, int col, int rows, int cols);
        void draw_spectrogram(tui::CellBuffer &, int row, int col, int rows, int cols);

        Mode mode_ = Mode::Off;
        rfft_plan_i *plan_ = nullptr;
        std::vector<double> samples_; // PT: mono acumulado / mono accum
        std::vector<double> window_;  // PT: janela Hann / Hann window
        std::vector<double> fft_buf_; // PT: kFftSize+2 interleaved / interleaved
        std::vector<double> mags_;    // PT: kFftSize/2+1 / magnitudes
        // PT: histórico do spectrogram: colunas de pixels RGB (2*rows por coluna).
        // EN: spectrogram history: RGB pixel columns (2*rows per column).
        std::deque<std::vector<tui::Rgb>> hist_;
        std::vector<double> peaks_; // PT: peak-hold por banda do spectrum / per-band peak-hold
};

} // namespace viz
