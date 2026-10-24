// PT: Implementação da Engine. Estrutura: audio thread (callback do
//     miniaudio) consome o ring lock-free; feeder (scheduler) produz; UI
//     chama play/pause/next/seek/volume sob um mutex curto. miniaudio só
//     existe neste TU (IMPLEMENTATION) e no decoder.cpp (declarações).
// EN: Engine implementation. Structure: the audio thread (miniaudio callback)
//     consumes the lock-free ring; the feeder (scheduler) produces; the UI
//     calls play/pause/next/seek/volume under a short mutex. miniaudio only
//     exists in this TU (IMPLEMENTATION) and decoder.cpp (declarations).
// ES: Implementación de la Engine. Estructura: el hilo de audio (callback de
//     miniaudio) consume el ring lock-free; el feeder (scheduler) produce; la
//     UI llama play/pause/next/seek/volume bajo un mutex corto. miniaudio
//     solo existe en este TU (IMPLEMENTATION) y en decoder.cpp (declaraciones).
// ZH: 引擎实现。结构：音频线程（miniaudio 回调）消费 lock-free 环形缓冲；
//     feeder（调度器）生产；UI 在短互斥锁下调用 play/pause/next/seek/
//     volume。miniaudio 只存在于本 TU（IMPLEMENTATION）与 decoder.cpp（声明）。
#include "audio/engine.hpp"

#include "krt/log.hpp"
#include "krt/scheduler.hpp"

#include <miniaudio.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <deque>
#include <mutex>
#include <vector>

namespace audio
{

// PT: Capacidade do ring em floats: 2^18 = 262144 ≈ 2,7s de stereo 48kHz.
// EN: Ring capacity in floats: 2^18 = 262144 ≈ 2.7s of stereo 48kHz.
// ES: Capacidad del ring en floats: 2^18 = 262144 ≈ 2,7s de stereo 48kHz.
// ZH: 环形缓冲容量（float）：2^18 = 262144 ≈ 48kHz 立体声 2.7 秒。
constexpr std::size_t kRingCap = 1u << 18;

struct Engine::Impl
{
        // PT: Contexto/device miniaudio. / EN: miniaudio context/device.
        // ES: Contexto/device de miniaudio. / ZH: miniaudio context/device。
        ma_context context{};
        ma_device device{};
        bool device_ok = false;
        int dev_rate = 48000;

        // PT: Ring SPSC: audio thread consome (rd), feeder produz (wr).
        // EN: SPSC ring: the audio thread consumes (rd), the feeder produces (wr).
        // ES: Ring SPSC: el hilo de audio consume (rd), el feeder produce (wr).
        // ZH: SPSC 环形缓冲：音频线程消费（rd），feeder 生产（wr）。
        std::vector<float> ring = std::vector<float>(kRingCap, 0.0f);
        std::atomic<std::size_t> rd{0};
        std::atomic<std::size_t> wr{0};
        std::atomic<long long> pos_us{0};
        std::atomic<unsigned> underruns{0};
        // PT: Fim do app: feeder para de agendar timers (run-loop termina).
        // EN: App end: the feeder stops scheduling timers (run-loop can finish).
        // ES: Fin de la app: el feeder deja de agendar timers (run-loop termina).
        // ZH: 应用结束：feeder 停止注册定时器（run-loop 得以结束）。
        std::atomic<bool> quit_{false};
        // PT: Geração do ring: seek/next invalidam cópias em vo da audio thread.
        // EN: Ring generation: seek/next invalidate in-flight copies on the audio thread.
        // ES: Generación del ring: seek/next invalidan copias en vuelo del hilo de audio.
        // ZH: 环形缓冲代次：seek/next 使音频线程进行中的拷贝失效。
        std::atomic<unsigned long long> gen{0};

        // PT: Estado compartilhado feeder<->UI (mutex curto, sem I/O).
        // EN: Shared feeder<->UI state (short mutex, no I/O).
        // ES: Estado compartido feeder<->UI (mutex corto, sin E/S).
        // ZH: feeder<->UI 共享状态（短互斥锁，无 I/O）。
        std::mutex m;
        std::deque<std::string> queue;
        MaDecoder cur;
        bool has_dec = false;    // PT: slot do decoder (libera no EOF p/ gapless).
                                 // EN: decoder slot (freed at EOF for gapless).
                                 // ES: slot del decoder (se libera en EOF para gapless).
                                 // ZH: 解码器槽位（EOF 时释放以实现 gapless）。
        bool has_playing = false; // PT: o que o ring ainda está tocando. / EN: what the ring is still playing.
        TrackInfo playing_info;   // ES: lo que el ring aún está sonando. / ZH: 环形缓冲仍在播放的曲目。
        float volume = 1.0f;
        bool paused = true;

        // PT: Audio thread entry: copia do ring para o output do device.
        // EN: Audio thread entry: copies from the ring to the device output.
        // ES: Entrada del hilo de audio: copia del ring a la salida del device.
        // ZH: 音频线程入口：从环形缓冲拷贝到设备输出。
        void consume(float *out, std::size_t frames)
        {
                const auto g0 = gen.load(std::memory_order_acquire);
                const std::size_t total = frames * 2;
                const std::size_t r = rd.load(std::memory_order_acquire);
                const std::size_t w = wr.load(std::memory_order_acquire);
                const std::size_t avail = w - r; // unsigned wrap-safe: wr só avança
                const std::size_t n = std::min(total, avail);
                if (n > 0 && g0 == gen.load(std::memory_order_acquire))
                {
                        const std::size_t r0 = r % kRingCap;
                        const std::size_t first = std::min(n, kRingCap - r0);
                        std::memcpy(out, ring.data() + r0, first * sizeof(float));
                        if (n > first)
                                std::memcpy(out + first, ring.data(), (n - first) * sizeof(float));
                        rd.store(r + n, std::memory_order_release);
                        pos_us.store(pos_us.load() + static_cast<long long>(n / 2) * 1'000'000 / dev_rate,
                                     std::memory_order_relaxed);
                }
                if (n < total || g0 != gen.load(std::memory_order_acquire))
                {
                        // PT: Underrun OU seek/next em vo: silêncio neste callback.
                        // EN: Underrun OR in-flight seek/next: silence this callback.
                        // ES: Underrun O seek/next en vuelo: silencio en este callback.
                        // ZH: underrun 或进行中的 seek/next：本次回调输出静音。
                        std::memset(out, 0, total * sizeof(float));
                        if (n < total)
                                underruns.fetch_add(1, std::memory_order_relaxed);
                }
        }

        // PT: Feeder: garante faixa atual (abre fora do lock), enche o ring.
        //     Retorna true se há atividade (para o sleep do caller).
        // EN: Feeder: ensures the current track (opens outside the lock), fills
        //     the ring. Returns true if there is activity (for the caller's sleep).
        // ES: Feeder: garantiza la pista actual (abre fuera del lock), llena el
        //     ring. Devuelve true si hay actividad (para el sleep del caller).
        // ZH: feeder：确保当前曲目（在锁外打开）、填充环形缓冲。返回是否有
        //     活动（供调用方决定睡眠时间）。
        bool pump()
        {
                if (paused || !device_ok)
                        return false;

                // PT: Espaço liver no ring (deixa margem para não lotar). / EN: Free ring space.
                // ES: Espacio libre en el ring. / ZH: 环形缓冲剩余空间。
                const std::size_t used = wr.load(std::memory_order_relaxed) - rd.load(std::memory_order_acquire);
                const std::size_t free_frames = (kRingCap - used) / 2;
                if (free_frames < 4096)
                        return true;

                // PT: Abre a próxima faixa se não há atual. / EN: Opens the next track if none current.
                // ES: Abre la siguiente pista si no hay actual. / ZH: 若当前无曲目则打开下一首。
                std::string next_path;
                bool need_open = false;
                {
                        std::lock_guard lk(m);
                        if (!has_dec)
                        {
                                if (queue.empty())
                                        return false;
                                next_path = queue.front();
                                need_open = true;
                        }
                }
                if (need_open)
                {
                        MaDecoder d;
                        if (!d.open(next_path, dev_rate))
                        {
                                std::lock_guard lk(m);
                                if (!queue.empty())
                                        queue.pop_front();
                                return true;
                        }
                        std::lock_guard lk(m);
                        if (!queue.empty())
                                queue.pop_front();
                        cur = std::move(d);
                        has_dec = true;
                        playing_info = cur.info(); // PT: o ring passa a tocar esta faixa (gapless: só troca aqui).
                        has_playing = true;        // EN: the ring now plays this track (gapless: only changes here).
                        pos_us.store(0, std::memory_order_relaxed);
                        krt::log::info("audio: now playing / tocando / sonando / 正在播放: {}", playing_info.title);
                }

                // PT: Lê do decoder atual direto para o ring (dois pedaços se
                //     cruzar a borda). read() é CPU-bound (sem I/O bloqueante).
                // EN: Reads from the current decoder straight into the ring (two
                //     chunks if it wraps). read() is CPU-bound (no blocking I/O).
                // ES: Lee del decoder actual directo al ring (dos trozos si
                //     cruza el borde). read() es CPU-bound (sin E/S bloqueante).
                // ZH: 从当前解码器直接读入环形缓冲（回绕时分两段）。read() 为
                //     CPU 密集（无阻塞 I/O）。
                const std::size_t w = wr.load(std::memory_order_relaxed);
                const std::size_t frames_wanted = std::min<std::size_t>(free_frames, 16384);
                std::size_t produced = 0;
                {
                        std::lock_guard lk(m);
                        if (!has_dec)
                                return true;
                        const std::size_t w0 = w % kRingCap;
                        const std::size_t first_frames = std::min(frames_wanted, (kRingCap - w0) / 2);
                        const std::size_t n1 = cur.read({ring.data() + w0, first_frames * 2});
                        produced = n1;
                        if (produced < frames_wanted)
                        {
                                const std::size_t n2 =
                                    cur.read({ring.data(), (frames_wanted - produced) * 2});
                                produced += n2;
                        }
                        if (cur.at_end())
                        {
                                // PT: Gapless: libera o decoder, mas playing_info
                                //     segue até a próxima faixa abrir (o ring ainda
                                //     toca esta). current() não fica vazio no meio.
                                // EN: Gapless: frees the decoder, but playing_info
                                //     stays until the next track opens (the ring is
                                //     still playing this one). current() never
                                //     goes empty in between.
                                // ES: Gapless: libera el decoder, pero playing_info
                                //     sigue hasta que abra la siguiente pista (el
                                //     ring aún la suena). current() no queda vacío.
                                // ZH: Gapless：释放解码器，但 playing_info 保持到下一首
                                //     打开（环形缓冲仍在播放它）。current() 不会在中途
                                //     变空。
                                has_dec = false;
                        }
                }
                wr.store(w + produced, std::memory_order_release);
                return true;
        }
};

// PT: Chamado pela audio thread do miniaudio: nunca bloqueia, nunca falha —
//     underrun vira silêncio (contabilizado para debug). Definido DEPOIS do
//     Impl porque precisa do tipo completo.
// EN: Called by the miniaudio audio thread: never blocks, never fails —
//     underrun becomes silence (accounted for debugging). Defined AFTER
//     Impl because it needs the complete type.
// ES: Llamado por el hilo de audio de miniaudio: nunca bloquea, nunca falla —
//     el underrun se vuelve silencio (contabilizado para debug). Definido
//     DESPUÉS del Impl porque necesita el tipo completo.
// ZH: 由 miniaudio 音频线程调用：绝不阻塞、绝不失败 —— underrun 记为静音
//     （计数供调试）。定义在 Impl 之后，因为需要完整类型。
void data_callback(ma_device *device, void *output, const void *, ma_uint32 frame_count)
{
        auto *self = static_cast<Engine::Impl *>(device->pUserData);
        self->consume(static_cast<float *>(output), frame_count);
}

Engine::Engine() : impl_(std::make_unique<Impl>())
{
        if (ma_context_init(nullptr, 0, nullptr, &impl_->context) != MA_SUCCESS)
        {
                krt::log::error("audio: ma_context_init falhou / failed / fallo / 失败");
                return;
        }
        ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
        cfg.playback.format = ma_format_f32;
        cfg.playback.channels = 2;
        cfg.sampleRate = 48000;
        cfg.dataCallback = data_callback;
        cfg.pUserData = impl_.get();
        if (ma_device_init(&impl_->context, &cfg, &impl_->device) != MA_SUCCESS)
        {
                krt::log::error("audio: ma_device_init falhou / failed / fallo / 失败");
                ma_context_uninit(&impl_->context);
                return;
        }
        impl_->dev_rate = static_cast<int>(impl_->device.sampleRate);
        impl_->device_ok = true;
        krt::log::info("audio: device ok ({} Hz, {} ch)", impl_->dev_rate,
                       impl_->device.playback.channels);
}

Engine::~Engine()
{
        if (impl_->device_ok)
        {
                ma_device_uninit(&impl_->device);
                ma_context_uninit(&impl_->context);
        }
}

bool Engine::enqueue(const std::string &path)
{
        std::lock_guard lk(impl_->m);
        impl_->queue.push_back(path);
        return true;
}

std::size_t Engine::queue_size() const
{
        std::lock_guard lk(impl_->m);
        return impl_->queue.size();
}

bool Engine::play()
{
        if (!impl_->device_ok)
        {
                krt::log::error("audio: play sem device / without device / sin device / 无设备");
                return false;
        }
        std::lock_guard lk(impl_->m);
        if (!impl_->paused)
                return true;
        const ma_result rc = ma_device_start(&impl_->device);
        if (rc != MA_SUCCESS)
        {
                krt::log::error("audio: ma_device_start falhou / failed / fallo / 失败: {}", static_cast<int>(rc));
                return false;
        }
        impl_->paused = false;
        krt::log::info("audio: play / playing / reproduciendo / 播放中");
        return true;
}

bool Engine::pause()
{
        if (!impl_->device_ok)
                return false;
        std::lock_guard lk(impl_->m);
        if (impl_->paused)
                return true;
        ma_device_stop(&impl_->device);
        impl_->paused = true;
        krt::log::info("audio: pause / pausiert / pausa / 已暂停");
        return true;
}

bool Engine::next()
{
        std::lock_guard lk(impl_->m);
        impl_->has_dec = false;
        impl_->has_playing = false;
        impl_->playing_info = TrackInfo{};
        impl_->cur = MaDecoder{};
        impl_->pos_us.store(0, std::memory_order_relaxed);
        impl_->gen.fetch_add(1, std::memory_order_acq_rel);
        impl_->rd.store(0, std::memory_order_release);
        impl_->wr.store(0, std::memory_order_release);
        return true;
}

bool Engine::seek_by(std::chrono::seconds delta)
{
        std::lock_guard lk(impl_->m);
        if (!impl_->has_dec)
                return false;
        const auto dur = impl_->playing_info.duration;
        const auto cur = std::chrono::microseconds(impl_->pos_us.load());
        const auto target = std::clamp(cur + delta, std::chrono::microseconds{0}, dur);
        if (!impl_->cur.seek(target))
                return false;
        impl_->pos_us.store(target.count(), std::memory_order_relaxed);
        impl_->gen.fetch_add(1, std::memory_order_acq_rel);
        impl_->rd.store(0, std::memory_order_release);
        impl_->wr.store(0, std::memory_order_release);
        return true;
}

void Engine::set_volume(float v01)
{
        const float v = std::clamp(v01, 0.0f, 1.0f);
        std::lock_guard lk(impl_->m);
        impl_->volume = v;
        if (impl_->device_ok)
                ma_device_set_master_volume(&impl_->device, v);
}

float Engine::volume() const
{
        std::lock_guard lk(impl_->m);
        return impl_->volume;
}

TrackInfo Engine::current() const
{
        std::lock_guard lk(impl_->m);
        return impl_->has_playing ? impl_->playing_info : TrackInfo{};
}

std::chrono::microseconds Engine::position() const
{
        return std::chrono::microseconds(impl_->pos_us.load(std::memory_order_relaxed));
}

bool Engine::is_playing() const
{
        std::lock_guard lk(impl_->m);
        return impl_->device_ok && !impl_->paused;
}

krt::Task<void> Engine::feeder_task()
{
        krt::log::info("audio: feeder iniciado / feeder started / feeder iniciado / feeder 已启动");
        while (!impl_->quit_.load(std::memory_order_acquire))
        {
                const bool active = impl_->pump();
                krt::log::debug("audio: pump active={}", active);
                co_await krt::sleep(std::chrono::milliseconds(active ? 8 : 100));
        }
        krt::log::info("audio: feeder encerrado / feeder stopped / feeder cerrado / feeder 已结束");
}

void Engine::shutdown() noexcept
{
        impl_->quit_.store(true, std::memory_order_release);
}

} // namespace audio
