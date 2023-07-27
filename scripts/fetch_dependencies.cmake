include(FetchContent)

###############################################################################
function(fetch_dependency dependency)
    list(GET dependency 0 url)
    list(GET dependency 1 tag)
    string(REPLACE "/" ";" dependency "${url}")
    list(POP_BACK dependency repo_name_with_ext)
    string(REPLACE "." ";" R "${repo_name_with_ext}")
    list(POP_FRONT R repo_name)

    message("Fetching Content: ${repo_name}")
    message(STATUS "url: ${url}")
    message(STATUS "tag: ${tag}")

    FetchContent_Declare(
        ${repo_name}
        GIT_REPOSITORY "${url}"
        GIT_TAG ${tag}
        SOURCE_DIR        "${CMAKE_BINARY_DIR}/${repo_name}-src"
        BINARY_DIR        "${CMAKE_BINARY_DIR}/${repo_name}-build"
        INSTALL_DIR       "${CMAKE_BINARY_DIR}"
        INSTALL_COMMAND   ""
        )
    FetchContent_MakeAvailable(${repo_name})
    FetchContent_GetProperties(${repo_name})

    set(_${repo_name}_src_path ${CMAKE_BINARY_DIR}/${repo_name}-src CACHE STRING "")
endfunction()
