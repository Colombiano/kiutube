// PT: MPRIS (org.mpris.MediaPlayer2) via sdbus-c++, numa thread própria
//     com event loop próprio. GLib sai do link por completo. Expõe o básico
//     do Player: Play/Pause/PlayPause/Next/Previous/Seek, propriedades
//     PlaybackStatus/Metadata/Position/Volume, sinal PropertiesChanged na
//     troca de faixa/estado (poller de 1s — engine é thread-safe).
// EN: MPRIS (org.mpris.MediaPlayer2) via sdbus-c++, on its own thread with
//     its own event loop. GLib leaves the link entirely. Exposes the Player
//     basics: Play/Pause/PlayPause/Next/Previous/Seek, PlaybackStatus/
//     Metadata/Position/Volume properties, PropertiesChanged on track/state
//     change (1s poller — the engine is thread-safe).
// ES: MPRIS vía sdbus-c++, en un hilo propio. Expone lo básico del Player
//     con PropertiesChanged en el cambio de pista/estado (poller de 1s).
// ZH: 经 sdbus-c++ 的 MPRIS（org.mpris.MediaPlayer2），运行于带独立事件
//     循环的专用线程。链接中彻底不再有 GLib。暴露 Player 基本接口与
//     PropertiesChanged（1s 轮询 —— 引擎线程安全）。
#pragma once

#include <memory>

namespace audio
{
class Engine;
}

namespace sys
{

// PT: Sem-KIUTUBE2_WITH_MPRIS: stub que não faz nada (mesma API).
// EN: Without KIUTUBE2_WITH_MPRIS: no-op stub (same API).
// ES: Sin KIUTUBE2_WITH_MPRIS: stub sin efecto (misma API).
// ZH: 无 KIUTUBE2_WITH_MPRIS 时：同名 no-op stub。
class Mpris
{
      public:
        explicit Mpris(audio::Engine &engine);
        ~Mpris();
        Mpris(const Mpris &) = delete;
        Mpris &operator=(const Mpris &) = delete;

        [[nodiscard]] bool running() const;

      private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
};

} // namespace sys
