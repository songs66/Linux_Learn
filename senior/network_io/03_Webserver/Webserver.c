#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/sendfile.h>
#include <errno.h>

#include "server.h"

#define WEBSERVER_ROOTDIR	"./"

//用于处理请求
int http_request(struct conn *c) {
    memset(c->wbuffer,0,BUFFER_LENGTH);
    c->wlength=0;

    c->status=0;

    return 0;
}

//用于组织回答
int http_response(struct conn *c) {
#if 0
    c->wlength=sprintf(c->wbuffer,
                            "HTTP/1.1 200 OK\r\n"
                          "Content-Type: text/html\r\n"
                          "Accept-Ranges: bytes\r\n"
                          "Content-Length: 156\r\n"
                          "Date: Tue, 30 Apr 2024 13:16:46 GMT\r\n\r\n"

                          "<!DOCTYPE html>"
                            "<html lang=\"zh-CN\">"
                            "<head>"
                                "<meta charset=\"UTF-8\">"
                                "<title>0voice.king</title>"
                            "</head>"
                            "<body>"
                                "<h1>宋子豪</h1>"
                            "</body>"
                          "</html>\r\n\r\n");
#elif 0
    int filefd = open("index.html", O_RDONLY);

    struct stat stat_buf;
    fstat(filefd, &stat_buf);

    c->wlength = sprintf(c->wbuffer,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Accept-Ranges: bytes\r\n"
        "Content-Length: %ld\r\n"
        "Date: Tue, 30 Apr 2024 13:16:46 GMT\r\n\r\n",
        stat_buf.st_size);

    int count = read(filefd, c->wbuffer + c->wlength, BUFFER_LENGTH - c->wlength);
    c->wlength += count;

    close(filefd);
#elif 1
    int filefd = open("index.html", O_RDONLY);

    //定义了一个 struct stat 类型的变量 stat_buf，用于存储文件的状态信息。struct stat 是一个结构体，包含文件的详细信息，如大小、权限、所有者等
    struct stat stat_buf;
    //调用 fstat 函数，通过文件描述符 filefd 获取文件的状态信息，并将其存储在 stat_buf 中。如果成功，fstat 返回 0；如果失败，返回 -1 并设置 errno
    fstat(filefd, &stat_buf);

    if (c->status==0) {
        c->wlength = sprintf(c->wbuffer,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Accept-Ranges: bytes\r\n"
        "Content-Length: %ld\r\n"
        "Date: Tue, 30 Apr 2024 13:16:46 GMT\r\n\r\n",
        stat_buf.st_size);

        c->status=1;
    }else if (c->status==1) {
        int ret = sendfile(c->fd,filefd,NULL,stat_buf.st_size);
        if (ret==-1) {
            printf("error:%s",strerror(errno));
        }

        c->status=2;
    }else if (c->status==2) {
        c->wlength=0;
        memset(c->wbuffer,0,BUFFER_LENGTH);

        c->status=0;
    }

    close(filefd);
#endif
    return c->wlength;
}







