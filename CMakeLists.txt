cmake_minimum_required(VERSION 3.16.3)
set(CMAKE_CXX_STANDARD 20)

project(system)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
    message("*** Build type not set.  defaulting to Release")
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(
        -g
        -O0
        -march=native
    )
else()
    add_compile_options(
        -O3
        -march=native
        -g
    )
endif()


include("./scripts/fetch_dependencies.cmake")

fetch_dependency("https://github.com/buildingcpp/include.git;main")


set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/${PROJECT_NAME})

add_subdirectory(src)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

