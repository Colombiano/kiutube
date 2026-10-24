// PT: Visualizadores de áudio: FFT Spectrum (barras log-freq com peak-hold,
//     estilo fft.png) e Spectrograma 2D scroll (colunas de cor por
//     magnitude, estilo espectrograma2d.png). FFT via pocketfft (plano C
//     RAII); tap de amostras vem do engine (read_viz). Cores 256 do
//     CellBuffer: barras usam fg, spectrogram usa bg em espaços.
// EN: Audio visualizers: FFT Spectrum (log-freq bars with peak-hold, like
//     fft.png) and scrolling 2D spectrogram (magnitude-colored columns,
//     like espectrograma2d.png). FFT via pocketfft (RAII C plan); samples
//     come from the engine tap (read_viz). CellBuffer 256 colors: bars use
//     fg, the spectrogram uses bg on spaces.
// ES: Visualizadores de audio: FFT Spectrum (barras log-freq con peak-hold)
//     y espectrograma 2D scroll. FFT vía pocketfft (plan C RAII); las
//     muestras vienen del tap del engine (read_viz).
// ZH: 音频可视化器：FFT 频谱（对数频率柱状 + 峰值保持，类似 fft.png）与
//     滚动 2D 频谱图（按幅度着色的列，类似 espectrograma2d.png）。
//     FFT 经 pocketfft（RAII C plan）；采样来自引擎 tap（read_viz）。
//     CellBuffer 256 色：柱状图用前景色，频谱图用空格背景色。
#pragma once

#include "tui/cell_buffer.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <span>
#include <vector>

struct rfft_plan_i; // PT: plano C do pocketfft (pocketfft.h) / pocketfft C plan

namespace viz
{

enum class Mode : std::uint8_t
{
        Off,
        Spectrum,   // PT: barras FFT + peak-hold / EN: FFT bars + peak-hold / ZH: FFT 柱状 + 峰值保持
        Spectrogram // PT: scroll 2D / EN: scrolling 2D / ZH: 滚动 2D
};

// PT: Normaliza um ganho (0..1) para a rampa "quente" do espectrograma
//     (preto→roxo→vermelho→laranja→amarelo→branco, ~inferno) em 256 cores.
// EN: Maps a gain (0..1) to the spectrogram "hot" ramp (black→purple→red→
//     orange→yellow→white, ~inferno) in 256 colors.
// ES: Normaliza una ganancia (0..1) a la rampa "caliente" del espectrograma.
// ZH: 将增益（0..1）映射到频谱图 "热" 色带（黑→紫→红→橙→黄→白，类 inferno）
//     的 256 色。
[[nodiscard]] std::uint8_t hot_palette(double gain01) noexcept;

class Visualizer
{
      public:
        static constexpr int kSampleRate = 48000;
        static constexpr int kFftSize = 4096;

        Visualizer();
        ~Visualizer();
        Visualizer(Visualizer &&) = delete;
        Visualizer &operator=(Visualizer &&) = delete;

        void set_mode(Mode m);
        [[nodiscard]] Mode mode() const;

        // PT: Acumula frames stereo; mantém só a janela mais recente.
        // EN: Accumulates stereo frames; keeps only the newest window.
        // ES: Acumula frames stereo; mantiene solo la ventana más reciente.
        // ZH: 累积立体声帧；仅保留最新窗口。
        void feed(std::span<const float> stereo_frames);

        // PT: Roda FFT(s) pendentes e pinta a área [row..row+rows) ×
        //     [col..col+cols) no buffer. Chamada ~20x/s.
        // EN: Runs pending FFT(s) and paints the [row..row+rows) ×
        //     [col..col+cols) area. Called ~20/s.
        // ES: Ejecuta FFT(s) pendientes y pinta el área. Llamada ~20/s.
        // ZH: 执行待处理的 FFT 并绘制指定区域。约 20 次/秒调用。
        void draw(tui::CellBuffer &buf, int row, int col, int rows, int cols);

        // PT: Ganho dos bins da última FFT (testes: pico em 440Hz etc.).
        // EN: Last FFT bin magnitudes (tests: 440Hz peak etc.).
        // ES: Ganancias de los bins de la última FFT (pruebas).
        // ZH: 最近一次 FFT 的各 bin 幅度（测试用：440Hz 峰值等）。
        [[nodiscard]] const std::vector<double> &mags() const;

      private:
        void run_fft();                 // PT: janela → pocketfft → mags_ / EN: window → pocketfft → mags_
        void draw_spectrum(tui::CellBuffer &, int row, int col, int rows, int cols);
        void draw_spectrogram(tui::CellBuffer &, int row, int col, int rows, int cols);

        Mode mode_ = Mode::Off;
        rfft_plan_i *plan_ = nullptr;
        std::vector<double> samples_; // PT: mono acumulado (até kFftSize+kHop) / mono accum
        std::vector<double> window_;  // PT: janela Hann / EN: Hann window / ZH: Hann 窗
        std::vector<double> fft_buf_; // PT: kFftSize+2 (complex interleaved) / complex interleaved
        std::vector<double> mags_;    // PT: kFftSize/2+1 magnitudes / magnitudes
        std::deque<std::vector<std::uint8_t>> hist_; // PT: colunas do spectrogram (índice de cor)
        std::vector<double> peaks_;                  // PT: peak-hold por coluna do spectrum
};

} // namespace viz
