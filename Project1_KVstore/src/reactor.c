#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <poll.h>
#include <sys/epoll.h>
#include <errno.h>
#include <sys/time.h>


#include "server.h"


#define CONNECTION_SIZE		1024

#define MAX_PORTS			1

#define TIME_SUB_MS(tv1, tv2)  ((tv1.tv_sec - tv2.tv_sec) * 1000 + (tv1.tv_usec - tv2.tv_usec) / 1000)

// reactor 版网络层通过业务处理函数指针，把请求交给协议层
#if ENABLE_KVSTORE

typedef int (*msg_handler)(char *msg, int length, char *response);
static msg_handler kvs_handler;

int kvs_request(struct conn *c) {

	c->wlength = kvs_handler(c->rbuffer, c->rlength, c->wbuffer);

}

int kvs_response(struct conn *c) {

}

#endif
// 全局 epoll fd：整个 reactor 事件循环只维护这一份
int epfd=0;
// 用 fd 直接作为下标，快速定位每个连接对应的状态
struct conn conn_list[CONNECTION_SIZE];

int accept_cb(int fd);  //sockfd可读的回调函数
int recv_cb(int fd);    //clientfd可读的回调函数
int send_cb(int fd);    //clientfd可写时的回调函数

// 封装 epoll_ctl：统一处理“新增监听”与“修改监听事件”
int set_event(int fd,int event,int flag) {
    if (flag) {                                             //为1时
        //定义了一个epoll_event结构体ev，用于描述要监听的事件
        struct epoll_event ev = {0};
        ev.events=event;      //表示监听可读事件
        ev.data.fd = fd;    //将ev结构体与服务器的监听套接字sockfd关联起来，通常用于在事件触发时快速获取文件描述符

        epoll_ctl(epfd,EPOLL_CTL_ADD,fd,&ev);
    }else {                                                 //为0时
        //定义了一个epoll_event结构体ev，用于描述要监听的事件
        struct epoll_event ev = {0};
        ev.events=event;      //表示监听可读事件
        ev.data.fd = fd;    //将ev结构体与服务器的监听套接字sockfd关联起来，通常用于在事件触发时快速获取文件描述符

        epoll_ctl(epfd,EPOLL_CTL_MOD,fd,&ev);
    }
    return 0;
}

int accept_cb(int fd) {
    // 为 accept 返回的 clientfd 准备地址结构体
    struct sockaddr_in clientaddr;
    socklen_t len = sizeof(clientaddr);

    printf("ready to accept\n");
    int clientfd=accept(fd,(struct sockaddr*)&clientaddr,&len);
    if (clientfd==-1) {
        printf("accept error:%s\n",strerror(errno));
        return -1;
    }
    printf("accept finished,using the number %d clientfd\n",clientfd);

    conn_list[clientfd].fd=clientfd;
    // 新连接建立后，后续读事件由 recv_cb 负责处理
    conn_list[clientfd].r_action.recv_callback=recv_cb;
    conn_list[clientfd].send_callback=send_cb;

    memset(conn_list[clientfd].rbuffer,0,BUFFER_LENGTH);
    conn_list[clientfd].rlength=0;
    memset(conn_list[clientfd].wbuffer,0,BUFFER_LENGTH);
    conn_list[clientfd].wlength=0;

    set_event(clientfd,EPOLLIN,1);

    return 0;
}

int recv_cb(int fd) {
    memset(conn_list[fd].rbuffer,0,BUFFER_LENGTH);
    int count=recv(fd,conn_list[fd].rbuffer,BUFFER_LENGTH,0);
    if (count==-1) {
        printf("recv error:%s\n",strerror(errno));

        //将第connfd号clientfd关掉
        close(fd);
        //使用EPOLL_CTL_DEL将其删除
        epoll_ctl(epfd,EPOLL_CTL_DEL,fd,NULL);
        memset(&conn_list[fd],0,sizeof(conn_list[fd]));
        return -1;
    }
    // 客户端主动断开时回收 fd 和对应连接状态
    if (count==0) {
        printf("client disconnect,won't use the number %d clientfd\n",fd);
        //将第connfd号clientfd关掉
        close(fd);
        //使用EPOLL_CTL_DEL将其删除
        epoll_ctl(epfd,EPOLL_CTL_DEL,fd,NULL);
        memset(&conn_list[fd],0,sizeof(conn_list[fd]));
        //继续进行下一次循环
        return 0;
    }
    conn_list[fd].rlength=count;

    printf("successful recv %d bytes form clientfd:%d\nRECV:%s\n",conn_list[fd].rlength,fd,conn_list[fd].rbuffer);

#if 0   //echo重复
    conn_list[fd].wlength=conn_list[fd].rlength;
    memset(conn_list[fd].wbuffer,0,BUFFER_LENGTH);
    memcpy(conn_list[fd].wbuffer,conn_list[fd].rbuffer,conn_list[fd].rlength);
#elif ENABLE_HTTP
    http_request(&conn_list[fd]);
#elif ENABLE_WEBSOCKET
    ws_request(&conn_list[fd]);
#elif ENABLE_KVSTORE
    kvs_request(&conn_list[fd]);
#endif

    // 业务处理完成后切到写事件，把响应发回客户端
    set_event(fd,EPOLLOUT,0);

    return count;
}

int send_cb(int fd) {

#if ENABLE_HTTP
	http_response(&conn_list[fd]);
#elif ENABLE_WEBSOCKET
	ws_response(&conn_list[fd]);
#elif ENABLE_KVSTORE
	kvs_response(&conn_list[fd]);
#endif

    int count=0;

#if ENABLE_HTTP
    if (conn_list[fd].status==1) {
        count=send(fd,conn_list[fd].wbuffer,conn_list[fd].wlength,0);
        printf("%s\n",conn_list[fd].wbuffer);
        printf("successful send %d bytes\n",count);

        set_event(fd,EPOLLOUT,0);
    }else if (conn_list[fd].status==2) {
        set_event(fd,EPOLLOUT,0);
    }else if (conn_list[fd].status==0) {
        set_event(fd,EPOLLIN,0);
    }
#elif ENABLE_KVSTORE
    if(conn_list[fd].wlength!=0){
        count=send(fd,conn_list[fd].wbuffer,conn_list[fd].wlength,0);
        printf("successful send %d bytes to clientfd:%d\nSEND:%s\n",count,fd,conn_list[fd].wbuffer);

    }    

    set_event(conn_list[fd].fd,EPOLLIN,0);
#endif

    return 0;
}

// 创建并初始化监听 socket，供 reactor 主循环注册到 epoll 中
static int init_server(const unsigned short port) {
    //创建用于监听的sockfd
    int sockfd=socket(AF_INET,SOCK_STREAM,0);

    //为sockfd绑定地址
    struct sockaddr_in servaddr;
    servaddr.sin_family=AF_INET;
    servaddr.sin_addr.s_addr=htons(INADDR_ANY);
    servaddr.sin_port=htons(port);
    int ret =bind(sockfd,(struct sockaddr*)&servaddr,sizeof(struct sockaddr));
    if (ret==-1) {
        printf("bind failed:%s\n",strerror(errno));
    }

    //开始监听发挥作用，最多连接队列为5
    listen(sockfd,10);
    printf("listen finished,using the number %d sockfd\n",sockfd);
    return sockfd;
}

int reactor_start(unsigned short port, msg_handler handler) {

    kvs_handler=handler;

    epfd=epoll_create(1);

    int i=0;
    for (i=0;i<MAX_PORTS;i++) {

        int sockfd=init_server(port+i);

        conn_list[sockfd].fd=sockfd;
        //仅有这一个在监听，所以只有他是accept_cb,连接fd在accept_cb中被创建并且分配recv——cb
        conn_list[sockfd].r_action.accept_callback=accept_cb;

        set_event(sockfd,EPOLLIN,1);
    }

    // reactor 主循环：等待事件，再按事件类型分发到对应回调
    while (1) {
        struct epoll_event events [1024]={0};
        int nready=epoll_wait(epfd,events,1024,-1);
        if (nready<0) {
            printf("epoll error:%s\n",strerror(errno));
            break;
        }

        int i=0;
        for (i=0;i<nready;i++) {
            int connfd=events[i].data.fd;
            // EPOLLIN / EPOLLOUT 是位标志，可能同时出现在同一个事件上
            if (events[i].events&EPOLLIN) {
                // 监听 socket 这里实际会走 accept_callback，普通连接会走 recv_callback
                conn_list[connfd].r_action.recv_callback(connfd);
            }
            if (events[i].events&EPOLLOUT) {
                conn_list[connfd].send_callback(connfd);
            }
        }
    }
    return 0;
}
