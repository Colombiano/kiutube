// PT: Corrotina Task<T> move-only + offload() — fundação do scheduler krt.
//     Diferença crucial vs. a versão da camada ytm: toda continuação passa por
//     krt::complete(), que roteia para o hook do scheduler quando ele está
//     ativo — assim NENHUMA corrotina retoma em thread errada (pool/worker).
// EN: Move-only Task<T> coroutine + offload() — foundation of the krt scheduler.
//     Crucial difference from the ytm layer version: every continuation goes
//     through krt::complete(), which routes to the scheduler hook when set —
//     so NO coroutine ever resumes on the wrong thread (pool/worker).
// ES: Corrutina Task<T> move-only + offload() — fundación del scheduler krt.
//     Diferencia crucial vs. la versión de la capa ytm: toda continuación pasa
//     por krt::complete(), que enruta al hook del scheduler cuando está activo:
//     ninguna corrutina retoma en un hilo incorrecto (pool/worker).
// ZH: move-only 协程 Task<T> + offload() —— krt 调度器的基础。与 ytm 层版本的关键
//     区别：所有续体都经由 krt::complete()，当调度器 hook 激活时会被路由到调度器
//     —— 任何协程都不会在错误的线程（pool/worker）上恢复。
#pragma once

#include "krt/thread_pool.hpp"

#include <atomic>
#include <coroutine>
#include <exception>
#include <functional>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace krt
{

// PT: Hook de conclusão instalado pelo Scheduler::run(). Quando ativo, recebe
//     TODO handle que seria retomado inline; nullptr = retomada inline (legado).
// EN: Completion hook installed by Scheduler::run(). When set, it receives EVERY
//     handle that would otherwise be resumed inline; nullptr = inline (legacy).
// ES: Hook de conclusión instalado por Scheduler::run(). Cuando está activo,
//     recibe TODO handle que se retomaría inline; nullptr = inline (legado).
// ZH: 由 Scheduler::run() 安装的完成 hook。激活后，所有本应内联恢复的 handle 都会
//     交给它；nullptr 表示保持内联恢复（旧行为）。
using CompletionHook = void (*)(std::coroutine_handle<>);

void set_completion_hook(CompletionHook fn);
void complete(std::coroutine_handle<> h); // hook ou retomada inline / hook or inline resume / hook o reanudación inline / hook 或内联恢复

namespace detail
{

// PT: awaitable genérico: roda F() no pool e completa o esperador pelo hook
//     (thread-safe por construção quando o scheduler está rodando).
// EN: Generic awaitable: runs F() on the pool and completes the waiter through
//     the hook (thread-safe by construction while the scheduler runs).
// ES: awaitable genérico: ejecuta F() en el pool y completa al que espera por
//     el hook (thread-safe por construcción con el scheduler en marcha).
// ZH: 通用 awaitable：在 pool 上执行 F()，并通过 hook 完成等待方（调度器运行时
//     按构造即是线程安全的）。
template <typename R, typename F>
struct OffloadOp
{
        F fn_;
        std::optional<R> result_;
        std::exception_ptr error_;

        explicit OffloadOp(F fn) : fn_(std::move(fn))
        {
        }

        OffloadOp(OffloadOp &&) = delete;
        OffloadOp &operator=(OffloadOp &&) = delete;

        [[nodiscard]] bool await_ready() const noexcept
        {
                return false;
        }

        void await_suspend(std::coroutine_handle<> h)
        {
                ThreadPool::instance().post([this, h] {
                        try
                        {
                                result_.emplace(std::move(fn_)());
                        }
                        catch (...)
                        {
                                error_ = std::current_exception();
                        }
                        complete(h);
                });
        }

        R await_resume()
        {
                if (error_)
                        std::rethrow_exception(error_);
                return std::move(*result_);
        }
};

// PT: Especialização para F() -> void. / EN: Specialization for F() -> void.
// ES: Especialización para F() -> void. / ZH: F() -> void 的特化。
template <typename F>
struct OffloadOp<void, F>
{
        F fn_;
        std::exception_ptr error_;

        explicit OffloadOp(F fn) : fn_(std::move(fn))
        {
        }

        OffloadOp(OffloadOp &&) = delete;
        OffloadOp &operator=(OffloadOp &&) = delete;

        [[nodiscard]] bool await_ready() const noexcept
        {
                return false;
        }

        void await_suspend(std::coroutine_handle<> h)
        {
                ThreadPool::instance().post([this, h] {
                        try
                        {
                                std::move(fn_)();
                        }
                        catch (...)
                        {
                                error_ = std::current_exception();
                        }
                        complete(h);
                });
        }

        void await_resume()
        {
                if (error_)
                        std::rethrow_exception(error_);
        }
};

} // namespace detail

// PT: Desvia um callable bloqueante para o pool: `auto r = co_await offload([&]{...});`
// EN: Offloads a blocking callable to the pool: `auto r = co_await offload([&]{...});`
// ES: Desvía un callable bloqueante al pool: `auto r = co_await offload([&]{...});`
// ZH: 将阻塞型 callable 转移到 pool：`auto r = co_await offload([&]{...});`
template <typename F>
auto offload(F &&f)
{
        using Fn = std::decay_t<F>;
        using R = std::invoke_result_t<Fn>;
        return detail::OffloadOp<R, Fn>(std::forward<F>(f));
}

/**
 * PT: promise base comum: estado de conclusão + continuação a repassar.
 * EN: Common promise base: completion state + continuation to hand off.
 * ES: promise base común: estado de conclusión + continuación a repasar.
 * ZH: 公共 promise 基类：完成状态 + 待交接的续体。
 */
struct PromiseState
{
        std::exception_ptr error_;
        std::atomic<bool> done_{false};
        std::coroutine_handle<> continuation_;
        bool started_ = false;
};

template <typename T>
class Task
{
      public:
        struct promise_type;

        struct FinalAwait
        {
                [[nodiscard]] bool await_ready() const noexcept
                {
                        return false;
                }
                // PT: Corrotina terminou: marca done e devolve a execução por
                //     krt::complete() (hook do scheduler ou inline).
                // EN: Coroutine finished: marks done and hands execution back
                //     through krt::complete() (scheduler hook or inline).
                // ES: Corrutina terminada: marca done y devuelve la ejecución
                //     por krt::complete() (hook del scheduler o inline).
                // ZH: 协程结束：标记 done 并通过 krt::complete()（调度器 hook 或内联）
                //     交还执行权。
                void await_suspend(std::coroutine_handle<promise_type> h) noexcept
                {
                        h.promise().done_.store(true, std::memory_order_release);
                        if (h.promise().continuation_)
                                complete(h.promise().continuation_);
                }
                void await_resume() noexcept
                {
                }
        };

        struct promise_type : PromiseState
        {
                std::optional<T> value_;

                Task get_return_object()
                {
                        return Task(std::coroutine_handle<promise_type>::from_promise(*this));
                }
                [[nodiscard]] std::suspend_always initial_suspend() noexcept
                {
                        return {};
                }
                [[nodiscard]] FinalAwait final_suspend() noexcept
                {
                        return {};
                }
                void return_value(T v)
                {
                        value_.emplace(std::move(v));
                }
                void unhandled_exception()
                {
                        error_ = std::current_exception();
                }
        };

        using handle_t = std::coroutine_handle<promise_type>;

        Task(Task &&other) noexcept : h_(std::exchange(other.h_, {}))
        {
        }
        Task &operator=(Task &&other) noexcept
        {
                if (this != &other)
                {
                        if (h_)
                                h_.destroy();
                        h_ = std::exchange(other.h_, {});
                }
                return *this;
        }
        Task(const Task &) = delete;
        Task &operator=(const Task &) = delete;
        ~Task()
        {
                if (h_)
                        h_.destroy();
        }

        // PT: Dispara a corrotina (na thread do scheduler). Sem-op se já iniciada.
        // EN: Starts the coroutine (on the scheduler thread). No-op if started.
        // ES: Dispara la corrutina (en el hilo del scheduler). Sin efecto si iniciada.
        // ZH: 启动协程（在调度器线程上）。若已启动则为无操作。
        void start()
        {
                if (h_ && !h_.promise().started_)
                {
                        h_.promise().started_ = true;
                        h_.resume();
                }
        }

        [[nodiscard]] bool done() const
        {
                return !h_ || h_.promise().done_.load(std::memory_order_acquire);
        }

        // ---- awaitable: co_await de Task dentro de outra corrotina ----

        [[nodiscard]] bool await_ready() const noexcept
        {
                return false;
        }
        void await_suspend(std::coroutine_handle<> waiter)
        {
                h_.promise().continuation_ = waiter;
                start();
        }
        T await_resume()
        {
                return take_value();
        }

        // PT: Roda a corrotina e bloqueia até o fim. NUNCA na thread do scheduler
        //     (a conclusão só é entregue no loop — deadlock). Ponte p/ C em M2+.
        // EN: Runs the coroutine and blocks until the end. NEVER on the scheduler
        //     thread (completion is only delivered in the loop — deadlock).
        //     Bridge for C code in M2+.
        // ES: Ejecuta la corrutina y bloquea hasta el final. NUNCA en el hilo del
        //     scheduler (la conclusión solo se entrega en el loop — deadlock).
        //     Puente para C en M2+.
        // ZH: 运行协程并阻塞至结束。切勿在调度器线程上调用（完成事件只在 loop 中
        //     交付 —— 会死锁）。M2+ 起用于 C 桥接。
        T sync_wait() &&
        {
                if (!h_)
                        throw std::runtime_error("krt::Task: sync_wait em task vazia / on empty task / en task vacía / 空任务");
                start();
                while (!h_.promise().done_.load(std::memory_order_acquire))
                        std::this_thread::yield();
                return take_value();
        }

      private:
        explicit Task(handle_t h) : h_(h)
        {
        }

        T take_value()
        {
                if (h_.promise().error_)
                        std::rethrow_exception(h_.promise().error_);
                return std::move(*h_.promise().value_);
        }

        handle_t h_{};
};

// PT: Especialização void: mesma semântica, sem valor de retorno.
// EN: void specialization: same semantics, no return value.
// ES: Especialización void: misma semántica, sin valor de retorno.
// ZH: void 特化：语义相同，无返回值。
template <>
class Task<void>
{
      public:
        struct promise_type;

        struct FinalAwait
        {
                [[nodiscard]] bool await_ready() const noexcept
                {
                        return false;
                }
                void await_suspend(std::coroutine_handle<promise_type> h) noexcept
                {
                        h.promise().done_.store(true, std::memory_order_release);
                        if (h.promise().continuation_)
                                complete(h.promise().continuation_);
                }
                void await_resume() noexcept
                {
                }
        };

        struct promise_type : PromiseState
        {
                Task get_return_object()
                {
                        return Task(std::coroutine_handle<promise_type>::from_promise(*this));
                }
                [[nodiscard]] std::suspend_always initial_suspend() noexcept
                {
                        return {};
                }
                [[nodiscard]] FinalAwait final_suspend() noexcept
                {
                        return {};
                }
                void return_void()
                {
                }
                void unhandled_exception()
                {
                        error_ = std::current_exception();
                }
        };

        using handle_t = std::coroutine_handle<promise_type>;

        Task(Task &&other) noexcept : h_(std::exchange(other.h_, {}))
        {
        }
        Task &operator=(Task &&other) noexcept
        {
                if (this != &other)
                {
                        if (h_)
                                h_.destroy();
                        h_ = std::exchange(other.h_, {});
                }
                return *this;
        }
        Task(const Task &) = delete;
        Task &operator=(const Task &) = delete;
        ~Task()
        {
                if (h_)
                        h_.destroy();
        }

        void start()
        {
                if (h_ && !h_.promise().started_)
                {
                        h_.promise().started_ = true;
                        h_.resume();
                }
        }

        [[nodiscard]] bool await_ready() const noexcept
        {
                return false;
        }
        void await_suspend(std::coroutine_handle<> waiter)
        {
                h_.promise().continuation_ = waiter;
                start();
        }
        void await_resume()
        {
                rethrow_if_error();
        }

        void sync_wait() &&
        {
                if (!h_)
                        throw std::runtime_error("krt::Task: sync_wait em task vazia / on empty task / en task vacía / 空任务");
                start();
                while (!h_.promise().done_.load(std::memory_order_acquire))
                        std::this_thread::yield();
                rethrow_if_error();
        }

      private:
        explicit Task(handle_t h) : h_(h)
        {
        }
        void rethrow_if_error()
        {
                if (h_.promise().error_)
                        std::rethrow_exception(h_.promise().error_);
        }
        handle_t h_{};
};

} // namespace krt
