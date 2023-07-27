
add_library(system
    ./threading/thread_pool.cpp
    ./system.cpp
    )

target_link_libraries(system 
    include
    )

target_include_directories(system
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
    )
