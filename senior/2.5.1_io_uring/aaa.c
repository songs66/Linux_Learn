// 编译：gcc -o uring_tcp_server uring_tcp_server.c -luring -static

#include <stdio.h>
#include <liburing.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>

#define EVENT_ACCEPT 0
#define EVENT_RECV 1
#define EVENT_SEND 2

#define ENTRIES_LENGTH 1024
#define BUFFER_LENGTH 1024

int init_server(unsigned short port) {
    // 定义监听所用的sockfd
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    // 定义上一步的sockfd的地址信息
    struct sockaddr_in serveraddr;
    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons(port);

    // sockfd与addr绑定
    if (-1 == bind(sockfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr))) {
        perror("bind");
    }

    // 监听
    listen(sockfd, 10);

    // 返回已经开始监听的sockfd
    return sockfd;
}



// 请求上下文信息
struct conn_info {
    int fd;    // 表示fd
    int event; // 表示事件
};

// 往 io_uring 提交一个“accept 任务 + 绑定上下文信息”
/*
    ring：io_uring 实例（提交队列入口）
    sockfd：监听 socket（listen 之后的 fd）
    addr：用于接收客户端地址
    addrlen：地址长度（输入输出参数）
    flags：accept 的 flags（如 SOCK_NONBLOCK）
*/
int set_event_accept(struct io_uring* ring, int sockfd, struct sockaddr* addr, socklen_t* addrlen, int flags) {
    // 从 io_uring 的 Submission Queue 里拿一个槽位（SQE）
    struct io_uring_sqe* sqe = io_uring_get_sqe(ring);

    // 自行构造上下文信息
    struct conn_info accept_info = {
        .fd = sockfd,
        .event = EVENT_ACCEPT,
    };

    // 准备 accept 请求，把这个 SQE 填充为一个 accept 操作
    io_uring_prep_accept(sqe, sockfd, (struct sockaddr*)addr, addrlen, flags);

    // 绑定 user_data
    memcpy(&sqe->user_data, &accept_info, sizeof(accept_info));

    return 0;
}

int set_event_recv(struct io_uring* ring, int sockfd, void* buf, size_t len, int flags) {
    struct io_uring_sqe* sqe = io_uring_get_sqe(ring);

    struct conn_info recv_info = {
        .fd = sockfd,
        .event = EVENT_RECV,
    };

    io_uring_prep_recv(sqe, sockfd, buf, len, flags);
    memcpy(&sqe->user_data, &recv_info, sizeof(struct conn_info));

}

int set_event_send(struct io_uring* ring, int sockfd, void* buf, size_t len, int flags) {
    struct io_uring_sqe* sqe = io_uring_get_sqe(ring);

    struct conn_info send_info = {
        .fd = sockfd,
        .event = EVENT_SEND,
    };

    io_uring_prep_send(sqe, sockfd, buf, len, flags);
    memcpy(&sqe->user_data, &send_info, sizeof(struct conn_info));

}




int main(int argc, char* argv[]) {
    // 第一个参数为文件名本身，第二个参数为此tcp服务器要监听的端口
    if (argc != 2) {
        printf("param error\n");
        return -1;
    }
    // 将端口号由字符串形式转成数字形式
    unsigned short port = atoi(argv[1]);

    // 开始监听
    int sockfd = init_server(port);

    // 初始化参数结构体
    struct io_uring_params params;
    memset(&params, 0, sizeof(params));

    // 创建 io_uring 实例
    struct io_uring ring;
    io_uring_queue_init_params(ENTRIES_LENGTH, &ring, &params);

    // 为 accept 准备输出参数
    struct sockaddr_in clientaddr;      // clientaddr：保存客户端地址
    socklen_t len = sizeof(clientaddr); // len：地址长度（输入/输出参数）

    // 提交 accept 请求
    set_event_accept(&ring, sockfd, (struct sockaddr*)&clientaddr, &len, 0);
    printf("set_event_accept\n");

    char buffer[BUFFER_LENGTH] = { 0 };
    while (1) {
        // 把你之前准备好的所有 IO 请求，真正“提交给内核去执行”
        io_uring_submit(&ring);

        // 以下两步是一种经典优化模式:先阻塞等一个(避免 CPU 空转)，再一次性捞一批(提高吞吐量,减少函数调用)
        // 阻塞等待一个完成事件:等内核干完一个 IO，再叫我”
        struct io_uring_cqe* cqe;
        io_uring_wait_cqe(&ring, &cqe); // 把第一个 CQE 的指针给你，并不消费
        // 一次性拿最多 128 个已经完成的 IO
        struct io_uring_cqe* cqes[128];
        int nready = io_uring_peek_batch_cqe(&ring, cqes, 128); // 当前 CQ head 开始，把还没消费的 CQE 全部拷出来

        for (int i = 0; i < nready; i++) {
            struct io_uring_cqe* entries = cqes[i];
            struct conn_info result;
            memcpy(&result, &entries->user_data, sizeof(struct conn_info));

            if (result.event == EVENT_ACCEPT) {
                set_event_accept(&ring, result.fd, (struct sockaddr*)&clientaddr, &len, 0);
                printf("set_event_accept\n");

                int connfd = entries->res;
                set_event_recv(&ring, connfd, buffer, BUFFER_LENGTH, 0);
                printf("set_event_recv\n");

            }
            else if (result.event == EVENT_RECV) {
                int ret = entries->res;
                printf("set_event_recv ret: %d, %s\n", ret, buffer);

                if (ret == 0) {
                    close(result.fd);
                }
                else if (ret > 0) {
                    set_event_send(&ring, result.fd, buffer, ret, 0);
                }
            }
            else if (result.event == EVENT_SEND) {
                int ret = entries->res;
                printf("set_event_send ret: %d, %s\n", ret, buffer);

                set_event_recv(&ring, result.fd, buffer, BUFFER_LENGTH, 0);
            }

        }

        io_uring_cq_advance(&ring, nready);

    }

    return 0;
}
