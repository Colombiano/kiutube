// PT: Pool global de workers para trabalho bloqueante (rede, subprocessos,
//     decodificação). Movido de src/ytm/async.hpp (camada ytm, já testada em
//     produção) — duplicação temporária aceita até a migração da ytm no M3.
// EN: Global worker pool for blocking work (network, subprocesses, decoding).
//     Moved from src/ytm/async.hpp (ytm layer, battle-tested in production) —
//     temporary duplication accepted until the ytm migration in M3.
// ES: Pool global de workers para trabajo bloqueante (red, subprocessos,
//     decodificación). Movido de src/ytm/async.hpp (capa ytm, probada en
//     producción) — duplicación temporal aceptada hasta la migración en M3.
// ZH: 用于阻塞型工作（网络、子进程、解码）的全局 worker 池。迁移自 src/ytm/async.hpp
//     （ytm 层，已在生产环境中验证）—— 在 M3 迁移 ytm 之前接受临时重复。
#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace krt
{

/** Pool global de workers para I/O bloqueante. / Global worker pool for blocking I/O.
 *  Pool global de workers para E/S bloqueante. / 用于阻塞型 I/O 的全局 worker 池。 */
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

} // namespace krt
