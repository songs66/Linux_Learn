#ifndef __KV_STORE_H__
#define __KV_STORE_H__


// 网络层实现切换：同一套协议层/存储层，按宏选择底层收发模型
#define NETWORK_REACTOR 	0
#define NETWORK_PROACTOR	1
#define NETWORK_NTYCO		2

#define NETWORK_SELECT		NETWORK_REACTOR


// 协议层一次请求最多拆分出的 token 数量
#define KVS_MAX_TOKENS		128


// 数据引擎开关：当前阶段通过编译期开关切到不同后端
#define ENABLE_ARRAY		0
#define ENABLE_HASH			0
#define ENABLE_RBTREE		1
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

// 给主程序提供 3 种“启动服务器”的统一入口
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



// 存储引擎统一操作表：协议层只认这组接口，不关心底层具体实现
typedef struct kvs_ops_s {
	int   (*create)(void* engine);
	void  (*destroy)(void* engine);

	int   (*set)(void* engine, char* key, char* value);
	char* (*get)(void* engine, char* key);
	int   (*del)(void* engine, char* key);
	int   (*mod)(void* engine, char* key, char* value);
	int   (*exist)(void* engine, char* key);
} kvs_ops_t;

// 当前被选中的引擎实例 + 对应操作表
typedef struct kvs_engine_s {
	void* engine;
	kvs_ops_t ops;
}kvs_engine_t;

















#endif
