/**
 * @file async.hpp
 * @brief Corrotinas C++20 + thread pool — camada ytm do kew-ytm.
 *
 * Task<T> é uma corrotina move-only (não copiável): o handle vive em
 * exclusividade no dono. Task é awaitable — corrotinas compostas com
 * `co_await` encadeiam via continuação (final_suspend repassa para quem
 * esperava). awaitables offload rodam trabalho bloqueante no ThreadPool e
 * retomam a corrotina na thread do pool; sync_wait() bloqueia a thread
 * chamadora até o valor final (ponte C <-> C++ e CLI).
 */
#pragma once

#include <atomic>
#include <condition_variable>
#include <coroutine>
#include <cstddef>
#include <exception>
#include <functional>
#include <mutex>
#include <optional>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace ytm
{

/** Pool global de workers para I/O bloqueante (rede, subprocessos). */
class ThreadPool
{
      public:
        static ThreadPool &instance()
        {
                static ThreadPool pool;
                return pool;
        }

        ThreadPool(const ThreadPool &) = delete;
        ThreadPool &operator=(const ThreadPool &) = delete;

        void post(std::function<void()> fn)
        {
                {
                        std::lock_guard lk(m_);
                        q_.push(std::move(fn));
                }
                cv_.notify_one();
        }

        [[nodiscard]] std::size_t worker_count() const
        {
                return workers_;
        }

      private:
        ThreadPool()
        {
                unsigned n = std::thread::hardware_concurrency();
                if (n == 0)
                        n = 4;
                workers_ = n;
                threads_.reserve(n);
                for (unsigned i = 0; i < n; ++i)
                        threads_.emplace_back([this] { loop(); });
        }

        ~ThreadPool()
        {
                {
                        std::lock_guard lk(m_);
                        stop_ = true;
                }
                cv_.notify_all();
                for (auto &t : threads_)
                        t.join();
        }

        void loop()
        {
                for (;;)
                {
                        std::function<void()> fn;
                        {
                                std::unique_lock lk(m_);
                                cv_.wait(lk, [&] { return stop_ || !q_.empty(); });
                                if (stop_ && q_.empty())
                                        return;
                                fn = std::move(q_.front());
                                q_.pop();
                        }
                        fn();
                }
        }

        std::vector<std::thread> threads_;
        std::queue<std::function<void()>> q_;
        std::mutex m_;
        std::condition_variable cv_;
        std::size_t workers_ = 0;
        bool stop_ = false;
};

namespace detail
{

/** awaitable genérico: roda F() no pool, resultado movido de volta. */
template <typename R, typename F>
struct OffloadOp
{
        F fn_;
        std::optional<R> result_;
        std::exception_ptr error_;
        std::atomic<bool> done_{false};
        std::coroutine_handle<> waiter_{};

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
                waiter_ = h;
                ThreadPool::instance().post([this] {
                        try
                        {
                                result_.emplace(std::move(fn_)());
                        }
                        catch (...)
                        {
                                error_ = std::current_exception();
                        }
                        done_.store(true, std::memory_order_release);
                        waiter_.resume();
                });
        }

        R await_resume()
        {
                if (error_)
                        std::rethrow_exception(error_);
                return std::move(*result_);
        }
};

/** especialização para F() -> void. */
template <typename F>
struct OffloadOp<void, F>
{
        F fn_;
        std::exception_ptr error_;
        std::atomic<bool> done_{false};
        std::coroutine_handle<> waiter_{};

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
                waiter_ = h;
                ThreadPool::instance().post([this] {
                        try
                        {
                                std::move(fn_)();
                        }
                        catch (...)
                        {
                                error_ = std::current_exception();
                        }
                        done_.store(true, std::memory_order_release);
                        waiter_.resume();
                });
        }

        void await_resume()
        {
                if (error_)
                        std::rethrow_exception(error_);
        }
};

} // namespace detail

/** Desvia um callable bloqueante para o pool: `auto r = co_await offload([&]{ ... });` */
template <typename F>
auto offload(F &&f)
{
        using Fn = std::decay_t<F>;
        using R = std::invoke_result_t<Fn>;
        return detail::OffloadOp<R, Fn>(std::forward<F>(f));
}

/**
 *promise base comum: estado de conclusão + continuação a repassar.
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
                // Corrotina filha terminou: marca done e devolve a execução
                // para quem estava esperando (ou ninguém, se sync_wait).
                void await_suspend(std::coroutine_handle<promise_type> h) noexcept
                {
                        h.promise().done_.store(true, std::memory_order_release);
                        if (h.promise().continuation_)
                                h.promise().continuation_.resume();
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

        [[nodiscard]] bool done() const
        {
                return !h_ || h_.promise().done_.load(std::memory_order_acquire);
        }

        // ---- awaitable: permite co_await em Task dentro de outra corrotina ----

        [[nodiscard]] bool await_ready() const noexcept
        {
                return false;
        }
        void await_suspend(std::coroutine_handle<> waiter)
        {
                h_.promise().continuation_ = waiter;
                if (!h_.promise().started_)
                {
                        h_.promise().started_ = true;
                        h_.resume();
                }
        }
        T await_resume()
        {
                return take_value();
        }

        /** Roda a corrotina e bloqueia até o fim. Consome a Task (move-only). */
        T sync_wait() &&
        {
                if (!h_)
                        throw std::runtime_error("ytm::Task: sync_wait em task vazia");
                if (!h_.promise().started_)
                {
                        h_.promise().started_ = true;
                        h_.resume();
                }
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

/** Especialização void: mesma semântica, sem valor de retorno. */
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
                                h.promise().continuation_.resume();
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

        [[nodiscard]] bool await_ready() const noexcept
        {
                return false;
        }
        void await_suspend(std::coroutine_handle<> waiter)
        {
                h_.promise().continuation_ = waiter;
                if (!h_.promise().started_)
                {
                        h_.promise().started_ = true;
                        h_.resume();
                }
        }
        void await_resume()
        {
                rethrow_if_error();
        }

        void sync_wait() &&
        {
                if (!h_)
                        throw std::runtime_error("ytm::Task: sync_wait em task vazia");
                if (!h_.promise().started_)
                {
                        h_.promise().started_ = true;
                        h_.resume();
                }
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

} // namespace ytm
