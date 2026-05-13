#include <stdio.h>
#include <stdlib.h>
#include <liburing.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>

#define EVENT_ACCEPT   	0
#define EVENT_RECV		1
#define EVENT_SEND		2

#define ENTRIES_LENGTH		1024    //决定提交队列容量
#define BUFFER_LENGTH		1024


// io_uring 版网络层也只关心“请求进来后交给谁处理”
typedef int (*msg_handler)(char *msg, int length, char *response);
static msg_handler kvs_handler;



// 创建监听 socket，供 io_uring proactor 模型使用
static int init_server(unsigned int port) {
    int sockfd = socket(AF_INET,SOCK_STREAM,0);
    struct sockaddr_in serveraddr;
    memset(&serveraddr,0,sizeof(struct sockaddr_in));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons(port);

    if(-1 == bind(sockfd,(struct sockaddr*)&serveraddr,sizeof(struct sockaddr))) {
        perror("bind:");
        return -1;
    }

    listen(sockfd,10);

    return sockfd;
}


// 每个客户端连接对应一份独立的收发上下文
typedef struct io_conn_s {
    int fd;
    char rbuf[BUFFER_LENGTH];
    int rlen;
    char wbuf[BUFFER_LENGTH];
    int wlen;
} io_conn_t;

// 每个提交到 io_uring 的异步请求都挂一份事件上下文，方便完成时分发处理
typedef struct io_event_s {
    int type;          // ACCEPT / READ / WRITE
    int listen_fd;
    io_conn_t *conn;
} io_event_t;


// 提交 accept 请求：监听 socket 上有新连接到来时，由 CQE 完成通知
int set_event_accept(struct io_uring *ring, int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags) {
	// 从 ring 里拿一个 SQE，用来描述本次 accept 请求
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    if (sqe == NULL) {
        return -1;
    }

    // 为这次 accept 准备一份事件上下文
    io_event_t *event = malloc(sizeof(struct io_event_s));
    if(event == NULL) return -1;
    memset(event,0,sizeof(struct io_event_s));

    event->type = EVENT_ACCEPT;
    event->listen_fd = sockfd;
    event->conn = NULL;

    // 提交一个 accept 请求，等待新连接到来
    io_uring_prep_accept(sqe,sockfd,addr,addrlen,flags);

    // 把事件上下文挂到 SQE 上，完成时可直接取回
    io_uring_sqe_set_data(sqe,event);

    return 0;
}

// 提交 recv 请求：把新收到的数据读入当前连接自己的读缓冲区
int set_event_recv(struct io_uring *ring, io_conn_t *conn, int flags) {
    // 从 ring 里拿一个 SQE，用来描述本次 recv 请求
	struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    if(sqe == NULL){
        return -1;
    }
    // 清空上一次接收残留的数据，并重置本次接收长度
    memset(conn->rbuf,0,BUFFER_LENGTH);
    conn->rlen = 0;

    // 为这次 recv 准备一份事件上下文
    io_event_t *event = malloc(sizeof(io_event_t));
    if (event == NULL) {
        return -1;
    }
    memset(event,0,sizeof(struct io_event_s));

    event->type = EVENT_RECV;
    event->listen_fd = -1;
    event->conn = conn;

    // 提交一个 recv 请求，把数据读入当前连接自己的读缓冲区
    io_uring_prep_recv(sqe,event->conn->fd,event->conn->rbuf,BUFFER_LENGTH,flags);

    // 把事件上下文挂到 SQE 上，完成时可直接取回
    io_uring_sqe_set_data(sqe, event);

    return 0;
}

// 提交 send 请求：把协议层生成的响应从写缓冲区发回客户端
int set_event_send(struct io_uring *ring, io_conn_t *conn, int flags) {
    // 从 ring 里拿一个 SQE，用来描述本次 send 请求
	struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    if(sqe == NULL){
        return -1;
    }

    // 为这次 send 准备一份事件上下文
    io_event_t *event = malloc(sizeof(io_event_t));
    if (event == NULL) {
        return -1;
    }
    memset(event,0,sizeof(struct io_event_s));

    event->type = EVENT_SEND;
    event->listen_fd = -1;
    event->conn = conn;

    // 提交一个 send 请求，把协议层生成的响应回发给客户端
    io_uring_prep_send(sqe,conn->fd,conn->wbuf,conn->wlen,flags);

    // 把事件上下文挂到 SQE 上，完成时可直接取回
    io_uring_sqe_set_data(sqe, event);

    return 0;
}



// io_uring 版网络层入口：负责初始化 ring，并在主循环中分发 accept/recv/send 事件
int proactor_start(unsigned short port, msg_handler handler) {
	int sockfd = init_server(port);
    printf("listen finished,using the number %d sockfd\n",sockfd);
	kvs_handler = handler;

    // 初始化 io_uring 实例
	struct io_uring ring;
	struct io_uring_params params;
	memset(&params, 0, sizeof(params));
	io_uring_queue_init_params(ENTRIES_LENGTH, &ring, &params);

    // 启动时先提交一个 accept，请求第一个客户端连接
    struct sockaddr_in clientaddr;	
	socklen_t len = sizeof(clientaddr);
    set_event_accept(&ring,sockfd,(struct sockaddr*)&clientaddr,&len,0);
    printf("set_event_accept success!\n");

    while(1) {
        // 把当前准备好的 SQE 全部提交给内核执行
        io_uring_submit(&ring);

        // 阻塞等待至少一个完成事件
		struct io_uring_cqe *cqe;
		io_uring_wait_cqe(&ring, &cqe);
        // 再批量把当前已经完成的事件尽量多取出来
		struct io_uring_cqe *cqes[128];
		int nready = io_uring_peek_batch_cqe(&ring, cqes, 128);

        // 逐个处理完成事件：按 accept / recv / send 三类状态分发
        for (int i = 0; i < nready; i++){
			struct io_uring_cqe *entries = cqes[i];
            io_event_t *event = io_uring_cqe_get_data(entries);

            switch (event->type)
            {
            case EVENT_ACCEPT:
                if (entries->res < 0) {
                    printf("accept error:%s\n", strerror(-entries->res));
                    len = sizeof(clientaddr);
                    set_event_accept(&ring, sockfd, (struct sockaddr*)&clientaddr, &len, 0);
                    free(event);
                    break;
                }
                // 为新连接创建一份独立的收发上下文
                io_conn_t *conn = malloc(sizeof(struct io_conn_s));
                if (conn == NULL) {
                    close(entries->res);
                    len = sizeof(clientaddr);
                    set_event_accept(&ring, sockfd, (struct sockaddr*)&clientaddr, &len, 0);
                    free(event);
                    break;
                }
                memset(conn,0,sizeof(struct io_conn_s));
                // 把新连接上下文挂到这次 accept 事件上
                event->conn=conn;
                // accept 的返回值就是新连接的客户端 fd
                event->conn->fd = entries->res;

                printf("accept finished,using the number %d clientfd\n",entries->res);

                // 给新连接提交第一次 recv，开始监听客户端请求
                set_event_recv(&ring,event->conn,0);
                printf("set_event_recv success!\n");
                // 再补一个 accept，保证后续客户端还能继续接入
                len = sizeof(clientaddr);
                set_event_accept(&ring,sockfd,(struct sockaddr*)&clientaddr,&len,0);
                printf("set_event_accept success!\n");
                free(event);
                break;

            case EVENT_RECV:
                // recv 的返回值就是本次实际收到的字节数
                event->conn->rlen = entries->res;
                if(event->conn->rlen == 0) {
                    // recv 返回 0 说明客户端主动断开连接
                    printf("client disconnect,won't use the number %d clientfd\n",event->conn->fd);
                    close(event->conn->fd);
                    free(event->conn);
                    free(event);
                    break;
                } else if (event->conn->rlen < 0) {
                    printf("recv error:%s\n", strerror(-event->conn->rlen));
                    close(event->conn->fd);
                    free(event->conn);
                    free(event);
                    break;
                } else if (event->conn->rlen > 0) {
                    // 收到请求后交给协议层处理，并把响应写入发送缓冲区
                    printf("successful recv %d bytes form clientfd:%d\nRECV:%s\n",event->conn->rlen,event->conn->fd,event->conn->rbuf);

                    // 清空上一次发送残留的数据，并重置本次响应长度
                    memset(event->conn->wbuf,0,BUFFER_LENGTH);
                    event->conn->wlen = 0;
                    event->conn->wlen = kvs_handler(event->conn->rbuf,event->conn->rlen,event->conn->wbuf);

                    // 提交一次 send，把协议层生成的响应发回去
                    set_event_send(&ring,event->conn,0);
                    printf("set_event_send success!\n");
                    free(event);
                    break;
                }
                free(event);
                break;

            case EVENT_SEND:
                // send 的返回值就是本次实际发出的字节数
                int send_ret = entries->res;
                if (send_ret < 0) {
                    printf("send error:%s\n", strerror(-send_ret));
                    close(event->conn->fd);
                    free(event->conn);
                    free(event);
                    break;
                }
                printf("successful send %d bytes to clientfd:%d\nSEND:%s\n",send_ret,event->conn->fd,event->conn->wbuf);

                // 当前请求处理完毕后，继续为该连接提交下一次 recv
                set_event_recv(&ring,event->conn,0);
                printf("set_event_recv success!\n");
                free(event);
                break;
            default:
                free(event);
                return -1;
            }
        }
        // 通知 io_uring：这批完成事件已经处理完，可以从 CQ 中释放掉
		io_uring_cq_advance(&ring, nready);
    }
    return 0;
}
