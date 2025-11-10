#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <poll.h>
#include <sys/epoll.h>


int main() {
    //创建用于监听的sockfd
    int sockfd=socket(AF_INET,SOCK_STREAM,0);

    //为sockfd绑定地址
    struct sockaddr_in servaddr;
    servaddr.sin_family=AF_INET;
    servaddr.sin_addr.s_addr=htons(INADDR_ANY);
    servaddr.sin_port=htons(8888);
    int ret =bind(sockfd,(struct sockaddr*)&servaddr,sizeof(struct sockaddr));
    if (ret==-1) {
        printf("bind failed:%s\n",strerror(errno));
    }

    //开始监听发挥作用，最多连接队列为5
    listen(sockfd,5);
    printf("listen finished,using the number %d sockfd\n",sockfd);

    //为accept返回的clientfd准备地址结构体用于被accept填满
    struct sockaddr_in clientaddr;
    socklen_t len = sizeof(clientaddr);

#if 0   //仅完成一个客户端一次收发
    //使用accept接收
    printf("ready to accept\n");
    int clientfd=accept(sockfd,(struct sockaddr*)&clientaddr,&len);
    printf("accept finished,using the number %d clientfd\n",clientfd);

    //接收
    char  buffer[1024]={0};
    int count=recv(clientfd,buffer,sizeof(buffer),0);
    printf("successful recv %d bytes\nRECV:%s\n",count,buffer);
    //发送
    count=send(clientfd,buffer,count,0);
    printf("successful send %d bytes\n",count);
#elif 0 //添加while循环,可以有多个客户端进行单次收发
    while (1) {
        printf("ready to accept\n");
        int clientfd=accept(sockfd,(struct sockaddr*)&clientaddr,&len);
        printf("accept finished,using the number %d clientfd\n",clientfd);

        //接收
        char  buffer[1024]={0};
        int count=recv(clientfd,buffer,sizeof(buffer),0);
        printf("successful recv %d bytes\nRECV:%s\n",count,buffer);
        //发送
        count=send(clientfd,buffer,count,0);
        printf("successful send %d bytes\n",count);
    }
#elif 1 //一请求一线程



#endif

    return 0;
}
