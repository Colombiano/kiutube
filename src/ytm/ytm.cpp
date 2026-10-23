/**
 * @file ytm.cpp
 * @brief Implementação do provider YouTube Music via yt-dlp.
 *
 * Estratégia idêntica à do ytm-player: o yt-dlp resolve a página do
 * YouTube/YouTube Music e entrega a URL assinada do fluxo de áudio.
 * Usamos templates --print para obter campos tab-separated e evitar
 * parser de JSON inteiro.
 */
#include "ytm.hpp"

#include "http.hpp"

#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <unistd.h>

namespace ytm
{

namespace
{

/** SHELL-escape para embedding em comando entre aspas simples. */
std::string shell_quote(const std::string &s)
{
        std::string out = "'";
        for (char c : s)
        {
                if (c == '\'')
                        out += "'\\''";
                else
                        out += c;
        }
        out += "'";
        return out;
}

std::string run_capture(const std::string &cmd)
{
        std::string out;
        std::array<char, 4096> buf{};
        FILE *pipe = popen(cmd.c_str(), "r");
        if (!pipe)
                throw YtmError("falha ao iniciar subprocesso");
        while (std::fgets(buf.data(), static_cast<int>(buf.size()), pipe))
                out += buf.data();
        int rc = pclose(pipe);
        if (rc != 0)
                throw YtmError("subprocesso retornou erro: " + cmd);
        return out;
}

/** Igual a run_capture, mas não lança em rc != 0 (para retry externo). */
std::string run_capture_quiet(const std::string &cmd)
{
        std::string out;
        std::array<char, 4096> buf{};
        FILE *pipe = popen(cmd.c_str(), "r");
        if (!pipe)
                return out;
        while (std::fgets(buf.data(), static_cast<int>(buf.size()), pipe))
                out += buf.data();
        pclose(pipe);
        return out;
}

/**
 * Localiza um yt-dlp funcional. O do sistema (Debian) costuma estar velho
 * demais para os streams atuais do YouTube ("page needs to be reloaded"),
 * então preferimos: $YTDLP, o venv do ytm-player, e por fim o PATH.
 */
std::string find_ytdlp()
{
        const char *env = std::getenv("YTDLP");
        if (env && access(env, X_OK) == 0)
                return env;

        const char *home = std::getenv("HOME");
        if (home)
        {
                std::string venv = std::string(home) + "/.local/share/pipx/venvs/ytm-player/bin/yt-dlp";
                if (access(venv.c_str(), X_OK) == 0)
                        return venv;
        }
        return "yt-dlp"; // PATH
}

} // namespace

/** Faz parse de saída tab-separated "id\ttitle\tuploader\tduration". */
std::vector<Track> parse_tracks_output(const std::string &out)
{
        std::vector<Track> tracks;
        size_t pos = 0;
        while (pos < out.size())
        {
                size_t eol = out.find('\n', pos);
                if (eol == std::string::npos)
                        eol = out.size();
                std::string line = out.substr(pos, eol - pos);
                pos = eol + 1;
                if (line.empty())
                        continue;

                // campos: id \t title \t uploader \t duration
                Track t;
                size_t f1 = line.find('\t');
                size_t f2 = f1 == std::string::npos ? f1 : line.find('\t', f1 + 1);
                size_t f3 = f2 == std::string::npos ? f2 : line.find('\t', f2 + 1);
                if (f1 == std::string::npos || f2 == std::string::npos)
                        continue;
                t.video_id = line.substr(0, f1);
                t.title = line.substr(f1 + 1, f2 - f1 - 1);
                t.artist = (f3 == std::string::npos) ? "" : line.substr(f2 + 1, f3 - f2 - 1);
                if (f3 != std::string::npos)
                {
                        try
                        {
                                t.duration = std::stod(line.substr(f3 + 1));
                        }
                        catch (...)
                        {
                                t.duration = 0.0;
                        }
                }
                if (!t.video_id.empty() && t.video_id.find(' ') == std::string::npos)
                        tracks.push_back(std::move(t));
        }
        return tracks;
}

Task<std::vector<Track>> search(std::string query, int limit)
{
        co_return co_await offload([q = std::move(query), limit] {
                std::string term = "ytsearch" + std::to_string(limit) + ":" + q;
                std::string cmd = find_ytdlp() + " --flat-playlist --no-warnings --ignore-errors "
                                  "--print \"%(id)s\t%(title)s\t%(uploader)s\t%(duration)s\" " +
                                  shell_quote(term) + " 2>/dev/null";

                auto tracks = parse_tracks_output(run_capture(cmd));
                if (tracks.empty())
                        throw YtmError("busca sem resultados: " + q);
                return tracks;
        });
}

/** Faz parse de saída "id\ttitle\tartist\talbum\tduration" (NA -> ""). */
std::vector<Track> parse_info_output(const std::string &out)
{
        std::vector<Track> tracks;
        std::istringstream in(out);
        std::string line;
        while (std::getline(in, line))
        {
                if (line.empty())
                        continue;
                size_t f[5];
                f[0] = line.find('\t');
                for (int i = 1; i < 4 && f[i - 1] != std::string::npos; ++i)
                        f[i] = line.find('\t', f[i - 1] + 1);
                if (f[0] == std::string::npos || f[1] == std::string::npos)
                        continue;

                Track t;
                t.video_id = line.substr(0, f[0]);
                t.title = line.substr(f[0] + 1, f[1] - f[0] - 1);
                if (f[2] != std::string::npos)
                        t.artist = line.substr(f[1] + 1, f[2] - f[1] - 1);
                if (f[3] != std::string::npos)
                        t.album = line.substr(f[2] + 1, f[3] - f[2] - 1);
                if (f[4] != std::string::npos)
                {
                        try
                        {
                                t.duration = std::stod(line.substr(f[3] + 1));
                        }
                        catch (...)
                        {
                                t.duration = 0.0;
                        }
                }
                if (t.artist == "NA")
                        t.artist.clear();
                if (t.album == "NA")
                        t.album.clear();
                if (!t.video_id.empty())
                        tracks.push_back(std::move(t));
        }
        return tracks;
}

Task<Track> info(std::string video_id)
{
        co_return co_await offload([vid = std::move(video_id)] {
                std::string cmd = find_ytdlp() + " --no-warnings --ignore-errors "
                                  "--print \"%(id)s\t%(title)s\t%(artist)s\t%(album)s\t%(duration)s\" " +
                                  shell_quote(watch_url(vid)) + " 2>/dev/null";
                auto tracks = parse_info_output(run_capture(cmd));
                if (tracks.empty())
                        throw YtmError("não achou metadados para " + vid);
                return std::move(tracks.front());
        });
}

Task<std::vector<Track>> info_multi(const std::vector<std::string> &video_ids)
{
        co_return co_await offload([ids = video_ids] {
                std::string cmd = find_ytdlp() + " --no-warnings --ignore-errors "
                                  "--print \"%(id)s\t%(title)s\t%(artist)s\t%(album)s\t%(duration)s\"";
                for (const auto &id : ids)
                        cmd += " " + shell_quote(watch_url(id));
                cmd += " 2>/dev/null";
                return parse_info_output(run_capture(cmd));
        });
}

Task<std::pair<std::string, std::string>> resolve_stream_ex(std::string video_id)
{
        co_return co_await offload([vid = std::move(video_id)] {
                std::string cmd = find_ytdlp() +
                                  " -f bestaudio/best --print \"%(url)s\t%(ext)s\" --no-warnings " +
                                  shell_quote(watch_url(vid)) + " 2>/dev/null";
                std::string out = run_capture(cmd);
                size_t eol = out.find('\n');
                std::string first = (eol == std::string::npos) ? out : out.substr(0, eol);
                size_t tab = first.find('\t');
                std::string url = (tab == std::string::npos) ? first : first.substr(0, tab);
                std::string ext = (tab == std::string::npos) ? "webm" : first.substr(tab + 1);
                if (url.rfind("http", 0) != 0)
                        throw YtmError("não resolveu stream para " + vid);
                if (ext.empty() || ext.size() > 5)
                        ext = "webm";
                return std::make_pair(std::move(url), std::move(ext));
        });
}

Task<std::string> resolve_stream(std::string video_id)
{
        co_return co_await offload([vid = std::move(video_id)]() -> std::string {
                auto r = resolve_stream_ex(std::move(vid));
                return std::move(r).sync_wait().first;
        });
}

Task<std::string> download_track(std::string video_id, std::string dest_base)
{
        // O download é feito pelo próprio yt-dlp (não por libcurl): a URL do
        // googlevideo é assinada p/ o IP de origem, e com IPv6 de privacidade
        // rotacionando, resolve+download em processos separados leva a 403.
        // Dentro do yt-dlp resolve+download são consistentes, e ele ainda
        // retry internamente. 3 tentativas externas p/ resiliência.
        for (int attempt = 0; attempt < 3; ++attempt)
        {
                bool ok = co_await offload([vid = video_id, dest_base] {
                        std::string tmpl = dest_base + ".%(ext)s";
                        std::string cmd = find_ytdlp() + " -f bestaudio/best --no-playlist --no-warnings "
                                          "--retries 5 --file-access-retries 3 "
                                          "--print after_move:filepath -o " +
                                          shell_quote(tmpl) + " " + shell_quote(watch_url(vid)) + " 2>/dev/null";
                        std::string out = run_capture_quiet(cmd); // não lança em rc!=0
                        // rc 0 e saída com o caminho => sucesso
                        return out.find(dest_base) != std::string::npos;
                });
                if (ok)
                {
                        // acha o arquivo produzido (ext vem do template)
                        for (const char *ext : {"webm", "m4a", "opus", "ogg", "mp4", "mkv"})
                        {
                                std::string cand = dest_base + "." + ext;
                                if (access(cand.c_str(), F_OK) == 0)
                                        co_return cand;
                        }
                }

                co_await offload([] { std::this_thread::sleep_for(std::chrono::seconds(2)); });
        }
        throw YtmError("falha ao baixar stream após 3 tentativas: " + dest_base);
}

} // namespace ytm
