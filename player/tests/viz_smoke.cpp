// PT: Smoke test do Visualizer hi-fi: alimenta seno 440Hz + ruído baixo,
//     roda a FFT e asserta o bin dominante (±1.2 bin com a janela Hann);
//     valida a LUT inferno (extremos/monotonia aproximada), a emissão de
//     SGR truecolor (COLORTERM=truecolor) e de paleta 256 com dither, e o
//     desenho em CellBuffer (meio-bloco presente).
// EN: Hi-fi Visualizer smoke test: feeds a 440Hz sine + low noise, runs the
//     FFT and asserts the dominant bin (±1.2 bins with the Hann window);
//     validates the inferno LUT, truecolor SGR emission (COLORTERM=truecolor)
//     and 256-palette dithered SGR, plus CellBuffer drawing (half-block).
// ES: Smoke test del Visualizer hi-fi: seno 440Hz, FFT, bin dominante,
//     LUT inferno, SGR truecolor y paleta 256 con dither, semibloque.
// ZH: 高保真 Visualizer 冒烟测试：喂入 440Hz 正弦 + 低噪声，执行 FFT 并断言
//     主导 bin（Hann 窗 ±1.2 bin）；验证 inferno LUT、truecolor SGR 输出
//     （COLORTERM=truecolor）、带抖动的 256 色 SGR 及半块字符绘制。
#include "tui/cell_buffer.hpp"
#include "tui/color.hpp"
#include "tui/visualizer.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace
{

int failures = 0;

void check(bool cond, const char *what)
{
        if (!cond)
        {
                std::fprintf(stderr, "viz_smoke: FAIL: %s\n", what);
                ++failures;
        }
}

bool segments_have(const std::vector<std::string> &segs, std::string_view needle)
{
        for (const auto &s : segs)
                if (s.find(needle) != std::string::npos)
                        return true;
        return false;
}

void run_fft_once(viz::Visualizer &viz, tui::CellBuffer &buf)
{
        constexpr int kFeed = viz::Visualizer::kFftSize + viz::Visualizer::kHop;
        std::vector<float> stereo(static_cast<std::size_t>(kFeed) * 2);
        for (int i = 0; i < kFeed; ++i)
        {
                const double s = std::sin(2.0 * M_PI * 440.0 * i / viz::Visualizer::kSampleRate) * 0.8;
                stereo[static_cast<std::size_t>(i) * 2] = static_cast<float>(s);
                stereo[static_cast<std::size_t>(i) * 2 + 1] = static_cast<float>(s);
        }
        viz.feed(stereo);
        viz.draw(buf, 2, 2, 10, 96);
}

} // namespace

int main()
{
        // PT: LUT inferno: extremos e crescimento do canal vermelho.
        // EN: inferno LUT: extremes and red-channel growth.
        // ES: LUT inferno: extremos y crecimiento del rojo. / ZH: inferno LUT：端点与红通道递增。
        const tui::Rgb dark = tui::inferno(0.0);
        const tui::Rgb mid = tui::inferno(0.5);
        const tui::Rgb bright = tui::inferno(1.0);
        check(dark.r <= 4 && dark.g <= 4, "inferno escuro / dark");
        check(bright.r >= 250 && bright.g >= 250, "inferno claro / bright");
        check(mid.r > dark.r && bright.r >= mid.r, "inferno monotono aprox / monotone-ish");

        // PT: truecolor detectado via COLORTERM e SGR 24-bit emitido.
        // EN: truecolor detected via COLORTERM and 24-bit SGR emitted.
        // ES: truecolor detectado vía COLORTERM y SGR 24-bit emitido.
        // ZH: 经 COLORTERM 检测 truecolor 并输出 24-bit SGR。
        setenv("COLORTERM", "truecolor", 1);
        {
                viz::Visualizer viz;
                viz.set_mode(viz::Mode::Spectrum);
                tui::CellBuffer buf(24, 100);
                run_fft_once(viz, buf);
                const auto segs = buf.flush_diff();
                check(tui::truecolor_enabled(), "truecolor detectado / detected");
                check(segments_have(segs, "\x1b[38;2;"), "SGR truecolor fg");
                check(segments_have(segs, "\x1b[48;2;"), "SGR truecolor bg");
        }
        unsetenv("COLORTERM");
        setenv("TERM", "xterm-256color", 1);
        {
                viz::Visualizer viz;
                viz.set_mode(viz::Mode::Spectrum);
                tui::CellBuffer buf(24, 100);
                run_fft_once(viz, buf);
                const auto segs = buf.flush_diff();
                check(!tui::truecolor_enabled(), "truecolor ausente / absent");
                check(segments_have(segs, "\x1b[48;5;"), "SGR paleta 256 / palette SGR");
        }

        // PT: pico do FFT em ~440Hz (±1.2 bin) em ambos os modos de desenho.
        // EN: FFT peak at ~440Hz (±1.2 bins) in both draw modes.
        // ES: pico del FFT en ~440Hz (±1.2 bins). / ZH: FFT 峰值在 ~440Hz（±1.2 bin）。
        const double binw = static_cast<double>(viz::Visualizer::kSampleRate) / viz::Visualizer::kFftSize;
        for (const auto mode : {viz::Mode::Spectrum, viz::Mode::Spectrogram})
        {
                viz::Visualizer viz;
                viz.set_mode(mode);
                tui::CellBuffer buf(24, 100);
                run_fft_once(viz, buf);
                const auto &mags = viz.mags();
                int peak_bin = 1;
                for (int b = 2; b < viz::Visualizer::kFftSize / 2; ++b)
                        if (mags[b] > mags[peak_bin])
                                peak_bin = b;
                const double peak_hz = static_cast<double>(peak_bin) * binw;
                std::printf("viz_smoke: modo / mode %d pico / peak / 峰值: %.1f Hz\n",
                            static_cast<int>(mode), peak_hz);
                check(std::abs(peak_hz - 440.0) <= std::max(1.2 * binw, 440.0 * 0.03),
                      "pico ~440Hz");
        }

        // PT: meio-bloco presente no spectrogram.
        // EN: half-block present in the spectrogram. / ZH: 频谱图含半块字符。
        {
                viz::Visualizer viz;
                viz.set_mode(viz::Mode::Spectrogram);
                tui::CellBuffer buf(24, 100);
                run_fft_once(viz, buf);
                const auto segs = buf.flush_diff();
                check(segments_have(segs, "\xE2\x96\x80"), "meio-bloco / half-block");
        }

        if (failures > 0)
                return 1;
        std::printf("viz_smoke: PASS\n");
        return 0;
}
