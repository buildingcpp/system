add_executable(mpmc_test main.cpp)

target_include_directories(mpmc_test
PRIVATE
    ${_concurrentqueue_src_path}
)

target_link_libraries(mpmc_test 
PRIVATE
    system
)