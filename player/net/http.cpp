// PT: Implementação do GET síncrono (CurlHandle RAII — padrão do ytm,
//     refeito enxuto). Só existe síncrono de propósito: o caller faz
//     co_await krt::offload(...).
// EN: Synchronous GET implementation (RAII CurlHandle — the ytm pattern,
//     redone lean). It is synchronous on purpose: the caller does
//     co_await krt::offload(...).
// ES: Implementación del GET síncrono (CurlHandle RAII — el patrón de ytm,
//     rehecho enjuto). Es síncrono a propósito: el caller hace
//     co_await krt::offload(...).
// ZH: 同步 GET 实现（RAII CurlHandle —— ytm 的模式，精简重写）。刻意为
//     同步：调用方负责 co_await krt::offload(...)。
#include "net/http.hpp"

#include <curl/curl.h>

#include <stdexcept>
#include <utility>

namespace net
{
namespace
{

// PT: CurlHandle RAII move-only (padrão de src/ytm/http.cpp).
// EN: Move-only RAII CurlHandle (pattern from src/ytm/http.cpp).
// ES: CurlHandle RAII move-only (patrón de src/ytm/http.cpp).
// ZH: move-only RAII CurlHandle（源自 src/ytm/http.cpp 的模式）。
class CurlHandle
{
      public:
        CurlHandle() : h_(curl_easy_init())
        {
                if (!h_)
                        throw std::runtime_error("curl_easy_init failed");
                curl_easy_setopt(h_, CURLOPT_USERAGENT, "kiutube/1.0 (+libcurl)");
                curl_easy_setopt(h_, CURLOPT_FOLLOWLOCATION, 1L);
                curl_easy_setopt(h_, CURLOPT_MAXREDIRS, 5L);
                curl_easy_setopt(h_, CURLOPT_ACCEPT_ENCODING, "gzip");
                curl_easy_setopt(h_, CURLOPT_WRITEFUNCTION,
                                 +[](char *ptr, size_t size, size_t nmemb, void *userdata) {
                                         auto *out = static_cast<std::string *>(userdata);
                                         out->append(ptr, size * nmemb);
                                         return size * nmemb;
                                 });
        }
        ~CurlHandle()
        {
                if (h_)
                        curl_easy_cleanup(h_);
        }
        CurlHandle(CurlHandle &&o) noexcept : h_(std::exchange(o.h_, nullptr))
        {
        }
        CurlHandle(const CurlHandle &) = delete;
        CurlHandle &operator=(const CurlHandle &) = delete;

        [[nodiscard]] CURL *get() const
        {
                return h_;
        }

      private:
        CURL *h_;
};

} // namespace

std::optional<std::string> get(std::string_view url, std::chrono::seconds timeout)
{
        CurlHandle curl;
        std::string body;
        const std::string url_str(url);
        curl_easy_setopt(curl.get(), CURLOPT_URL, url_str.c_str());
        curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, static_cast<long>(timeout.count()));
        curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &body);
        if (curl_easy_perform(curl.get()) != CURLE_OK)
                return std::nullopt;
        long status = 0;
        curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &status);
        if (status < 200 || status >= 300)
                return std::nullopt;
        return body;
}

} // namespace net
