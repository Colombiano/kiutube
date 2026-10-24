// PT: Implementação net/ytm: subprocessos yt-dlp com templates --print,
//     shell-quote, parse tab-separated. Port fiel da lógica de
//     src/ytm/ytm.cpp em forma síncrona mínima.
// EN: net/ytm implementation: yt-dlp subprocesses with --print templates,
//     shell quoting, tab-separated parsing. Faithful port of
//     src/ytm/ytm.cpp's logic in minimal synchronous form.
// ES: Implementación net/ytm: subprocessos yt-dlp con templates --print,
//     shell-quote, parse tab-separated. Port fiel de src/ytm/ytm.cpp.
// ZH: net/ytm 实现：使用 --print 模板的 yt-dlp 子进程、shell 引号、
//     制表符分隔解析。忠实移植 src/ytm/ytm.cpp 的逻辑，最小同步形态。
#include "net/ytm.hpp"

#include "krt/log.hpp"

#include <sys/stat.h>
#include <unistd.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

namespace net
{
namespace
{

// PT: Quote para shell single-quoted. / EN: single-quoted shell quoting.
// ES: Quote para shell single-quoted. / ZH: shell 单引号转义。
std::string shell_quote(const std::string &s)
{
        std::string out = "'";
        for (const char c : s)
        {
                if (c == '\'')
                        out += "'\\''";
                else
                        out += c;
        }
        out += "'";
        return out;
}

// PT: Roda cmd e captura stdout; vazio em qualquer falha (retry fica com o
//     caller). / EN: Runs cmd, captures stdout; empty on any failure.
// ES: Ejecuta cmd y captura stdout; vacío ante cualquier fallo.
// ZH: 执行命令并捕获 stdout；任何失败都返回空（重试交由调用方）。
std::string run_capture(const std::string &cmd)
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

} // namespace

std::string ytdlp_path()
{
        const char *env = std::getenv("YTDLP");
        if (env && access(env, X_OK) == 0)
                return env;
        const char *home = std::getenv("HOME");
        if (home)
        {
                const std::string venv = std::string(home) + "/.local/share/pipx/venvs/ytm-player/bin/yt-dlp";
                if (access(venv.c_str(), X_OK) == 0)
                        return venv;
        }
        return "yt-dlp"; // PATH
}

std::optional<std::vector<YtmResult>> ytm_search(const std::string &query, int limit)
{
        // PT: --flat-playlist é rápido mas traz duration "NA" às vezes;
        //     tratamos NA como 0. / EN: --flat-playlist is fast but may
        //     yield "NA" durations; we map NA to 0.
        // ES: --flat-playlist es rápido pero trae duration "NA" a veces.
        // ZH: --flat-playlist 快，但 duration 可能是 "NA"；按 0 处理。
        // PT: \t AQUI É TAB REAL (0x09): o compilador converte o escape no
        // literal e o shell preserva entre aspas; o yt-dlp então emite tabs
        // reais. '\\\\t' (literal) faria o yt-dlp imprimir "\t" — o parse
        // quebra. Armadilha do port por cópia do source. / EN: \t here is a
        // REAL TAB (0x09): the compiler converts the escape, the shell keeps
        // it inside quotes, yt-dlp then emits real tabs.
        // ZH: 此处的 \t 是真实 TAB（0x09）：编译器转换字面转义，shell 在引号内
        // 保留，yt-dlp 因此输出真实制表符。'\\\\t'（字面反斜杠 t）会让输出变成
        // 字面 "\\t" —— 解析即断裂。
        const std::string term = "ytsearch" + std::to_string(limit) + ":" + query;
        const std::string cmd = ytdlp_path() + " --flat-playlist --no-warnings --ignore-errors "
                                "--print \"%(id)s\t%(title)s\t%(uploader)s\t%(duration)s\" " +
                                shell_quote(term) + " 2>/dev/null";
        const std::string out = run_capture(cmd);
        if (out.empty())
        {
                krt::log::warn("ytm: busca vazia / empty search / búsqueda vacía / 搜索为空");
                return std::nullopt;
        }

        std::vector<YtmResult> results;
        std::size_t pos = 0;
        while (pos < out.size())
        {
                std::size_t eol = out.find('\n', pos);
                if (eol == std::string::npos)
                        eol = out.size();
                const std::string line = out.substr(pos, eol - pos);
                pos = eol + 1;
                if (line.empty())
                        continue;

                const auto t1 = line.find('\t');
                const auto t2 = t1 == std::string::npos ? std::string::npos : line.find('\t', t1 + 1);
                const auto t3 = t2 == std::string::npos ? std::string::npos : line.find('\t', t2 + 1);
                if (t1 == std::string::npos || t2 == std::string::npos)
                        continue; // PT: linha malformada / malformed line / 畸形行

                YtmResult r;
                r.video_id = line.substr(0, t1);
                r.title = line.substr(t1 + 1, t2 - t1 - 1);
                r.artist = t3 == std::string::npos ? "" : line.substr(t2 + 1, t3 - t2 - 1);
                const std::string dur = t3 == std::string::npos ? "" : line.substr(t3 + 1);
                try
                {
                        r.duration_seconds = std::stoi(dur); // PT: "NA" lança -> 0 / "NA" throws -> 0
                }
                catch (...)
                {
                        r.duration_seconds = 0;
                }
                results.push_back(std::move(r));
        }
        if (results.empty())
                return std::nullopt;
        return results;
}

std::optional<std::string> ytm_download(const std::string &video_id, const std::string &dest_base)
{
        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(dest_base).parent_path(), ec);
        if (ec)
        {
                krt::log::warn("ytm: dir falhou / dir failed / dir falló / 目录失败: {}", dest_base);
                return std::nullopt;
        }
        // PT: -o dest_base.%(ext)s: o yt-dlp escolhe a extensão real; o
        //     after_move:filepath devolve o caminho FINAL pós-processamento.
        // EN: -o dest_base.%(ext)s: yt-dlp picks the real extension;
        //     after_move:filepath returns the FINAL post-processing path.
        // ES: -o dest_base.%(ext)s: el yt-dlp elige la extensión real.
        // ZH: -o dest_base.%(ext)s：扩展名由 yt-dlp 决定；
        //     after_move:filepath 返回后处理后的最终路径。
        const std::string url = "https://www.youtube.com/watch?v=" + video_id;
        const std::string cmd = ytdlp_path() + " -f bestaudio/best --no-playlist --no-warnings "
                                "--ignore-errors --print after_move:filepath -o " +
                                shell_quote(dest_base + ".%(ext)s") + " " + shell_quote(url) + " 2>/dev/null";
        const std::string out = run_capture(cmd);
        std::size_t end = out.find('\n');
        if (end == std::string::npos)
                end = out.size();
        const std::string path = out.substr(0, end);
        if (path.empty() || access(path.c_str(), F_OK) != 0)
        {
                krt::log::warn("ytm: download falhou / download failed / descarga falló / 下载失败: {}", video_id);
                return std::nullopt;
        }
        return path;
}

} // namespace net
