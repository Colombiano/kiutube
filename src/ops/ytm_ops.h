/**
 * @file ytm_ops.h
 * @brief Busca e download no YouTube Music a partir da TUI (tecla F8).
 *
 * kiutube: as operações de rede rodam em threads detached (padrão de
 * library_ops.c); o tick consome os resultados via flags no Model.
 */

#ifndef YTM_OPS_H
#define YTM_OPS_H

#include "common/model.h"

/**
 * @brief Envia a query do prompt (state->ui.ytm_search_text) p/ busca YTM.
 *
 * Desativa o prompt, mostra status e dispara a thread de busca.
 * Sem-op se já houver busca/download em andamento ou query vazia.
 */
void ytm_submit_search(void);

/**
 * @brief Inicia o download da faixa YTM da linha escolhida.
 *
 * Dispara thread detached; o tick consome ytm_download_ready.
 * Sem-op se já houver download em andamento ou linha inválida.
 *
 * @param model
 * @param row índice em model->ytm_results
 */
void ytm_start_download(Model *model, int row);

/**
 * @brief Cria a FileSystemEntry standalone do arquivo baixado e aponta
 * state->ui.current_search_entry para ela (enqueue em seguida via
 * MSG_ENQUEUEANDPLAY).
 *
 * @return 0 em sucesso, -1 em falha de alocação
 */
int ytm_prepare_downloaded_entry(Model *model);

/**
 * @brief Nº de resultados ativos na SEARCH_VIEW (YTM se ytm_results_active,
 * senão a busca fuzzy normal).
 */
int ytm_effective_results_count(const Model *model);

#endif /* YTM_OPS_H */
