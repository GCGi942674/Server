# common/CMakeLists.txt

add_library(common STATIC
    src/Utils.cpp
    src/Thread_pool.cpp
    src/Logging.cpp
    src/Buffer.cpp
    src/MetricCounter.cpp
    src/protocol/message_codec.cpp
)

# 头文件路径
target_include_directories(common
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 编译选项
target_compile_options(common
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

find_package(Threads REQUIRED)
target_link_libraries(common PUBLIC Threads::Threads)
