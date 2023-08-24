
add_library(system
    ./threading/thread_pool.cpp
    ./work_contract/basic_work_contract_group.cpp
    ./system.cpp
)

target_link_libraries(system 
    pthread
    include
)


target_include_directories(system
PUBLIC
    ${_system_dir}/src
)
