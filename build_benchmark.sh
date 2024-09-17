#!/usr/bin/env bash

set -exo pipefail

LIB_INSTALL_PREFIX=${LIB_INSTALL_PREFIX:-/usr/local}
LIB_BUILD_PREFIX=${LIB_BUILD_PREFIX:-/tmp}

LIB_BENCHMARK_INSTALL_PREFIX=${LIB_BENCHMARK_INSTALL_PREFIX:-${LIB_INSTALL_PREFIX}}
LIB_BENCHMARK_BUILD_PREFIX=${LIB_BENCHMARK_BUILD_PREFIX:-${LIB_BUILD_PREFIX}}

LIB_BENCHMARK_VERSION="1.9.0"

cd "${LIB_BENCHMARK_BUILD_PREFIX}"
if [ ! -d benchmark ]; then
    HOME=/dev/null git clone https://github.com/google/benchmark.git
fi

cd benchmark
git fetch
git checkout "v${LIB_BENCHMARK_VERSION}"

function build_benchmark() {
    echo "Building google_benchmark build_type=${1}."

    cd "${LIB_BENCHMARK_BUILD_PREFIX}/benchmark"
    rm -rf build
    mkdir -p build
    cmake --no-warn-unused-cli \
          -GNinja \
          -DCMAKE_CXX_COMPILER=${CXX} \
          -DCMAKE_C_COMPILER=${CC} \
          -DCMAKE_CXX_STANDARD=20 \
          -DCMAKE_CXX_STANDARD_REQUIRED=ON \
          -DCMAKE_INSTALL_PREFIX=${LIB_BENCHMARK_INSTALL_PREFIX} \
          -DCMAKE_BUILD_TYPE="${1}" \
          -DCMAKE_INSTALL_LIBDIR=lib \
          -DBENCHMARK_ENABLE_TESTING=ON \
          -DBENCHMARK_ENABLE_GTEST_TESTS=ON \
          -DBENCHMARK_ENABLE_LTO=OFF \
          -DBENCHMARK_DOWNLOAD_DEPENDENCIES=ON \
          -BENCHMARK_USE_BUNDLED_GTEST=ON \
          -S . -B "build"
    cmake --build "build" --config ${1} --target install
    ./build/test/benchmark_gtest
}

build_benchmark Debug
mv ${LIB_BENCHMARK_INSTALL_PREFIX}/lib/libbenchmark.a ${LIB_BENCHMARK_INSTALL_PREFIX}/lib/libbenchmark-debug.a
mv ${LIB_BENCHMARK_INSTALL_PREFIX}/lib/libbenchmark_main.a ${LIB_BENCHMARK_INSTALL_PREFIX}/lib/libbenchmark_main-debug.a

build_benchmark Release
mv ${LIB_BENCHMARK_INSTALL_PREFIX}/lib/libbenchmark.a ${LIB_BENCHMARK_INSTALL_PREFIX}/lib/libbenchmark-release.a
mv ${LIB_BENCHMARK_INSTALL_PREFIX}/lib/libbenchmark_main.a ${LIB_BENCHMARK_INSTALL_PREFIX}/lib/libbenchmark_main-release.a
