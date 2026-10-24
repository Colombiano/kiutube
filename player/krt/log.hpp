// PT: Logging mínimo do kiutube2 (stderr, com timestamp e nível). Convenção:
//     logs internos ficam em EN (audiência = desenvolvedor); nível vem de
//     KIUTUBE2_LOG=debug|info|warn|error (padrão: warn).
// EN: Minimal kiutube2 logging (stderr, timestamped, leveled). Convention:
//     internal logs stay in EN (audience = developer); level comes from
//     KIUTUBE2_LOG=debug|info|warn|error (default: warn).
// ES: Logging mínimo de kiutube2 (stderr, con timestamp y nivel). Convención:
//     logs internos quedan en EN (audiencia = desarrollador); el nivel viene
//     de KIUTUBE2_LOG=debug|info|warn|error (por defecto: warn).
// ZH: kiutube2 的最小日志（stderr，带时间戳与级别）。约定：内部日志使用英文（受众
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
        const std::string line = std::format("[kiutube2 {}.{:03d} {}] {}", tbuf,
                                             static_cast<int>(ms.count()), level_name(lv),
                                             std::format(fmt, std::forward<A>(args)...));
        std::fprintf(stderr, "%s\n", line.c_str());
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
