/**
 * @file ytm_bridge.h
 * @brief Ponte extern "C" entre o código C do kew e a camada C++ ytm.
 *
 * Toda a API é bloqueante (internamente usa corrotinas + thread pool).
 * Strings devolvidas são malloc'ed; liberar com ytm_free().
 */
#ifndef YTM_BRIDGE_H
#define YTM_BRIDGE_H

#ifdef __cplusplus
extern "C"
{
#endif

        typedef struct
        {
                char *video_id;
                char *title;
                char *artist;
                double duration;
        } YtmTrack;

        /** Busca no YouTube Music. Retorna nº de resultados; *out malloc'ed. -1 em erro. */
        int ytm_search_blocking(const char *query, int limit, YtmTrack **out);

        /** Resolve URL do stream de áudio. Retorna string malloc'ed ou NULL. */
        char *ytm_resolve_stream_blocking(const char *video_id);

        /** Letra LRC (LRCLIB) para artista/título. malloc'ed ou NULL. */
        char *ytm_fetch_lyrics_blocking(const char *artist, const char *title);

        /** Igual, com duração em segundos (>0) p/ filtrar música homônima. */
        char *ytm_fetch_lyrics_ex_blocking(const char *artist, const char *title, int duration_seconds);

        /** Tradução da letra inteira para PT (Google -> MyMemory, c/ cache). malloc'ed ou NULL. */
        char *ytm_translate_blocking(const char *text_lrc);

        /** Baixa thumb para o cache; retorna caminho malloc'ed ou NULL. */
        char *ytm_fetch_thumb_blocking(const char *video_id, const char *thumb_url);

        /** 0/1: tradução habilitada pela config (init.lua). */
        int ytm_translation_enabled(void);

        /** Entry point da CLI headless: kew --ytm <comando>. */
        int ytm_cli_main(int argc, char **argv);

        /** libera qualquer ponteiro devolvido por esta API (incluindo arrays de YtmTrack). */
        void ytm_free(void *ptr);

#ifdef __cplusplus
}
#endif

#endif /* YTM_BRIDGE_H */
