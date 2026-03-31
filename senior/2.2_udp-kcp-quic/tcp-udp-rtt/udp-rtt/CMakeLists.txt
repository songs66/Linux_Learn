CMAKE_MINIMUM_REQUIRED(VERSION 3.16)

project(kcp LANGUAGES C)

include(CTest)
include(GNUInstallDirs)

# 设置 UDP-RTT 的输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/senior/2.2_udp-kcp-quic/tcp-udp-rtt/udp-rtt)

SET(CMAKE_BUILD_TYPE "Debug")
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb -std=c++11")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O0 -Wall -std=c++11")

 

add_library(kcp_rtt STATIC ikcp.c)

install(FILES ikcp.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(TARGETS kcp_rtt
    EXPORT kcp-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT kcp-targets
    FILE kcp-config.cmake
    NAMESPACE kcp::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/kcp
)

if (BUILD_TESTING)
    enable_language(CXX)
    
    add_executable(client_rtt client.c ikcp.c delay.c)
    target_link_libraries(client_rtt pthread)
    set_target_properties(client_rtt PROPERTIES OUTPUT_NAME "udp_rtt_client")
    
    add_executable(server_rtt server.c ikcp.c)
    set_target_properties(server_rtt PROPERTIES OUTPUT_NAME "udp_rtt_server")
    
    add_executable(test_kcp_rtt test.cpp ikcp.c)
    set_target_properties(test_kcp_rtt PROPERTIES OUTPUT_NAME "udp_rtt_test_kcp")
    
    add_executable(chat_server_rtt chat_server.cc kcp_server.cc kcp_session.cc udp_socket.cc ikcp.c)
    target_link_libraries(chat_server_rtt pthread)
    set_target_properties(chat_server_rtt PROPERTIES OUTPUT_NAME "udp_rtt_chat_server")
    
    add_executable(chat_client_rtt chat_client.cc kcp_client.cc kcp_session.cc udp_socket.cc ikcp.c)
    target_link_libraries(chat_client_rtt pthread)
    set_target_properties(chat_client_rtt PROPERTIES OUTPUT_NAME "udp_rtt_chat_client")
    
    if(MSVC AND NOT (MSVC_VERSION LESS 1900))
        target_compile_options(kcp_test PRIVATE /utf-8)
    endif()
endif ()