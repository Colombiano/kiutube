// PT: Implementação do cover::Canvas sobre chafa (KIUTUBE2_WITH_CHAFA) + stub.
//     Decodificação via stb_image (STB_IMAGE_IMPLEMENTATION vive aqui — TU
//     única). Cores das células vêm empacotadas 0x00RRGGBB de
//     chafa_canvas_get_colors_at; o char é um gunichar convertido depois.
// EN: cover::Canvas on chafa (KIUTUBE2_WITH_CHAFA) + stub. Decoding through
//     stb_image (STB_IMAGE_IMPLEMENTATION lives here — single TU). Cell
//     colors come packed 0x00RRGGBB from chafa_canvas_get_colors_at; the
//     char is a gunichar converted downstream.
// ES: Implementación sobre chafa + stub. Decodificación vía stb_image.
// ZH: 基于 chafa 的实现 + stub。经 stb_image 解码（STB_IMAGE_IMPLEMENTATION
//     仅此一个 TU）。单元格颜色来自 chafa_canvas_get_colors_at（打包为
//     0x00RRGGBB）；字符为 gunichar。
#include "library/canvas.hpp"

#include "krt/log.hpp"

#ifdef KIUTUBE2_WITH_CHAFA

#include <chafa.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image/stb_image.h"

#include <vector>

namespace cover
{
namespace
{

CellPaint decode_cell(ChafaCanvas *cv, int x, int y)
{
        CellPaint c;
        c.ch = static_cast<unsigned int>(chafa_canvas_get_char_at(cv, x, y));
        int fg = 0, bg = 0;
        chafa_canvas_get_colors_at(cv, x, y, &fg, &bg);
        // PT: cores empacotadas 0x00RRGGBB (probe: chafa 1.14 / libchafa 0.9.5).
        // EN: colors packed 0x00RRGGBB (probe: chafa 1.14 / libchafa 0.9.5).
        // ZH: 颜色打包为 0x00RRGGBB（已探测：chafa 1.14 / libchafa 0.9.5）。
        c.fg_set = true;
        c.r = static_cast<unsigned char>((fg >> 16) & 0xff);
        c.g = static_cast<unsigned char>((fg >> 8) & 0xff);
        c.b = static_cast<unsigned char>(fg & 0xff);
        c.bg_set = true;
        c.br = static_cast<unsigned char>((bg >> 16) & 0xff);
        c.bg = static_cast<unsigned char>((bg >> 8) & 0xff);
        c.bb = static_cast<unsigned char>(bg & 0xff);
        return c;
}

} // namespace

struct Canvas::Impl
{
        Impl(int c, int r) : cols_(c), rows_(r)
        {
        }

        int cols_;
        int rows_;
        // PT: grade final já decodificada (render é quente; at() fica trivial).
        // EN: final decoded grid (render is hot; at() stays trivial).
        // ZH: 最终解码网格（render 完成全部工作；at() 保持简单）。
        std::vector<CellPaint> grid;
};

Canvas::Canvas(int cols, int rows) : impl_(std::make_unique<Impl>(cols, rows))
{
}

Canvas::~Canvas() = default;

bool Canvas::render(const std::string &image_path)
{
        impl_->grid.clear();
        int w = 0, h = 0, comp = 0;
        // PT: 4 canais pedidos (RGBA8_UNASSOCIATED) — comp original vem em comp.
        // EN: 4 channels requested (RGBA8_UNASSOCIATED) — original count in comp.
        // ZH: 请求 4 通道（RGBA8_UNASSOCIATED）——原始通道数在 comp 中。
        stbi_uc *px = stbi_load(image_path.c_str(), &w, &h, &comp, 4);
        if (!px || w <= 0 || h <= 0)
        {
                if (px)
                        stbi_image_free(px);
                krt::log::warn("cover: falha ao decodificar / decode failed / fallo / 解码失败: {}", image_path);
                return false;
        }

        ChafaCanvasConfig *cfg = chafa_canvas_config_new();
        chafa_canvas_config_set_geometry(cfg, impl_->cols_, impl_->rows_);
        chafa_canvas_config_set_canvas_mode(cfg, CHAFA_CANVAS_MODE_TRUECOLOR);
        chafa_canvas_config_set_pixel_mode(cfg, CHAFA_PIXEL_MODE_SYMBOLS);
        ChafaCanvas *cv = chafa_canvas_new(cfg);
        chafa_canvas_draw_all_pixels(cv, CHAFA_PIXEL_RGBA8_UNASSOCIATED, px, w, h, w * 4);
        stbi_image_free(px);

        impl_->grid.resize(static_cast<std::size_t>(impl_->cols_) * static_cast<std::size_t>(impl_->rows_));
        for (int y = 0; y < impl_->rows_; ++y)
                for (int x = 0; x < impl_->cols_; ++x)
                        impl_->grid[static_cast<std::size_t>(y) * static_cast<std::size_t>(impl_->cols_) +
                                    static_cast<std::size_t>(x)] = decode_cell(cv, x, y);

        chafa_canvas_unref(cv);
        chafa_canvas_config_unref(cfg);
        return true;
}

int Canvas::cols() const
{
        return impl_->cols_;
}

int Canvas::rows() const
{
        return impl_->rows_;
}

CellPaint Canvas::at(int x, int y) const
{
        if (x < 0 || y < 0 || x >= impl_->cols_ || y >= impl_->rows_ ||
            impl_->grid.empty())
                return {};
        return impl_->grid[static_cast<std::size_t>(y) * static_cast<std::size_t>(impl_->cols_) +
                           static_cast<std::size_t>(x)];
}

} // namespace cover

#else // PT: sem chafa: stub / stub without chafa / 无 chafa 时的 stub

namespace cover
{

struct Canvas::Impl
{
        int cols_;
        int rows_;
};

Canvas::Canvas(int cols, int rows) : impl_(std::make_unique<Impl>(cols, rows))
{
        krt::log::warn("cover: chafa ausente / missing / ausente / 缺失 (stub)");
}

Canvas::~Canvas() = default;

bool Canvas::render(const std::string &)
{
        return false;
}

int Canvas::cols() const
{
        return impl_->cols_;
}

int Canvas::rows() const
{
        return impl_->rows_;
}

CellPaint Canvas::at(int, int) const
{
        return {};
}

} // namespace cover

#endif // KIUTUBE2_WITH_CHAFA
