cmake_minimum_required(VERSION 3.10)
project(KVstore C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(SRC_FILES
    src/kvstore.c
    src/reactor.c
    src/kvs_array.c
    src/kvs_hash.c
    src/proactor.c
    src/ntyco_server.c
)

set(TEST_SRC_FILES
    src/testcase.c
)

add_executable(kvstore ${SRC_FILES})

add_executable(kvstore_test ${TEST_SRC_FILES})

find_package(Threads REQUIRED)

target_include_directories(kvstore PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/NtyCo/core
)

target_link_libraries(kvstore PRIVATE 
    uring
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/NtyCo/libntyco.a
    Threads::Threads
    ${CMAKE_DL_LIBS}
)

target_include_directories(kvstore_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
