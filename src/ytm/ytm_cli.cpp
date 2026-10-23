/**
 * @file ytm_cli.cpp
 * @brief CLI headless da camada ytm: `kew --ytm <comando>`.
 *
 *   kew --ytm search "<query>"          — lista resultados YouTube Music
 *   kew --ytm play "<busca>" | <videoId> — baixa top 5 (ou 1), escreve .lrc/.pt.lrc
 *                                          e reexecuta o kew tocando-os na TUI
 *   kew --ytm lyrics "<artista>" "<título>" — LRC original + tradução PT
 */
#include "ytm_bridge.h"

#include "ytm.hpp"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace
{

int cmd_search(int argc, char **argv)
{
        if (argc < 1)
        {
                std::fprintf(stderr, "uso: kew --ytm search \"<query>\"\n");
                return 2;
        }
        YtmTrack *tracks = nullptr;
        int n = ytm_search_blocking(argv[0], 10, &tracks);
        if (n < 0)
        {
                std::fprintf(stderr, "erro: busca falhou (yt-dlp instalado? rede?)\n");
                return 1;
        }
        for (int i = 0; i < n; ++i)
        {
                int m = static_cast<int>(tracks[i].duration) / 60;
                int s = static_cast<int>(tracks[i].duration) % 60;
                std::printf("%2d) %-12s %s — %s (%d:%02d)\n", i + 1, tracks[i].video_id, tracks[i].title,
                            tracks[i].artist ? tracks[i].artist : "", m, s);
        }
        ytm_free(tracks);
        return 0;
}

namespace
{

bool looks_like_video_id(const char *s)
{
        if (!s || std::strlen(s) != 11)
                return false;
        for (const char *p = s; *p; ++p)
        {
                bool ok = (*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') ||
                          *p == '-' || *p == '_';
                if (!ok)
                        return false;
        }
        return true;
}

/** "Artist - Title [id6]" -> nome de arquivo seguro. */
std::string sanitize_filename(std::string s)
{
        std::string out;
        for (char c : s)
        {
                unsigned char u = static_cast<unsigned char>(c);
                if (std::isalnum(u) || c == ' ' || c == '-' || c == '_' || c == '(' || c == ')' || c == '[' ||
                    c == ']' || c == '.')
                        out += c;
                else
                        out += '_';
        }
        if (out.size() > 80)
                out.resize(80);
        while (!out.empty() && out.back() == ' ')
                out.pop_back();
        return out;
}

std::string cache_streams_dir()
{
        const char *home = std::getenv("HOME");
        return std::string(home ? home : ".") + "/.cache/kiutube/streams";
}

bool file_exists(const std::string &p)
{
        return access(p.c_str(), F_OK) == 0;
}

void write_sidecars(const std::string &base, const std::string &artist, const std::string &title,
                    int duration_seconds)
{
        char *lrc = ytm_fetch_lyrics_ex_blocking(artist.c_str(), title.c_str(), duration_seconds);
        if (!lrc)
                return;

        std::string lrc_path = base + ".lrc";
        if (!file_exists(lrc_path))
        {
                FILE *f = std::fopen(lrc_path.c_str(), "wb");
                if (f)
                {
                        std::fwrite(lrc, 1, std::strlen(lrc), f);
                        std::fclose(f);
                }
        }

        if (ytm_translation_enabled())
        {
                char *trans = ytm_translate_blocking(lrc);
                if (trans)
                {
                        std::string pt_path = base + ".pt.lrc";
                        if (!file_exists(pt_path))
                        {
                                FILE *f = std::fopen(pt_path.c_str(), "wb");
                                if (f)
                                {
                                        std::fwrite(trans, 1, std::strlen(trans), f);
                                        std::fclose(f);
                                }
                        }
                        ytm_free(trans);
                }
        }
        ytm_free(lrc);
}

/**
 * kew pede "Type a path:" no 1º start quando path= está vazio no kewrc.
 * Se estiver vazio, apontamos para o cache de streams do YTM — assim o
 * prompt não atrapalha e o cache vira bibliável na view de library.
 */
void ensure_kewrc_path(const std::string &streams_dir)
{
        const char *home = std::getenv("HOME");
        std::string cfg = std::string(home ? home : ".") + "/.config/kew/kewrc";

        std::ifstream in(cfg);
        if (!in)
                return; // sem kewrc, deixa o kew pedir (comportamento vanilla)

        std::vector<std::string> lines;
        std::string line;
        bool replaced = false;
        while (std::getline(in, line))
        {
                if (!replaced && line.rfind("path=", 0) == 0 && line.size() == 5)
                {
                        lines.push_back("path=" + streams_dir);
                        replaced = true;
                }
                else
                {
                        lines.push_back(line);
                }
        }
        in.close();

        if (!replaced)
                return; // path já configurado ou sem linha path= (não inventamos)

        std::ofstream out(cfg, std::ios::trunc);
        if (!out)
                return;
        for (const auto &l : lines)
                out << l << "\n";
}

/** Reexecuta o próprio binário como `kew play <arquivos...>` (entra na TUI). */
int exec_kew_play(const std::vector<std::string> &files)
{
        char self[4096];
        ssize_t n = readlink("/proc/self/exe", self, sizeof(self) - 1);
        if (n <= 0)
        {
                std::fprintf(stderr, "erro: não achei o próprio executável\n");
                return 1;
        }
        self[n] = '\0';

        ensure_kewrc_path(cache_streams_dir());

        std::vector<char *> argv;
        argv.push_back(self);
        argv.push_back(const_cast<char *>("play"));
        for (const auto &f : files)
                argv.push_back(const_cast<char *>(f.c_str()));
        argv.push_back(nullptr);

        std::fprintf(stderr, "tocando %zu faixa(s) do YouTube Music no kew...\n", files.size());
        execv(self, argv.data());
        std::fprintf(stderr, "erro: execv falhou\n");
        return 1;
}

} // namespace

int cmd_play(int argc, char **argv)
{
        if (argc < 1)
        {
                std::fprintf(stderr, "uso: kew --ytm play \"<busca>\" | <videoId>\n");
                return 2;
        }

        // monta a lista de faixas: videoId direto ou busca (top 5)
        std::vector<ytm::Track> tracks;
        try
        {
                if (looks_like_video_id(argv[0]))
                {
                        ytm::Track t = ytm::info(argv[0]).sync_wait();
                        tracks.push_back(std::move(t));
                }
                else
                {
                        std::fprintf(stderr, "buscando: %s\n", argv[0]);
                        auto found = ytm::search(argv[0], 5).sync_wait();
                        tracks.insert(tracks.end(), std::make_move_iterator(found.begin()),
                                      std::make_move_iterator(found.end()));
                }
        }
        catch (const std::exception &e)
        {
                std::fprintf(stderr, "erro: %s\n", e.what());
                return 1;
        }

        // enriquece com metadados limpos do YTM (artist=%(artist)s, não uploader)
        try
        {
                std::vector<std::string> ids;
                ids.reserve(tracks.size());
                for (auto &t : tracks)
                        ids.push_back(t.video_id);
                auto infos = ytm::info_multi(ids).sync_wait();
                for (auto &info : infos)
                {
                        for (auto &t : tracks)
                        {
                                if (t.video_id != info.video_id)
                                        continue;
                                if (!info.artist.empty())
                                        t.artist = info.artist;
                                if (!info.album.empty())
                                        t.album = info.album;
                                if (info.duration > 0)
                                        t.duration = info.duration;
                        }
                }
        }
        catch (...)
        {
                // sem enriquecimento, segue com o que veio da busca
        }

        std::vector<std::string> files;
        for (auto &t : tracks)
        {
                std::string base = cache_streams_dir() + "/" +
                                   sanitize_filename(t.artist + " - " + t.title) + " [" + t.video_id.substr(0, 6) + "]";

                std::string final_path;
                for (const char *ext : {"webm", "m4a", "opus", "ogg"})
                {
                        std::string cand = base + "." + ext;
                        if (file_exists(cand))
                        {
                                final_path = cand;
                                break;
                        }
                }

                if (final_path.empty())
                {
                        std::fprintf(stderr, "baixando: %s — %s\n", t.title.c_str(), t.artist.c_str());
                        try
                        {
                                final_path = ytm::download_track(t.video_id, base).sync_wait();
                        }
                        catch (const std::exception &e)
                        {
                                std::fprintf(stderr, "pulando (%s)\n", e.what());
                                continue;
                        }
                }

                write_sidecars(base, t.artist, t.title, static_cast<int>(t.duration));
                files.push_back(final_path);
        }

        if (files.empty())
        {
                std::fprintf(stderr, "erro: nada pôde ser baixado\n");
                return 1;
        }
        if (std::getenv("YTM_NO_EXEC"))
        {
                for (const auto &f : files)
                        std::printf("%s\n", f.c_str());
                return 0;
        }
        return exec_kew_play(files);
}

int cmd_lyrics(int argc, char **argv)
{
        if (argc < 2)
        {
                std::fprintf(stderr, "uso: kew --ytm lyrics \"<artista>\" \"<título>\"\n");
                return 2;
        }
        char *lrc = ytm_fetch_lyrics_blocking(argv[0], argv[1]);
        if (!lrc)
        {
                std::fprintf(stderr, "letra não encontrada no LRCLIB\n");
                return 1;
        }
        std::printf("=== LRC original ===\n%s\n", lrc);

        if (ytm_translation_enabled())
        {
                char *trans = ytm_translate_blocking(lrc);
                if (trans)
                {
                        std::printf("=== Tradução PT ===\n%s\n", trans);
                        ytm_free(trans);
                }
                else
                {
                        std::printf("=== Tradução PT indisponível (provider falhou) ===\n");
                }
        }
        ytm_free(lrc);
        return 0;
}

} // namespace

/** Entry point chamado do main() do kew quando argv[1] == "--ytm". */
int ytm_cli_main(int argc, char **argv)
{
        if (argc < 1)
        {
                std::fprintf(stderr,
                             "Kiutube — YouTube Music no kew (fork)\n"
                             "uso:\n"
                             "  kew --ytm search \"<query>\"\n"
                             "  kew --ytm play \"<busca>\" | <videoId>\n"
                             "  kew --ytm lyrics \"<artista>\" \"<título>\"\n");
                return 2;
        }
        if (std::strcmp(argv[0], "search") == 0)
                return cmd_search(argc - 1, argv + 1);
        if (std::strcmp(argv[0], "play") == 0)
                return cmd_play(argc - 1, argv + 1);
        if (std::strcmp(argv[0], "lyrics") == 0)
                return cmd_lyrics(argc - 1, argv + 1);
        std::fprintf(stderr, "comando desconhecido: %s\n", argv[0]);
        return 2;
}
