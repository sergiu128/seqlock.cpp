file(GLOB SOURCES CONFIGURE_DEPENDS "*.cpp")
list(FILTER SOURCES EXCLUDE REGEX "(\.bm)|(\.test)")

add_library(seqlock_seqlock STATIC ${SOURCES})

target_include_directories(seqlock_seqlock PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
add_library(seqlock::seqlock ALIAS seqlock_seqlock)

include("${CMAKE_SOURCE_DIR}/cmake/link_benchmark.cmake")
list(APPEND LINK_BM_BOTH seqlock::seqlock)
link_benchmarks("" "" ${LINK_BM_BOTH})
