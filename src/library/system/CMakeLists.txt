
add_library(system
    ./threading/thread_pool.cpp
    ./work_contract/sub_work_contract_group.cpp
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
