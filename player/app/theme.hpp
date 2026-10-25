// PT: Temas no formato .theme do kew (INI: [theme] name=..., chave=cor).
//     Cores: #rrggbb, índice ANSI 0-15, -1 (herda o terminal). 101-104
//     (cores do álbum) caem num cinza neutro pela ausência do álbum no
//     momento do parse. Fontes: temas embutidos (built-in), depois
//     ~/.config/kiutube/themes/*.theme e ~/.config/kew/themes/*.theme
//     (compat com a coleção do kew). Chaves desconhecidas são ignoradas.
// EN: Themes in kew's .theme format (INI: [theme] name=..., key=color).
//     Colors: #rrggbb, ANSI index 0-15, -1 (inherit terminal). 101-104
//     (album colors) fall back to a neutral gray — the album isn't known
//     at parse time. Sources: built-in themes, then
//     ~/.config/kiutube/themes/*.theme and ~/.config/kew/themes/*.theme
//     (compat with the kew collection). Unknown keys are ignored.
// ES: Temas en el formato .theme de kew (INI). Colores: #rrggbb, ANSI
//     0-15, -1 (hereda). Claves desconocidas se ignoran.
// ZH: kew 的 .theme 主题格式（INI：[theme] name=...，键=颜色）。颜色支持
//     #rrggbb、ANSI 0-15、-1（继承终端）。未知键忽略。
#pragma once

#include "tui/color.hpp"

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace app
{

// PT: Um tema carregado: id (stem do arquivo), título ([theme] name=) e
//     mapa chave→cor. get() resolve com fallback no tema default.
// EN: A loaded theme: id (file stem), title ([theme] name=) and a
//     key→color map. get() falls back to the default theme.
// ZH: 已加载主题：id（文件名主干）、标题（[theme] name=）与 键→颜色 表。
struct Theme
{
        std::string id;
        std::string title;
        std::map<std::string, tui::Rgb> colors;

        // PT: Cor de uma chave; sem definição = herda o terminal ({}).
        // EN: A key's color; unset = inherit the terminal ({}).
        // ZH: 取键颜色；未定义 = 继承终端（{}）。
        [[nodiscard]] tui::Color get(std::string_view key) const;
};

// PT: O tema embutido "default" — reproduz a cara histórica da TUI
//     (visualizador verde→amarelo→vermelho, pico rosa, resto herda).
// EN: The built-in "default" theme — reproduces the historical TUI look
//     (green→yellow→red visualizer, pink peak, the rest inherits).
// ZH: 内建 "default" 主题——复刻 TUI 历史外观（绿→黄→红可视化器、粉色峰值，
//     其余继承终端）。
[[nodiscard]] const Theme &default_theme();

// PT: Parse de UM arquivo .theme. nullopt = arquivo ilegível/sem [theme].
// EN: Parses ONE .theme file. nullopt = unreadable/no [theme].
// ZH: 解析单个 .theme 文件。nullopt = 不可读/缺少 [theme]。
[[nodiscard]] std::optional<Theme> parse_theme_file(const std::string &path);

// PT: Todos os temas disponíveis: built-in + ~/.config/kiutube/themes +
//     ~/.config/kew/themes (ordem alfabética por id, "default" primeiro).
// EN: All available themes: built-in + ~/.config/kiutube/themes +
//     ~/.config/kew/themes (alphabetical by id, "default" first).
// ZH: 全部可用主题：内建 + 两个目录（按 id 字母序，"default" 在前）。
[[nodiscard]] std::vector<Theme> load_themes();

// PT: Busca por id (ex. "gruvbox"); sem acento = o default. / Lookup by id.
[[nodiscard]] const Theme *find_theme(const std::vector<Theme> &themes, std::string_view id);

} // namespace app
