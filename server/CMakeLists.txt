# server/CMakeLists.txt

add_executable(server
    src/main.cpp
    src/Connection.cpp
    src/echo_server.cpp
)

target_include_directories(server
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(server
    PRIVATE
        common
)

# Linux 下 epoll / socket 一般不用额外库
