// PT: Smoke test do cover::Canvas (chafa + stb_image): gera um BMP de 4
//     quadrantes (verde/vermelho azul/amarelo), renderiza e afirma que o
//     canvas devolve células coloridas próximas das cores reais. Sem chafa
//     (KIUTUBE2_WITH_CHAFA ausente), afirma apenas o stub (render = false).
// EN: cover::Canvas smoke test (chafa + stb_image): generates a 4-quadrant
//     BMP (red/green top, blue/yellow bottom), renders and asserts cells
//     close to the real colors. Without chafa, asserts the stub behavior.
// ES: Smoke test de cover::Canvas: genera un BMP de 4 cuadrantes y verifica
//     las celdas coloreadas. Sin chafa, verifica el stub.
// ZH: cover::Canvas 冒烟测试：生成四象限 BMP 并断言彩色单元格。无 chafa 时
//     断言 stub 行为。
#include "library/canvas.hpp"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

namespace
{

// PT: BMP 24-bit little-endian de 2x2 pixels (4 quadrantes). Linhas de
//     baixo para cima, pixels em BGR.
// EN: 24-bit little-endian BMP, 2x2 pixels (4 quadrants). Bottom-up rows,
//     BGR pixels.
bool write_probe_bmp(const std::string &path)
{
        const int w = 2, h = 2;
        const int row_size = (w * 3 + 3) & ~3; // PT: linha alinhada a 4 bytes / 4-byte aligned row
        const std::uint32_t img_size = static_cast<std::uint32_t>(row_size * h);
        const std::uint32_t off = 54;
        std::ofstream f(path, std::ios::binary);
        if (!f)
                return false;
        const auto u16 = [&f](std::uint16_t v) {
                f.put(static_cast<char>(v & 0xff));
                f.put(static_cast<char>((v >> 8) & 0xff));
        };
        const auto u32 = [&f](std::uint32_t v) {
                for (int i = 0; i < 4; ++i)
                        f.put(static_cast<char>((v >> (8 * i)) & 0xff));
        };
        f.put('B');
        f.put('M');
        u32(off + img_size);
        u32(0);
        u32(off);
        u32(40);
        u32(w);
        u32(h);
        u16(1);
        u16(24);
        u32(0);
        u32(img_size);
        u32(2835);
        u32(2835);
        u32(0);
        u32(0);
        const auto px = [&f](std::uint8_t b, std::uint8_t g, std::uint8_t r) {
                f.put(static_cast<char>(b));
                f.put(static_cast<char>(g));
                f.put(static_cast<char>(r));
        };
        // PT: linha de baixo: azul | amarelo. / EN: bottom row: blue | yellow.
        px(255, 0, 0);
        px(0, 255, 255);
        for (int i = w * 3; i < row_size; ++i)
                f.put('\0');
        // PT: linha de cima: vermelho | verde. / EN: top row: red | green.
        px(0, 0, 255);
        px(0, 255, 0);
        for (int i = w * 3; i < row_size; ++i)
                f.put('\0');
        return static_cast<bool>(f);
}

// PT: Mede a distância máxima de uma cor a uma das cores alvo (quadrantes).
// EN: Max distance of a color to a set of target colors (quadrant probes).
// ZH: 颜色到目标色集合（象限采样）的最大距离。
int color_distance(const cover::CellPaint &c, int r, int g, int b)
{
        const int dr = static_cast<int>(c.r) - r;
        const int dg = static_cast<int>(c.g) - g;
        const int db = static_cast<int>(c.b) - b;
        return dr * dr + dg * dg + db * db;
}

} // namespace

int main()
{
        const std::string path = "/tmp/kiutube_canvas_smoke.bmp";
        if (!write_probe_bmp(path))
        {
                std::fprintf(stderr, "canvas_smoke: falha ao escrever BMP / failed to write BMP / 无法写入 BMP\n");
                return 1;
        }

        cover::Canvas cv(16, 8);
#ifdef KIUTUBE2_WITH_CHAFA
        if (!cv.render(path))
        {
                std::fprintf(stderr, "canvas_smoke: render falhou / render failed / 渲染失败\n");
                return 1;
        }
        if (cv.cols() != 16 || cv.rows() != 8)
        {
                std::fprintf(stderr, "canvas_smoke: geometria errada / wrong geometry / 几何错误\n");
                return 1;
        }

        // PT: Varre o canvas coletando as cores de frente dominantes por
        //     metade (topo = vermelho/verde, baixo = azul/amarelo) e afirma
        //     que cada metade contém uma cor próxima do esperado.
        // EN: Scans the canvas collecting dominant fg colors per half (top =
        //     red/green, bottom = blue/yellow), asserting each half holds a
        //     color close to the expected one.
        bool top_match = false, bottom_match = false;
        for (int y = 0; y < cv.rows(); ++y)
        {
                for (int x = 0; x < cv.cols(); ++x)
                {
                        const auto c = cv.at(x, y);
                        if (!c.fg_set || c.ch == 0x20)
                                continue;
                        const bool top = y < cv.rows() / 2;
                        // PT: tolerância folgada: chafa mistura cores ao
                        //     amostrar; o assert é "tem vermelhudo no topo e
                        //     azulado embaixo", não igualdade exata.
                        // EN: loose tolerance: chafa blends when sampling; the
                        //     assert is "reddish on top, bluish below".
                        // ZH: 宽松容差：chafa 采样会混合颜色；断言的是"上半偏红、
                        //     下半偏蓝"，不是精确相等。
                        if (top && (color_distance(c, 255, 0, 0) < 20000 || color_distance(c, 0, 200, 0) < 26000))
                                top_match = true;
                        if (!top && (color_distance(c, 0, 0, 255) < 20000 || color_distance(c, 220, 200, 0) < 30000))
                                bottom_match = true;
                }
        }
        if (!top_match || !bottom_match)
        {
                std::fprintf(stderr, "canvas_smoke: cores nao batem / colors mismatch / 颜色不匹配 (top=%d bottom=%d)\n",
                             static_cast<int>(top_match), static_cast<int>(bottom_match));
                return 1;
        }

        // PT: Negativo: arquivo inexistente tem de falhar. / EN: Negative:
        //     missing file must fail. / ZH: 负路径：文件不存在必须失败。
        if (cv.render("/tmp/kiutube_canvas_smoke_nope.bmp"))
        {
                std::fprintf(stderr, "canvas_smoke: negativo falhou / negative failed / 负路径失败\n");
                return 1;
        }
        std::printf("canvas_smoke: OK (chafa)\n");
#else
        if (cv.render(path))
        {
                std::fprintf(stderr, "canvas_smoke: stub renderizou / stub rendered / stub 渲染了\n");
                return 1;
        }
        std::printf("canvas_smoke: OK (stub)\n");
#endif
        return 0;
}
