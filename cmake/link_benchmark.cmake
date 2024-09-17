function (link_benchmarks LIBS_DEBUG LIBS_RELEASE LIBS_BOTH)
    file(GLOB BM_SOURCES "*.bm.cpp")
    foreach (BM_SOURCE ${BM_SOURCES})
        get_filename_component(BM_NAME ${BM_SOURCE} NAME_WLE)
        add_executable("${BM_NAME}" "${BM_SOURCE}")

        # NOTE: debug/optimize/general only apply to the first following keyword
        foreach (LIB_DEBUG ${LIBS_DEBUG})
            target_link_libraries("${BM_NAME}" PRIVATE
                    debug ${LIB_DEBUG})
        endforeach ()

        foreach (LIB_RELEASE ${LIBS_RELEASE})
            target_link_libraries("${BM_NAME}" PRIVATE
                    optimized ${LIB_RELEASE})
        endforeach ()

        foreach (LIB_BOTH ${LIBS_BOTH})
            target_link_libraries("${BM_NAME}" PRIVATE
                    general ${LIB_BOTH})
        endforeach ()

        target_link_libraries("${BM_NAME}" PRIVATE
                debug benchmark_debug
                optimized benchmark_release)
    endforeach ()
endfunction ()
