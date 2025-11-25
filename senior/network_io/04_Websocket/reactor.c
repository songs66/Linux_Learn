//编译指令：gcc -fpic 02_reactor_listen_multiple_ports.c -o 02_reactor_listen_multiple_ports
//大数组改变了内存布局，使得全局变量超出了32位相对寻址的范围，导致链接错误，使用-fpic可以解决这个问题
#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <poll.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <stdint.h>
#include <arpa/inet.h>

#include "server.h"

#define CONNECTION_SIZE 1024

#define MAX_PORT    1



//定义全局的epfd
int epfd=0;
//结构体conn数组conn_list：(使用fd的数字作为下标)
struct conn conn_list[CONNECTION_SIZE];

int accept_cb(int fd);  //sockfd可读的回调函数
int recv_cb(int fd);    //clientfd可读的回调函数
int send_cb(int fd);    //clientfd可写时的回调函数


//用于将clientfd添加进epfd，flag为1时为新添加fd，flag为0时为更改fd
int set_event(int fd,int event,int flag) {
    if (flag) {
        //定义了一个epoll_event结构体ev，用于描述要监听的事件
        struct epoll_event ev = {0};
        ev.events=event;      //表示监听可读事件
        ev.data.fd = fd;    //将ev结构体与服务器的监听套接字sockfd关联起来，通常用于在事件触发时快速获取文件描述符

        epoll_ctl(epfd,EPOLL_CTL_ADD,fd,&ev);
    }else {
        //定义了一个epoll_event结构体ev，用于描述要监听的事件
        struct epoll_event ev = {0};
        ev.events=event;      //表示监听可读事件
        ev.data.fd = fd;    //将ev结构体与服务器的监听套接字sockfd关联起来，通常用于在事件触发时快速获取文件描述符

        epoll_ctl(epfd,EPOLL_CTL_MOD,fd,&ev);
    }
    return 0;
}

int accept_cb(int fd) {
    //为accept返回的clientfd准备地址结构体用于被accept填满
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
    conn_list[clientfd].r_action.recv_callback=recv_cb;
    conn_list[clientfd].send_callback=send_cb;

    memset(conn_list[clientfd].rbuffer,0,BUFFER_LENGTH);
    conn_list[clientfd].rlength=0;
    memset(conn_list[clientfd].wbuffer,0,BUFFER_LENGTH);
    conn_list[clientfd].wlength=0;

    conn_list[clientfd].status=0;

    set_event(clientfd,EPOLLIN,1);

    return 0;
}

int recv_cb(int fd) {
    memset(conn_list[fd].rbuffer, 0, BUFFER_LENGTH);
    int count = recv(fd, conn_list[fd].rbuffer, BUFFER_LENGTH, 0);

    if (count == -1) {
        printf("recv error:%s\n", strerror(errno));
        close(fd);
        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
        memset(&conn_list[fd], 0, sizeof(conn_list[fd]));
        return -1;
    }

    if (count == 0) {
        printf("client disconnect,won't use the number %d clientfd\n", fd);
        close(fd);
        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
        memset(&conn_list[fd], 0, sizeof(conn_list[fd]));
        return 0;
    }

    conn_list[fd].rlength = count;

    printf("Received %d bytes in state %d\n", count, conn_list[fd].status);

    // 根据状态处理接收到的数据
    if (conn_list[fd].status == 0) {
        // 状态0：处理握手请求
        printf("Processing handshake request...\n");
        ws_request(&conn_list[fd]);
        // 握手处理后需要发送响应
        set_event(fd, EPOLLOUT, 0);
    } else if (conn_list[fd].status == 1) {
        // 状态1：处理WebSocket数据帧
        printf("Processing WebSocket data frame...\n");
        ws_request(&conn_list[fd]);

        // 如果处理后有数据要发送，切换到可写状态
        if (conn_list[fd].status == 2) {
            set_event(fd, EPOLLOUT, 0);
        }
    }

    return count;
}

int send_cb(int fd) {
    if (conn_list[fd].status == 0) {
        // 发送握手响应
        printf("Sending handshake response, length: %d\n", conn_list[fd].wlength);
        int count = send(fd, conn_list[fd].wbuffer, conn_list[fd].wlength, 0);
        printf("Handshake response sent: %d bytes\n", count);

        if (count > 0) {
            // 握手完成后，状态切换到1，准备接收数据帧
            conn_list[fd].status = 1;
            printf("Handshake completed, switching to state 1\n");
            set_event(fd, EPOLLIN, 0);
        }

    } else if (conn_list[fd].status == 2) {
        // 准备WebSocket数据帧响应
        ws_response(&conn_list[fd]);

        if (conn_list[fd].wlength > 0) {
            printf("Sending WebSocket data, length: %d\n", conn_list[fd].wlength);
            int count = send(fd, conn_list[fd].wbuffer, conn_list[fd].wlength, 0);
            printf("WebSocket data sent: %d bytes\n", count);

            if (count > 0) {
                // 发送完成后，状态回到1，准备接收下一个数据帧
                conn_list[fd].status = 1;
                printf("Data sent, switching back to state 1\n");
            }
        }

        set_event(fd, EPOLLIN, 0);
    }

    return 0;
}

//用于初始化sockfd，并且开始监听，返回sockfd的值
int init_server(const unsigned short port) {
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
    listen(sockfd,5);
    printf("listen finished,using the number %d sockfd\n",sockfd);
    return sockfd;
}

int main() {
    epfd=epoll_create(1);

    int i=0;
    for (i=0;i<MAX_PORT;i++) {
        unsigned short port=8888;
        int sockfd=init_server(port+i);

        conn_list[sockfd].fd=sockfd;
        conn_list[sockfd].r_action.accept_callback=accept_cb;

        set_event(sockfd,EPOLLIN,1);
    }

    //主循环
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
            //EPOLLIN和EPOLLOUT是位标志(bit flags)，而不是普通的数值，所以可以同时存在，即可读又可写
            if (events[i].events&EPOLLIN) {
                //联合体共享内存，如果此fd为clientfd，则调用recv_callback就是调用accept_callback
                conn_list[connfd].r_action.recv_callback(connfd);
            }
            if (events[i].events&EPOLLOUT) {
                conn_list[connfd].send_callback(connfd);
            }
        }
    }
    return 0;
}
