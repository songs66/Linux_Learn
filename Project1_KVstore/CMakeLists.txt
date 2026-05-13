cmake_minimum_required(VERSION 3.10)
project(KVstore C)

# 统一整个项目的 C 标准，避免不同源文件的编译行为不一致
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 服务端主程序：共享协议层、存储层和多种网络层实现
set(SRC_FILES
    src/kvstore.c
    src/reactor.c
    src/kvs_array.c
    src/kvs_hash.c
    src/proactor.c
    src/ntyco_server.c
)

# 客户端测试程序：负责发命令并校验服务端返回值
set(TEST_SRC_FILES
    src/testcase.c
)

add_executable(kvstore ${SRC_FILES})

add_executable(kvstore_test ${TEST_SRC_FILES})

find_package(Threads REQUIRED)

# kvstore 需要同时看到项目头文件和第三方协程库头文件
target_include_directories(kvstore PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/NtyCo/core
)

target_link_libraries(kvstore PRIVATE 
    # io_uring / 协程 / 线程相关依赖都统一链接到服务端
    uring
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/NtyCo/libntyco.a
    Threads::Threads
    ${CMAKE_DL_LIBS}
)

target_include_directories(kvstore_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
