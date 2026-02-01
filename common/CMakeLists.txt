# common/CMakeLists.txt

add_library(common STATIC
    src/codec.cpp
    src/buffer.cpp
    src/net_utils.cpp
)

# 头文件路径（给别人用）
target_include_directories(common
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 编译选项（可选，但很专业）
target_compile_options(common
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

# 如果以后用 pthread
find_package(Threads REQUIRED)
target_link_libraries(common PUBLIC Threads::Threads)
