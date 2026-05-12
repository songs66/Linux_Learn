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


typedef int (*msg_handler)(char *msg, int length, char *response);
static msg_handler kvs_handler;



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


typedef struct io_conn_s {
    int fd;
    char rbuf[BUFFER_LENGTH];
    int rlen;
    char wbuf[BUFFER_LENGTH];
    int wlen;
} io_conn_t;

typedef struct io_event_s {
    int type;          // ACCEPT / READ / WRITE
    int listen_fd;
    io_conn_t *conn;
} io_event_t;


//设置accept事件
int set_event_accept(struct io_uring *ring, int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags) {
	//从 ring 里拿一个 SQE
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    if (sqe == NULL) {
        return -1;
    }

    //准备一个 io_event_t
    io_event_t *event = malloc(sizeof(struct io_event_s));
    if(event == NULL) return -1;
    memset(event,0,sizeof(struct io_event_s));

    event->type = EVENT_ACCEPT;
    event->listen_fd = sockfd;
    event->conn = NULL;

    //提交一个 accept 请求
    io_uring_prep_accept(sqe,sockfd,addr,addrlen,flags);

    //把 event 挂到这个 SQE 上
    io_uring_sqe_set_data(sqe,event);

    return 0;
}



int set_event_recv(struct io_uring *ring, io_conn_t *conn, int flags) {
    //从 ring 里拿一个 SQE
	struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    if(sqe == NULL){
        return -1;
    }
    //清空一下上一次的读数据
    memset(conn->rbuf,0,BUFFER_LENGTH);
    conn->rlen = 0;

    //准备一个 io_event_t
    io_event_t *event = malloc(sizeof(io_event_t));
    if (event == NULL) {
        return -1;
    }
    memset(event,0,sizeof(struct io_event_s));

    event->type = EVENT_RECV;
    event->listen_fd = -1;
    event->conn = conn;

    //提交一个 recv 请求
    io_uring_prep_recv(sqe,event->conn->fd,event->conn->rbuf,BUFFER_LENGTH,flags);

    //把 event 挂到这个 SQE 上
    io_uring_sqe_set_data(sqe, event);

    return 0;
}


int set_event_send(struct io_uring *ring, io_conn_t *conn, int flags) {
    //从 ring 里拿一个 SQE
	struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    if(sqe == NULL){
        return -1;
    }

    //准备一个 io_event_t
    io_event_t *event = malloc(sizeof(io_event_t));
    if (event == NULL) {
        return -1;
    }
    memset(event,0,sizeof(struct io_event_s));

    event->type = EVENT_SEND;
    event->listen_fd = -1;
    event->conn = conn;

    //提交一个 recv 请求
    io_uring_prep_send(sqe,conn->fd,conn->wbuf,conn->wlen,flags);

    //把 event 挂到这个 SQE 上
    io_uring_sqe_set_data(sqe, event);

    return 0;
}



int proactor_start(unsigned short port, msg_handler handler) {
	int sockfd = init_server(port);
    printf("listen finished,using the number %d sockfd\n",sockfd);
	kvs_handler = handler;

	struct io_uring ring;
	struct io_uring_params params;
	memset(&params, 0, sizeof(params));
	io_uring_queue_init_params(ENTRIES_LENGTH, &ring, &params);

    struct sockaddr_in clientaddr;	
	socklen_t len = sizeof(clientaddr);
    set_event_accept(&ring,sockfd,(struct sockaddr*)&clientaddr,&len,0);
    printf("set_event_accept success!\n");

    while(1) {
        //把前面准备好的 SQE 提交给内核，让内核真正开始执行这些异步 I/O 请求。
        io_uring_submit(&ring);

        //先看一眼
		struct io_uring_cqe *cqe;
		io_uring_wait_cqe(&ring, &cqe);
        //再捞一堆上来
		struct io_uring_cqe *cqes[128];
		int nready = io_uring_peek_batch_cqe(&ring, cqes, 128);

        //开始挨个处理
        for (int i = 0; i < nready; i++){
			struct io_uring_cqe *entries = cqes[i];
            io_event_t *event = io_uring_cqe_get_data(entries);

            switch (event->type)
            {
            case EVENT_ACCEPT:
                //创建 conn
                io_conn_t *conn = malloc(sizeof(struct io_conn_s));
                memset(conn,0,sizeof(struct io_conn_s));
                //将新创建的conn赋值给io_event_t
                event->conn=conn;
                //将accept事件返回的客户端fd赋值
                event->conn->fd = entries->res;//新连接的 fd

                printf("accept finished,using the number %d clientfd\n",entries->res);

                // 给新连接提交 recv
                set_event_recv(&ring,event->conn,0);
                printf("set_event_recv success!\n");
                // 再补一个 accept
                set_event_accept(&ring,sockfd,(struct sockaddr*)&clientaddr,&len,0);
                printf("set_event_accept success!\n");
                break;

            case EVENT_RECV:
                event->conn->rlen = entries->res;//实际收到的字节数
                if(event->conn->rlen == 0) {
                    printf("client disconnect,won't use the number %d clientfd\n",event->conn->fd);
                    close(event->conn->fd);
                    free(event->conn);
                    free(event);
                    break;
                } else if (event->conn->rlen > 0) {
                    printf("successful recv %d bytes form clientfd:%d\nRECV:%s\n",event->conn->rlen,event->conn->fd,event->conn->rbuf);

                    //清空一下上一次发出数据
                    memset(event->conn->wbuf,0,BUFFER_LENGTH);
                    event->conn->wlen = 0;
                    event->conn->wlen = kvs_handler(event->conn->rbuf,event->conn->rlen,event->conn->wbuf);

                    set_event_send(&ring,event->conn,0);
                    printf("set_event_send success!\n");
                    break;
                }

            case EVENT_SEND:
                int send_ret = entries->res;//实际发出的字节数
                printf("successful send %d bytes to clientfd:%d\nSEND:%s\n",send_ret,event->conn->fd,event->conn->wbuf);

                set_event_recv(&ring,event->conn,0);
                printf("set_event_recv success!\n");
                break;
            default:
                return -1;
            }
        }
        //把 io_uring 内部完成队列 CQ 的 head 指针向前移动 nready 格，表示CQ 队列最前面的 nready 个完成事件已经消费完了
		io_uring_cq_advance(&ring, nready);
    }
    return 0;
}
