add_executable(shm shm.cpp)
target_link_libraries(shm PRIVATE general seqlock)
