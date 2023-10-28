add_executable(async_delete 
    main.cpp
    widget_impl.cpp
    widget_registry.cpp
    )

target_link_libraries(async_delete 
PRIVATE
    system
)