#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <netdb.h>
#include <fcntl.h>


#define HTTP_VERSION		"HTTP/1.1"
#define CONNECTION_TYPE		"Connection: close\r\n"

#define BUFFER_SIZE		4096

//hostname转换为ip地址，使用gethostbyname()
char* host_to_ip(const char *hostname) {
    //使用结构体*host_entry来存储返回结果
    struct hostent *host_entry = gethostbyname(hostname);
    //inet_ntoa()将二进制(struct in_addr)转换为点分十进制字符串（如 "192.168.1.1"）
    if (host_entry) {
        return inet_ntoa(*(struct in_addr*)*host_entry->h_addr_list);
    }
    /**解析inet_ntoa(*(struct in_addr*)*host_entry->h_addr_list)
     1.host_entry->h_addr_list：返回的数据结构体中host_entry->h_addr_list是指向的4字节网络字节序IPv4地址的二重指针，
        因为可能返回多个ip地址(host_entry->h_addr_list[i]即为指向第i个IP地址的原始二进制数据)
     2.*host_entry->h_addr_list：为取第一个IP地址的指（char* 类型） 实际上指向的是一个struct in_addr的内存块
     3.(struct in_addr*)*host_entry->h_addr_list：将char*强制转换为struct in_addr *
     4.*(struct in_addr*)*host_entry->h_addr_list：解引用，得到 struct in_addr 类型的值
        也就是一个完整的 in_addr 结构体（里面只包含 s_addr(uint32_t:32位无符号整数)，且inet_ntoa()以in_addr 结构体为单位）
     5.inet_ntoa(*(struct in_addr*)*host_entry->h_addr_list)：将 struct in_addr转换为点分十进制字符串
     **/
    return NULL;
}

//创建sockfd并与tcp服务器(发起三次握手)建立连接,再把已连接套接字设成非阻塞模式
int http_create_socket(char *ip) {
    int sockfd=socket(AF_INET,SOCK_STREAM,0);
    //定义 IPv4 专用地址结构
    struct sockaddr_in sin={0};
    sin.sin_family=AF_INET;
    sin.sin_port=htons(80);
    //inet_addr()与上个函数中inet_ntoa()的作用相反：字符串→二进制（网络字节序 uint32_t）
    //可以直接传二进制
    sin.sin_addr.s_addr=inet_addr(ip);

    //发起三次握手连接tcp服务器
    int ret=connect(sockfd,(struct sockaddr*)&sin,sizeof(struct sockaddr_in));
    if (ret!=0) {
        return -1;
    }
    //把已连接套接字设成非阻塞模式
    fcntl(sockfd,F_SETFL,O_NONBLOCK);
    /**fcntl()
     第二个参数是命令(cmd)
     第三个参数是命令所需的附加数据
     ---------阻塞和非阻塞的区别：----------
     场景                        阻塞模式                           非阻塞模式
     读缓冲区空                   `read/recv`挂起线程直到有数据         立即返回 `-1`，`errno=EAGAIN`；线程继续跑
     写缓冲区满                   `write/send`挂起线程直到有空间        立即返回 `-1`，`errno=EAGAIN`；线程继续跑
     `accept` 无已完成连接        睡眠等待新连接                        立即返回 `-1`，`errno=EAGAIN`
     `connect`TCP 三次握手       直到握手完成才返回                     立即返回 `-1`，`errno=EINPROGRESS`，后续用 `select/poll/epoll`检测可写
     **/
    return sockfd;
}

char * http_send_request(const char *hostname, const char *resource) {
    //根据网址获取ip
    char *ip = host_to_ip(hostname);
    //根据IP连接服务器
    int sockfd = http_create_socket(ip);
    //准备向tcp服务器发送的数据，并且使用sprintf()格式化字符串并写入字符数组
    char buffer[BUFFER_SIZE] = {0};
    sprintf(buffer,"GET %s %s\r\n"
                   "Host: %s\r\n"
                   "\r\n",
                   resource, HTTP_VERSION,
                   hostname
    );
    //发送数据
    send(sockfd, buffer, strlen(buffer), 0);


    //select：使用select()轮询socket是否可读，直到超时或连接关闭
    //先声明一个文件描述符集合fdread，用于select()监听可读事件
    fd_set fdread;
    //清空集合，防止未初始化的垃圾值导致未定义行为
    FD_ZERO(&fdread);
    //将sockfd加入到集合中，在下一次的select()中被对应位置的功能监听
    FD_SET(sockfd, &fdread);

    //先申请一个内存用于返回，result在循环中会被重新分配
    char *result = malloc(0);
    result[0]='\0';

    while (1) {
        //设置select的超时时间为5秒
        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;

        //调用 select，监听 sockfd 是否可读
        int selection = select(sockfd+1, &fdread, NULL, NULL, &tv);
        //如果select返回0(超时)或sockfd不在就绪集合中(sockfd的数据被读取)，就退出循环
        if (!selection || !FD_ISSET(sockfd, &fdread)) {
            break;
        }

        //重用刚才发送数据的buffer用于接收数据
        memset(buffer, 0, BUFFER_SIZE);
        //接收数据
        int len = recv(sockfd, buffer, BUFFER_SIZE, 0);
        //如果接收到的字符数量为0说明disconnect
        if (len == 0) {
            break;
        }
        //result的内存被重新分配，存储返回的数据
        result = realloc(result, (strlen(result) + len + 1) * sizeof(char));
        memset(result, 0, (strlen(result) + len + 1) * sizeof(char));
        strncat(result, buffer, len);
    }
    return result;
}

int main(int argc, char *argv[]) {

    if (argc != 3) {
        printf("param error\n");
        return -1;
    }

    char *response = http_send_request(argv[1], argv[2]);
    printf("response:\n %s\n", response);
    free(response);
}


#if 0
int main(int argc,char *argv[]) {
    printf("%s\n",host_to_ip(argv[1]));

}
#endif
