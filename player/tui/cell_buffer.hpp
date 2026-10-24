// PT: Buffer de células em linhas com diff por linha. M0: texto puro, sem
//     cor; diff por célula (runs) e atributos chegam no M4 com o
//     visualizador. Linha alterada é reescrita inteira — simples e correto.
// EN: Line-based cell buffer with per-line diff. M0: plain text, no color;
//     per-cell diff (runs) and attributes land in M4 with the visualizer.
//     A changed line is rewritten whole — simple and correct.
// ES: Buffer de celdas en líneas con diff por línea. M0: texto plano, sin
//     color; diff por celda (runs) y atributos llegan en M4 con el
//     visualizador. Una línea cambiada se reescribe entera — simple y correcto.
// ZH: 按行组织的单元格缓冲，按行 diff。M0：纯文本、无颜色；按单元格 diff（runs）
//     与属性随 M4 的可视化器加入。发生变化的行整行重写 —— 简单而正确。
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace tui
{

class CellBuffer
{
      public:
        CellBuffer(int rows, int cols) : rows_(rows), cols_(cols), front_(rows), back_(rows)
        {
        }

        [[nodiscard]] int rows() const
        {
                return rows_;
        }
        [[nodiscard]] int cols() const
        {
                return cols_;
        }

        // PT: Limpa o frame em edição (espaços). / EN: Clears the editing frame (spaces).
        // ES: Limpa el frame en edición (espacios). / ZH: 清空正在编辑的帧（空格）。
        void clear()
        {
                for (auto &line : back_)
                        line.assign(static_cast<std::size_t>(cols_), ' ');
        }

        // PT: Escreve texto em (row,col) com clip simples por bytes; UTF-8 pode
        //     cortar caractere na borda (layout com wcwidth chega no M4).
        // EN: Writes text at (row,col) with simple byte clipping; UTF-8 may
        //     split a char at the edge (wcwidth layout lands in M4).
        // ES: Escribe texto en (row,col) con clip simple por bytes; UTF-8 puede
        //     cortar un carácter en el borde (layout wcwidth llega en M4).
        // ZH: 在 (row,col) 写入文本，按字节简单裁剪；UTF-8 可能在边界截断字符
        //     （wcwidth 布局在 M4 加入）。
        void put(int row, int col, std::string_view text)
        {
                if (row < 0 || row >= rows_ || col < 0 || col >= cols_ || text.empty())
                        return;
                const std::size_t avail = static_cast<std::size_t>(cols_ - col);
                back_[row].replace(static_cast<std::size_t>(col), std::min(avail, text.size()), text.substr(0, avail));
        }

        // PT: Troca front/back e devolve segmentos (escape de posição + texto)
        //     prontos para um único writev — minimiza syscalls para o terminal.
        // EN: Swaps front/back and returns segments (position escape + text)
        //     ready for a single writev — minimizes syscalls to the terminal.
        // ES: Intercambia front/back y devuelve segmentos (escape de posición +
        //     texto) listos para un único writev — minimiza syscalls.
        // ZH: 交换 front/back 并返回段（定位转义 + 文本），可直接用于单次 writev
        //     —— 最大限度减少对终端的系统调用。
        [[nodiscard]] std::vector<std::string> flush_diff()
        {
                std::vector<std::string> segments;
                for (int r = 0; r < rows_; ++r)
                {
                        if (front_[r] == back_[r])
                                continue;
                        segments.push_back("\x1b[" + std::to_string(r + 1) + ";1H\x1b[2K");
                        segments.push_back(back_[r]);
                        front_[r] = back_[r];
                }
                return segments;
        }

      private:
        int rows_;
        int cols_;
        std::vector<std::string> front_; // último frame enviado / last frame sent / último frame enviado / 已发送的上一帧
        std::vector<std::string> back_;  // frame em edição / frame being edited / frame en edición / 正在编辑的帧
};

} // namespace tui
