// PT: Buffer de células RGB (24-bit quando o terminal permite, com
//     quantização+dither pro cubo 256 caso contrário). Cada célula: char
//     UTF-8 + fg/bg opcionais (Color). flush_diff reescreve linhas
//     alteradas emitindo SGR só nas transições de cor.
// EN: RGB cell buffer (24-bit when the terminal allows, with quantization +
//     dither to the 256 cube otherwise). Each cell: UTF-8 char + optional
//     fg/bg (Color). flush_diff rewrites changed lines emitting SGR only on
//     color transitions.
// ES: Buffer de celdas RGB (24-bit si la terminal lo permite, con
//     cuantización+dither al cubo 256 si no). flush_diff reescribe líneas
//     emitiendo SGR solo en las transiciones de color.
// ZH: RGB 单元格缓冲（终端支持时用 24-bit，否则量化 + 抖动到 256 色立方）。
//     每格：UTF-8 字符 + 可选 fg/bg（Color）。flush_diff 重写变化的行，
//     仅在颜色过渡处输出 SGR。
#pragma once

#include "tui/color.hpp"

#include <algorithm>
#include <format>
#include <string>
#include <string_view>
#include <vector>

namespace tui
{

class CellBuffer
{
      public:
        CellBuffer(int rows, int cols) : rows_(rows), cols_(cols)
        {
                front_.assign(static_cast<std::size_t>(rows_), std::vector<Cell>(static_cast<std::size_t>(cols_)));
                back_.assign(static_cast<std::size_t>(rows_), std::vector<Cell>(static_cast<std::size_t>(cols_)));
        }

        [[nodiscard]] int rows() const
        {
                return rows_;
        }
        [[nodiscard]] int cols() const
        {
                return cols_;
        }

        void clear()
        {
                back_.assign(static_cast<std::size_t>(rows_), std::vector<Cell>(static_cast<std::size_t>(cols_)));
        }

        // PT: Escreve texto com cores herdadas (default do terminal).
        // EN: Writes text with inherited (terminal default) colors.
        // ES: Escribe texto con colores heredados. / ZH: 以继承色写入文本。
        void put(int row, int col, std::string_view text)
        {
                put(row, col, text, Color{}, Color{});
        }

        // PT: Escreve texto com cor de frente; clip por células UTF-8.
        // EN: Writes text with a foreground color; per-cell UTF-8 clipping.
        // ES: Escribe texto con color de frente. / ZH: 以前景色写入文本。
        void put(int row, int col, std::string_view text, Color fg)
        {
                put(row, col, text, fg, Color{});
        }

        void put(int row, int col, std::string_view text, Color fg, Color bg)
        {
                if (row < 0 || row >= rows_ || col < 0 || col >= cols_ || text.empty())
                        return;
                std::size_t x = static_cast<std::size_t>(col);
                const std::size_t limit = static_cast<std::size_t>(cols_);
                for (std::size_t i = 0; i < text.size() && x < limit;)
                {
                        const std::size_t n = utf8_len(text[i]);
                        Cell &c = back_[static_cast<std::size_t>(row)][x];
                        c.len = static_cast<std::uint8_t>(std::min(n, text.size() - i));
                        for (std::uint8_t k = 0; k < c.len; ++k)
                                c.ch[k] = text[i + k];
                        c.fg = fg;
                        c.bg = bg;
                        i += c.len;
                        ++x;
                }
        }

        // PT: Uma célula colorida de char único (ASCII) ou sequência UTF-8
        //     curta (ex.: meio-bloco ▀, espaço com bg).
        // EN: One colored cell: single char (ASCII) or short UTF-8 sequence
        //     (e.g. half-block ▀, bg space).
        // ES: Una celda coloreada de char único o secuencia UTF-8 corta.
        // ZH: 单个彩色单元格：单字符（ASCII）或短 UTF-8 序列（如半块 ▀、
        //     带背景色的空格）。
        void put_cell(int row, int col, std::string_view ch, Color fg, Color bg)
        {
                if (row < 0 || row >= rows_ || col < 0 || col >= cols_ || ch.empty())
                        return;
                Cell &c = back_[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)];
                c.len = static_cast<std::uint8_t>(std::min<std::size_t>(ch.size(), sizeof(c.ch)));
                for (std::uint8_t k = 0; k < c.len; ++k)
                        c.ch[k] = ch[k];
                c.fg = fg;
                c.bg = bg;
        }

        // PT: Troca front/back; devolve segmentos (posição + linha com SGR).
        // EN: Swaps front/back; returns segments (position + SGR line).
        // ES: Intercambia front/back; devuelve segmentos.
        // ZH: 交换 front/back；返回段（定位 + 带 SGR 的行）。
        [[nodiscard]] std::vector<std::string> flush_diff()
        {
                std::vector<std::string> segments;
                for (int r = 0; r < rows_; ++r)
                {
                        if (front_[r] == back_[r])
                                continue;
                        segments.push_back("\x1b[" + std::to_string(r + 1) + ";1H\x1b[2K");
                        segments.push_back(render_line(back_[r], r));
                        front_[r] = back_[r];
                }
                return segments;
        }

      private:
        struct Cell
        {
                char ch[8] = {' '};
                std::uint8_t len = 1;
                Color fg{};
                Color bg{};

                bool operator==(const Cell &o) const
                {
                        return len == o.len && fg.set == o.fg.set && bg.set == o.bg.set &&
                               (!fg.set || (fg.rgb.r == o.fg.rgb.r && fg.rgb.g == o.fg.rgb.g &&
                                            fg.rgb.b == o.fg.rgb.b)) &&
                               (!bg.set || (bg.rgb.r == o.bg.rgb.r && bg.rgb.g == o.bg.rgb.g &&
                                            bg.rgb.b == o.bg.rgb.b)) &&
                               std::equal(ch, ch + len, o.ch);
                }
        };

        static std::size_t utf8_len(char lead)
        {
                const auto b = static_cast<unsigned char>(lead);
                if (b < 0x80)
                        return 1;
                if ((b & 0xe0) == 0xc0)
                        return 2;
                if ((b & 0xf0) == 0xe0)
                        return 3;
                if ((b & 0xf8) == 0xf0)
                        return 4;
                return 1;
        }

        // PT: SGR de uma cor de frente/fundo: truecolor direto ou
        //     quantização com dither Bayer pela posição da célula.
        // EN: SGR for one fg/bg: direct truecolor or Bayer-dithered
        //     quantization by cell position.
        // ES: SGR de fg/bg: truecolor directo o cuantización con dither.
        // ZH: 单个 fg/bg 的 SGR：直接 truecolor，或按单元格位置做 Bayer
        //     抖动量化。
        [[nodiscard]] static std::string sgr(Color c, bool fg, int x, int y)
        {
                if (!c.set)
                        return {};
                if (truecolor_enabled())
                        return std::format("\x1b[{};2;{};{};{}m", fg ? 38 : 48, c.rgb.r, c.rgb.g, c.rgb.b);
                return std::format("\x1b[{};5;{}m", fg ? 38 : 48, static_cast<int>(dither256(c.rgb, x, y)));
        }

        // PT: Renderiza a linha com SGR só quando o estado de cor muda; o
        //     índice da linha entra no dither Bayer (y).
        // EN: Renders the line with SGR only on color-state changes; the row
        //     index feeds the Bayer dither (y).
        // ES: Renderiza la línea con SGR solo al cambiar el estado de color;
        //     el índice de línea entra en el dither Bayer (y).
        // ZH: 渲染行：仅在颜色状态变化时输出 SGR；行索引参与 Bayer 抖动（y）。
        [[nodiscard]] static std::string render_line(const std::vector<Cell> &line, int row)
        {
                std::string out;
                Color cur_fg{}, cur_bg{};
                for (std::size_t x = 0; x < line.size(); ++x)
                {
                        const Cell &c = line[x];
                        if (c.fg.set != cur_fg.set || c.bg.set != cur_bg.set ||
                            (c.fg.set && (c.fg.rgb.r != cur_fg.rgb.r || c.fg.rgb.g != cur_fg.rgb.g ||
                                          c.fg.rgb.b != cur_fg.rgb.b)) ||
                            (c.bg.set && (c.bg.rgb.r != cur_bg.rgb.r || c.bg.rgb.g != cur_bg.rgb.g ||
                                          c.bg.rgb.b != cur_bg.rgb.b)))
                        {
                                if (cur_fg.set || cur_bg.set)
                                        out += "\x1b[0m";
                                out += sgr(c.fg, true, static_cast<int>(x), row);
                                out += sgr(c.bg, false, static_cast<int>(x), row);
                                cur_fg = c.fg;
                                cur_bg = c.bg;
                        }
                        out.append(c.ch, c.len);
                }
                if (cur_fg.set || cur_bg.set)
                        out += "\x1b[0m";
                return out;
        }

        int rows_;
        int cols_;
        std::vector<std::vector<Cell>> front_;
        std::vector<std::vector<Cell>> back_;
};

} // namespace tui
