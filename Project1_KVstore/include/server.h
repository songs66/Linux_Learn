#ifndef __SERVER_H__
#define __SERVER_H__

#define BUFFER_LENGTH		1024

#define ENABLE_HTTP			0
#define ENABLE_WEBSOCKET	0
#define ENABLE_KVSTORE		1

//回调函数指针
typedef int (*RCALLBACK)(int fd);


struct conn {
	int fd;

	char rbuffer[BUFFER_LENGTH];
	int rlength;

	char wbuffer[BUFFER_LENGTH];
	int wlength;

	RCALLBACK send_callback;

	union {
		RCALLBACK recv_callback;
		RCALLBACK accept_callback;
	} r_action;

#if ENABLE_HTTP && ENABLE_WEBSOCKET // webserver和websocket专用属性
	//状态机，用于区别回应文件是否发送完成
    int status;
/**
    根据 c->status 的值，分阶段处理 HTTP 响应：
        如果 c->status == 0，构造 HTTP 响应头。
        如果 c->status == 1，使用 sendfile 发送文件内容。
        如果 c->status == 2，清理状态，准备处理下一个请求。
    根据 c->status 的值，分阶段处理 Websocket 响应：
        如果 c->status == 0，表示尚未握手，调用 handshark 函数进行握手。
        如果 c->status == 1，表示握手完成，调用 decode_packet 函数解码数据帧。
        如果 c->status == 2，表示需要发送响应，调用 encode_packet 函数编码数据帧。
**/
#endif

#if ENABLE_WEBSOCKET // websocket专用属性
	char *payload;
	char mask[4];
#endif
};


#if ENABLE_HTTP
int http_request(struct conn *c);
int http_response(struct conn *c);

#elif ENABLE_WEBSOCKET
int ws_request(struct conn *c);
int ws_response(struct conn *c);

#elif ENABLE_KVSTORE
int kvs_request(struct conn *c);
int kvs_response(struct conn *c);

#endif



#endif
