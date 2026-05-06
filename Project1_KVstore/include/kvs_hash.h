#ifndef _KVS_HASH_H_
#define _KVS_HASH_H_


#define ENABLE_KEY_POINTER	1	//开启后kv值变为指针

#define MAX_TABLE_SIZE	1024
#define MAX_KEY_LEN	128
#define MAX_VALUE_LEN	512

//哈希表链表节点：“单个键值对”的存储单元
typedef struct hashnode_s {
#if ENABLE_KEY_POINTER
	char *key;
	char *value;
#else
	char key[MAX_KEY_LEN];
	char value[MAX_VALUE_LEN];
#endif
	struct hashnode_s *next;//拉链法解决哈希冲突
	
} hashnode_t;

//整个哈希表对象的定义：整张表
typedef struct hashtable_s {
	//指向 hashnode_t*（桶数组）的指针
	hashnode_t **nodes; //链表头指针

	int max_slots;//表示这张哈希表一共有多少个桶，后期直接赋值
	int count;//表示当前表里已经存了多少个键值对节点
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
