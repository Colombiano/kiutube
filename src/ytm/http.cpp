/**
 * @file http.cpp
 * @brief Implementação do wrapper libcurl (RAII, move-only).
 */
#include "http.hpp"

#include <curl/curl.h>
#include <cstdio>
#include <utility>

namespace ytm
{

namespace
{

size_t write_to_string(char *ptr, size_t size, size_t nmemb, void *userdata)
{
        auto *out = static_cast<std::string *>(userdata);
        out->append(ptr, size * nmemb);
        return size * nmemb;
}

size_t write_to_file(char *ptr, size_t size, size_t nmemb, void *userdata)
{
        return std::fwrite(ptr, size, nmemb, static_cast<FILE *>(userdata));
}

} // namespace

struct CurlHandle::Impl
{
        CURL *h = nullptr;
};

CurlHandle::CurlHandle() : impl_(new Impl)
{
        impl_->h = curl_easy_init();
        if (!impl_->h)
        {
                delete impl_;
                impl_ = nullptr;
                throw CurlError("curl_easy_init falhou");
        }
        curl_easy_setopt(impl_->h, CURLOPT_USERAGENT, "kiutube/1.0 (+libcurl)");
        curl_easy_setopt(impl_->h, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(impl_->h, CURLOPT_MAXREDIRS, 5L);
        curl_easy_setopt(impl_->h, CURLOPT_ACCEPT_ENCODING, "gzip");
        curl_easy_setopt(impl_->h, CURLOPT_WRITEFUNCTION, write_to_string);
}

CurlHandle::~CurlHandle()
{
        if (impl_)
        {
                curl_easy_cleanup(impl_->h);
                delete impl_;
        }
}

CurlHandle::CurlHandle(CurlHandle &&other) noexcept : impl_(std::exchange(other.impl_, nullptr))
{
}

CurlHandle &CurlHandle::operator=(CurlHandle &&other) noexcept
{
        if (this != &other)
        {
                if (impl_)
                {
                        curl_easy_cleanup(impl_->h);
                        delete impl_;
                }
                impl_ = std::exchange(other.impl_, nullptr);
        }
        return *this;
}

std::string CurlHandle::get(const std::string &url, const std::vector<std::string> &headers, long timeout_s)
{
        struct curl_slist *hl = nullptr;
        for (const auto &h : headers)
                hl = curl_slist_append(hl, h.c_str());

        std::string body;
        curl_easy_setopt(impl_->h, CURLOPT_URL, url.c_str());
        curl_easy_setopt(impl_->h, CURLOPT_HTTPGET, 1L);
        curl_easy_setopt(impl_->h, CURLOPT_HTTPHEADER, hl);
        curl_easy_setopt(impl_->h, CURLOPT_TIMEOUT, timeout_s);
        curl_easy_setopt(impl_->h, CURLOPT_WRITEDATA, &body);

        CURLcode rc = curl_easy_perform(impl_->h);
        if (hl)
                curl_slist_free_all(hl);
        if (rc != CURLE_OK)
                throw CurlError(std::string("GET ") + url + ": " + curl_easy_strerror(rc));
        return body;
}

std::string CurlHandle::post(const std::string &url, const std::string &body,
                             const std::vector<std::string> &headers, long timeout_s)
{
        struct curl_slist *hl = nullptr;
        for (const auto &h : headers)
                hl = curl_slist_append(hl, h.c_str());

        std::string resp;
        curl_easy_setopt(impl_->h, CURLOPT_URL, url.c_str());
        curl_easy_setopt(impl_->h, CURLOPT_POST, 1L);
        curl_easy_setopt(impl_->h, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(impl_->h, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
        curl_easy_setopt(impl_->h, CURLOPT_HTTPHEADER, hl);
        curl_easy_setopt(impl_->h, CURLOPT_TIMEOUT, timeout_s);
        curl_easy_setopt(impl_->h, CURLOPT_WRITEDATA, &resp);

        CURLcode rc = curl_easy_perform(impl_->h);
        if (hl)
                curl_slist_free_all(hl);
        if (rc != CURLE_OK)
                throw CurlError(std::string("POST ") + url + ": " + curl_easy_strerror(rc));
        return resp;
}

bool CurlHandle::download(const std::string &url, const std::string &out_path, long timeout_s)
{
        FILE *f = std::fopen(out_path.c_str(), "wb");
        if (!f)
                return false;

        curl_easy_setopt(impl_->h, CURLOPT_URL, url.c_str());
        curl_easy_setopt(impl_->h, CURLOPT_HTTPGET, 1L);
        curl_easy_setopt(impl_->h, CURLOPT_HTTPHEADER, nullptr);
        curl_easy_setopt(impl_->h, CURLOPT_TIMEOUT, timeout_s);
        curl_easy_setopt(impl_->h, CURLOPT_WRITEFUNCTION, write_to_file);
        curl_easy_setopt(impl_->h, CURLOPT_WRITEDATA, f);

        CURLcode rc = curl_easy_perform(impl_->h);
        std::fclose(f);

        curl_easy_setopt(impl_->h, CURLOPT_WRITEFUNCTION, write_to_string);
        if (rc != CURLE_OK)
        {
                std::string err = curl_easy_strerror(rc);
                long http_dbg = 0;
                curl_easy_getinfo(impl_->h, CURLINFO_RESPONSE_CODE, &http_dbg);
                std::fprintf(stderr, "ytm: download falhou (%s, http %ld): %s\n", url.c_str(), http_dbg, err.c_str());
                return false;
        }
        long http = 0;
        curl_easy_getinfo(impl_->h, CURLINFO_RESPONSE_CODE, &http);
        return http >= 200 && http < 300;
}

std::string CurlHandle::escape(const std::string &s)
{
        char *e = curl_easy_escape(impl_->h, s.c_str(), static_cast<int>(s.size()));
        if (!e)
                return s;
        std::string out(e);
        curl_free(e);
        return out;
}

} // namespace ytm
