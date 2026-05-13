#ifndef _KVS_HASH_H_
#define _KVS_HASH_H_


#define ENABLE_KEY_POINTER	1	// 开启后节点里的 key/value 使用动态分配的指针

#define MAX_TABLE_SIZE	1024
#define MAX_KEY_LEN	128
#define MAX_VALUE_LEN	512

// 拉链法节点：一个桶里挂一条链表，每个节点保存一条键值对
typedef struct hashnode_s {
#if ENABLE_KEY_POINTER
	char *key;
	char *value;
#else
	char key[MAX_KEY_LEN];
	char value[MAX_VALUE_LEN];
#endif
	struct hashnode_s *next; // 拉链法解决哈希冲突
	
} hashnode_t;

// 整个哈希表对象：本质上是“桶数组 + 每个桶上的链表”
typedef struct hashtable_s {
	// 指向桶数组，每个元素都是一个链表头指针
	hashnode_t **nodes;

	int max_slots; // 当前哈希表一共有多少个桶
	int count;     // 当前已经存了多少个键值对节点
} hashtable_t;
typedef struct hashtable_s kvs_hash_t;


int kvs_hash_create(void *engine);
void kvs_hash_destroy(void *engine);
int kvs_hash_set(void *engine, char *key, char *value);
char * kvs_hash_get(void *engine, char *key);
int kvs_hash_mod(void *engine, char *key, char *value);
int kvs_hash_del(void *engine, char *key);
int kvs_hash_exist(void *engine, char *key);

#endif
