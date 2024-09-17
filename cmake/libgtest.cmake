function(setup_gtest)
    if (DEFINED ENV{LIB_GTEST_INSTALL_PREFIX})
        set(LIB_GTEST_INSTALL_PREFIX "$ENV{LIB_GTEST_INSTALL_PREFIX}" CACHE PATH "Location of gtest for the local build")
    elseif (DEFINED ENV{LIB_INSTALL_PREFIX})
        set(LIB_GTEST_INSTALL_PREFIX "$ENV{LIB_INSTALL_PREFIX}" CACHE PATH "Location of gtest for the local build")
    else()
        set(LIB_GTEST_INSTALL_PREFIX "/usr/local" CACHE PATH "Location of gtest for the local build")
    endif()
    set(LIB_GTEST_INCLUDE_PREFIX "${LIB_GTEST_INSTALL_PREFIX}/include" CACHE PATH "Location of gtest headers")
    set(LIB_GTEST_LIB_PREFIX "${LIB_GTEST_INSTALL_PREFIX}/lib" CACHE PATH "Location of gtest libraries")

    message("libgtest: setting up...")
    message("libgtest: include_path=${LIB_GTEST_INCLUDE_PREFIX} lib_path=${LIB_GTEST_LIB_PREFIX} ")

    add_library(gtest_debug STATIC IMPORTED GLOBAL)
    target_include_directories(gtest_debug INTERFACE "${LIB_GTEST_INCLUDE_PREFIX}")
    set_target_properties(gtest_debug PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${LIB_GTEST_INCLUDE_PREFIX}"
        IMPORTED_LOCATION "${LIB_GTEST_LIB_PREFIX}/libgtest-debug.a")

    add_library(gtest_release STATIC IMPORTED GLOBAL)
    target_include_directories(gtest_release INTERFACE "${LIB_GTEST_INCLUDE_PREFIX}")
    set_target_properties(gtest_release PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${LIB_GTEST_INCLUDE_PREFIX}"
        IMPORTED_LOCATION "${LIB_GTEST_LIB_PREFIX}/libgtest-release.a")

    message("libgtest: setup successful")
endfunction()
