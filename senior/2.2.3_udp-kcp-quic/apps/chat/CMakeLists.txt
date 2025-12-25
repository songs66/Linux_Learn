find_package(Threads REQUIRED)

# chat_server
add_executable(chat_server
        chat_server.cc
        kcp_server.cc
        kcp_session.cc
        udp_socket.cc
)

target_link_libraries(chat_server PRIVATE kcp::kcp Threads::Threads)
target_compile_features(chat_server PRIVATE cxx_std_11)
target_include_directories(chat_server PRIVATE ${CMAKE_CURRENT_LIST_DIR})

# chat_client
add_executable(chat_client
        chat_client.cc
        kcp_client.cc
        kcp_session.cc
        udp_socket.cc
)

target_link_libraries(chat_client PRIVATE kcp::kcp Threads::Threads)
target_compile_features(chat_client PRIVATE cxx_std_11)
target_include_directories(chat_client PRIVATE ${CMAKE_CURRENT_LIST_DIR})
