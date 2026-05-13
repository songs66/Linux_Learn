#ifndef _KVS_RBTREE_H_
#define _KVS_RBTREE_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stddef.h>



#define RED				1
#define BLACK 			2

#define ENABLE_KEY_CHAR		1

// 红黑树版当前仍沿用字符串 key，便于和其它引擎保持统一命令格式
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


// 统一对外暴露的红黑树引擎类型
typedef struct _rbtree kvs_rbtree_t;

int kvs_rbtree_create(kvs_rbtree_t *inst);
void kvs_rbtree_destory(kvs_rbtree_t *inst);
int kvs_rbtree_set(kvs_rbtree_t *inst, char *key, char *value);
char* kvs_rbtree_get(kvs_rbtree_t *inst, char *key);
int kvs_rbtree_del(kvs_rbtree_t *inst, char *key);
int kvs_rbtree_mod(kvs_rbtree_t *inst, char *key, char *value);
int kvs_rbtree_exist(kvs_rbtree_t *inst, char *key);


#endif
