#ifndef __KV_STORE_H__
#define __KV_STORE_H__


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stddef.h>


//网络层
#define NETWORK_REACTOR 	0
#define NETWORK_PROACTOR	1
#define NETWORK_NTYCO		2

#define NETWORK_SELECT		NETWORK_REACTOR


//协议层
#define KVS_MAX_TOKENS		128


//数据引擎层
#define ENABLE_ARRAY		1
#define ENABLE_RBTREE		0
#define ENABLE_HASH			0


//消息处理函数（业务函数），传入message和length  返回response
typedef int (*msg_handler)(char *msg, int length, char *response);

//给主程序提供 3 种“启动服务器”的入口
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



#if ENABLE_ARRAY

//定义数组中的一个元素（一个槽位里的数据）：一条键值对记录（最底层的数据单元）
typedef struct kvs_array_item_s {
	char *key;
	char *value;
} kvs_array_item_t;

//定义数组型 KV 存储(kvs_array_t)默认最多开 1024 个槽位(kvs_array_item_t)
#define KVS_ARRAY_SIZE		1024

//数组版 KV 存储实例：整个数组型 KV 容器(整个数组引擎)
typedef struct kvs_array_s {
    //指向真正存放键值对的数组
	kvs_array_item_t *table;
    //表示当前已存入的元素数量，当前逻辑上已占用的条目计数
	int total;
} kvs_array_t;

int kvs_array_create(kvs_array_t *inst);
void kvs_array_destory(kvs_array_t *inst);

int kvs_array_set(kvs_array_t *inst, char *key, char *value);
char* kvs_array_get(kvs_array_t *inst, char *key);
int kvs_array_del(kvs_array_t *inst, char *key);
int kvs_array_mod(kvs_array_t *inst, char *key, char *value);
int kvs_array_exist(kvs_array_t *inst, char *key);

#endif



#if ENABLE_RBTREE

#define RED				1
#define BLACK 			2

#define ENABLE_KEY_CHAR		1

#if ENABLE_KEY_CHAR
typedef char* KEY_TYPE;
#else
typedef int KEY_TYPE; // key
#endif

typedef struct _rbtree_node {
	unsigned char color;
	struct _rbtree_node *right;
	struct _rbtree_node *left;
	struct _rbtree_node *parent;
	KEY_TYPE key;
	void *value;
} rbtree_node;

typedef struct _rbtree {
	rbtree_node *root;
	rbtree_node *nil;
} rbtree;


typedef struct _rbtree kvs_rbtree_t;

int kvs_rbtree_create(kvs_rbtree_t *inst);
void kvs_rbtree_destory(kvs_rbtree_t *inst);
int kvs_rbtree_set(kvs_rbtree_t *inst, char *key, char *value);
char* kvs_rbtree_get(kvs_rbtree_t *inst, char *key);
int kvs_rbtree_del(kvs_rbtree_t *inst, char *key);
int kvs_rbtree_mod(kvs_rbtree_t *inst, char *key, char *value);
int kvs_rbtree_exist(kvs_rbtree_t *inst, char *key);

#endif



#if ENABLE_HASH

#define MAX_KEY_LEN	128
#define MAX_VALUE_LEN	512
#define MAX_TABLE_SIZE	1024

#define ENABLE_KEY_POINTER	1


typedef struct hashnode_s {
#if ENABLE_KEY_POINTER
	char *key;
	char *value;
#else
	char key[MAX_KEY_LEN];
	char value[MAX_VALUE_LEN];
#endif
	struct hashnode_s *next;
	
} hashnode_t;


typedef struct hashtable_s {

	hashnode_t **nodes; //* change **, 

	int max_slots;
	int count;

} hashtable_t;

typedef struct hashtable_s kvs_hash_t;


int kvs_hash_create(kvs_hash_t *hash);
void kvs_hash_destory(kvs_hash_t *hash);
int kvs_hash_set(hashtable_t *hash, char *key, char *value);
char * kvs_hash_get(kvs_hash_t *hash, char *key);
int kvs_hash_mod(kvs_hash_t *hash, char *key, char *value);
int kvs_hash_del(kvs_hash_t *hash, char *key);
int kvs_hash_exist(kvs_hash_t *hash, char *key);

#endif







#endif
