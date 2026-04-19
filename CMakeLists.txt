cmake_minimum_required(VERSION 3.16)

# 顶层项目：统一管理整个仓库中的学习示例和子模块
project(mycode_learning LANGUAGES C CXX)

# 在顶层统一指定语言标准，子项目默认继承这些设置
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 统一所有子项目的输出目录，避免可执行文件散落在各自目录下
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)   # 可执行文件
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)   # 动态库
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)   # 静态库

# 将各个子目录纳入总工程构建
add_subdirectory(junior)
add_subdirectory(senior)
add_subdirectory(Project1_KVstore)
add_subdirectory(test1)
