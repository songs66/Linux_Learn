find_package(Threads REQUIRED)

# 设置 demo_c 的输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/senior/2.2.3_udp-kcp-quic/demo_c)

add_executable(client client.c delay.c)
target_link_libraries(client PRIVATE kcp::kcp Threads::Threads)
target_include_directories(client PRIVATE ${CMAKE_CURRENT_LIST_DIR})

add_executable(server server.c)
target_link_libraries(server PRIVATE kcp::kcp Threads::Threads)
target_include_directories(server PRIVATE ${CMAKE_CURRENT_LIST_DIR})

# 重命名可执行文件以匹配目录结构
set_target_properties(client PROPERTIES OUTPUT_NAME "kcp_client")
set_target_properties(server PROPERTIES OUTPUT_NAME "kcp_server")