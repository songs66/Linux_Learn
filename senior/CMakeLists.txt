# Senior 学习项目
cmake_minimum_required(VERSION 3.16)

# 网络 I/O 部分
add_subdirectory(2.1_network_io)

# UDP-KCP-QUIC 部分
add_subdirectory(2.2_udp-kcp-quic)

# 协程部分
add_subdirectory(2.3_coroutine)

# DPDK用户态协议栈部分
add_subdirectory(2.4_Ustack)