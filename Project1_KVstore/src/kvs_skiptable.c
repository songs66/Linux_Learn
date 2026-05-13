#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stddef.h>

#include "kvs_skiptable.h"
#include "kvstore.h"

// 当前跳表引擎的全局实例，由上层统一注册到 g_engine 中
kvs_skiptable_t global_skiptable = {0};

// 创建一个跳表节点，并在节点内部复制 key/value
static skipnode_t *skiptable_create_node(char *key, char *value, int level) {
    skipnode_t *node = (skipnode_t *)kvs_malloc(sizeof(skipnode_t));
    if (node == NULL) return NULL;

    memset(node, 0, sizeof(skipnode_t));
    node->level = level;

    if (key != NULL) {
        node->key = (char *)kvs_malloc(strlen(key) + 1);
        if (node->key == NULL) {
            kvs_free(node);
            return NULL;
        }
        strcpy(node->key, key);
    }

    if (value != NULL) {
        node->value = (char *)kvs_malloc(strlen(value) + 1);
        if (node->value == NULL) {
            kvs_free(node->key);
            kvs_free(node);
            return NULL;
        }
        strcpy(node->value, value);
    }

    return node;
}

// 随机生成节点层数，让跳表在概率意义上保持接近平衡
static int skiptable_random_level(void) {
    int level = 1;
    while (((double)rand() / RAND_MAX) < SKIPTABLE_P && level < SKIPTABLE_MAX_LEVEL) {
        level++;
    }
    return level;
}

// 跳表内部搜索：从最高层开始逐层向下逼近目标 key
static skipnode_t *skiptable_search_node(kvs_skiptable_t *table, char *key) {
    if (table == NULL || key == NULL) return NULL;

    skipnode_t *cur = table->head;

    for (int i = table->level - 1; i >= 0; i--) {
        while (cur->forward[i] != NULL &&
               strcmp(cur->forward[i]->key, key) < 0) {
            cur = cur->forward[i];
        }
    }

    cur = cur->forward[0];
    if (cur != NULL && strcmp(cur->key, key) == 0) {
        return cur;
    }

    return NULL;
}

// 初始化跳表：创建头节点，并把当前有效层数设为 1
int kvs_skiptable_create(void *engine) {
    kvs_skiptable_t *table = (kvs_skiptable_t *)engine;
    if (table == NULL) return -1;

    static int seeded = 0;
    if (!seeded) {
        srand((unsigned int)time(NULL));
        seeded = 1;
    }

    table->head = skiptable_create_node(NULL, NULL, SKIPTABLE_MAX_LEVEL);
    if (table->head == NULL) return -1;

    table->level = 1;
    table->count = 0;

    return 0;
}

// 销毁跳表：沿第 0 层链表释放所有节点即可覆盖整张表
void kvs_skiptable_destroy(void *engine) {
    kvs_skiptable_t *table = (kvs_skiptable_t *)engine;
    if (table == NULL || table->head == NULL) return;

    skipnode_t *cur = table->head->forward[0];
    while (cur != NULL) {
        skipnode_t *next = cur->forward[0];
        kvs_free(cur->key);
        kvs_free(cur->value);
        kvs_free(cur);
        cur = next;
    }

    kvs_free(table->head);
    table->head = NULL;
    table->level = 0;
    table->count = 0;
}

// 跳表版 SET：先记录每一层插入点，再把新节点挂到对应层上
int kvs_skiptable_set(void *engine, char *key, char *value) {
    kvs_skiptable_t *table = (kvs_skiptable_t *)engine;
    if (table == NULL || key == NULL || value == NULL) return -1;

    skipnode_t *update[SKIPTABLE_MAX_LEVEL] = {0};
    skipnode_t *cur = table->head;

    for (int i = table->level - 1; i >= 0; i--) {
        while (cur->forward[i] != NULL &&
               strcmp(cur->forward[i]->key, key) < 0) {
            cur = cur->forward[i];
        }
        update[i] = cur;
    }

    cur = cur->forward[0];
    if (cur != NULL && strcmp(cur->key, key) == 0) {
        return 1;
    }

    int level = skiptable_random_level();
    if (level > table->level) {
        for (int i = table->level; i < level; i++) {
            update[i] = table->head;
        }
        table->level = level;
    }

    skipnode_t *node = skiptable_create_node(key, value, level);
    if (node == NULL) return -1;

    for (int i = 0; i < level; i++) {
        node->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = node;
    }

    table->count++;
    return 0;
}

// 跳表版 GET：复用内部搜索函数，命中后直接返回 value
char *kvs_skiptable_get(void *engine, char *key) {
    kvs_skiptable_t *table = (kvs_skiptable_t *)engine;
    skipnode_t *node = skiptable_search_node(table, key);
    if (node == NULL) return NULL;
    return node->value;
}

// 跳表版 MOD：找到节点后只替换 value，不调整层级结构
int kvs_skiptable_mod(void *engine, char *key, char *value) {
    kvs_skiptable_t *table = (kvs_skiptable_t *)engine;
    if (table == NULL || key == NULL || value == NULL) return -1;

    skipnode_t *node = skiptable_search_node(table, key);
    if (node == NULL) return 1;

    char *new_value = (char *)kvs_malloc(strlen(value) + 1);
    if (new_value == NULL) return -1;
    strcpy(new_value, value);

    kvs_free(node->value);
    node->value = new_value;

    return 0;
}

// 跳表版 DEL：同样依赖 update[] 记录每层前驱，再逐层摘链
int kvs_skiptable_del(void *engine, char *key) {
    kvs_skiptable_t *table = (kvs_skiptable_t *)engine;
    if (table == NULL || key == NULL) return -1;

    skipnode_t *update[SKIPTABLE_MAX_LEVEL] = {0};
    skipnode_t *cur = table->head;

    for (int i = table->level - 1; i >= 0; i--) {
        while (cur->forward[i] != NULL &&
               strcmp(cur->forward[i]->key, key) < 0) {
            cur = cur->forward[i];
        }
        update[i] = cur;
    }

    cur = cur->forward[0];
    if (cur == NULL || strcmp(cur->key, key) != 0) {
        return 1;
    }

    for (int i = 0; i < table->level; i++) {
        if (update[i]->forward[i] != cur) {
            break;
        }
        update[i]->forward[i] = cur->forward[i];
    }

    kvs_free(cur->key);
    kvs_free(cur->value);
    kvs_free(cur);

    while (table->level > 1 && table->head->forward[table->level - 1] == NULL) {
        table->level--;
    }

    table->count--;
    return 0;
}

// EXIST 本质上是一次“只判断有没有命中”的搜索
int kvs_skiptable_exist(void *engine, char *key) {
    kvs_skiptable_t *table = (kvs_skiptable_t *)engine;
    skipnode_t *node = skiptable_search_node(table, key);
    return (node == NULL) ? 1 : 0;
}
