#ifndef __KV_STORE_H__
#define __KV_STORE_H__


//网络层
#define NETWORK_REACTOR 	0
#define NETWORK_PROACTOR	1
#define NETWORK_NTYCO		2

#define NETWORK_SELECT		NETWORK_PROACTOR


//协议层
#define KVS_MAX_TOKENS		128


//数据引擎层
#define ENABLE_ARRAY		0
#define ENABLE_HASH			1
#define ENABLE_RBTREE		0
#define ENABLE_SKIPTABLE	0


#if ENABLE_ARRAY
#include "kvs_array.h"
#endif
#if ENABLE_HASH
#include "kvs_hash.h"
#endif
#if ENABLE_RBTREE
#include "kvs_rbtree.h"
#endif
#if ENABLE_SKIPTABLE
#include "kvs_skiptable.h"
#endif


//消息处理函数（业务函数），传入message和length  返回response
typedef int (*msg_handler)(char *msg, int length, char *response);

//给主程序提供 3 种“启动服务器”的入口(相当于#include reactor.h/proactor.h/ntyco.h)
extern int reactor_start    (unsigned short port, msg_handler handler);
extern int proactor_start   (unsigned short port, msg_handler handler);
extern int ntyco_start      (unsigned short port, msg_handler handler);


/*  
    统一整个项目的内存申请和释放入口
    让底层实现以后更容易替换
    让各个存储模块和系统库稍微解耦
*/
void* kvs_malloc(size_t size);
void  kvs_free(void *ptr);




typedef struct kvs_ops_s {
	int   (*create)(void* engine);
	void  (*destroy)(void* engine);

	int   (*set)(void* engine, char* key, char* value);
	char* (*get)(void* engine, char* key);
	int   (*del)(void* engine, char* key);
	int   (*mod)(void* engine, char* key, char* value);
	int   (*exist)(void* engine, char* key);
} kvs_ops_t;

typedef struct kvs_engine_s {
	void* engine;
	kvs_ops_t ops;
}kvs_engine_t;

















#endif
