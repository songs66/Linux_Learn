#ifndef SERVER_H
#define SERVER_H

#define BUFFER_LENGTH   4096

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

    //状态机，用于区别回应文件是否发送完成
    int status;
};


int http_request(struct conn *c);
int http_response(struct conn *c);

#endif //SERVER_H
