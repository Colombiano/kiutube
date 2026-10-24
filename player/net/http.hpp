// PT: HTTP síncrono mínimo (libcurl) para a engine de letras: GET com
// redirecionamentos, timeout e User-Agent do kiutube. Rede SÍNCRONA —
// chamar sempre via krt::offload (nunca na thread do scheduler).
// EN: Minimal synchronous HTTP (libcurl) for the lyrics engine: GET with
// redirects, timeout and kiutube's User-Agent. SYNCHRONOUS networking —
// always call through krt::offload (never on the scheduler thread).
// ES: HTTP síncrono mínimo (libcurl) para el motor de letras: GET con
// redirecciones, timeout y User-Agent de kiutube. Red SÍNCRONA — llamar
// siempre vía krt::offload (nunca en el hilo del scheduler).
// ZH: 歌词引擎用的最小同步 HTTP（libcurl）：GET、跟随重定向、超时与
// kiutube User-Agent。网络为同步 —— 务必经 krt::offload 调用（切勿在
// 调度器线程上运行）。
#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

using namespace std::chrono_literals;

namespace net
{

// PT: GET simples: resposta completa ou nullopt em falha/timeout.
// EN: Simple GET: full response body or nullopt on failure/timeout.
// ES: GET simple: cuerpo completo o nullopt en caso de fallo/timeout.
// ZH: 简单 GET：返回完整响应体；失败/超时返回 nullopt。
[[nodiscard]] std::optional<std::string> get(std::string_view url, std::chrono::seconds timeout = 10s);

} // namespace net
