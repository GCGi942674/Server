# client/CMakeLists.txt

add_executable(client
    src/main.cpp
    src/EchoClient.cpp
)

add_executable(load_client
    src/Load_main.cpp
    src/EchoClient.cpp
)

target_include_directories(client
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(load_client
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(client
    PRIVATE
        common
)

target_link_libraries(load_client
    PRIVATE
        common
)

target_include_directories(client
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(client
    PRIVATE
        common
)
