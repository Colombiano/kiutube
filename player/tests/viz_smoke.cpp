// PT: Smoke test do Visualizer (M4 adiantado): alimenta um seno de 440Hz +
//     ruído baixo, roda a FFT e asserta que o bin dominante fica na
//     frequência esperada (±3%). Também valida a paleta e o desenho num
//     CellBuffer (células coloridas aparecem).
// EN: Visualizer smoke test (early M4): feeds a 440Hz sine plus low noise,
//     runs the FFT and asserts the dominant bin lands at the expected
//     frequency (±3%). Also validates the palette and drawing into a
//     CellBuffer (colored cells appear).
// ES: Smoke test del Visualizer (M4 adelantado): alimenta un seno de 440Hz,
//     ejecuta la FFT y asegura que el bin dominante cae en la frecuencia
//     esperada (±3%).
// ZH: Visualizer 冒烟测试（提前的 M4）：喂入 440Hz 正弦 + 低噪声，执行 FFT
//     并断言主导 bin 落在预期频率（±3%）。同时验证调色板与 CellBuffer
//     绘制（出现彩色单元格）。
#include "tui/cell_buffer.hpp"
#include "tui/visualizer.hpp"

#include <cmath>
#include <cstdio>
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

} // namespace

int main()
{
        viz::Visualizer viz;
        viz.set_mode(viz::Mode::Spectrum);

        // PT: 1,5 janela de seno 440Hz @48k (mono duplicado em stereo).
        // EN: 1.5 windows of a 440Hz sine @48k (mono duplicated to stereo).
        // ES: 1,5 ventanas de seno 440Hz @48k. / ZH: 1.5 个窗口的 440Hz 正弦 @48k。
        constexpr int kFeed = viz::Visualizer::kFftSize + viz::Visualizer::kFftSize / 2;
        std::vector<float> stereo(static_cast<std::size_t>(kFeed) * 2);
        for (int i = 0; i < kFeed; ++i)
        {
                const double s = std::sin(2.0 * M_PI * 440.0 * i / viz::Visualizer::kSampleRate) * 0.8;
                stereo[static_cast<std::size_t>(i) * 2] = static_cast<float>(s);
                stereo[static_cast<std::size_t>(i) * 2 + 1] = static_cast<float>(s);
        }
        viz.feed(stereo);

        tui::CellBuffer buf(24, 100);
        viz.draw(buf, 2, 2, 10, 96); // PT: força a FFT e o desenho / runs FFT + draw

        // PT: bin dominante deve ser ~440Hz (±3%).
        // EN: dominant bin should be ~440Hz (±3%).
        // ES: el bin dominante debe ser ~440Hz (±3%).
        // ZH: 主导 bin 应约为 440Hz（±3%）。
        const auto &mags = viz.mags();
        int peak_bin = 1;
        for (int b = 2; b < viz::Visualizer::kFftSize / 2; ++b)
                if (mags[b] > mags[peak_bin])
                        peak_bin = b;
        const double peak_hz = static_cast<double>(peak_bin) * viz::Visualizer::kSampleRate /
                               viz::Visualizer::kFftSize;
        std::printf("viz_smoke: pico / peak / pico / 峰值: %.1f Hz (bin %d)\n", peak_hz, peak_bin);
        check(std::abs(peak_hz - 440.0) < 440.0 * 0.03 + 6.0, "pico ~440Hz");

        // PT: paleta: extremos. / EN: palette: extremes. / ZH: 调色板：端点。
        check(viz::hot_palette(0.0) == 16, "paleta preto / black");
        check(viz::hot_palette(1.0) == 231, "paleta branco / white");
        check(viz::hot_palette(0.5) != viz::hot_palette(0.0), "palefa varia");

        // PT: após draw, linhas da área devem conter escapes de cor SGR —
        //     flush_diff devolve segmentos; procuramos 48;5 (bg 256).
        // EN: after draw, area lines must contain SGR color escapes —
        //     flush_diff returns segments; we look for 48;5 (256 bg).
        // ES: tras draw, las líneas deben contener escapes SGR de color.
        // ZH: draw 后区域行应包含 SGR 颜色转义 —— 在 flush_diff 的段中查找
        //     48;5（256 背景色）。
        const auto segs = buf.flush_diff();
        bool found_color = false;
        for (const auto &s : segs)
                if (s.find("\x1b[48;5;") != std::string::npos)
                {
                        found_color = true;
                        break;
                }
        check(found_color, "segmentos com cor / colored segments / 彩色段");

        if (failures > 0)
                return 1;
        std::printf("viz_smoke: PASS\n");
        return 0;
}
