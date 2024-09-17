#include <benchmark/benchmark.h>

#include <iostream>

static void BM_Lib(benchmark::State &state) {
    int sum{0};
    for (auto _ : state) sum += 10;
    std::cout << "sum=" << sum << std::endl;
}

BENCHMARK(BM_Lib);
BENCHMARK_MAIN();
