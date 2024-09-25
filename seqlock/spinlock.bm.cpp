#include "seqlock/spinlock.hpp"

#include <benchmark/benchmark.h>

#include <atomic>

using seqlock::SpinLock;

SpinLock lock{};

static void BM_SpinLock(benchmark::State& state) {
    if (state.thread_index() == 0) {
        benchmark::DoNotOptimize(lock);
    }

    for (auto _ : state) {
        lock.Acquire();
        lock.Release();
    }
}

/// NaiveSpinLock shows how not to implemented a spin-lock. See `Acquire()`.
class NaiveSpinLock {
   public:
    NaiveSpinLock() = default;
    ~NaiveSpinLock() = default;

    NaiveSpinLock(const NaiveSpinLock&) = delete;
    NaiveSpinLock& operator=(const NaiveSpinLock&) = delete;

    NaiveSpinLock(NaiveSpinLock&&) = delete;
    NaiveSpinLock& operator=(NaiveSpinLock&&) = delete;

    void Acquire() noexcept {
        // The innefficiency lies in the fact that everytime `test_and_set` executes, the owning CPU core must have
        // write access to the part of the cache where `acquired_` is. Since only one core can have write-access on a
        // cache at any point in time, this creates contention on `acquired_`. Since multiple cores can have read-access
        // on a cache at any point in time, the `SpinLock` in `spinlock.hpp` bypasses the contention by only calling
        // `test_and_set` after it sees that `acquired_` is false. This results massive speedups in parallel workloads.
        // See the benchmarks below.
        while (acquired_.test_and_set(std::memory_order_acquire)) {
        }
    }

    void Release() noexcept { acquired_.clear(std::memory_order_release); }

   private:
    std::atomic_flag acquired_{false};
} naive_lock;

static void BM_NaiveSpinLock(benchmark::State& state) {
    if (state.thread_index() == 0) {
        benchmark::DoNotOptimize(naive_lock);
    }

    for (auto _ : state) {
        naive_lock.Acquire();
        naive_lock.Release();
    }
}

BENCHMARK(BM_SpinLock)->ThreadRange(1, 16)->UseRealTime();
BENCHMARK(BM_NaiveSpinLock)->ThreadRange(1, 16)->UseRealTime();

BENCHMARK_MAIN();

/*
  2024-09-26T17:11:02+02:00
Running ./build_rel/seqlock/spinlock.bm
Run on (10 X 24 MHz CPU s)
CPU Caches:
  L1 Data 64 KiB
  L1 Instruction 128 KiB
  L2 Unified 4096 KiB (x10)
Load Average: 2.17, 2.13, 2.20
--------------------------------------------------------------------------------
Benchmark                                      Time             CPU   Iterations
--------------------------------------------------------------------------------
BM_SpinLock/real_time/threads:1             4.95 ns         4.95 ns    140733975
BM_SpinLock/real_time/threads:2             30.7 ns         30.7 ns     26881162
BM_SpinLock/real_time/threads:4             97.0 ns         97.0 ns     10584300
BM_SpinLock/real_time/threads:8              467 ns          466 ns      1609248
BM_SpinLock/real_time/threads:16             799 ns          671 ns       961984
BM_NaiveSpinLock/real_time/threads:1        4.95 ns         4.95 ns    141137450
BM_NaiveSpinLock/real_time/threads:2        30.3 ns         30.3 ns     18453032
BM_NaiveSpinLock/real_time/threads:4         114 ns          114 ns      6015044
BM_NaiveSpinLock/real_time/threads:8        1240 ns         1223 ns       750584
BM_NaiveSpinLock/real_time/threads:16       5111 ns         3448 ns       355632
*/
