/**
 * @file translate.hpp
 * @brief Tradução de letras: Google Translate (endpoint não-oficial, primário)
 *        e MyMemory (fallback oficial com limite diário).
 *
 * Ambos satisfazem o concept ytm::Translator (providers.hpp).
 */
#pragma once

#include "async.hpp"
#include "providers.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace ytm
{

/** Google Translate via client=gtx (endpoint usado pelo próprio site). */
struct GoogleTranslator
{
        [[nodiscard]] std::string_view name() const
        {
                return "google";
        }
        Task<std::optional<std::string>> translate(TranslationRequest req) const;
};

/** MyMemory — https://api.mymemory.translated.net/get (grátis, c/ limite).
 *  Não faz detecção de idioma: usa source_hint do request (default "en";
 *  "auto" é resolvido para "en"). */
struct MyMemoryTranslator
{
        [[nodiscard]] std::string_view name() const
        {
                return "mymemory";
        }
        Task<std::optional<std::string>> translate(TranslationRequest req) const;
};

/**
 * Traduz texto para o idioma alvo usando a cadeia Google -> MyMemory
 * (fallback em tempo de compilação via translate_with_fallback).
 */
Task<std::optional<std::string>> translate_to(const std::string &text, const std::string &target_lang);

/** Igual, mas selecionável por engine: "mymemory" (só MyMemory) ou "google" (cadeia).
 *  source: idioma-fonte ("auto" -> detecção onde suportado; MyMemory resolve para "en"). */
Task<std::optional<std::string>> translate_to_engine(const std::string &text, const std::string &target_lang,
                                                     const std::string &engine, std::string source);

} // namespace ytm
