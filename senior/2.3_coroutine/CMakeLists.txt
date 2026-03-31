# 协程学习项目

# 设置协程的输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/senior/2.3_coroutine)

add_executable(coroutine_async_dns async_dns_client_noblock.c)
add_executable(coroutine_hook hook.c)
add_executable(coroutine_mini mini_ntyco.c)
add_executable(coroutine_server server_mulport_epoll.c)
add_executable(coroutine_setjmp setjmp.c)
add_executable(coroutine_ucontext ucontext-backup.c)

# 链接线程库
find_package(Threads REQUIRED)
foreach(target
            coroutine_async_dns
            coroutine_hook
            coroutine_mini
            coroutine_server
            coroutine_setjmp
            coroutine_ucontext
        )
    target_link_libraries(${target} Threads::Threads)
endforeach()