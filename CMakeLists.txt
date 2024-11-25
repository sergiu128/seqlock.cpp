cmake_minimum_required(VERSION 3.13)

project(template LANGUAGES CXX)

include("cmake/setup_environment.cmake")
setup_environment()

add_subdirectory("seqlock")
add_subdirectory("tests")
add_subdirectory("examples")
