/**
 * @file providers.hpp
 * @brief Metaprogramação da camada ytm: concepts de providers e fallback
 *        resolvido em tempo de compilação (recursão variádica).
 *
 * Um Translator satisfaz: name() -> string_view e
 * translate(TranslationRequest) -> Task<optional<string>>.
 * translate_with_fallback tenta os providers na ordem dada; o texto é
 * compartilhado via shared_ptr<const string> (sem cópias por tentativa).
 */
#pragma once

#include "async.hpp"

#include <concepts>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace ytm
{

/** Pedido de tradução: texto imutável compartilhado + idioma alvo + fonte (opcional). */
struct TranslationRequest
{
        std::shared_ptr<const std::string> text;
        std::string target_lang;
        std::string source_hint; ///< ""/"auto" = provider detecta; caso contrário ISO 639-1
};

template <typename P>
concept Translator = requires(const P &p, TranslationRequest req) {
        {
                p.name()
                } -> std::convertible_to<std::string_view>;
        {
                p.translate(req)
        } -> std::same_as<Task<std::optional<std::string>>>;
};

/**
 * Tenta cada provider em ordem; retorna o primeiro sucesso.
 * Uma falha (nullopt/exception) promove o próximo da lista.
 */
template <Translator First, Translator... Rest>
Task<std::optional<std::string>> translate_with_fallback(TranslationRequest req, First first, Rest... rest)
{
        std::optional<std::string> r;
        try
        {
                r = co_await first.translate(req);
        }
        catch (...)
        {
                r = std::nullopt;
        }
        if (r && !r->empty())
                co_return std::move(*r);

        if constexpr (sizeof...(Rest) > 0)
        {
                co_return co_await translate_with_fallback(std::move(req), std::move(rest)...);
        }
        else
        {
                co_return std::optional<std::string>{};
        }
}

/** Conveniência: empacota o texto e dispara a cadeia. */
template <Translator... Ts>
Task<std::optional<std::string>> translate_text(std::string_view text, std::string target, Ts... providers)
{
        TranslationRequest req{std::make_shared<const std::string>(text), std::move(target),
                               "auto"};
        co_return co_await translate_with_fallback(std::move(req), std::move(providers)...);
}

/** Mesma cadeia, mas com idioma-fonte explícito (ex.: "en" p/ MyMemory, que
 *  não aceita detecção automática). Providers que detectam sozinhos ignoram. */
template <Translator... Ts>
Task<std::optional<std::string>> translate_text_from(std::string_view text, std::string target, std::string source,
                                                     Ts... providers)
{
        TranslationRequest req{std::make_shared<const std::string>(text), std::move(target),
                               std::move(source)};
        co_return co_await translate_with_fallback(std::move(req), std::move(providers)...);
}

} // namespace ytm
