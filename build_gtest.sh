#!/usr/bin/env bash

LIB_INSTALL_PREFIX=${LIB_INSTALL_PREFIX:-/usr/local}
LIB_BUILD_PREFIX=${LIB_BUILD_PREFIX:-/tmp}

LIB_GTEST_INSTALL_PREFIX=${LIB_GTEST_INSTALL_PREFIX:-${LIB_INSTALL_PREFIX}}
LIB_GTEST_BUILD_PREFIX=${LIB_GTEST_BUILD_PREFIX:-${LIB_BUILD_PREFIX}}

LIB_GTEST_VERSION="1.15.2"

cd "${LIB_GTEST_BUILD_PREFIX}"
if [ ! -d googletest ]; then
    HOME=/dev/null git clone https://github.com/google/googletest.git
fi

cd googletest
git fetch
git checkout "v${LIB_GTEST_VERSION}"

function build_gtest() {
    echo "Building google_test target=${1}."

    cd "${LIB_GTEST_BUILD_PREFIX}/googletest"
    rm -rf build
    mkdir -p build
    cmake --no-warn-unused-cli \
          -GNinja \
          -DCMAKE_CXX_COMPILER=${CXX} \
          -DCMAKE_C_COMPILER=${CC} \
          -DCMAKE_CXX_STANDARD=20 \
          -DCMAKE_CXX_STANDARD_REQUIRED=ON \
          -DCMAKE_INSTALL_PREFIX=${LIB_GTEST_INSTALL_PREFIX} \
          -DCMAKE_INSTALL_LIBDIR=lib \
          -DCMAKE_BUILD_TYPE="${1}" \
          -S . -B "build"
    cmake --build "build" --config ${1} --target install
}

build_gtest Debug
mv ${LIB_GTEST_INSTALL_PREFIX}/lib/libgmock.a ${LIB_GTEST_INSTALL_PREFIX}/lib/libgmock-debug.a
mv ${LIB_GTEST_INSTALL_PREFIX}/lib/libgmock_main.a ${LIB_GTEST_INSTALL_PREFIX}/lib/libgmock_main-debug.a
mv ${LIB_GTEST_INSTALL_PREFIX}/lib/libgtest.a ${LIB_GTEST_INSTALL_PREFIX}/lib/libgtest-debug.a
mv ${LIB_GTEST_INSTALL_PREFIX}/lib/libgtest_main.a ${LIB_GTEST_INSTALL_PREFIX}/lib/libgtest_main-debug.a

build_gtest Release
mv ${LIB_GTEST_INSTALL_PREFIX}/lib/libgmock.a ${LIB_GTEST_INSTALL_PREFIX}/lib/libgmock-release.a
mv ${LIB_GTEST_INSTALL_PREFIX}/lib/libgmock_main.a ${LIB_GTEST_INSTALL_PREFIX}/lib/libgmock_main-release.a
mv ${LIB_GTEST_INSTALL_PREFIX}/lib/libgtest.a ${LIB_GTEST_INSTALL_PREFIX}/lib/libgtest-release.a
mv ${LIB_GTEST_INSTALL_PREFIX}/lib/libgtest_main.a ${LIB_GTEST_INSTALL_PREFIX}/lib/libgtest_main-release.a
