// PT: Engine de áudio do kiutube2: device miniaudio (RAII) + ring lock-free
//     SPSC + feeder corrotina no scheduler. Gapless por construção: ao fim da
//     faixa o feeder abre a próxima SEM parar o device — o ring nunca é drenado
//     entre faixas. pause/next/seek/volume são thread-safe (mutex pequeno).
// EN: kiutube2 audio engine: miniaudio device (RAII) + lock-free SPSC ring +
//     scheduler coroutine feeder. Gapless by construction: at track end the
//     feeder opens the next one WITHOUT stopping the device — the ring is
//     never drained between tracks. pause/next/seek/volume are thread-safe
//     (small mutex).
// ES: Engine de audio de kiutube2: device miniaudio (RAII) + ring lock-free
//     SPSC + feeder corrutina en el scheduler. Gapless por construcción: al
//     final de la pista el feeder abre la siguiente SIN parar el device — el
//     ring nunca se drena entre pistas. pause/next/seek/volume son
//     thread-safe (mutex pequeño).
// ZH: kiutube2 音频引擎：miniaudio 设备（RAII）+ lock-free SPSC 环形缓冲 +
//     调度器协程 feeder。按构造实现 gapless：曲目结束时 feeder 打开下一首
//     而不停止设备 —— 曲目之间环形缓冲不被排空。pause/next/seek/volume
//     线程安全（小互斥锁）。
#pragma once

#include "audio/decoder.hpp"
#include "krt/task.hpp"

#include <chrono>
#include <cstddef>
#include <memory>
#include <string>

namespace audio
{

class Engine
{
      public:
        Engine();
        ~Engine();
        Engine(const Engine &) = delete;
        Engine &operator=(const Engine &) = delete;

        // PT: Fila e transporte (thread-safe). / EN: Queue and transport (thread-safe).
        // ES: Cola y transporte (thread-safe). / ZH: 队列与传输控制（线程安全）。
        bool enqueue(const std::string &path);
        [[nodiscard]] std::size_t queue_size() const;
        bool play();                                        // inicia/retoma o device
        bool pause();
        bool next();                                        // pula faixa atual (draina o ring)
        bool seek_by(std::chrono::microseconds delta);      // seek relativo, clampado
        void set_volume(float v01);                         // 0.0 .. 1.0
        [[nodiscard]] float volume() const;

        [[nodiscard]] TrackInfo current() const;            // path vazio = nenhuma faixa
        [[nodiscard]] std::chrono::microseconds position() const;
        [[nodiscard]] bool is_playing() const;

        // PT: Drena o tap de visualização (frames stereo que o device tocou).
        //     Thread-safe SPSC; chamar do scheduler (~20x/s). Retorna frames.
        // EN: Drains the visualization tap (stereo frames the device played).
        //     Thread-safe SPSC; call from the scheduler (~20/s). Returns frames.
        // ES: Drena el tap de visualización. Thread-safe SPSC; llamar desde
        //     el scheduler (~20/s).
        // ZH: 排空可视化 tap（设备已播放的立体声帧）。线程安全 SPSC；由调度器
        //     调用（约 20 次/秒）。返回帧数。
        [[nodiscard]] std::size_t read_viz(std::span<float> out);

        // PT: Corrotina produtora: roda no scheduler, enche o ring a partir do
        //     decoder atual. Para quando shutdown() for chamado (fim do app) —
        //     senão seus timers impediriam o run-loop de terminar.
        // EN: Producer coroutine: runs on the scheduler, fills the ring from
        //     the current decoder. Stops when shutdown() is called (app end) —
        //     otherwise its timers would keep the run-loop alive forever.
        // ES: Corrutina productora: corre en el scheduler, llena el ring desde
        //     el decoder actual. Para cuando shutdown() es llamado (fin de la
        //     app) — si no, sus timers impedirían que el run-loop termine.
        // ZH: 生产者协程：运行在调度器上，从当前解码器填充环形缓冲。shutdown()
        //     被调用后停止 —— 否则它的定时器会让 run-loop 永远无法结束。
        [[nodiscard]] krt::Task<void> feeder_task();

        // PT: Sinaliza fim do app: o feeder para de agendar timers e o
        //     run-loop pode terminar. Idempotente, thread-safe.
        // EN: Signals app end: the feeder stops scheduling timers so the run
        //     loop can finish. Idempotent, thread-safe.
        // ES: Señala fin de la app: el feeder deja de agendar timers y el run
        //     loop puede terminar. Idempotente, thread-safe.
        // ZH: 标记应用结束：feeder 停止注册定时器，run-loop 得以结束。
        //     幂等且线程安全。
        void shutdown() noexcept;

        // PT: pimpl da engine (definição só no .cpp). Pública-incompleta porque
        //     o callback C do miniaudio precisa nomear o tipo.
        // EN: engine pimpl (definition only in the .cpp). Public-incomplete
        //     because miniaudio's C callback must name the type.
        // ES: pimpl de la engine (definición solo en el .cpp). Pública-incompleta
        //     porque el callback C de miniaudio necesita nombrar el tipo.
        // ZH: 引擎 pimpl（定义仅在 .cpp）。public-incomplete：miniaudio 的 C 回调
        //     需要命名该类型。
        struct Impl;

      private:
        std::unique_ptr<Impl> impl_;
};

} // namespace audio
