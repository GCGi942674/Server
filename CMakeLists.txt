cmake_minimum_required(VERSION 3.10)

project(galxcore LANGUAGES CXX)

# =====================
# C++ 标准
# =====================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# =====================
# 输出路径（可选但强烈推荐）
# =====================
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# =====================
# 子模块
# =====================
add_subdirectory(common)
add_subdirectory(server)
add_subdirectory(client)
