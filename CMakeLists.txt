cmake_minimum_required(VERSION 3.31)
project(mycode)

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_CXX_STANDARD 20)

# 把 Windows 上的“假”头文件目录塞给编译器（其实不编译，只索引）
set(LINUX_HEADERS "D:/linux_headers/usr")
include_directories(${LINUX_HEADERS}/include)
include_directories(${LINUX_HEADERS}/include/x86_64-linux-gnu)
include_directories(${LINUX_HEADERS}/lib/gcc/x86_64-linux-gnu/12/include)
include_directories(${LINUX_HEADERS}/lib/gcc/x86_64-linux-gnu/12/include-fixed)
include_directories(D:/linux_headers/mysql_headers/mysql)

# 打开 Linux 宏，补全/跳转更精准
add_definitions(-D__linux__ -D__gnu_linux__ -D_POSIX_SOURCE -D_GNU_SOURCE)


add_executable(mycode
        junior/01_count/count.cpp
        junior/06_dns/dns.c
        senior/2.1_network_io/04_Websocket/websocket.c
        senior/2.1_network_io/04_Websocket/reactor.c

)
