
add_library(system
    ./threading/thread_pool.cpp
    ./system.cpp
    ./memory/shared_memory.cpp
    ./memory/memory_mapping.cpp
    ./memory/anonymous_mapping.cpp
)

target_link_libraries(system 
    pthread
    rt
)


target_include_directories(system
PUBLIC
    ${_system_dir}/src
    ${_include_dir}/src
)
