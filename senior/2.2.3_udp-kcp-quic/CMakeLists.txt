cmake_minimum_required(VERSION 3.16)
project(kcp LANGUAGES C CXX)

include(CTest)
include(GNUInstallDirs)

# 设置 KCP 可执行文件的输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/senior/2.2.3_udp-kcp-quic)

# ===== Core library: ikcp =====
add_library(kcp STATIC src/ikcp.c)
add_library(kcp::kcp ALIAS kcp)

# 让"链接 kcp 的人"也能找到 ikcp.h
target_include_directories(kcp
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# (可选) 你想强制 C 标准的话：
# target_compile_features(kcp PUBLIC c_std_99)

# ===== Subdirectories =====
add_subdirectory(apps/demo_c)
add_subdirectory(apps/chat)
add_subdirectory(tcp-udp-rtt/tcp-rtt)
add_subdirectory(tcp-udp-rtt/udp-rtt)

if (BUILD_TESTING)
    add_subdirectory(tests)
endif()

# ===== Install =====
install(TARGETS kcp
        EXPORT kcp-targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(FILES include/ikcp.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT kcp-targets
        FILE kcp-config.cmake
        NAMESPACE kcp::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/kcp
)