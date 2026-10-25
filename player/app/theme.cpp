// PT: Implementação do sistema de temas: parse do INI do kew (.theme),
//     tabela ANSI 0-15 → RGB, e o registro (built-in + diretórios).
// EN: Theme system implementation: kew (.theme) INI parsing, the ANSI
//     0-15 → RGB table, and the registry (built-in + directories).
// ES: Implementación del sistema de temas: parse del INI del kew, tabla
//     ANSI → RGB y el registro (built-in + directorios).
// ZH: 主题系统实现：解析 kew（.theme）INI、ANSI 0-15 → RGB 表，以及注册表
//     （内建 + 目录扫描）。
#include "app/theme.hpp"

#include "app/paths.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>

namespace app
{
namespace
{

// PT: Índice ANSI 0-15 → RGB (valores xterm clássicos).
// EN: ANSI index 0-15 → RGB (classic xterm values).
// ZH: ANSI 0-15 → RGB（经典 xterm 值）。
constexpr tui::Rgb kAnsi16[16] = {
    {0x00, 0x00, 0x00}, {0xcd, 0x00, 0x00}, {0x00, 0xcd, 0x00}, {0xcd, 0xcd, 0x00},
    {0x00, 0x00, 0xee}, {0xcd, 0x00, 0xcd}, {0x00, 0xcd, 0xcd}, {0xe5, 0xe5, 0xe5},
    {0x7f, 0x7f, 0x7f}, {0xff, 0x00, 0x00}, {0x00, 0xff, 0x00}, {0xff, 0xff, 0x00},
    {0x5c, 0x5c, 0xff}, {0xff, 0x00, 0xff}, {0x00, 0xff, 0xff}, {0xff, 0xff, 0xff},
};

// PT: 101-104 (cores do álbum) caem num cinza neutro — o álbum só é
//     conhecido em runtime e a v1 não resolve dinamicamente.
// EN: 101-104 (album colors) fall back to a neutral gray — the album is
//     only known at runtime and v1 doesn't resolve them dynamically.
// ZH: 101-104（专辑色）回退为中性灰——专辑封面只在运行时可知，v1 不做动态解析。
constexpr tui::Rgb kAlbumFallback{0x92, 0x83, 0x74};

// PT: Parse de um valor de cor: #rrggbb | 0..15 | -1 (unset) | 101..104.
// EN: Parses a color value: #rrggbb | 0..15 | -1 (unset) | 101..104.
// ZH: 解析颜色值：#rrggbb | 0..15 | -1（不设置）| 101..104。
std::optional<tui::Rgb> parse_color(std::string_view v)
{
        while (!v.empty() && (v.front() == ' ' || v.front() == '\t'))
                v.remove_prefix(1);
        while (!v.empty() && (v.back() == ' ' || v.back() == '\t' || v.back() == '\r'))
                v.remove_suffix(1);
        if (v.size() == 7 && v.front() == '#')
        {
                const auto hex = [](char c) -> int {
                        if (c >= '0' && c <= '9')
                                return c - '0';
                        if (c >= 'a' && c <= 'f')
                                return c - 'a' + 10;
                        if (c >= 'A' && c <= 'F')
                                return c - 'A' + 10;
                        return -1;
                };
                const int r = hex(v[1]) * 16 + hex(v[2]);
                const int g = hex(v[3]) * 16 + hex(v[4]);
                const int b = hex(v[5]) * 16 + hex(v[6]);
                if (r >= 0 && g >= 0 && b >= 0)
                        return tui::Rgb{static_cast<std::uint8_t>(r), static_cast<std::uint8_t>(g),
                                        static_cast<std::uint8_t>(b)};
                return std::nullopt;
        }
        try
        {
                int n = 0;
                for (const char c : v)
                        if (!std::isdigit(static_cast<unsigned char>(c)))
                                return std::nullopt;
                n = std::stoi(std::string(v));
                if (n >= 0 && n <= 15)
                        return kAnsi16[n];
                if (n >= 101 && n <= 104)
                        return kAlbumFallback;
        }
        catch (...)
        {
        }
        return std::nullopt; // PT: -1 e lixo = herda o terminal / EN: -1 and junk = inherit
}

} // namespace

tui::Color Theme::get(const std::string_view key) const
{
        const auto it = colors.find(std::string(key));
        if (it == colors.end())
                return {};
        return tui::Color{it->second};
}

const Theme &default_theme()
{
        // PT: Só as chaves que a v1 consome; o resto herda o terminal —
        //     assim o "default" é idêntico à cara pré-temas.
        // EN: Only the keys v1 consumes; everything else inherits the
        //     terminal — so "default" looks identical to pre-themes.
        // ZH: 仅包含 v1 消费的键；其余继承终端——"default" 与启用主题前一致。
        static const Theme kDefault{
            .id = "default",
            .title = "Default",
            .colors =
                {
                    {"progress.filled", {0x3e, 0xcf, 0x6e}},
                    {"progress.empty", {0x40, 0x40, 0x40}},
                    {"viz.low", {0x2e, 0xd5, 0x73}},
                    {"viz.mid", {0xe8, 0xd3, 0x4e}},
                    {"viz.high", {0xe2, 0x54, 0x3f}},
                    {"viz.peak", {0xff, 0x6e, 0x96}},
                },
        };
        return kDefault;
}

std::optional<Theme> parse_theme_file(const std::string &path)
{
        std::ifstream f(path);
        if (!f)
                return std::nullopt;
        Theme t;
        t.id = std::filesystem::path(path).stem().string();
        std::string line;
        bool in_theme = false;
        while (std::getline(f, line))
        {
                // PT: '#' é comentário só no início da linha ou após
                //     espaço/tab; dentro de um valor é cor hex (#rrggbb
                //     segue '=' diretamente, sem espaço).
                // EN: '#' is a comment only at line start or after a
                //     space/tab; inside a value it's a hex color (#rrggbb
                //     directly follows '=', with no space).
                // ZH: '#' 仅在行首或空格/制表符后视为注释；在值内部是十六进制
                //     颜色（#rrggbb 紧跟 '='，无空格）。
                if (const auto hash = line.find('#'); hash != std::string::npos &&
                    (hash == 0 || line[hash - 1] == ' ' || line[hash - 1] == '\t'))
                        line.erase(hash);
                while (!line.empty() && (line.back() == ' ' || line.back() == '\t' || line.back() == '\r'))
                        line.pop_back();
                if (line.empty())
                        continue;
                if (line.front() == '[')
                {
                        in_theme = (line == "[theme]");
                        continue;
                }
                const auto eq = line.find('=');
                if (eq == std::string::npos)
                        continue;
                std::string key = line.substr(0, eq);
                std::string val = line.substr(eq + 1);
                while (!key.empty() && (key.back() == ' ' || key.back() == '\t'))
                        key.pop_back();
                if (key == "name" && in_theme)
                {
                        t.title = val;
                        continue;
                }
                if (auto c = parse_color(val))
                        t.colors[key] = *c;
        }
        if (t.title.empty())
                t.title = t.id;
        return t;
}

std::vector<Theme> load_themes()
{
        std::vector<Theme> out{default_theme()};
        for (const char *dir : {"kiutube/themes", "kew/themes"})
        {
                const auto d = config_dir(dir);
                std::error_code ec;
                if (!std::filesystem::is_directory(d, ec) || ec)
                        continue;
                for (const auto &e : std::filesystem::directory_iterator(d, ec))
                {
                        if (!e.is_regular_file(ec) || e.path().extension() != ".theme")
                                continue;
                        if (auto t = parse_theme_file(e.path().string()))
                        {
                                // PT: id repetido (ex. default.theme do kew)
                                //     não sobrescreve o já carregado.
                                // EN: a repeated id (e.g. kew's
                                //     default.theme) doesn't override.
                                // ZH: 重复的 id（如 kew 的 default.theme）
                                //     不覆盖已加载项。
                                if (find_theme(out, t->id))
                                        continue;
                                out.push_back(std::move(*t));
                        }
                }
        }
        std::sort(out.begin() + 1, out.end(), [](const Theme &a, const Theme &b) { return a.id < b.id; });
        return out;
}

const Theme *find_theme(const std::vector<Theme> &themes, const std::string_view id)
{
        for (const auto &t : themes)
                if (t.id == id)
                        return &t;
        return nullptr;
}

} // namespace app
