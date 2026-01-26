cmake_minimum_required(VERSION 3.16)
project(mycode_learning)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 11)

# 统一输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin) # 可执行文件
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib) # 动态库
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib) # 静态库

# 子项目目录
add_subdirectory(junior)
add_subdirectory(senior)