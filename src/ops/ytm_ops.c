/**
 * @file ytm_ops.c
 * @brief Busca e download no YouTube Music a partir da TUI (tecla F8).
 *
 * kiutube: as operações de rede rodam em threads detached (padrão de
 * library_ops.c); o tick consome os resultados via flags no Model.
 * Não chamar dispatch_msg a partir das threads (fila não é thread-safe).
 */

#include "ytm_ops.h"

#include "common/appstate.h"
#include "common/common.h"
#include "data/directorytree.h"
#include "utils/utils.h"
#include "ytm/ytm_bridge.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define YTM_SANITIZED_NAME_MAX 80 /* mesmo cap de sanitize_filename() do CLI */

typedef struct
{
        Model *model;
        char query[64];
} YtmSearchArgs;

typedef struct
{
        Model *model;
        char video_id[16];
        char dest_base[KEW_PATH_MAX];
} YtmDownloadArgs;

/* kiutube: espelha sanitize_filename() de ytm_cli.cpp (ASCII seguro p/ path) */
static void ytm_sanitize_filename(const char *in, char *out, size_t out_size)
{
        size_t j = 0;
        for (size_t i = 0; in != NULL && in[i] != '\0' && j + 1 < out_size; i++) {
                unsigned char c = (unsigned char)in[i];
                if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                    c == ' ' || c == '-' || c == '_' || c == '(' || c == ')' || c == '[' || c == ']' ||
                    c == '.')
                        out[j++] = (char)c;
                else
                        out[j++] = '_';
        }
        out[j] = '\0';

        while (j > 0 && out[j - 1] == ' ')
                out[--j] = '\0';
}

static void *ytm_search_thread(void *arg)
{
        YtmSearchArgs *args = (YtmSearchArgs *)arg;
        Model *model = args->model;

        YtmTrack *tracks = NULL;
        int count = ytm_search_blocking(args->query, YTM_MAX_RESULTS, &tracks);

        pthread_mutex_lock(&(model->state.library_mutex));

        model->ytm_results_count = 0;
        if (count > 0) {
                int n = (count > YTM_MAX_RESULTS) ? YTM_MAX_RESULTS : count;
                for (int i = 0; i < n; i++) {
                        c_strcpy(model->ytm_results[i].video_id,
                                 tracks[i].video_id ? tracks[i].video_id : "",
                                 sizeof(model->ytm_results[i].video_id));
                        c_strcpy(model->ytm_results[i].title,
                                 tracks[i].title ? tracks[i].title : "",
                                 sizeof(model->ytm_results[i].title));
                        c_strcpy(model->ytm_results[i].artist,
                                 tracks[i].artist ? tracks[i].artist : "",
                                 sizeof(model->ytm_results[i].artist));
                        model->ytm_results[i].duration = (int)(tracks[i].duration + 0.5);
                }
                model->ytm_results_count = n;
        }

        model->ytm_searching = false;
        model->ytm_results_ready = true;

        pthread_mutex_unlock(&(model->state.library_mutex));

        ytm_free(tracks);
        free(args);

        return NULL;
}

static void *ytm_download_thread(void *arg)
{
        YtmDownloadArgs *args = (YtmDownloadArgs *)arg;
        Model *model = args->model;

        char *path = ytm_download_track_blocking(args->video_id, args->dest_base);

        pthread_mutex_lock(&(model->state.library_mutex));

        if (path != NULL) {
                c_strcpy(model->ytm_downloaded_path, path, sizeof(model->ytm_downloaded_path));
                ytm_free(path);
        } else {
                model->ytm_downloaded_path[0] = '\0';
        }

        model->ytm_download_ready = true;

        pthread_mutex_unlock(&(model->state.library_mutex));

        free(args);

        return NULL;
}

void ytm_submit_search(void)
{
        Model *model = get_model();

        if (model->ytm_searching || model->ytm_downloading)
                return;

        const char *query = model->state.ui.ytm_search_text;
        if (query[0] == '\0') {
                model->state.ui.ytm_search_prompt = false;
                return;
        }

        YtmSearchArgs *args = malloc(sizeof(YtmSearchArgs));
        if (!args)
                return;

        args->model = model;
        c_strcpy(args->query, query, sizeof(args->query));

        c_strcpy(model->ytm_query, query, sizeof(model->ytm_query));
        model->state.ui.ytm_search_prompt = false;
        model->ytm_searching = true;
        set_error_message("Buscando no YouTube Music...");

        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, ytm_search_thread, args) != 0) {
                perror("Failed to create thread");
                model->ytm_searching = false;
                free(args);
                return;
        }
        pthread_detach(thread_id);
}

void ytm_start_download(Model *model, int row)
{
        if (model->ytm_downloading || model->ytm_searching)
                return;

        if (row < 0 || row >= model->ytm_results_count)
                return;

        YtmDownloadArgs *args = malloc(sizeof(YtmDownloadArgs));
        if (!args)
                return;

        const YtmSearchResult *track = &model->ytm_results[row];

        char *cache_dir = ytm_default_cache_dir();
        if (cache_dir == NULL) {
                free(args);
                set_error_message("Falha ao baixar do YouTube Music");
                return;
        }

        char artist[YTM_SANITIZED_NAME_MAX + 1];
        char title[YTM_SANITIZED_NAME_MAX + 1];
        ytm_sanitize_filename(track->artist, artist, sizeof(artist));
        ytm_sanitize_filename(track->title, title, sizeof(title));

        snprintf(args->dest_base, sizeof(args->dest_base), "%s/%s - %s [%.6s]",
                 cache_dir, artist, title, track->video_id);
        free(cache_dir);

        args->model = model;
        c_strcpy(args->video_id, track->video_id, sizeof(args->video_id));

        model->ytm_downloading = true;
        set_error_message("Baixando do YouTube Music...");

        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, ytm_download_thread, args) != 0) {
                perror("Failed to create thread");
                model->ytm_downloading = false;
                free(args);
                return;
        }
        pthread_detach(thread_id);
}

int ytm_prepare_downloaded_entry(Model *model)
{
        const char *path = model->ytm_downloaded_path;
        if (path[0] == '\0')
                return -1;

        char dir[KEW_PATH_MAX];
        c_strcpy(dir, path, sizeof(dir));
        char *slash = strrchr(dir, '/');
        const char *file_name = slash ? slash + 1 : dir;
        if (slash)
                *slash = '\0';

        /* kiutube: entries standalone, fora da árvore da library. O parent é
         necessário p/ enqueue_song() (child->parent->is_enqueued). Encadeadas
         via ->next no anchor, liberadas no model_shutdown(). */
        FileSystemEntry *parent = create_entry("YouTube Music", 1, NULL, time(NULL));
        if (parent == NULL)
                return -1;

        set_full_path(parent, dir, "YouTube Music");

        FileSystemEntry *entry = create_entry(file_name, 0, parent, time(NULL));
        if (entry == NULL)
                return -1;

        entry->full_path = strdup(path);
        if (entry->full_path == NULL)
                return -1;

        add_child(parent, entry);

        parent->next = model->ytm_fs_anchor;
        model->ytm_fs_anchor = parent;

        model->state.ui.current_search_entry = entry;

        return 0;
}

int ytm_effective_results_count(const Model *model)
{
        if (model->ytm_results_active)
                return model->ytm_results_count;

        return model->state.ui.search_results_count;
}
