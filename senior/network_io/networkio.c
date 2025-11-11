#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <poll.h>
#include <sys/epoll.h>


void* client_thread(void *arg) {

    int clientfd=*(int*)arg;
    //每个子线程中所作的事情
    while (1) {
        //接收
        char  buffer[1024]={0};
        int count=recv(clientfd,buffer,sizeof(buffer),0);
        if (count==0) {
            printf("client disconnect,won't use the number %d clientfd\n",clientfd);
            close(clientfd);
            break;
        }
        printf("successful recv %d bytes\nRECV:%s\n",count,buffer);
        //发送
        count=send(clientfd,buffer,count,0);
        printf("successful send %d bytes\n",count);
    }
    return 0;
}

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
#elif 0 //一请求一线程
    while (1) {
        printf("ready to accept\n");
        int clientfd=accept(sockfd,(struct sockaddr*)&clientaddr,&len);
        printf("accept finished,using the number %d clientfd\n",clientfd);
        //上一步中接收到accept返回的clientfd，开始创建子线程
        pthread_t thid;
        pthread_create(&thid,NULL,client_thread, &clientfd);
    }
#elif 1 //使用select实现io多路复用
    //使用fd_set创建集合，用于表示一组文件描述符的状态。它是一个固定大小的数组，每个位对应一个文件描述符
    fd_set rfds,rset;
    /**为什么要建立两个集合？
    rfds：用于存储需要监视的文件描述符集合。
    rset：在每次调用 select() 之前，将 rfds 的内容复制到 rset 中，因为 select() 会修改传入的集合。
     **/

    //使用FD_ZERO进行初始化:清空rfds集合，将所有位设置为0
    FD_ZERO(&rfds);
    //将指定的文件描述符sockfd添加到fd_set集合中.具体来说，它将fd_set中对应fd的位设置为1
    FD_SET(sockfd, &rfds);
    //设置最大文件描述符
    int maxfd = sockfd;

    while (1) {
        //将rfds的内容复制到rset中：因为select()会修改传入的文件描述符集合，但是我们希望保留原始的rfds集合
        rset=rfds;
        //调用select()函数，监视文件描述符集合rset
        int nready=select(maxfd+1,&rset,NULL,NULL,NULL);
        /**参数以及返回值,作用原理：
        第一个参数:nfds：需要监视的最大文件描述符值加1。
        第二个参数:readfds：指向一个 fd_set 类型的指针，表示需要监视的 可读 文件描述符集合。
        第三个参数:writefds：指向一个 fd_set 类型的指针，表示需要监视的 可写 文件描述符集合。
        第四个参数:exceptfds：指向一个 fd_set 类型的指针，表示需要监视的 异常条件 文件描述符集合。
        第五个参数:timeout：指向一个 timeval 结构体的指针，表示超时时间。NULL为一直阻塞状态
        返回值:所有已经准备好进行 I/O 操作的文件描述符的总数。这包括可读、可写和异常条件的文件描述符，返回小于0时出错
        作用原理：对rset集合中的位进行修改，就绪的置为1，非就绪的置回0
        **/
        if (nready<0) {
            printf("select error:%s",strerror(errno));
            break;
        }

        //如果sockfd准备好，调用 accept() 接受新的连接，返回新的客户端文件描述符 clientfd
        if (FD_ISSET(sockfd,&rset)) {
            printf("ready to accept\n");
            int clientfd=accept(sockfd,(struct sockaddr*)&clientaddr,&len);
            printf("accept finished,using the number %d clientfd\n",clientfd);

            //再将clientfd添加到fd_set集合中
            FD_SET(clientfd,&rfds);
            //更新最大fd数值，因为可能会有被闲置60s的旧fd被重新利用
            if (clientfd>maxfd) {
                maxfd=clientfd;
            }
        }

        //遍历所有文件描述符，从sockfd + 1开始，到maxfd结束
        int i=0;
        for (i=sockfd+1;i<maxfd+1;i++) {
            //如果此clientfd为在集合rset中的且准备好
            if (FD_ISSET(i,&rset)) {
                //接收
                char buffer[1024]={0};
                int count=recv(i,buffer,sizeof(buffer),0);
                //如果客户端断开连接
                if (count==0) {
                    printf("client disconnect,won't use the number %d clientfd\n",i);
                    //将第i号clientfd关掉
                    close(i);
                    //将fd_set中对应fd的位设置回0
                    FD_CLR(i,&rfds);
                    //继续进行下一次循环
                    continue;
                }
                printf("successful recv %d bytes\nRECV:%s\n",count,buffer);
                //发送
                count=send(i,buffer,count,0);
                printf("successful send %d bytes\n",count);
            }
        }
    }
#elif

#endif

    return 0;
}
