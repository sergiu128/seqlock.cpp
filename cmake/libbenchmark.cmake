function(setup_benchmark)
    if (DEFINED ENV{LIB_BENCHMARK_INSTALL_PREFIX})
        set(LIB_BENCHMARK_INSTALL_PREFIX "$ENV{LIB_BENCHMARK_INSTALL_PREFIX}" CACHE PATH "Location of benchmark for the local build")
    elseif (DEFINED ENV{LIB_INSTALL_PREFIX})
        set(LIB_BENCHMARK_INSTALL_PREFIX "$ENV{LIB_INSTALL_PREFIX}" CACHE PATH "Location of benchmark for the local build")
    else()
        set(LIB_BENCHMARK_INSTALL_PREFIX "/usr/local" CACHE PATH "Location of benchmark for the local build")
    endif()
    set(LIB_BENCHMARK_INCLUDE_PREFIX "${LIB_BENCHMARK_INSTALL_PREFIX}/include" CACHE PATH "Location of benchmark headers")
    set(LIB_BENCHMARK_LIB_PREFIX "${LIB_BENCHMARK_INSTALL_PREFIX}/lib" CACHE PATH "Location of benchmark libraries")
    
    message("libbenchmark: setting up...")
    message("libbenchmark: include_path=${LIB_BENCHMARK_INCLUDE_PREFIX} lib_path=${LIB_BENCHMARK_LIB_PREFIX}")

    add_library(benchmark_debug STATIC IMPORTED GLOBAL)
    target_include_directories(benchmark_debug INTERFACE "${LIB_BENCHMARK_INCLUDE_PREFIX}")
    set_target_properties(benchmark_debug PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${LIB_BENCHMARK_INCLUDE_PREFIX}"
        IMPORTED_LOCATION "${LIB_BENCHMARK_LIB_PREFIX}/libbenchmark-debug.a")

    add_library(benchmark_release STATIC IMPORTED GLOBAL)
    target_include_directories(benchmark_release INTERFACE "${LIB_BENCHMARK_INCLUDE_PREFIX}")
    set_target_properties(benchmark_release PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${LIB_BENCHMARK_INCLUDE_PREFIX}"
        IMPORTED_LOCATION "${LIB_BENCHMARK_LIB_PREFIX}/libbenchmark-release.a")
    
    message("libbenchmark: setup successful")
endfunction()
