#ifndef _KVS_ARRAY_H_
#define _KVS_ARRAY_H_


// 数组中的一个槽位，对应一条最简单的 key/value 记录
typedef struct kvs_array_item_s {
	char *key;
	char *value;
} kvs_array_item_t;

// 数组版存储默认最大容量，适合做最小可运行原型
#define KVS_ARRAY_SIZE		1024

// 数组版引擎实例：本质上是一个顺序表
typedef struct kvs_array_s {
    // 指向真正存放键值对的数组
	kvs_array_item_t *table;
    // 当前已存入的元素数量
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
