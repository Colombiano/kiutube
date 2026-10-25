// PT: Logging mínimo do kiutube (stderr, com timestamp e nível). Convenção:
//     logs internos ficam em EN (audiência = desenvolvedor); nível vem de
//     KIUTUBE2_LOG=debug|info|warn|error (padrão: warn).
// EN: Minimal kiutube logging (stderr, timestamped, leveled). Convention:
//     internal logs stay in EN (audience = developer); level comes from
//     KIUTUBE2_LOG=debug|info|warn|error (default: warn).
// ES: Logging mínimo de kiutube (stderr, con timestamp y nivel). Convención:
//     logs internos quedan en EN (audiencia = desarrollador); el nivel viene
//     de KIUTUBE2_LOG=debug|info|warn|error (por defecto: warn).
// ZH: kiutube 的最小日志（stderr，带时间戳与级别）。约定：内部日志使用英文（受众
//     为开发者）；级别由 KIUTUBE2_LOG=debug|info|warn|error 控制（默认 warn）。
#pragma once

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <format>
#include <string>
#include <string_view>

namespace krt::log
{

enum class Level
{
        Debug,
        Info,
        Warn,
        Error
};

// PT: Nível ativo (lê KIUTUBE2_LOG uma vez). / EN: Active level (reads KIUTUBE2_LOG once).
// ES: Nivel activo (lee KIUTUBE2_LOG una vez). / ZH: 当前级别（只读取一次 KIUTUBE2_LOG）。
inline Level level()
{
        static const Level lv = [] {
                const char *env = std::getenv("KIUTUBE2_LOG");
                if (!env)
                        return Level::Warn;
                const std::string_view s(env);
                if (s == "debug")
                        return Level::Debug;
                if (s == "info")
                        return Level::Info;
                if (s == "error")
                        return Level::Error;
                return Level::Warn;
        }();
        return lv;
}

inline const char *level_name(Level l)
{
        switch (l)
        {
        case Level::Debug:
                return "DEBUG";
        case Level::Info:
                return "INFO";
        case Level::Warn:
                return "WARN";
        case Level::Error:
                return "ERROR";
        }
        return "?";
}

// PT: Sink alternativo: use_file() troca a saída para um arquivo (a TUI
//     redireciona stderr para lá — logs e warnings de bibliotecas Qt/gtk
//     não corrompem mais a tela). A troca acontece no boot (main, uma
//     thread); depois o ponteiro só é lido.
// EN: Alternate sink: use_file() swaps the output to a file (the TUI
//     redirects stderr to it — logs and Qt/gtk library warnings no
//     longer corrupt the screen). The swap happens at boot (main,
//     single-threaded); afterwards the pointer is only read.
// ZH: 备用输出：use_file() 将日志切换到文件（TUI 会把 stderr 重定向到该
//     文件——日志与 Qt/gtk 库的警告不再破坏屏幕）。切换发生在启动
//     （main，单线程）；之后只读。
inline std::FILE *&sink_slot()
{
        static std::FILE *f = stderr;
        return f;
}

// PT: Abre `path` (append) e passa a gravar nele. false = sem permissão.
// EN: Opens `path` (append) and starts writing there. false = no access.
// ZH: 以追加模式打开 path 并切换到该文件输出；失败返回 false。
inline bool use_file(const std::string &path)
{
        if (std::FILE *f = std::fopen(path.c_str(), "a"))
        {
                sink_slot() = f;
                return true;
        }
        return false;
}

template <typename... A>
void write(Level lv, std::format_string<A...> fmt, A &&...args)
{
        if (static_cast<int>(lv) < static_cast<int>(level()))
                return;
        const auto now = std::chrono::system_clock::now();
        const auto t = std::chrono::system_clock::to_time_t(now);
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
        char tbuf[16] = {0};
        std::strftime(tbuf, sizeof(tbuf), "%H:%M:%S", std::localtime(&t));
        const std::string line = std::format("[kiutube {}.{:03d} {}] {}", tbuf,
                                             static_cast<int>(ms.count()), level_name(lv),
                                             std::format(fmt, std::forward<A>(args)...));
        std::fprintf(sink_slot(), "%s\n", line.c_str());
        std::fflush(sink_slot());
}

template <typename... A>
void debug(std::format_string<A...> fmt, A &&...args)
{
        write(Level::Debug, fmt, std::forward<A>(args)...);
}

template <typename... A>
void info(std::format_string<A...> fmt, A &&...args)
{
        write(Level::Info, fmt, std::forward<A>(args)...);
}

template <typename... A>
void warn(std::format_string<A...> fmt, A &&...args)
{
        write(Level::Warn, fmt, std::forward<A>(args)...);
}

template <typename... A>
void error(std::format_string<A...> fmt, A &&...args)
{
        write(Level::Error, fmt, std::forward<A>(args)...);
}

} // namespace krt::log
