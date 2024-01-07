add_executable(concurrent_even_number_counting main.cpp)

target_include_directories(concurrent_even_number_counting
PRIVATE
)


target_link_libraries(concurrent_even_number_counting 
PRIVATE
    system
)