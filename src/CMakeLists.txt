add_executable(main main.cpp)
target_link_libraries(main PRIVATE general seqlock::seqlock)

# Setup test binary. We pick up all files with the ".test.cpp" suffix.
file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/*.test.cpp")
add_executable(test ${TEST_SOURCES})
target_link_libraries(test PUBLIC
    debug gtest_debug
    optimized gtest_release
    general seqlock::seqlock)
