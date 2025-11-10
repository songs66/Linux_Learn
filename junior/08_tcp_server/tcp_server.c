#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <pthread.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/epoll.h>

#define BUFFER_LENGTH		1024
#define EPOLL_SIZE			1024


//一请求一线程的回调函数
void *client_routine(void *arg) {
    int clientfd = *(int *) arg;

    while (1) {
        //准备内存用来存储要接收的字节
        char buffer[BUFFER_LENGTH] = {0};
        int len = recv(clientfd, buffer, BUFFER_LENGTH, 0);
        if (len < 0) {
            close(clientfd);
            break;
        }
        if (len == 0) {
            // disconnect
            close(clientfd);
            break;
        }
        printf("成功接收到%d字节数据\n", len);
        printf("Recv: %s\n", buffer);
    }
    return NULL;
}


int main(int argc, char *argv[]) {
    //第一个参数为文件名本身，第二个参数为此tcp服务器要监听的端口
    if (argc != 2) {
        printf("Param Error\n");
        return -1;
    }
    //将端口号由字符串形式转成数字形式
    int port = atoi(argv[1]);

    //创建一个sockfd用于监听
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    //创建一个地址结构体用于下一步和sockfd的bind
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    //INADDR_ANY表示监听本地的所有ipv4的地址，因为一个网卡可以有n多个IP地址
    addr.sin_addr.s_addr = INADDR_ANY;

    //bind():将地址结构体绑定到sockfd上，相当于给sockfd分配工作，返回0成功，返回-1失败
    if (bind(sockfd, (struct sockaddr *) &addr, sizeof(struct sockaddr_in)) < 0) {
        perror("bind error");
        return -2;
    }

    //listen:sockfd正式开始监听工作，第二个参数表示已完成三次握手后正在等待accept()取走的连接队列”的长度
    if (listen(sockfd, 5) < 0) {
        perror("listen error");
        return -3;
    }

#if 0
    //一请求一线程(已被淘汰)
    while (1) {
        //准备一个IPv4地址盒子，用来接收对端（客户端）的IP和端口
        struct sockaddr_in client_addr;
        memset(&client_addr, 0, sizeof(struct sockaddr_in));
        socklen_t client_len = sizeof(client_addr);

        //accept()取出已完成的连接 然后返回新的fd，然后立刻创建线程去服务(默认为阻塞)
        int clientfd = accept(sockfd, (struct sockaddr*)&client_addr, &client_len);
        if (clientfd==-1) {
            break;
        }

        pthread_t thread_id;
        pthread_create(&thread_id, NULL, client_routine, &clientfd);
    }

#else
    //epool实现io多路复用

    //创建一个epoll实例，返回一个文件描述符epfd用于后续管理所有socket事件，参数只要大于0就行。相当于快递员
    const int epfd = epoll_create(1);
    //定义一个epool事件数组，是epoll_wait的“输出缓冲区”，相当于快递员的袋子
    struct epoll_event events[EPOLL_SIZE] = {0};

    //将用于监听的sockfd交给epool管理
    struct epoll_event ev;
    ev.events = EPOLLIN; //只关注是否可读（EPOLLOUT关心是否可写）
    ev.data.fd = sockfd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ev);

    //主循环：等待事件
    while (1) {
        //epool_wait()等待一组文件描述符（events[]）上的I/O事件
        const int nready = epoll_wait(epfd, events, EPOLL_SIZE, 5);
        /**epool_wait()参数 返回值详解：
         epfd       ：由epoll_create()返回的epoll实例文件描述符
         events     ：输出参数，用于接收就绪的事件列表，必须提前分配好内存。
         maxevents  ：告诉内核最多返回多少个事件，不能超过 events 数组的大小。
         timeout	：超时时间（毫秒）：-1：永久阻塞   0：立即返回（非阻塞） >0：等待指定毫秒数
         返回值      ：> 0  就绪的事件个数;0 超时，没有事件就绪;-1 出错，errno会被设置
         **/
        if (nready == -1) {
            continue;
        }

        //开始循环遍历events数组，处理每个就绪事件
        int i = 0;
        for (i = 0; i < nready; i++) {
            //如果是监听socket(listen)就绪（新连接）
            if (events[i].data.fd == sockfd) {
                //准备一个IPv4地址盒子，用来接收对端（客户端）的IP和端口
                struct sockaddr_in client_addr;
                memset(&client_addr, 0, sizeof(struct sockaddr_in));
                socklen_t client_len = sizeof(client_addr);
                //accept()取出已完成的连接 然后返回新的fd
                int clientfd = accept(sockfd, (struct sockaddr *) &client_addr, &client_len);

                //然后将新的clientfd添加到epool
                ev.events = EPOLLIN | EPOLLET;
                /**水平触发(LT)和边缘触发(ET)的区别：
                 水平触发(LT)：只要有数据每次epoll_wait就会返回，一次没有读完可以多次接收数据
                 边缘触发(ET)：只在状态变化时通知一次，效率高，但必须一次性把数据读完
                 **/
                ev.data.fd = clientfd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, clientfd, &ev);
            } else {//如果是客户端 socket 就绪（有数据）
                int clientfd = events[i].data.fd;
                //调用recv读取clientfd中收到的数据
                char buffer[BUFFER_LENGTH] = {0};
                int len = recv(clientfd, buffer, BUFFER_LENGTH, 0);
                if (len < 0) {
                    //如果 recv 返回负值，表示出错，关闭连接并从 epoll 中移除
                    close(clientfd);
                    ev.events = EPOLLIN;
                    ev.data.fd = clientfd;
                    epoll_ctl(epfd, EPOLL_CTL_DEL, clientfd, &ev);
                } else if (len == 0) {
                    //如果 len == 0，表示客户端主动断开连接（TCP 四次挥手），同样关闭并移除
                    close(clientfd);
                    ev.events = EPOLLIN;
                    ev.data.fd = clientfd;
                    epoll_ctl(epfd, EPOLL_CTL_DEL, clientfd, &ev);
                } else {
                    printf("成功接收到来自clientfd:%d的%d字节数据\n", clientfd,len);
                    printf("Recv: %s\n", buffer);
                }
            }
        }
    }
#endif
    return 0;
}
