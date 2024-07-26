
add_library(system
    ./threading/thread_pool.cpp
    ./system.cpp
)

target_link_libraries(system 
    pthread
)


target_include_directories(system
PUBLIC
    ${_system_dir}/src
    ${_include_dir}/src
)
