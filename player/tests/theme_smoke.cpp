// PT: Smoke test do sistema de temas (.theme do kew): parse de hex,
//     índice ANSI, -1 (herda), 101-104 (fallback cinza), chave
//     desconhecida, comentários, e [theme] ausente (arquivo inválido).
// EN: Theme system smoke test (kew .theme): hex parsing, ANSI index,
//     -1 (inherit), 101-104 (gray fallback), unknown key, comments,
//     and a missing [theme] section (invalid file).
// ES: Smoke test del sistema de temas. / ZH: 主题系统冒烟测试。
#include "app/theme.hpp"

#include <cstdio>
#include <fstream>
#include <string>

namespace
{

int failures = 0;

void check(const bool ok, const char *what)
{
        if (!ok)
        {
                std::fprintf(stderr, "theme_smoke: FAIL: %s\n", what);
                ++failures;
        }
}

} // namespace

int main()
{
        const std::string path = "/tmp/kiutube_theme_smoke.theme";
        {
                std::ofstream f(path);
                f << "# comentario / comment\n"
                     "[theme]\n"
                     "name=Teste\n"
                     "accent=#fabd2f\n"
                     "playlist.playing=11\n"
                     "status.info=-1\n"
                     "trackview.visualizer=101\n"
                     "chave.qualquer=#010203\n"
                     "\n";
        }
        const auto t = app::parse_theme_file(path);
        check(t.has_value(), "parse do arquivo / file parse");
        if (t)
        {
                check(t->id == "kiutube_theme_smoke", "id = stem do arquivo / id = file stem");
                check(t->title == "Teste", "title = [theme] name=");
                const auto accent = t->get("accent");
                check(accent.set && accent.rgb.r == 0xfa && accent.rgb.g == 0xbd && accent.rgb.b == 0x2f,
                      "hex #fabd2f");
                const auto playing = t->get("playlist.playing");
                check(playing.set && playing.rgb.r == 255 && playing.rgb.g == 255 && playing.rgb.b == 0,
                      "ANSI 11 = amarelo brilhante / bright yellow");
                check(!t->get("status.info").set, "-1 = herda o terminal / inherits terminal");
                const auto viz = t->get("trackview.visualizer");
                check(viz.set && viz.rgb.r == 0x92 && viz.rgb.g == 0x83 && viz.rgb.b == 0x74,
                      "101 = fallback cinza / gray fallback");
                const auto unk = t->get("chave.qualquer");
                check(unk.set && unk.rgb.r == 1 && unk.rgb.g == 2 && unk.rgb.b == 3,
                      "chave arbitraria guardada / arbitrary key stored");
                check(!t->get("nao.existe").set, "chave ausente = herda / missing key inherits");
        }

        // PT: Arquivo sem [theme]: parse ainda devolve (title = id) — o
        //     formato é tolerante; inválido = só arquivo ilegível.
        // EN: File without [theme]: parse still returns (title = id) — the
        //     format is tolerant; invalid = unreadable file only.
        const std::string bare = "/tmp/kiutube_theme_bare.theme";
        {
                std::ofstream f(bare);
                f << "accent=#112233\n";
        }
        const auto tb = app::parse_theme_file(bare);
        check(tb.has_value() && tb->title == tb->id, "sem [theme]: title cai no id / title falls back to id");

        check(app::parse_theme_file("/tmp/kiutube_theme_nope.theme") == std::nullopt,
              "arquivo inexistente = nullopt / missing file = nullopt");

        // PT: O tema default cobre as chaves que a TUI/viz consomem.
        // EN: The default theme covers the keys the TUI/viz consume.
        for (const char *k : {"progress.filled", "progress.empty", "viz.low", "viz.mid", "viz.high", "viz.peak"})
                check(app::default_theme().get(k).set, "default theme tem a chave / has key");

        if (failures > 0)
                return 1;
        std::printf("theme_smoke: PASS\n");
        return 0;
}
