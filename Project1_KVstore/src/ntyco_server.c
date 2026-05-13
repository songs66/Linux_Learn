#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>

#include "nty_coroutine.h"

#define BUFFER_LENGTH 1024


// 协程版网络层复用同一套业务处理入口
typedef int (*msg_handler)(char *msg, int length, char *response);
static msg_handler kvs_handler;


// 单个连接对应一个 reader 协程，负责循环收包、处理协议并回包
void server_reader(void *arg) {
    // acceptor 中为每个连接单独申请了一份 fd 参数，这里取出后立即释放
    int fd = *(int *)arg;
    free(arg);

    int ret = 0;
    while(1) {
        // 每轮请求使用一份独立的读缓冲区，避免残留上一次数据
        char rbuffer[BUFFER_LENGTH] = {0};
        ret = recv(fd,rbuffer,BUFFER_LENGTH,0);

        // recv 返回 0 说明客户端主动断开连接
        if(ret == 0) {
            close(fd);
            break;
        } else if(ret > 0) {
            // 协议层将响应写入发送缓冲区，并返回响应长度
            char wbuffer[BUFFER_LENGTH] = {0};
            int wlength = kvs_handler(rbuffer, ret, wbuffer);

            // 将协议层生成的响应回发给客户端
            ret = send(fd,wbuffer,wlength,0);
            if(ret == -1) {
                close(fd);
                break;
            }
        }
    }
}

// acceptor 协程：负责监听端口并为每个新连接创建 reader 协程
void server_acceptor(void *arg) {
    unsigned short port = *(unsigned short *)arg;

    // 创建监听 socket
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) return ;

    // 初始化监听地址和客户端地址结构
	struct sockaddr_in local, remote;
    memset(&local, 0, sizeof(local));
    memset(&remote, 0, sizeof(remote));

    // 绑定到指定端口并监听所有网卡地址
	local.sin_family = AF_INET;
	local.sin_port = htons(port);
	local.sin_addr.s_addr = htonl(INADDR_ANY);

	if(-1 == bind(sockfd, (struct sockaddr*)&local, sizeof(struct sockaddr_in))) {
        perror("bind:");
        close(sockfd);
        return;
    }
	if (-1 == listen(sockfd, 10)) {
        perror("listen:");
        close(sockfd);
        return;
    }
	printf("listen finished,using the number %d sockfd\n",sockfd);

    while(1) {
        // 协程库会把阻塞式 accept 包装成可调度的挂起/恢复流程
        socklen_t len = sizeof(struct sockaddr_in);
        int cli_fd = accept(sockfd, (struct sockaddr*)&remote, &len);
        if (cli_fd < 0) {
            perror("accept:");
            continue;
        }

        // 为每个协程单独分配一份 fd，避免传递局部变量地址
        int *fd_ptr = malloc(sizeof(int));
        if (fd_ptr == NULL) {
            close(cli_fd);
            continue;
        }
        *fd_ptr = cli_fd;

        // 为每个客户端连接创建一个 reader 协程，让每个连接各自顺序处理请求
        nty_coroutine *read_co;
        nty_coroutine_create(&read_co,server_reader,fd_ptr);
    }
}


// 协程版网络层入口：保存业务回调并启动 acceptor 协程
int ntyco_start(unsigned short port, msg_handler handler) {
    kvs_handler = handler;

    nty_coroutine *co = NULL;
    nty_coroutine_create(&co, server_acceptor, &port);

    // 启动协程调度器后，acceptor/reader 会在同一线程中被调度执行
    nty_schedule_run();

    return 0;
}
