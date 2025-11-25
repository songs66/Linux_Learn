//编译指令：gcc reactor.c websocket.c -o webserver -lssl -lcrypto
#include <stdio.h>

#include "server.h"

#include <openssl/sha.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/evp.h>

#define GUID  "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
/*
    key: "fUNa6rJwr4/VDpwcgvceYA=="
    fUNa6rJwr4/VDpwcgvceYA==258EAFA5-E914-47DA-95CA-C5AB0DC85B11

    SHA-1

    20 bytes

    base64


    handshark

    transmission
      decode
      encode
*/

/**
        定义了一个 WebSocket 数据帧的头部结构 _nty_ophdr。
        opcode：操作码，表示帧的类型（如文本帧、二进制帧、控制帧等）。
        rsv3、rsv2、rsv1：保留位，必须为 0。
        fin：表示是否是最后一个分片帧。
        payload_length：负载长度，表示数据部分的长度。
        mask：掩码标志，表示数据是否被掩码。
        __attribute__ ((packed))：确保结构体按字节对齐，避免填充字节。
 */
struct _nty_ophdr {
    unsigned char opcode:4,
         rsv3:1,
         rsv2:1,
         rsv1:1,
         fin:1;
    unsigned char payload_length:7,
        mask:1;

} __attribute__ ((packed));

/**
        定义了当负载长度为 126 时的数据帧头部结构 _nty_websocket_head_126。
        payload_length：扩展的负载长度，为 16 位。
        mask_key：掩码密钥，用于解码数据。
        data：数据部分。
 */
struct _nty_websocket_head_126 {
    unsigned short payload_length;
    char mask_key[4];
    unsigned char data[8];
} __attribute__ ((packed));

/**
        定义了当负载长度为 127 时的数据帧头部结构 _nty_websocket_head_127。
        payload_length：扩展的负载长度，为 64 位。
        mask_key：掩码密钥，用于解码数据。
        data：数据部分。
 */
struct _nty_websocket_head_127 {
    unsigned long long payload_length;
    char mask_key[4];

    unsigned char data[8];

} __attribute__ ((packed));

/**
        使用 typedef 为上述结构体定义了别名，方便后续使用。
 */
typedef struct _nty_websocket_head_127 nty_websocket_head_127;
typedef struct _nty_websocket_head_126 nty_websocket_head_126;
typedef struct _nty_ophdr nty_ophdr;

/**Base64 编码函数：
        实现了 Base64 编码功能。
        使用 OpenSSL 的 BIO 和 BUF_MEM 结构来处理 Base64 编码。
        输入参数 in_str 是要编码的字符串，in_len 是其长度，out_str 是输出的 Base64 编码字符串。
        返回编码后的字符串长度。
 */
int base64_encode(char *in_str, int in_len, char *out_str) {
	BIO *b64, *bio;
	BUF_MEM *bptr = NULL;
	size_t size = 0;

	if (in_str == NULL || out_str == NULL)
		return -1;

	b64 = BIO_new(BIO_f_base64());
	bio = BIO_new(BIO_s_mem());
	bio = BIO_push(b64, bio);

	// 不添加换行符
	BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);

	BIO_write(bio, in_str, in_len);
	BIO_flush(bio);

	BIO_get_mem_ptr(bio, &bptr);
	if (bptr->length > 0) {
		memcpy(out_str, bptr->data, bptr->length);
		out_str[bptr->length] = '\0';
		size = bptr->length;
	}

	BIO_free_all(bio);
	return size;
}

/**读取一行数据的函数：
		从 allbuf 中读取一行数据，直到遇到 \r\n。
		level 是起始位置，linebuf 是存储读取到的行的缓冲区。
		返回读取到的行的长度，如果未找到 \r\n 则返回 -1。
 */
int readline(char* allbuf,int level,char* linebuf){
	int len = strlen(allbuf);

	for (;level < len; ++level)    {
		if(allbuf[level]=='\r' && allbuf[level+1]=='\n')
			return level+2;
		else
			*(linebuf++) = allbuf[level];
	}

	return -1;
}

/**解码数据帧的函数：
		解码 WebSocket 数据帧的负载数据。
		使用掩码密钥 mask 对数据 data 进行解码，长度为 len。
		按照 WebSocket 协议，每个字节的数据与掩码密钥的相应字节进行异或操作。
 */
void demask(char *data,int len,char *mask){
	int i;
	for (i = 0;i < len;i ++)
		*(data+i) ^= *(mask+(i%4));
}
/**解码数据帧的函数：
		解码 WebSocket 数据帧。
		输入参数 stream 是接收到的数据帧，mask 是掩码密钥，length 是数据帧的长度，ret 是解码后的数据长度。
		根据负载长度的不同（126 或 127），选择不同的头部结构进行解码。
		解码后的数据存储在 stream + start 中，返回解码后的数据指针。
 */
char* decode_packet(unsigned char *stream, char *mask, int length, int *ret) {
	if (length < 2) {
		*ret = 0;
		return NULL;
	}

	nty_ophdr *hdr = (nty_ophdr*)stream;
	int offset = 2; // 基本头部长度

	// 获取payload长度
	uint64_t payload_len = hdr->payload_length;

	if (payload_len == 126) {
		if (length < offset + 2) {
			*ret = 0;
			return NULL;
		}
		uint16_t len16;
		memcpy(&len16, stream + offset, 2);
		payload_len = ntohs(len16);
		offset += 2;
	} else if (payload_len == 127) {
		if (length < offset + 8) {
			*ret = 0;
			return NULL;
		}
		uint64_t len64;
		memcpy(&len64, stream + offset, 8);
		payload_len = be64toh(len64); // 网络字节序转主机字节序
		offset += 8;
	}

	// 读取掩码
	if (hdr->mask) {
		if (length < offset + 4) {
			*ret = 0;
			return NULL;
		}
		memcpy(mask, stream + offset, 4);
		offset += 4;
	}

	// 检查数据长度是否足够
	if (length < offset + payload_len) {
		*ret = 0;
		return NULL;
	}

	*ret = payload_len;
	char *payload_data = (char*)(stream + offset);

	// 如果有掩码，解码数据
	if (hdr->mask && payload_len > 0) {
		demask(payload_data, payload_len, mask);
	}

	return payload_data;
}

/**编码数据帧的函数：
		编码 WebSocket 数据帧。
		输入参数 buffer 是编码后的数据帧存储位置，mask 是掩码密钥，stream 是要编码的数据，length 是数据长度。
		根据数据长度选择不同的头部结构进行编码。
		编码后的数据帧存储在 buffer 中，返回数据帧的总长度。
 */
int encode_packet(char *buffer, char *mask, char *stream, int length) {
	nty_ophdr head = {0};
	head.fin = 1;
	head.opcode = 1; // 文本帧
	head.mask = 0;   // 服务器发送不需要掩码

	int offset = 2; // 基本头部长度

	if (length < 126) {
		head.payload_length = length;
	} else if (length < 65536) {
		head.payload_length = 126;
	} else {
		head.payload_length = 127;
	}

	// 写入基本头部
	memcpy(buffer, &head, 2);

	// 写入扩展长度
	if (length < 126) {
		// 不需要额外长度字段
	} else if (length < 65536) {
		uint16_t len16 = htons(length);
		memcpy(buffer + offset, &len16, 2);
		offset += 2;
	} else {
		uint64_t len64 = htobe64(length);
		memcpy(buffer + offset, &len64, 8);
		offset += 8;
	}

	// 服务器发送不需要掩码
	// 直接复制数据
	memcpy(buffer + offset, stream, length);

	return offset + length;
}

/**WebSocket 握手函数：
		处理 WebSocket 握手过程。
		输入参数 c 是一个连接结构体，包含请求和响应的缓冲区。
		从客户端请求中提取 Sec-WebSocket-Key，并将其与 GUID 拼接。
		使用 SHA-1 算法对拼接后的字符串进行哈希，然后进行 Base64 编码，生成 Sec-WebSocket-Accept。
		构造 WebSocket 握手响应，发送给客户端。
 */
#define WEBSOCK_KEY_LENGTH	19
int handshark(struct conn *c) {
	char linebuf[1024] = {0};
	int idx = 0;
	char sec_data[128] = {0};
	char sec_accept[32] = {0};
	char websocket_key[128] = {0};

	do {
		memset(linebuf, 0, 1024);
		idx = readline(c->rbuffer, idx, linebuf);

		if (strstr(linebuf, "Sec-WebSocket-Key")) {
			// 正确提取Sec-WebSocket-Key的值
			char *key_start = strchr(linebuf, ':');
			if (key_start) {
				key_start++; // 跳过冒号
				// 跳过前导空格
				while (*key_start == ' ') key_start++;

				// 复制key值（去除末尾的\r\n）
				char *key_end = strstr(key_start, "\r\n");
				if (key_end) {
					int key_len = key_end - key_start;
					strncpy(websocket_key, key_start, key_len);
					websocket_key[key_len] = '\0';
				}
			}

			// 拼接GUID
			strcat(websocket_key, GUID);

			// 计算SHA1哈希
			unsigned char sha_hash[SHA_DIGEST_LENGTH];
			SHA1((unsigned char*)websocket_key, strlen(websocket_key), sha_hash);

			// Base64编码
			base64_encode((char*)sha_hash, SHA_DIGEST_LENGTH, sec_accept);

			memset(c->wbuffer, 0, BUFFER_LENGTH);
			c->wlength = sprintf(c->wbuffer,
				"HTTP/1.1 101 Switching Protocols\r\n"
				"Upgrade: websocket\r\n"
				"Connection: Upgrade\r\n"
				"Sec-WebSocket-Accept: %s\r\n\r\n", sec_accept);

			printf("ws response : %s\n", c->wbuffer);
			break;
		}
	} while(idx != -1 && idx < c->rlength);

	return 0;
}

/**WebSocket 请求处理函数
处理 WebSocket 请求。
如果状态为 0，表示尚未握手，调用 handshark 函数进行握手。
如果状态为 1，表示握手完成，调用 decode_packet 函数解码数据帧。
解码后的数据存储在 c->payload 中，长度存储在 c->wlength 中。
 */
int ws_request(struct conn *c) {
	if (c->status == 0) {
		// 状态0：处理握手请求
		printf("Processing WebSocket handshake...\n");
		handshark(c);
		// 状态保持为0，等待发送握手响应

	} else if (c->status == 1) {
		// 状态1：处理WebSocket数据帧
		printf("Processing WebSocket data frame, length: %d\n", c->rlength);

		// 检查是否是有效的WebSocket帧（至少2字节）
		if (c->rlength >= 2) {
			int ret = 0;

			// 解码数据帧
			c->payload = decode_packet((unsigned char*)c->rbuffer, c->mask, c->rlength, &ret);

			if (ret > 0 && c->payload != NULL) {
				printf("Decoded WebSocket frame: %d bytes\n", ret);
				printf("Received: %.*s\n", ret, c->payload);

				// 准备回复相同的数据
				c->wlength = ret;
				c->status = 2;

				printf("Switching to state 2 for response\n");
			} else {
				printf("Failed to decode WebSocket frame\n");
			}
		} else {
			printf("Invalid WebSocket frame length: %d\n", c->rlength);
		}
	}

	return 0;
}


/**WebSocket 响应处理函数
处理 WebSocket 响应。
如果状态为 2，表示需要发送响应，调用 encode_packet 函数编码数据帧。
编码后的数据帧存储在 c->wbuffer 中，长度存储在 c->wlength 中。
 */
int ws_response(struct conn *c) {
	if (c->status == 2) {
		printf("Encoding WebSocket response, data length: %d\n", c->wlength);

		// 编码要发送的数据
		if (c->payload && c->wlength > 0) {
			int encoded_len = encode_packet(c->wbuffer, c->mask, c->payload, c->wlength);

			if (encoded_len > 0) {
				c->wlength = encoded_len;
				printf("Encoded response: %d bytes\n", encoded_len);
			} else {
				printf("Failed to encode WebSocket frame\n");
			}
		}
	}
	return 0;
}



