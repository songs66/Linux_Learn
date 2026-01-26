# Junior 学习项目
cmake_minimum_required(VERSION 3.16)

# 设置 Junior 的输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/junior)

# 基础练习项目
add_executable(count 01_count/count.cpp)

add_executable(contacts 02_contacts/contacts.cpp)

# 多线程练习
add_executable(00_thread 03_pthread/00_thread.cpp)
add_executable(01_mutexlock 03_pthread/01_mutexlock.cpp)
add_executable(02_spinlock 03_pthread/02_spinlock.cpp)
add_executable(03_atomiclock 03_pthread/03_atomiclock.cpp)

# 线程池
add_executable(threadpool 04_threadpool/Threadpool.c)

# MySQL
find_package(PkgConfig)
if(PkgConfig_FOUND)
    pkg_check_modules(MYSQLCLIENT mysqlclient)
    if(MYSQLCLIENT_FOUND)
        add_executable(mysql 05_mysql/mysql.c)
        target_link_libraries(mysql ${MYSQLCLIENT_LIBRARIES})
        target_include_directories(mysql PRIVATE ${MYSQLCLIENT_INCLUDE_DIRS})
    endif()
endif()

# DNS
add_executable(dns 06_dns/dns.c)

# HTTP
add_executable(httprequest 07_http/httprequest.c)
add_executable(xinzhi_httprequest 07_http/xinzhi_httprequest.c)

# TCP
add_executable(tcp_server 08_tcp_server/tcp_server.c)
add_executable(tcp_server_listen_multiple_ports 09_tcp_server_listen_multiple_ports/tcp_server_listen_multiple_ports.c)

#TCP测试客户端
add_executable(mul_port_client_epoll mul_port_client_epoll.c)

# 链接线程库
find_package(Threads REQUIRED)
foreach(target 
            00_thread
            01_mutexlock
            02_spinlock
            03_atomiclock
            04_threadpool
            08_tcp_server
            tcp_server_listen_multiple_ports
            mul_port_client_epoll
        )
    if(TARGET ${target})
        target_link_libraries(${target} Threads::Threads)
    endif()
endforeach()