add_executable(baseline main.cpp)

target_include_directories(baseline
PRIVATE
    ${_concurrentqueue_src_path}
)

target_link_libraries(baseline 
PRIVATE
    system
)