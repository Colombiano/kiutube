// PT: Buffer de células com COR (256 cores): cada célula tem char UTF-8 +
//     fg/bg (255 = default). flush_diff reescreve linhas alteradas emitindo
//     SGR nas transições de cor. Visualizadores (spectrum/spectrogram)
//     usam bg colorido em espaços para blocos sólidos.
// EN: Cell buffer with COLOR (256 colors): each cell has a UTF-8 char +
//     fg/bg (255 = default). flush_diff rewrites changed lines emitting SGR
//     on color transitions. Visualizers (spectrum/spectrogram) use colored
//     backgrounds on spaces for solid blocks.
// ES: Buffer de celdas con COLOR (256 colores): cada celda tiene char UTF-8
//     + fg/bg (255 = default). flush_diff reescribe líneas cambiadas
//     emitiendo SGR en las transiciones de color.
// ZH: 带颜色的单元格缓冲（256 色）：每格为 UTF-8 字符 + fg/bg（255 = 默认）。
// flush_diff 重写变化的行，在颜色过渡处输出 SGR。可视化器
//（spectrum/spectrogram）用带背景色的空格渲染纯色块。
#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace tui
{

inline constexpr std::uint8_t kColorDefault = 255; // PT: cor herdada do terminal / EN: inherited terminal color / ZH: 继承终端颜色

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

        // PT: Escreve texto com as cores default (herdadas).
        // EN: Writes text with default (inherited) colors.
        // ES: Escribe texto con los colores default (heredados).
        // ZH: 以默认（继承）颜色写入文本。
        void put(int row, int col, std::string_view text)
        {
                put(row, col, text, kColorDefault, kColorDefault);
        }

        // PT: Escreve texto com cores; clip por bytes (UTF-8 pode cortar na
        //     borda — wcwidth fica para o layout M4).
        // EN: Writes text with colors; byte clipping (UTF-8 may split at the
        //     edge — wcwidth lands with the M4 layout).
        // ES: Escribe texto con colores; clip por bytes.
        // ZH: 带颜色写入文本；按字节裁剪（UTF-8 可能在边界截断 —— wcwidth
        //     随 M4 布局加入）。
        void put(int row, int col, std::string_view text, std::uint8_t fg, std::uint8_t bg)
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

        // PT: Uma célula colorida (ex.: espaço com bg para o spectrogram).
        // EN: One colored cell (e.g. space with bg for the spectrogram).
        // ES: Una celda coloreada (p. ej. espacio con bg para el spectrogram).
        // ZH: 单个彩色单元格（如 spectrogram 中带背景色的空格）。
        void put_cell(int row, int col, char ch, std::uint8_t fg, std::uint8_t bg)
        {
                if (row < 0 || row >= rows_ || col < 0 || col >= cols_)
                        return;
                Cell &c = back_[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)];
                c.len = 1;
                c.ch[0] = ch;
                c.fg = fg;
                c.bg = bg;
        }

        // PT: Troca front/back e devolve segmentos (escape de posição + linha
        //     com SGR) prontos para um único writev.
        // EN: Swaps front/back and returns segments (position escape + line
        //     with SGR) ready for a single writev.
        // ES: Intercambia front/back y devuelve segmentos (escape de posición
        //     + línea con SGR) listos para un único writev.
        // ZH: 交换 front/back 并返回段（定位转义 + 带 SGR 的行），可直接
        //     用于单次 writev。
        [[nodiscard]] std::vector<std::string> flush_diff()
        {
                std::vector<std::string> segments;
                for (int r = 0; r < rows_; ++r)
                {
                        if (front_[r] == back_[r])
                                continue;
                        segments.push_back("\x1b[" + std::to_string(r + 1) + ";1H\x1b[2K");
                        segments.push_back(render_line(back_[r]));
                        front_[r] = back_[r];
                }
                return segments;
        }

      private:
        struct Cell
        {
                char ch[8] = {' '};
                std::uint8_t len = 1;
                std::uint8_t fg = kColorDefault;
                std::uint8_t bg = kColorDefault;

                bool operator==(const Cell &o) const
                {
                        return len == o.len && fg == o.fg && bg == o.bg &&
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

        // PT: Renderiza uma linha inserindo SGR nas transições de cor.
        // EN: Renders a line inserting SGR on color transitions.
        // ES: Renderiza una línea insertando SGR en las transiciones de color.
        // ZH: 渲染一行，在颜色过渡处插入 SGR。
        [[nodiscard]] static std::string render_line(const std::vector<Cell> &line)
        {
                std::string out;
                std::uint8_t cur_fg = kColorDefault, cur_bg = kColorDefault;
                for (const Cell &c : line)
                {
                        if (c.fg != cur_fg || c.bg != cur_bg)
                        {
                                out += "\x1b[0m";
                                if (c.fg != kColorDefault)
                                        out += "\x1b[38;5;" + std::to_string(c.fg) + "m";
                                if (c.bg != kColorDefault)
                                        out += "\x1b[48;5;" + std::to_string(c.bg) + "m";
                                cur_fg = c.fg;
                                cur_bg = c.bg;
                        }
                        out.append(c.ch, c.len);
                }
                if (cur_fg != kColorDefault || cur_bg != kColorDefault)
                        out += "\x1b[0m";
                return out;
        }

        int rows_;
        int cols_;
        std::vector<std::vector<Cell>> front_;
        std::vector<std::vector<Cell>> back_;
};

} // namespace tui
