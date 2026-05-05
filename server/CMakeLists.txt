# server/CMakeLists.txt

add_executable(server
    src/main.cpp
    src/Connection.cpp
    src/EchoServer.cpp
    src/EventLoop.cpp
    src/Acceptor.cpp
    src/ServerMetrics.cpp
    src/EchoHandler.cpp
    src/EventLoopThread.cpp
    src/EventLoopThreadPool.cpp
)

target_include_directories(server
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(server
    PRIVATE
        common
)

