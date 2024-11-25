# Setup test binary. We pick up all files with the ".test.cpp" suffix.
file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/*.test.cpp")
add_executable(run ${TEST_SOURCES})
target_link_libraries(run PUBLIC
    debug gtest_debug
    optimized gtest_release
    general seqlock)
