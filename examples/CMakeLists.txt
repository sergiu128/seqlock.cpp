add_executable(shm shm.cpp)
target_link_libraries(shm PRIVATE general seqlock)

add_executable(writer writer.cpp)
target_link_libraries(writer PRIVATE general seqlock)
