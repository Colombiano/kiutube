// PT: Smoke test de library + fuzzy (M3b): OSA (mesmos valores canônicos do
//     typesense), normalização, tolerância de typos e um scan real em disco
//     temporário com arquivos WAV sem tags (fallback para filename).
// EN: Library + fuzzy smoke test (M3b): OSA (canonical values as in
//     typesense), normalization, typo tolerance and a real scan on a
//     temporary directory with tagless WAV files (filename fallback).
// ES: Smoke test de library + fuzzy (M3b): OSA (valores canónicos de
//     typesense), normalización, tolerancia de typos y un scan real en disco.
// ZH: library + fuzzy 冒烟测试（M3b）：OSA（与 typesense 相同的标准值）、归一化、
//     typo 容错，以及对临时目录中无标签 WAV 文件（回退文件名）的真实扫描。
#include "library/fuzzy.hpp"
#include "library/library.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace
{

int failures = 0;

void check(bool cond, const char *what)
{
        if (!cond)
        {
                std::fprintf(stderr, "library_smoke: FAIL: %s\n", what);
                ++failures;
        }
}

bool write_wav(const std::string &path)
{
        std::ofstream f(path, std::ios::binary);
        if (!f)
                return false;
        const auto put_u32 = [&f](std::uint32_t v) { f.write(reinterpret_cast<const char *>(&v), 4); };
        const auto put_u16 = [&f](std::uint16_t v) { f.write(reinterpret_cast<const char *>(&v), 2); };
        const int rate = 44100, frames = rate / 10;
        f.write("RIFF", 4);
        put_u32(36 + frames * 4);
        f.write("WAVE", 4);
        f.write("fmt ", 4);
        put_u32(16);
        put_u16(1);
        put_u16(2);
        put_u32(rate);
        put_u32(rate * 4);
        put_u16(4);
        put_u16(16);
        f.write("data", 4);
        put_u32(frames * 4);
        for (int i = 0; i < frames; ++i)
        {
                const auto v = static_cast<std::int16_t>(std::sin(i * 0.1) * 1000);
                put_u16(static_cast<std::uint16_t>(v));
                put_u16(static_cast<std::uint16_t>(v));
        }
        return static_cast<bool>(f);
}

} // namespace

int main()
{
        // PT: OSA canônica: kitten->sitting = 3; transposição adjacente ca<->ac = 1.
        // EN: Canonical OSA: kitten->sitting = 3; adjacent transposition ca<->ac = 1.
        // ES: OSA canónica: kitten->sitting = 3; transposición ca<->ac = 1.
        // ZH: 标准 OSA：kitten->sitting = 3；相邻转置 ca<->ac = 1。
        check(library::osa_distance("kitten", "sitting") == 3, "kitten/sitting");
        check(library::osa_distance("ca", "ac") == 1, "transposicao OSA / OSA transposition / 转置");
        check(library::osa_distance("abc", "abc") == 0, "identicos / identical / 相同");
        check(library::osa_distance("", "abc") == 3, "vazio / empty / 空串");

        // PT: Normalização (estilo typesense): minúsculas, só alnum. / EN: Typesense-style normalization.
        // ES: Normalización (estilo typesense). / ZH: 归一化（typesense 风格）。
        check(library::normalize("Bee Gees!") == "beegees", "normalize");

        // PT: Token: prefixo casa com 0; typo dentro da tolerância; fora = INF.
        // EN: Token: prefix at 0; typo within tolerance; beyond = INF.
        // ES: Token: prefijo a 0; typo dentro de la tolerancia; fuera = INF.
        // ZH: token：前缀计 0；容错内 typo 计距离；超容错为 INF。
        check(library::token_match("songbird", "songbird") == 0, "token exato");
        check(library::token_match("songbird", "songbirdtonight") == 0, "token prefixo");
        check(library::token_match("songbirt", "songbird") == 1, "token 1 typo (8 chars)");
        check(library::token_match("abc", "axc") == INT_MAX, "token 1 char -> 0 typos");

        // PT: fuzzy_filter em tracks sintéticos. / EN: fuzzy_filter on synthetic tracks.
        // ES: fuzzy_filter en pistas sintéticas. / ZH: 对合成曲目使用 fuzzy_filter。
        std::vector<library::Track> tracks = {
            {.path = "/a", .title = "Songbird", .artist = "Bee Gees", .album = "", .duration_seconds = 0},
            {.path = "/b", .title = "Washing Machine Heart", .artist = "Mitski", .album = "", .duration_seconds = 0},
            {.path = "/c", .title = "El Cantante", .artist = "Hector Lavoe", .album = "", .duration_seconds = 0},
        };
        auto r1 = library::fuzzy_filter(tracks, "songbird");
        check(!r1.empty() && r1.front().index == 0, "fuzzy exato acha Songbird");
        auto r2 = library::fuzzy_filter(tracks, "songbirt"); // PT: 1 typo tolerado / EN: 1 typo tolerated
        check(!r2.empty() && r2.front().index == 0, "fuzzy com typo");
        auto r3 = library::fuzzy_filter(tracks, "bee gees");
        check(!r3.empty() && r3.front().index == 0, "fuzzy artista+titulo");
        auto r4 = library::fuzzy_filter(tracks, "zzzzqqqq");
        check(r4.empty(), "fuzzy sem match");

        // PT: Scan real em disco: 2 WAVs sem tags -> title = filename.
        // EN: Real on-disk scan: 2 tagless WAVs -> title = filename.
        // ES: Scan real en disco: 2 WAVs sin tags -> title = filename.
        // ZH: 真实磁盘扫描：2 个无标签 WAV -> title = 文件名。
        const std::string dir = "/tmp/kiutube_library_smoke";
        std::filesystem::create_directories(dir);
        check(write_wav(dir + "/Alpha One.wav"), "escrever wav A");
        check(write_wav(dir + "/Beta Two.wav"), "escrever wav B");
        const auto scanned = library::scan(dir);
        check(scanned.size() == 2, "scan acha 2 faixas / finds 2 tracks / 找到 2 首");
        if (scanned.size() == 2)
        {
                check(scanned[0].title == "Alpha One.wav", "ordenacao alpha");
                check(scanned[1].title == "Beta Two.wav", "ordenacao beta");
        }
        std::filesystem::remove_all(dir);

        if (failures > 0)
                return 1;
        std::printf("library_smoke: PASS\n");
        return 0;
}
