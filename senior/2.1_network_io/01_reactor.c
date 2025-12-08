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

#define BUFFER_LENGTH   1024
#define CONNECTION_SIZE 1024


//定义了一个函数指针类型RCALLBACK，它指向一个函数，该函数接收一个int类型的参数fd，并返回一个int类型的值
typedef int (*RCALLBACK)(int fd);
//网络通信的连接结构体conn
struct conn {
    int fd;

    char rbuffer[BUFFER_LENGTH];
    int rlength;

    char wbuffer[BUFFER_LENGTH];
    int wlength;

    //可写时的事件
    RCALLBACK send_callback;
    //联合体的所有成员共享同一块内存，因此在任何时刻，联合体中只有一个成员的值是有效的
    union {
        //可读时的事件
        RCALLBACK recv_callback;
        RCALLBACK accept_callback;
    }r_action;
};
//结构体conn数组conn_list：(使用fd的数字作为下标)
struct conn conn_list[CONNECTION_SIZE];
//定义全局的epfd
int epfd=0;
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
    printf("accept finished,using the number %d clientfd\n",clientfd);

    conn_list[clientfd].fd=clientfd;
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
    //如果客户端断开连接
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

    printf("successful recv %d bytes\nRECV:%s\n",conn_list[fd].rlength,conn_list[fd].rbuffer);

#if 1   //echo重复
    conn_list[fd].wlength=conn_list[fd].rlength;
    memset(conn_list[fd].wbuffer,0,BUFFER_LENGTH);
    memcpy(conn_list[fd].wbuffer,conn_list[fd].rbuffer,conn_list[fd].rlength);
#endif

    set_event(fd,EPOLLOUT,0);

    return count;
}

int send_cb(int fd) {
    //发送
    int count=send(fd,conn_list[fd].wbuffer,conn_list[fd].wlength,0);
    printf("successful send %d bytes\n",count);

    set_event(fd,EPOLLIN,0);

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
    unsigned short port=8888;
    int sockfd=init_server(port);

    epfd=epoll_create(1);

    conn_list[sockfd].fd=sockfd;
    conn_list[sockfd].r_action.accept_callback=accept_cb;

    set_event(sockfd,EPOLLIN,1);

    //主循环
    while (1) {
        struct epoll_event events [1024]={0};
        int nready=epoll_wait(epfd,events,1024,-1);
        if (nready<0) {
            printf("epoll error:%s",strerror(errno));
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
