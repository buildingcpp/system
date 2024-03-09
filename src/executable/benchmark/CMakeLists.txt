add_executable(benchmark main.cpp)


include("${CMAKE_SOURCE_DIR}/scripts/fetch_dependencies.cmake")
fetch_dependency("https://github.com/cameron314/concurrentqueue.git;master")

target_include_directories(benchmark
PRIVATE
    ${_concurrentqueue_src_path}
)

target_link_libraries(benchmark 
PRIVATE
    system
)