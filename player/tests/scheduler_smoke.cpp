// PT: Smoke test do scheduler krt: ordenação de timers, offload pelo pool com
//     conclusão na thread do loop e parada limpa. Gate de regressão do M0.
// EN: krt scheduler smoke test: timer ordering, offload through the pool with
//     completion on the loop thread, and clean shutdown. M0 regression gate.
// ES: Smoke test del scheduler krt: ordenación de timers, offload por el pool
//     con conclusión en el hilo del loop y parada limpia. Gate de regresión M0.
// ZH: krt 调度器冒烟测试：定时器排序、经 pool 的 offload（完成事件交付到 loop
//     线程）以及干净停止。M0 回归门槛。
#include "krt/scheduler.hpp"

#include <chrono>
#include <cstdio>
#include <deque>
#include <format>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace
{

krt::Task<void> delayed(std::chrono::milliseconds d, std::string tag, std::vector<std::string> &log)
{
        co_await krt::sleep(d);
        log.push_back(std::move(tag));
}

krt::Task<void> stop_after(std::chrono::milliseconds d)
{
        co_await krt::sleep(d);
        krt::Scheduler::instance().request_stop();
}

krt::Task<void> offload_work(std::vector<std::string> &log)
{
        const int v = co_await krt::offload([] {
                std::this_thread::sleep_for(5ms); // trabalho bloqueante fora do loop
                return 42;
        });
        log.push_back(std::format("W{}", v));
}

} // namespace

int main()
{
        std::vector<std::string> log;

        std::deque<krt::Task<void>> tasks;
        tasks.push_back(delayed(30ms, "A", log));
        tasks.push_back(delayed(10ms, "B", log));
        tasks.push_back(delayed(20ms, "C", log));
        tasks.push_back(stop_after(40ms));
        tasks.push_back(offload_work(log));

        for (auto &t : tasks)
                t.start();
        krt::Scheduler::instance().run();

        // PT: Invariantes: B<C<A (timers), W42 presente (pool), loop parou.
        // EN: Invariants: B<C<A (timers), W42 present (pool), loop stopped.
        // ES: Invariantes: B<C<A (timers), W42 presente (pool), loop paró.
        // ZH: 不变式：B<C<A（定时器）、W42 存在（pool）、循环已停止。
        const auto pos = [&log](const std::string &s) {
                for (std::size_t i = 0; i < log.size(); ++i)
                        if (log[i] == s)
                                return static_cast<int>(i);
                return -1;
        };
        const int b = pos("B"), c = pos("C"), a = pos("A"), w = pos("W42");

        const bool ok = b >= 0 && c > b && a > c && w >= 0;
        if (!ok)
        {
                std::fprintf(stderr, "scheduler_smoke: FAIL [");
                for (const auto &s : log)
                        std::fprintf(stderr, "%s ", s.c_str());
                std::fprintf(stderr, "]\n");
                return 1;
        }
        std::printf("scheduler_smoke: PASS (%zu eventos / events / eventos / 事件)\n", log.size());
        return 0;
}
