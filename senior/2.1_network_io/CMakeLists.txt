# 网络 I/O 学习项目

# 设置网络 I/O 的输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/senior/2.1_network_io)

add_executable(00_networkio 00_networkio.c)
add_executable(01_reactor 01_reactor.c)
add_executable(02_reactor_listen_multiple_ports 02_reactor_listen_multiple_ports.c)

# WebSocket 部分
add_executable(reactor 03_Webserver/Webserver.c 03_Webserver/reactor.c)
add_executable(Websocket 04_Websocket/Websocket.c 04_Websocket/reactor.c)

# 链接线程库
find_package(Threads REQUIRED)
foreach(target 
            00_networkio
            01_reactor
            02_reactor_listen_multiple_ports 
            reactor
            Websocket
        )
    target_link_libraries(${target} Threads::Threads)
endforeach()

# 找到 OpenSSL
find_package(OpenSSL REQUIRED)

# 把 WebSocket 可执行文件链接到 crypto
target_link_libraries(Websocket OpenSSL::Crypto)