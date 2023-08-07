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


option(WORKCONTRACT_BUILD_DEMO "Build the CLI demo" ON)

include("./scripts/fetch_dependencies.cmake")

fetch_dependency("https://github.com/buildingcpp/include.git;main")

fetch_dependency("https://github.com/fmtlib/fmt.git;5f774c0aed4af6494b412cd3883e0f09295bd79d")
fetch_dependency("https://github.com/ericniebler/range-v3.git;master")


set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/${PROJECT_NAME})
set(_${PROJECT_NAME}_dir ${CMAKE_CURRENT_SOURCE_DIR} CACHE STRING "")

add_subdirectory(src)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

