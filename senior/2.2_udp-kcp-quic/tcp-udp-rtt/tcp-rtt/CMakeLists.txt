cmake_minimum_required(VERSION 3.16)
project(tcp_rtt LANGUAGES C CXX)

# 设置 TCP-RTT 的输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/senior/2.2_udp-kcp-quic/tcp-udp-rtt/tcp-rtt)

# 找到线程库
find_package(Threads REQUIRED)

# TCP RTT 客户端 (C++)
add_executable(tcp_rtt_client rtt_client.cpp)
target_link_libraries(tcp_rtt_client Threads::Threads)

# TCP RTT 服务端 (C)
add_executable(tcp_rtt_server rtt_server.c)
target_link_libraries(tcp_rtt_server Threads::Threads)

# 设置 C++ 标准
set_target_properties(tcp_rtt_client PROPERTIES CXX_STANDARD 11)