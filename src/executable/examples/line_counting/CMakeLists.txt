add_executable(line_counting main.cpp)

target_include_directories(line_counting
PRIVATE
)


target_link_libraries(line_counting 
PRIVATE
    system
)