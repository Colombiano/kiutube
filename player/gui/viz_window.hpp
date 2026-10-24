// PT: Janela gráfica do visualizador (Qt 6 + QCustomPlot 2.1.1, vendored
//     em third_party/, GPL-3). Roda numa thread dedicada: o QApplication e
//     todos os widgets vivem e morrem lá; o kiutube (scheduler) continua
//     dono do fluxo. Amostras vêm do MESMO tap SPSC do engine — por isso a
//     TUI suspende o próprio visualizador CLI enquanto a janela está aberta
//     (o tap é SPSC: um consumidor por vez).
// EN: GUI visualizer window (Qt 6 + QCustomPlot 2.1.1, vendored under
//     third_party/, GPL-3). Runs on a dedicated thread: QApplication and all
//     widgets live and die there; kiutube (scheduler) keeps owning the
//     flow. Samples come from the SAME engine SPSC tap — that is why the
//     TUI suspends its own CLI visualizer while the window is open (the tap
//     is SPSC: one consumer at a time).
// ES: Ventana gráfica del visualizador (Qt 6 + QCustomPlot, vendored). Corre
//     en un hilo dedicado; las muestras vienen del MISMO tap SPSC del engine
//     — la TUI suspende su visualizador CLI mientras la ventana está abierta.
// ZH: 可视化图形窗口（Qt 6 + QCustomPlot 2.1.1，vendored 于 third_party/，
//     GPL-3）。运行在专用线程：QApplication 与所有 widget 在该线程内创建
//     与销毁；kiutube（调度器）仍主导流程。采样来自引擎同一个 SPSC
//     tap —— 因此窗口打开期间 TUI 暂停自身的 CLI 可视化器（tap 为
//     SPSC：同一时刻仅一个消费者）。
#pragma once

#include "tui/visualizer.hpp"

#include <atomic>
#include <functional>
#include <memory>

namespace audio
{
class Engine;
}

namespace gui
{

// PT: Controle thread-safe. show() cria a thread na primeira chamada;
//     set_mode(Off) faz a janela se fechar (poll de um timer interno,
//     sem invokeMethod — sem corridas com o teardown). Fechar pela janela
//     (X) dispara on_closed NO SCHEDULER via krt::Scheduler::post_function.
// EN: Thread-safe control. show() spawns the thread on first call;
//     set_mode(Off) makes the window close itself (an internal timer
//     polls the mode — no invokeMethod, no teardown races). Closing from
//     the window (X) fires on_closed ON THE SCHEDULER via
//     krt::Scheduler::post_function.
// ES: Control thread-safe. show() crea el hilo; set_mode(Off) cierra la
//     ventana (un timer interno hace poll). Cerrar por la ventana dispara
//     on_closed EN EL SCHEDULER vía post_function.
// ZH: 线程安全控制。show() 首次调用时创建线程；set_mode(Off) 使窗口自行
//     关闭（内部定时器轮询模式，无 invokeMethod，规避 teardown 竞争）。
//     从窗口（X）关闭会在调度器上经 post_function 触发 on_closed。
class VizWindow
{
      public:
        VizWindow(audio::Engine &engine, std::function<void()> on_closed);
        ~VizWindow();
        VizWindow(const VizWindow &) = delete;
        VizWindow &operator=(const VizWindow &) = delete;

        void show();
        void set_mode(viz::Mode m);
        [[nodiscard]] bool is_open() const;

      private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
};

} // namespace gui
