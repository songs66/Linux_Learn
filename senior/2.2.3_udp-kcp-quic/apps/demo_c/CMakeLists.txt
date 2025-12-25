find_package(Threads REQUIRED)

add_executable(client client.c delay.c)
target_link_libraries(client PRIVATE kcp::kcp Threads::Threads)
target_include_directories(client PRIVATE ${CMAKE_CURRENT_LIST_DIR})

add_executable(server server.c)
target_link_libraries(server PRIVATE kcp::kcp Threads::Threads)
target_include_directories(server PRIVATE ${CMAKE_CURRENT_LIST_DIR})
