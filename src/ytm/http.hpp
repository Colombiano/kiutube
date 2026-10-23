/**
 * @file http.hpp
 * @brief Cliente HTTP mínimo sobre libcurl — RAII, move-only.
 */
#pragma once

#include <stdexcept>
#include <string>
#include <vector>

namespace ytm
{

class CurlError : public std::runtime_error
{
      public:
        explicit CurlError(const std::string &what) : std::runtime_error(what)
        {
        }
};

/**
 * Handle CURL em propriedade exclusiva. Não copiável, transferível via move.
 * Uma instância por requisição sequencial; não é thread-safe por si só
 * (o uso via ThreadPool é seguro — cada corrotina tem o seu).
 */
class CurlHandle
{
      public:
        CurlHandle();
        ~CurlHandle();

        CurlHandle(CurlHandle &&other) noexcept;
        CurlHandle &operator=(CurlHandle &&other) noexcept;
        CurlHandle(const CurlHandle &) = delete;
        CurlHandle &operator=(const CurlHandle &) = delete;

        /** GET; lança CurlError em falha. */
        [[nodiscard]] std::string get(const std::string &url, const std::vector<std::string> &headers = {},
                                      long timeout_s = 15);

        /** POST com body; lança CurlError em falha. */
        [[nodiscard]] std::string post(const std::string &url, const std::string &body,
                                       const std::vector<std::string> &headers = {}, long timeout_s = 15);

        /** Baixa url para out_path (substitui). true em sucesso. */
        bool download(const std::string &url, const std::string &out_path, long timeout_s = 180);

        /** URL-encode de parâmetros. */
        [[nodiscard]] std::string escape(const std::string &s);

      private:
        struct Impl;
        Impl *impl_; // pimpl: isola o include <curl/curl.h> do resto da camada
};

} // namespace ytm
