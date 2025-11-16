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
#elif 1
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
#endif
    return c->wlength;
}







