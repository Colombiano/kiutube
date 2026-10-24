// PT: Scheduler de corrotinas do kiutube2: substitui o loop GLib do kew
//     (g_timeout_add + GIOChannel). Run-loop epoll com: heap de timers,
//     eventfd para wake thread-safe e fds de I/O. Invariante central: NENHUMA
//     corrotina retoma fora da thread do run-loop (krt::complete -> post).
// EN: kiutube2 coroutine scheduler: replaces kew's GLib loop (g_timeout_add +
//     GIOChannel). epoll run-loop with: timer heap, thread-safe eventfd wake
//     and I/O fds. Central invariant: NO coroutine ever resumes outside the
//     run-loop thread (krt::complete -> post).
// ES: Scheduler de corrutinas de kiutube2: reemplaza el loop GLib de kew
//     (g_timeout_add + GIOChannel). Run-loop epoll con: heap de timers,
//     eventfd para wake thread-safe y fds de E/S. Invariante central: NINGUNA
//     corrutina retoma fuera del hilo del run-loop (krt::complete -> post).
// ZH: kiutube2 协程调度器：替代 kew 的 GLib 主循环（g_timeout_add +
//     GIOChannel）。基于 epoll 的 run-loop：定时器堆、线程安全的 eventfd
//     唤醒以及 I/O fd。核心不变式：任何协程都不会在 run-loop 线程之外恢复
//     （krt::complete -> post）。
#pragma once

#include "krt/task.hpp"

#include <chrono>
#include <coroutine>
#include <cstdint>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace krt
{

// PT: Implementa o hook de conclusão (ver task.hpp). / EN: Implements the completion hook.
// ES: Implementa el hook de conclusión. / ZH: 实现完成 hook（见 task.hpp）。
void set_completion_hook(CompletionHook fn);
void complete(std::coroutine_handle<> h);

class Scheduler
{
      public:
        static Scheduler &instance();

        Scheduler(const Scheduler &) = delete;
        Scheduler &operator=(const Scheduler &) = delete;

        /**
         * PT: Roda o loop até request_stop(). Só retorna quando não há mais
         *     timers nem handles pendentes. Instala o completion hook.
         * EN: Runs the loop until request_stop(). Only returns when there are
         *     no pending timers or handles. Installs the completion hook.
         * ES: Ejecuta el loop hasta request_stop(). Solo retorna cuando no
         *     hay timers ni handles pendientes. Instala el completion hook.
         * ZH: 运行循环直到 request_stop()。仅当没有待处理的定时器或 handle 时才
         *     返回。安装完成 hook。
         */
        void run();

        // PT: Pedido de parada (thread-safe). / EN: Stop request (thread-safe).
        // ES: Petición de parada (thread-safe). / ZH: 停止请求（线程安全）。
        void request_stop() noexcept;

        // PT: Enfileira um handle para retomar na thread do loop + acorda o epoll.
        // EN: Enqueues a handle to resume on the loop thread + wakes epoll.
        // ES: Encola un handle para reanudar en el hilo del loop + despierta el epoll.
        // ZH: 将 handle 入队以便在 loop 线程上恢复，并唤醒 epoll。
        void post(std::coroutine_handle<> h);

        // PT: Enfileira um callback genérico para rodar na thread do loop
        //     (ex.: janela gráfica fechada pelo usuário). Thread-safe.
        // EN: Enqueues a generic callback to run on the loop thread (e.g. the
        //     GUI window closed by the user). Thread-safe.
        // ES: Encola un callback genérico para correr en el hilo del loop.
        // ZH: 将通用回调入队以在 loop 线程上执行（如用户关闭图形窗口）。
        //     线程安全。
        void post_function(std::function<void()> fn);

        // PT: Agenda retomada em deadline (thread-safe; usado por krt::sleep).
        // EN: Schedules a resume at a deadline (thread-safe; used by krt::sleep).
        // ES: Agenda reanudación en deadline (thread-safe; usado por krt::sleep).
        // ZH: 安排在截止时间恢复（线程安全；krt::sleep 使用）。
        void add_timer(std::chrono::steady_clock::time_point at, std::coroutine_handle<> h);

        // PT: Registra EPOLLIN one-shot para fd; UMA corrotina por fd (M0: stdin).
        // EN: Registers one-shot EPOLLIN for fd; ONE coroutine per fd (M0: stdin).
        // ES: Registra EPOLLIN one-shot para fd; UNA corrutina por fd (M0: stdin).
        // ZH: 为 fd 注册 one-shot EPOLLIN；每个 fd 一个协程（M0：stdin）。
        void add_fd(int fd, std::coroutine_handle<> h);

      private:
        Scheduler();
        ~Scheduler();

        using time_point = std::chrono::steady_clock::time_point;

        void wake() noexcept;
        void drain_wake() noexcept;
        [[nodiscard]] int timeout_for(time_point now) const; // -1 = sem timers / no timers / sin timers / 无定时器

        int epollfd_ = -1;
        int eventfd_ = -1;
        std::mutex mtx_;
        std::vector<std::coroutine_handle<>> posted_; // sob mtx_ / under mtx_ / bajo mtx_ / 受 mtx_ 保护
        std::vector<std::function<void()>> funcs_;    // PT: callbacks p/ thread do loop / loop-thread callbacks / 等待 loop 线程的回调
        std::vector<std::pair<time_point, std::coroutine_handle<>>> timers_; // min-heap / sob mtx_ / bajo mtx_ / 受 mtx_ 保护
        std::unordered_map<int, std::coroutine_handle<>> fds_; // aguardando EPOLLIN / awaiting EPOLLIN / esperando EPOLLIN / 等待 EPOLLIN
        bool stopping_ = false;
};

// ---- Awaitables ----

// PT: Suspende a corrotina por `ms`. / EN: Suspends the coroutine for `ms`.
// ES: Suspende la corrutina por `ms`. / ZH: 将协程挂起 `ms` 毫秒。
[[nodiscard]] Task<void> sleep(std::chrono::milliseconds ms);

// PT: Suspende até fd ficar legível (one-shot). / EN: Suspends until fd is readable (one-shot).
// ES: Suspende hasta que fd sea legible (one-shot). / ZH: 挂起直到 fd 可读（one-shot）。
[[nodiscard]] Task<void> fd_readable(int fd);

} // namespace krt
