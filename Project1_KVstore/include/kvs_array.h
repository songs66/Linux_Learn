#ifndef _KVS_ARRAY_H_
#define _KVS_ARRAY_H_


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

int kvs_array_create(void *engine);
void kvs_array_destroy(void *engine);

int kvs_array_set(void *engine, char *key, char *value);
char* kvs_array_get(void *engine, char *key);
int kvs_array_del(void *engine, char *key);
int kvs_array_mod(void *engine, char *key, char *value);
int kvs_array_exist(void *engine, char *key);

#endif