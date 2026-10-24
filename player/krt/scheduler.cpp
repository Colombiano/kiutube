// PT: Implementação do Scheduler — ver scheduler.hpp para o contrato.
// EN: Scheduler implementation — see scheduler.hpp for the contract.
// ES: Implementación del Scheduler — ver scheduler.hpp para el contrato.
// ZH: Scheduler 实现 —— 契约见 scheduler.hpp。
#include "krt/scheduler.hpp"

#include "krt/log.hpp"

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <functional>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace krt
{

// PT: Hook global: quando o loop está ativo, TODA conclusão de corrotina é
//     entregue ao loop (thread-safe). Quando inativo, retomada inline (legado).
// EN: Global hook: while the loop is active, EVERY coroutine completion is
//     delivered to the loop (thread-safe). When inactive, inline (legacy).
// ES: Hook global: con el loop activo, TODA conclusión de corrutina se entrega
//     al loop (thread-safe). Inactivo: inline (legado).
// ZH: 全局 hook：loop 激活时，所有协程完成事件都交付给 loop（线程安全）。
//     未激活时按旧行为内联恢复。
namespace
{
CompletionHook g_hook = nullptr;
}

void set_completion_hook(CompletionHook fn)
{
        g_hook = fn;
}

void complete(std::coroutine_handle<> h)
{
        if (g_hook)
                g_hook(h);
        else
                h.resume();
}

Scheduler &Scheduler::instance()
{
        static Scheduler sched;
        return sched;
}

Scheduler::Scheduler()
{
        epollfd_ = epoll_create1(EPOLL_CLOEXEC);
        if (epollfd_ < 0)
                throw std::runtime_error("krt::Scheduler: epoll_create1 falhou / failed / falló / 失败");
        eventfd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        if (eventfd_ < 0)
                throw std::runtime_error("krt::Scheduler: eventfd falhou / failed / falló / 失败");

        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = eventfd_;
        if (epoll_ctl(epollfd_, EPOLL_CTL_ADD, eventfd_, &ev) < 0)
                throw std::runtime_error("krt::Scheduler: epoll_ctl(eventfd) falhou / failed / falló / 失败");
}

Scheduler::~Scheduler()
{
        if (eventfd_ >= 0)
                close(eventfd_);
        if (epollfd_ >= 0)
                close(epollfd_);
}

void Scheduler::request_stop() noexcept
{
        // PT: Rastreio de ciclo de vida: quem pede parada e quando. / EN: Lifecycle tracing: who requests stop and when.
        // ES: Rastreo de ciclo de vida: quién pide parada y cuándo. / ZH: 生命周期追踪：谁在何时请求停止。
        krt::log::info("scheduler: stop solicitado / requested / solicitado / 收到停止请求");
        stopping_ = true;
        wake();
}

void Scheduler::wake() noexcept
{
        const std::uint64_t one = 1;
        // PT: EAGAIN = despertar já pendente; ignora. / EN: EAGAIN = wake already pending; ignore.
        // ES: EAGAIN = despertar ya pendiente; ignora. / ZH: EAGAIN 表示唤醒已挂起，忽略。
        if (write(eventfd_, &one, sizeof(one)) < 0 && errno != EAGAIN)
        {
        }
}

void Scheduler::drain_wake() noexcept
{
        std::uint64_t sink[16];
        while (read(eventfd_, sink, sizeof(sink)) > 0)
        {
        }
}

int Scheduler::timeout_for(time_point now) const
{
        // PT: mtx_ já está segurado pelo caller (run loop). / EN: mtx_ is already held by the caller.
        // ES: mtx_ ya está tomado por el caller. / ZH: 调用方已持有 mtx_。
        if (timers_.empty())
                return -1;
        const auto d = std::chrono::duration_cast<std::chrono::milliseconds>(timers_.front().first - now);
        return d.count() <= 0 ? 0 : static_cast<int>(d.count());
}

void Scheduler::post(std::coroutine_handle<> h)
{
        {
                std::lock_guard lk(mtx_);
                posted_.push_back(h);
        }
        wake();
}

void Scheduler::post_function(std::function<void()> fn)
{
        {
                std::lock_guard lk(mtx_);
                funcs_.push_back(std::move(fn));
        }
        wake();
}

void Scheduler::add_timer(time_point at, std::coroutine_handle<> h)
{
        {
                std::lock_guard lk(mtx_);
                timers_.emplace_back(at, h);
                std::push_heap(timers_.begin(), timers_.end(),
                               [](const auto &a, const auto &b) { return a.first > b.first; });
        }
        wake();
}

void Scheduler::add_fd(int fd, std::coroutine_handle<> h)
{
        // PT: Só é chamado de await_suspend, ou seja, na thread do loop.
        // EN: Only called from await_suspend, i.e. on the loop thread.
        // ES: Solo se llama desde await_suspend, es decir, en el hilo del loop.
        // ZH: 只在 await_suspend 中调用，即在 loop 线程上。
        fds_[fd] = h;

        epoll_event ev{};
        ev.events = EPOLLIN | EPOLLONESHOT;
        ev.data.fd = fd;
        if (epoll_ctl(epollfd_, EPOLL_CTL_MOD, fd, &ev) < 0)
        {
                if (epoll_ctl(epollfd_, EPOLL_CTL_ADD, fd, &ev) < 0)
                {
                        fds_.erase(fd);
                        throw std::runtime_error("krt::Scheduler: epoll_ctl(fd) falhou / failed / falló / 失败");
                }
        }
}

void Scheduler::run()
{
        krt::log::info("scheduler: run() iniciado / started / iniciado / 已启动");
        set_completion_hook([](std::coroutine_handle<> h) { Scheduler::instance().post(h); });

        std::vector<std::coroutine_handle<>> ready;
        std::vector<std::function<void()>> pending_funcs;
        epoll_event events[16];
        auto last_beat = std::chrono::steady_clock::now();

        for (;;)
        {
                const auto now = std::chrono::steady_clock::now();
                if (now - last_beat > std::chrono::seconds(1))
                {
                        last_beat = now;
                        std::lock_guard lk(mtx_);
                        krt::log::info("scheduler: heartbeat timers={} fds={} posted={}", timers_.size(),
                                       fds_.size(), posted_.size());
                }
                {
                        std::lock_guard lk(mtx_);
                        while (!timers_.empty() && timers_.front().first <= now)
                        {
                                ready.push_back(timers_.front().second);
                                std::pop_heap(timers_.begin(), timers_.end(),
                                              [](const auto &a, const auto &b) { return a.first > b.first; });
                                timers_.pop_back();
                        }
                        ready.insert(ready.end(), posted_.begin(), posted_.end());
                        posted_.clear();
                        pending_funcs.insert(pending_funcs.end(), std::make_move_iterator(funcs_.begin()),
                                             std::make_move_iterator(funcs_.end()));
                        funcs_.clear();

                        if (stopping_ && timers_.empty())
                                break;
                }

                for (auto &fn : pending_funcs)
                        fn();
                pending_funcs.clear();

                if (!ready.empty())
                {
                        // PT: Retomar fora do lock: handlers podem registrar novos timers.
                        // EN: Resume outside the lock: handlers may register new timers.
                        // ES: Reanudar fuera del lock: los handlers pueden registrar timers.
                        // ZH: 在锁外恢复：handler 可能会注册新的定时器。
                        for (auto h : ready)
                                h.resume();
                        ready.clear();
                        continue;
                }

                const auto timeout = [&] {
                        std::lock_guard lk(mtx_);
                        return timeout_for(std::chrono::steady_clock::now());
                }();

                const int n = epoll_wait(epollfd_, events, 16, timeout);
                if (n < 0)
                {
                        if (errno == EINTR)
                                continue;
                        krt::log::error("scheduler: epoll_wait erro / error / error / 错误: {} — saindo / exiting / saliendo / 退出", errno);
                        break;
                }
                for (int i = 0; i < n; ++i)
                {
                        const int fd = events[i].data.fd;
                        if (fd == eventfd_)
                        {
                                drain_wake();
                                std::lock_guard lk(mtx_);
                                ready.insert(ready.end(), posted_.begin(), posted_.end());
                                posted_.clear();
                        }
                        else
                        {
                                const auto it = fds_.find(fd);
                                if (it != fds_.end())
                                {
                                        ready.push_back(it->second);
                                        fds_.erase(it);
                                }
                        }
                }
        }

        set_completion_hook(nullptr);
}

Task<void> sleep(std::chrono::milliseconds ms)
{
        // PT: co_await suspender esta corrotina e a registra como timer.
        // EN: co_await suspends this coroutine and registers it as a timer.
        // ES: co_await suspende esta corrutina y la registra como timer.
        // ZH: co_await 挂起当前协程并将其注册为定时器。
        struct SleepAwait
        {
                std::chrono::milliseconds ms_;

                [[nodiscard]] bool await_ready() const noexcept
                {
                        return false;
                }
                void await_suspend(std::coroutine_handle<> h) const
                {
                        krt::log::debug("scheduler: timer +{}ms", ms_.count());
                        Scheduler::instance().add_timer(std::chrono::steady_clock::now() + ms_, h);
                }
                void await_resume() const noexcept
                {
                }
        };
        co_await SleepAwait{ms};
}

Task<void> fd_readable(int fd)
{
        struct FdAwait
        {
                int fd_;

                [[nodiscard]] bool await_ready() const noexcept
                {
                        return false;
                }
                void await_suspend(std::coroutine_handle<> h) const
                {
                        Scheduler::instance().add_fd(fd_, h);
                }
                void await_resume() const noexcept
                {
                }
        };
        co_await FdAwait{fd};
}

} // namespace krt
