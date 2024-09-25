#include "seqlock/seqlock.hpp"

#include <benchmark/benchmark.h>

#include <atomic>
#include <thread>

using seqlock::SeqLock;

static void BM_SeqLockReference(benchmark::State& state) {
    int from{0};
    int to{0};
    benchmark::DoNotOptimize(from);
    benchmark::DoNotOptimize(to);

    for (auto _ : state) {
        to = from;
        from++;
    }
}

SeqLock lock{};
int shared{0};
std::thread* writer{nullptr};
std::atomic_flag writer_done{};

static void BM_SeqLockSingleWriter(benchmark::State& state) {
    if (state.thread_index() == 0) {
        benchmark::DoNotOptimize(shared);
        benchmark::DoNotOptimize(lock);
        benchmark::DoNotOptimize(writer);
        benchmark::DoNotOptimize(writer_done);

        writer_done.clear(std::memory_order::relaxed);

        writer = new std::thread{[&] {
            if (not writer_done.test(std::memory_order_relaxed)) {
                lock.StoreSingle([&] { shared++; });
            }
        }};
    }

    int shared_copy{0};
    benchmark::DoNotOptimize(shared_copy);
    for (auto _ : state) {
        lock.TryLoad([&] { shared_copy = shared; });
    }

    if (state.thread_index() == 0) {
        writer_done.notify_all();
        writer->join();
        delete writer;
    }
}

BENCHMARK(BM_SeqLockReference);
BENCHMARK(BM_SeqLockSingleWriter)->ThreadRange(1, 8)->UseRealTime();

BENCHMARK_MAIN();
