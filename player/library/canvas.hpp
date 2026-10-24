// PT: Renderizador de capa em células de terminal sobre chafa (wrapper RAII):
//     decodifica a imagem com stb_image (vendored em include/), desenha num
//     canvas chafa em modo símbolo/truecolor e expõe cada célula como
//     codepoint + fg/bg RGB — o app plota no CellBuffer com put_cell. Sem
//     chafa no sistema, compila com stub de mesma API (render = false).
// EN: Terminal-cell album art renderer on chafa (RAII wrapper): decodes the
//     image with stb_image (vendored under include/), draws into a chafa
//     symbol-mode/truecolor canvas and exposes each cell as codepoint + RGB
//     fg/bg — the app plots into CellBuffer with put_cell. Without chafa on
//     the system it builds a same-API stub (render = false).
// ES: Renderizador de carátula en celdas de terminal sobre chafa: decodifica
//     con stb_image, dibuja en un canvas chafa y expone cada celda como
//     codepoint + fg/bg RGB. Sin chafa, compila con stub de misma API.
// ZH: 基于 chafa 的终端单元格封面渲染器：用 stb_image（vendored 于 include/）
//     解码，绘制到 chafa 符号模式/truecolor 画布，逐格暴露 codepoint + RGB
//     fg/bg，由应用用 put_cell 画入 CellBuffer。无 chafa 时编译同名 stub。
#pragma once

#include <memory>
#include <string>

namespace cover
{

// PT: Uma célula pintada: codepoint + cores RGB; fg/bg opcionais (chafa pode
//     deixar uma cor sem definição quando o símbolo a ignora).
// EN: One painted cell: codepoint + RGB colors; fg/bg optional (chafa may
//     leave a color unset when the symbol ignores it).
// ES: Una celda pintada: codepoint + colores RGB; fg/bg opcionales.
// ZH: 一格绘制结果：codepoint + RGB 颜色；fg/bg 可选（符号忽略时 chafa 不设色）。
struct CellPaint
{
        unsigned int ch = 0x20; // PT: codepoint (espaço por padrão) / codepoint (space default) / 码点（默认空格）
        unsigned char r = 0, g = 0, b = 0;
        unsigned char br = 0, bg = 0, bb = 0;
        bool fg_set = false;
        bool bg_set = false;
};

class Canvas
{
      public:
        Canvas(int cols, int rows);
        ~Canvas();
        Canvas(const Canvas &) = delete;
        Canvas &operator=(const Canvas &) = delete;

        // PT: Decodifica + renderiza o arquivo de imagem no tamanho do
        //     canvas. false = sem imagem/sem chafa (stub).
        // EN: Decodes + renders the image file at the canvas size.
        //     false = no image/no chafa (stub).
        // ZH: 解码并以画布尺寸渲染图像文件。false = 无图像/无 chafa（stub）。
        [[nodiscard]] bool render(const std::string &image_path);

        [[nodiscard]] int cols() const;
        [[nodiscard]] int rows() const;
        [[nodiscard]] CellPaint at(int x, int y) const;

      private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
};

} // namespace cover
