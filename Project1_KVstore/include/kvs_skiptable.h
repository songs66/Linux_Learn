#ifndef _KVS_SKIPTABLE_H_
#define _KVS_SKIPTABLE_H_


// 跳表的最高层数：层数越高，查找路径越短，但额外指针也越多
#define SKIPTABLE_MAX_LEVEL 16
// 每向上一层的晋升概率，经典跳表一般取 0.5
#define SKIPTABLE_P 0.5

// 跳表节点：保存一条键值对，以及每一层向前的“跳跃指针”
typedef struct skipnode_s {
    char *key;
    char *value;
    int level;
    struct skipnode_s *forward[SKIPTABLE_MAX_LEVEL];
} skipnode_t;

// 跳表对象：包含头节点、当前有效层数和元素总数
typedef struct kvs_skiptable_s {
    skipnode_t *head;
    int level;
    int count;
} kvs_skiptable_t;

int kvs_skiptable_create(void *engine);
void kvs_skiptable_destroy(void *engine);

int kvs_skiptable_set(void *engine, char *key, char *value);
char *kvs_skiptable_get(void *engine, char *key);
int kvs_skiptable_del(void *engine, char *key);
int kvs_skiptable_mod(void *engine, char *key, char *value);
int kvs_skiptable_exist(void *engine, char *key);

#endif
