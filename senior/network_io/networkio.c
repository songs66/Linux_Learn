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
    /**为什么从3开始?
    文件描述符0(stdin) :标准 输入 ,通常用于从用户或其他程序获取输入
    文件描述符1(stdout):标准 输出 ,通常用于将程序的输出结果发送到屏幕或其他输出设备
    文件描述符2(stderr):标准 错误 ,通常用于输出错误信息或警告信息
     **/

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
#elif 0 //一请求一线程实现并发
    while (1) {
        printf("ready to accept\n");
        int clientfd=accept(sockfd,(struct sockaddr*)&clientaddr,&len);
        printf("accept finished,using the number %d clientfd\n",clientfd);
        //上一步中接收到accept返回的clientfd，开始创建子线程
        pthread_t thid;
        pthread_create(&thid,NULL,client_thread, &clientfd);
    }
#elif 0 //使用select实现io多路复用
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
        返回值:所有已经准备好进行 I/O 操作的文件描述符的总数。这包括可读、可写和异常条件的文件描述符，返回等于0时表示超时但没有任何就绪的fd，返回小于0时出错
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
#elif 0 //使用pool来代替select，因为参数简洁，代码比select简洁
    struct pollfd fds[1024]={0};
    fds[sockfd].fd=sockfd;
    fds[sockfd].events=POLLIN;
    /**结构体pollfd的结构：
    struct pollfd {
        int fd;        // 文件描述符
        short events;  // 要监听的事件 POLLIN：文件描述符准备好读取。POLLOUT：文件描述符准备好写入。
                                    POLLERR：发生错误。         POLLHUP：挂起（如对方关闭连接）。
                                    POLLNVAL：文件描述符无效。
        short revents; // 实际发生的事件（由 poll() 填充）并且使用按位与操作符&来比较两个数的二进制表示(与events比较)
                                                     如果两个对应的位都是1，则结果为1，否则，结果为0
    };
     **/

    int maxfd=sockfd;

    while (1) {
        int nready= poll(fds,maxfd+1,-1);
        /**poll()的参数和返回值
        struct pollfd *fds：指向一个 struct pollfd 类型的数组，每个元素表示一个文件描述符及其感兴趣的事件
        nfds_t nfds：表示 fds 数组中的元素数量
        int timeout：指定 poll() 的超时时间，单位为毫秒
                     如果 timeout 为负值（如 -1）poll() 将无限期阻塞，直到有文件描述符准备好
                     如果 timeout 为 0，poll() 将立即返回，进行轮询操作。
        返回值:所有已经准备好进行 I/O 操作的文件描述符的总数。这包括可读、可写和异常条件的文件描述符，返回等于0时表示超时但没有任何就绪的fd，返回小于0时出错
         **/
        if (nready<0) {
            printf("poll error:%s",strerror(errno));
            break;
        }

        if (fds[sockfd].revents&POLLIN) {
            printf("ready to accept\n");
            int clientfd=accept(sockfd,(struct sockaddr*)&clientaddr,&len);
            printf("accept finished,using the number %d clientfd\n",clientfd);

            //再将clientfd添加到fd_set集合中
            fds[clientfd].fd=clientfd;
            fds[clientfd].events=POLLIN;
            //更新最大fd数值，因为可能会有被闲置60s的旧fd被重新利用
            if (clientfd>maxfd) {
                maxfd=clientfd;
            }
        }

        //遍历所有文件描述符，从sockfd + 1开始，到maxfd结束
        int i=0;
        for (i=sockfd+1;i<maxfd+1;i++) {
            //如果此clientfd为在集合rset中的且准备好
            if (fds[i].revents&POLLIN) {
                //接收
                char buffer[1024]={0};
                int count=recv(i,buffer,sizeof(buffer),0);
                //如果客户端断开连接
                if (count==0) {
                    printf("client disconnect,won't use the number %d clientfd\n",i);
                    //将第i号clientfd关掉
                    close(i);
                    //将文件描述符设置为无效值，并且清除监视的事件
                    fds[i].fd = -1;
                    fds[i].events=0;
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
#elif 1 //使用最高级的epoll来实现io多路复用
    //epoll_create()用于创建一个epoll实例,返回一个文件描述符epfd,参数1在老版中表示预期的最大事件数量，新版大于0即可
    int epfd = epoll_create(1);

    //定义了一个epoll_event结构体ev，用于描述要监听的事件
    struct epoll_event ev = {0};
    ev.events=EPOLLIN;      //表示监听可读事件
    ev.data.fd = sockfd;    //将ev结构体与服务器的监听套接字sockfd关联起来，通常用于在事件触发时快速获取文件描述符

    //将sockfd与其对应的event结构体添加进epfd
    epoll_ctl(epfd,EPOLL_CTL_ADD,sockfd,&ev);

    while (1) {
        struct epoll_event events[1024]={0};
        int nready=epoll_wait(epfd,events,sizeof(events)/sizeof(events[0]),-1);
        /**epool_wait()参数 返回值详解：
         epfd       ：由epoll_create()返回的epoll实例文件描述符
         events     ：输出参数，用于接收就绪的事件列表，必须提前分配好内存。
         maxevents  ：告诉内核最多返回多少个事件，不能超过 events 数组的大小。
         timeout	：超时时间（毫秒）：-1：永久阻塞   0：立即返回（非阻塞） >0：等待指定毫秒数
         返回值      ：> 0  就绪的事件个数;0 超时，没有事件就绪;-1 出错，errno会被设置
         **/
        if (nready<0) {
            printf("epoll error:%s",strerror(errno));
            break;
        }

        int i=0;
        for (i=0;i<nready;i++) {
            int connfd=events[i].data.fd;

            //如果是监听sockfd且为可读状态
            if (connfd==sockfd&&events[i].events&EPOLLIN) {
                printf("ready to accept\n");
                int clientfd=accept(sockfd,(struct sockaddr*)&clientaddr,&len);
                printf("accept finished,using the number %d clientfd\n",clientfd);

                //重用ev将clientfd存入epfd，且不会影响上一步使用ev的sockfd，因为EPOLL_CTL_ADD时会将整个ev结构体复制一份进入epoll实例的内部数据结构
                ev.events=EPOLLIN;
                ev.data.fd=clientfd;
                epoll_ctl(epfd,EPOLL_CTL_ADD,clientfd,&ev);
            }
            //如果是clientfd且为可读状态
            else if (events[i].events&EPOLLIN) {
                char buffer[1024]={0};
                int count=recv(connfd,buffer,sizeof(buffer),0);
                //如果客户端断开连接
                if (count==0) {
                    printf("client disconnect,won't use the number %d clientfd\n",connfd);
                    //将第connfd号clientfd关掉
                    close(connfd);
                    //使用EPOLL_CTL_DEL将其删除
                    epoll_ctl(epfd,EPOLL_CTL_DEL,connfd,NULL);
                    //继续进行下一次循环
                    continue;
                }
                printf("successful recv %d bytes\nRECV:%s\n",count,buffer);
                //发送
                count=send(connfd,buffer,count,0);
                printf("successful send %d bytes\n",count);
            }
        }

    }

#endif
    getchar();
    printf("exit\n");
    return 0;
}
