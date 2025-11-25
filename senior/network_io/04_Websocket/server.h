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
    /**
    根据 c->status 的值，分阶段处理 HTTP 响应：
        如果 c->status == 0，构造 HTTP 响应头。
        如果 c->status == 1，使用 sendfile 发送文件内容。
        如果 c->status == 2，清理状态，准备处理下一个请求。
     **/
    /**
    根据 c->status 的值，分阶段处理 Websocket 响应：
        如果 c->status == 0，表示尚未握手，调用 handshark 函数进行握手。
        如果 c->status == 1，表示握手完成，调用 decode_packet 函数解码数据帧。
        如果 c->status == 2，表示需要发送响应，调用 encode_packet 函数编码数据帧。
     **/
#if 1 // websocket
    char *payload;
    char mask[4];
#endif
};

int ws_request(struct conn *c);
int ws_response(struct conn *c);

#endif //SERVER_H
