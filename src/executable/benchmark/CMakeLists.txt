add_executable(benchmark main.cpp)

target_include_directories(benchmark
PRIVATE
    ${_concurrentqueue_src_path}
)

target_link_libraries(benchmark 
PRIVATE
    system
    tbb
)