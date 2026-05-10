#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stddef.h>

#include "kvs_hash.h"
#include "kvstore.h"


kvs_hash_t global_hash;

//简易哈希函数
static int _hash(const char *key, int size) {
	if (!key || size<=0) return -1;

	unsigned int sum = 0;
	int i = 0;

	while (key[i] != 0) {
		sum += (unsigned char)key[i];
		i++;
	}

	return (int)(sum % (unsigned int)size);
}

//在哈希表里创建一个新的键值对节点
hashnode_t *_create_node(char *key, char *value) {

	hashnode_t *node = (hashnode_t*)kvs_malloc(sizeof(hashnode_t));
	if (!node) return NULL;
	
#if ENABLE_KEY_POINTER
	char *kcopy = kvs_malloc(strlen(key) + 1);
	if (kcopy == NULL) {
		kvs_free(node);
		return NULL;
	}
	
	memset(kcopy, 0, strlen(key) + 1);
	strncpy(kcopy, key, strlen(key));

	node->key = kcopy;

	char *kvalue = kvs_malloc(strlen(value) + 1);
	if (kvalue == NULL) { 
		kvs_free(node);
		kvs_free(kcopy);
		return NULL;
	}
	memset(kvalue, 0, strlen(value) + 1);
	strncpy(kvalue, value, strlen(value));

	node->value = kvalue;
	
#else
	strncpy(node->key, key, MAX_KEY_LEN);
	strncpy(node->value, value, MAX_VALUE_LEN);
#endif
	node->next = NULL;

	return node;
}


//创建表
int kvs_hash_create(void* engine) {
	kvs_hash_t *hash = (kvs_hash_t*)engine;

	if(!hash) return -1;

	hash->nodes=kvs_malloc(sizeof(hashnode_t*) * MAX_TABLE_SIZE);
	if (!hash->nodes) return -1;
	memset(hash->nodes,0,sizeof(hashnode_t*) * MAX_TABLE_SIZE);

	hash->max_slots = MAX_TABLE_SIZE;
	hash->count = 0; 

	return 0;
}

//删除表
void kvs_hash_destroy(void* engine) {
	kvs_hash_t *hash = (kvs_hash_t*)engine;

	if (!hash) return;

	int i = 0;
	for (i = 0;i < hash->max_slots;i ++) {
		hashnode_t *node = hash->nodes[i];
		while (node != NULL) {
			kvs_free(node->key);
			kvs_free(node->value);

			hashnode_t *tmp = node;
			node = node->next;
			hash->nodes[i] = node;
			
			kvs_free(tmp);
		}
	}

	kvs_free(hash->nodes);
}


int kvs_hash_set(void* engine, char *key, char *value) {
	kvs_hash_t *hash = (kvs_hash_t*)engine;

	if (!hash || !key || !value) return -1;

	int idx = _hash(key, MAX_TABLE_SIZE);

	hashnode_t *node = hash->nodes[idx];

	while (node != NULL) {
		if (strcmp(node->key, key) == 0) {
			return 1;
		}
		node = node->next;
	}

	hashnode_t *new_node = _create_node(key, value);
	new_node->next = hash->nodes[idx];
	hash->nodes[idx] = new_node;
	
	hash->count ++;

	return 0;
}


char * kvs_hash_get(void* engine, char *key) {
	kvs_hash_t *hash = (kvs_hash_t*)engine;

	if (!hash || !key) return NULL;

	int idx = _hash(key, MAX_TABLE_SIZE);

	hashnode_t *node = hash->nodes[idx];

	while (node != NULL) {
		if (strcmp(node->key, key) == 0) {
			return node->value;
		}

		node = node->next;
	}

	return NULL;
}


int kvs_hash_mod(void* engine, char *key, char *value) {
	kvs_hash_t *hash = (kvs_hash_t*)engine;

	if (!hash || !key || !value) return -1;

	int idx = _hash(key, MAX_TABLE_SIZE);

	hashnode_t *node = hash->nodes[idx];

	while (node != NULL) {
		if (strcmp(node->key, key) == 0) {
			break;
		}

		node = node->next;
	}

	if (node == NULL) {
		return 1;
	}

	kvs_free(node->value);

	char *kvalue = kvs_malloc(strlen(value) + 1);
	if (kvalue == NULL) return -2;
	memset(kvalue, 0, strlen(value) + 1);
	strncpy(kvalue, value, strlen(value));

	node->value = kvalue;
	
	return 0;
}


int kvs_hash_count(void* engine) {
	kvs_hash_t *hash = (kvs_hash_t*)engine;

	return hash->count;
}


int kvs_hash_del(void* engine, char *key) {
	kvs_hash_t *hash = (kvs_hash_t*)engine;

	if (!hash || !key) return -2;

	int idx = _hash(key, MAX_TABLE_SIZE);

	hashnode_t *head = hash->nodes[idx];
	if (head == NULL) return -1;

	if (strcmp(head->key, key) == 0) {
		kvs_free(head->key);
		kvs_free(head->value);

		hashnode_t *tmp = head->next;
		hash->nodes[idx] = tmp;
		
		kvs_free(head);
		hash->count --;
		
		return 0;
	}
	

	hashnode_t *cur = head;
	while (cur->next != NULL) {
		if (strcmp(cur->next->key, key) == 0) break; // search node
		
		cur = cur->next;
	}

	if (cur->next == NULL) {
		
		return -1;
	}

	hashnode_t *tmp = cur->next;
	cur->next = tmp->next;
#if ENABLE_KEY_POINTER
	kvs_free(tmp->key);
	kvs_free(tmp->value);
#endif
	kvs_free(tmp);
	
	hash->count --;

	return 0;
}


int kvs_hash_exist(void* engine,char *key) {
	kvs_hash_t *hash = (kvs_hash_t*)engine;

	char *value = kvs_hash_get(hash, key);
	if (!value) return 1;

	return 0;
}
