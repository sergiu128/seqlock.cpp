include(GNUInstallDirs)

file(GLOB SOURCES CONFIGURE_DEPENDS "*.cpp")
list(FILTER SOURCES EXCLUDE REGEX "(\.bm)|(\.test)")

add_library(seqlock STATIC ${SOURCES})

target_include_directories(seqlock PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

include("${CMAKE_SOURCE_DIR}/cmake/link_benchmark.cmake")
list(APPEND LINK_BM_BOTH seqlock)
link_benchmarks("" "" ${LINK_BM_BOTH})

install(
    TARGETS seqlock
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/seqlock"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
