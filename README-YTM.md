# Kiutube — fork do kew com YouTube Music, letras e tradução PT

Fork do [kew](https://github.com/ravachol/kew) (ravachol, GPL-2) com uma camada
C++20 (`src/ytm/`) que agrega o que o ytm-player propicia — streaming do
YouTube Music e letras — mais **tradução automática das letras para português**.

## O que a camada ytm adiciona

| Feature | Como funciona |
|---|---|
| Busca YouTube Music | `yt-dlp` (mesma estratégia do ytm-player) |
| Playback YTM | resolve a URL assinada (`bestaudio`) e **o próprio yt-dlp baixa** pro cache (ver nota sobre IP abaixo), tocando pelo pipeline nativo do kew |
| Letras sincronizadas | LRCLIB (grátis, sem auth) — funciona também p/ faixas **locais** sem `.lrc`; título é limpo ("(1975)", "[LYRICS]"...) e há fallback de busca só por título |
| Tradução PT | Google Translate (não-oficial) → MyMemory (fallback); LRC sai com timestamps preservados |
| Config em Lua | `~/.config/kiutube/init.lua` |

## Uso

```bash
# CLI (funciona sem TUI)
kew --ytm search "songbird bee gees"     # lista resultados
kew --ytm play "songbird bee gees"       # baixa top 5 e ABRE O KEW tocando
kew --ytm play 8xCLvLAIxLg               # por videoId (1 faixa)
kew --ytm lyrics "Bee Gees" "Songbird"   # LRC original + tradução PT

# Na TUI
m          # página de letras (como no kew vanilla)
F8         # prompt de busca no YouTube Music; Enter no resultado baixa e toca
F9         # alterna letra original <-> traduzida (quando há tradução)
```

Faixas YTM baixadas vão para `~/.cache/kiutube/streams/`. O `load_meta_data()`
do kew já busca letra no LRCLIB (com tradução PT) sozinho, então não são
escritos sidecars `.lrc` — a não ser pelo `--ytm play`, que mantém o
comportamento antigo (`.lrc`/`.pt.lrc` lado a lado).
Se o `path=` do `~/.config/kew/kewrc` estiver vazio, o `--ytm play` aponta
ele automaticamente para esse diretório (evita o prompt "Type a path:" e o
cache vira navegável na library view).

## Configuração (Lua)

`~/.config/kiutube/init.lua`:

```lua
translation_enabled = true
translation_target  = "pt"
translation_engine  = "google"   -- "google" (cadeia) | "mymemory"
translation_source  = "auto"     -- MyMemory resolve "auto" -> "en"
search_key              = "F8"   -- tecla do prompt de busca TUI (padrão F8)
toggle_translation_key  = "F9"
cache_dir = getenv("HOME") .. "/.cache/kiutube"
```

Variáveis ausentes caem nos defaults. As teclas em si são configuráveis pelo
`kewrc` como bindings nativas (`bind=F8,ytmSearch`, `bind=F9,toggleTranslation`);
`search_key`/`toggle_translation_key` são espelhos lidos pela camada ytm.

## Arquitetura da camada C++20 (`src/ytm/`)

- **`async.hpp`** — `Task<T>` coroutine **move-only**, awaitable, com
  continuação em `final_suspend`; `ThreadPool` global; `co_await offload(fn)`
  desvia trabalho bloqueante p/ as threads do pool.
- **`http.hpp/cpp`** — `CurlHandle` RAII **move-only** (pimpl), GET/POST/download.
- **`providers.hpp`** — concept `Translator` + fallback em **tempo de
  compilação** (`translate_with_fallback`: recursão variádica, tenta na ordem,
  texto compartilhado via `shared_ptr<const string>`).
- **`ytm.cpp`** — search/resolve/info/download via subprocesso yt-dlp
  (templates `--print`, sem parser de JSON inteiro); retry com re-resolve.
- **`lrclib.cpp` / `translate.cpp`** — letras LRC e tradução chunkada
  (<400 chars/request, limite dos providers), com decodificação `\uXXXX` → UTF-8.
- **`lua_config.cpp`** — máquina Lua 5.4 embutida (sandbox: base/string/table
  + `getenv`), `LuaState` RAII move-only.
- **`ytm_bridge.h/.cpp`** — ponte `extern "C"` bloqueante p/ o código C do kew,
  com cache em disco (letras/traduções/thumbs).
- **`ytm_cli.cpp`** — CLI `--ytm` (search/play/lyrics).

## Integração com o C do kew (patches mínimos)

- `src/kew.c` — hook `--ytm` no `main` (antes de qualquer estado de terminal).
- `src/loader/songdatatype.h` — campo `SongData.translated_lyrics`.
- `src/loader/lyrics.cpp` — `loadLyricsFromBuffer()` (parse LRC de memória).
- `src/loader/song_loader.c` — `load_meta_data()`: sem letra local → LRCLIB
  (com tradução) + sidecar `<base>.pt.lrc`; free/copy dos dois ponteiros.
- `src/common/events.h`, `src/ui/settings.c`, `src/update/update.c` —
  `MSG_TOGGLETRANSLATION` + binding default F9 (swap de ponteiros).
- `src/ops/ytm_ops.c` — busca/download YTM da TUI (F8): threads detached +
  flags consumidas no tick (padrão de `library_ops.c`); entry standalone do
  arquivo baixado ancorada em `Model.ytm_fs_anchor` e liberada no shutdown.
- `src/ytm/ytm_bridge.h/.cpp` — `ytm_download_track_blocking()` e
  `ytm_default_cache_dir()` (cria `<cache_dir>/streams`, destino do CLI).
- `Makefile` — `YTM_*` sources/flags/libs (`libcurl`, `lua5.4`).

## Notas e limitações

- **yt-dlp**: o do Debian fica velho e o YouTube bloqueia ("page needs to be
  reloaded"). A camada procura, nesta ordem: `$YTDLP`, o venv do ytm-player
  (`~/.local/share/pipx/venvs/ytm-player/bin/yt-dlp`), `yt-dlp` no PATH.
- **Download pela assinatura de IP**: a URL do googlevideo é assinada para o
  IP de origem. Com IPv6 de privacidade rotacionando, resolver fora e baixar
  em outro processo dá 403. Por isso o download é feito pelo próprio yt-dlp
  (resolve+download consistentes), com 3 tentativas externas. Em redes
  lentas (~30-40 KB/s) faixas demoram alguns minutos — paciência.
- **Capa**: faixas YTM não têm capa embutida (stream cru). Letras e título
  vêm do nome do arquivo / LRCLIB.
- **Google Translate** pode rate-limitar por IP; o fallback MyMemory assume
  (source `en` quando `auto`). Traduções ficam em cache — cada música é
  traduzida uma vez só.
- Na busca TUI (F8), digitar não filtra os resultados YTM em tempo real
  (a lista é estática até a próxima busca); ESC fecha os resultados e volta
  à view anterior, e o download roda em background — dá p/ continuar usando
  o player enquanto isso.

## Build

```bash
sudo apt install libopusfile-dev libvorbis-dev libogg-dev libtag1-dev \
                 libchafa-dev libgdk-pixbuf-2.0-dev libcurl4-openssl-dev \
                 libfaad-dev   # deps do kew vanilla + libcurl/lua5.4
make -j$(nproc)
sudo make install   # OU, sem sobrescrever o kew do apt:
sudo mkdir -p /usr/local/share/kew
sudo cp -r layouts themes /usr/local/share/kew/
```

> O kew 4.x precisa dos layouts/themes no sistema (`/usr/local/share/kew`
> ou `/usr/share/kew`) — sem eles dá "Latest layout not found" ao abrir.
