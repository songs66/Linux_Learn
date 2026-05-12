#include <stdio.h>
#include <stdlib.h>
#include <liburing.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "nty_coroutine.h"

#define BUFFER_LENGTH 1024


typedef int (*msg_handler)(char *msg, int length, char *response);
static msg_handler kvs_handler;


void server_reader(void *arg) {
    int fd = *(int *)arg;
    free(arg);

    int ret = 0;
    while(1) {
        char rbuffer[BUFFER_LENGTH] = {0};
        ret = recv(fd,rbuffer,BUFFER_LENGTH,0);
        if(ret == 0) {
            close(fd);
            break;
        } else if(ret > 0) {
            char wbuffer[BUFFER_LENGTH] = {0};
            int wlength = kvs_handler(rbuffer, ret, wbuffer);

            ret = send(fd,wbuffer,wlength,0);
            if(ret == -1) {
                close(fd);
                break;
            }
        }
    }
}

//监听和派发连接
void server_acceptor(void *arg) {
    unsigned short port = *(unsigned short *)arg;

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) return ;

	struct sockaddr_in local, remote;
    memset(&local, 0, sizeof(local));
    memset(&remote, 0, sizeof(remote));

	local.sin_family = AF_INET;
	local.sin_port = htons(port);
	local.sin_addr.s_addr = htonl(INADDR_ANY);

	if(-1 == bind(sockfd, (struct sockaddr*)&local, sizeof(struct sockaddr_in))) {
        perror("bind:");
    }
	listen(sockfd, 10);
	printf("listen finished,using the number %d sockfd\n",sockfd);

    while(1) {
        socklen_t len = sizeof(struct sockaddr_in);
        int cli_fd = accept(sockfd, (struct sockaddr*)&remote, &len);

        int *fd_ptr = malloc(sizeof(int));
        *fd_ptr = cli_fd;

        nty_coroutine *read_co;
        nty_coroutine_create(&read_co,server_reader,fd_ptr);
    }
}

int ntyco_start(unsigned short port, msg_handler handler) {
    kvs_handler = handler;

    nty_coroutine *co = NULL;
    nty_coroutine_create(&co, server_acceptor, &port);

    nty_schedule_run();

    return 0;
}
