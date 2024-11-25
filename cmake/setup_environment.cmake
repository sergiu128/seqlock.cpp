function(set_version)
    execute_process(
        COMMAND bash -c "git describe --tags"
        OUTPUT_VARIABLE TEMPLATE_VERSION)
    string(REGEX REPLACE "\n$" "" TEMPLATE_VERSION "${TEMPLATE_VERSION}")
    message("version: ${TEMPLATE_VERSION}")
    add_definitions("-DTEMPLATE_VERSION=\"${TEMPLATE_VERSION}\"")
endfunction()

function(set_revision)
    execute_process(
        COMMAND bash -c "git rev-parse HEAD"
        OUTPUT_VARIABLE TEMPLATE_REVISION)
    string(REGEX REPLACE "\n$" "" TEMPLATE_REVISION "${TEMPLATE_REVISION}")
    message("revision: ${TEMPLATE_REVISION}")
    add_definitions("-DTEMPLATE_REVISION=\"${TEMPLATE_REVISION}\"")
endfunction()

function(config_compiler)
    message("Configuring compiler...")

    if ("${CMAKE_BUILD_TYPE}" STREQUAL "" OR
            "${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
        message("Building Debug target.")
        set(CMAKE_BUILD_TYPE "Debug")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g")
    elseif ("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
        message("Building Release target.")
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON) # enable LTO
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
    else ()
        message(FATAL_ERROR "Invalid build type: ${CMAKE_BUILD_TYPE}")
    endif()
    
    add_compile_options(-Wall -Wextra -Wno-c99-extensions -Wno-missing-field-initializers -Werror=format -std=c++23)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")

    message("Configured compiler. Using ${CMAKE_CXX_COMPILER_ID} \
${CMAKE_CXX_COMPILER_VERSION}")
endfunction()

include("cmake/libbenchmark.cmake")
include("cmake/libgtest.cmake")

function(load_libs)
    message("-----------------------------libs-----------------------------")
    setup_benchmark()
    message("--------------------------------------------------------------")
    setup_gtest()
    message("--------------------------------------------------------------")
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
    message("-----------------------------libs-----------------------------")
endfunction()

function(setup_environment)
    config_compiler()
    set_version()
    set_revision()
    load_libs()
endfunction()
