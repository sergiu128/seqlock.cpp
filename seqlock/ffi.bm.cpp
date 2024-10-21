#include "seqlock/ffi.h"

#include <benchmark/benchmark.h>

#include <cstring>

static void BM_load_store(benchmark::State& state) {
    char shared_data[8];
    char load[8];
    char store[8];
    auto* lock = seqlock_single_writer_create(shared_data, 8);

    benchmark::DoNotOptimize(shared_data);
    benchmark::DoNotOptimize(load);
    benchmark::DoNotOptimize(store);
    benchmark::DoNotOptimize(lock);

    for (auto _ : state) {
        seqlock_single_writer_load(lock, load, 8);
        seqlock_single_writer_store(lock, store, 8);
    }

    seqlock_single_writer_destroy(lock);
}

BENCHMARK(BM_load_store);

BENCHMARK_MAIN();
