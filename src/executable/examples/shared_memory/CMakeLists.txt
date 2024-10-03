add_executable(shared_memory main.cpp)

target_include_directories(shared_memory
PRIVATE
)


target_link_libraries(shared_memory 
PRIVATE
    system
)