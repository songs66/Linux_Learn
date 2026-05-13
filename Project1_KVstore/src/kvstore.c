#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stddef.h>

#include "kvstore.h"


// 当前统一使用的存储引擎实例，由 init_kvengine() 完成注册
kvs_engine_t g_engine;

#if ENABLE_ARRAY
extern kvs_array_t global_array;
#endif

#if ENABLE_RBTREE
extern kvs_rbtree_t global_rbtree;
#endif

#if ENABLE_HASH
extern kvs_hash_t global_hash;
#endif


// 统一内存入口：当前只是简单封装，后面切换内存策略时更集中
void *kvs_malloc(size_t size) {
	return malloc(size);
}

void kvs_free(void *ptr) {
	return free(ptr);
}


// 当前支持的文本命令集合，协议层通过它把字符串映射为具体操作
const char *command[] = {
	"SET", "GET", "DEL", "MOD", "EXIST"
};

enum {
	KVS_CMD_START = 0,

	// array
	KVS_CMD_SET = KVS_CMD_START,
	KVS_CMD_GET,
	KVS_CMD_DEL,
	KVS_CMD_MOD,
	KVS_CMD_EXIST,
	
	KVS_CMD_COUNT,
};



int kvs_split_token(char *msg, char *tokens[]) {

	if (msg == NULL || tokens == NULL) return -1;

	int idx = 0;
	char *token = strtok(msg, " ");
	
	while (token != NULL) {
		//printf("idx: %d, %s\n", idx, token);
		
		tokens[idx ++] = token;
		token = strtok(NULL, " ");
	}

	return idx;
}


// 协议分发层：根据 tokens[0] 决定调用哪一个引擎接口
int kvs_filter_protocol(char **tokens, int count, char *response) {

	if (tokens[0] == NULL || count == 0 || response == NULL) return -1;

	int cmd = KVS_CMD_START;
	for (cmd = KVS_CMD_START;cmd < KVS_CMD_COUNT;cmd ++) {
		if (strcmp(tokens[0], command[cmd]) == 0) {
			break;
		} 
	}

	int length = 0;
	int ret = 0;
	char *key = tokens[1];
	char *value = tokens[2];

	switch(cmd) {

	case KVS_CMD_SET:
		ret = g_engine.ops.set(g_engine.engine ,key, value);
		if (ret < 0) {
			length = sprintf(response, "ERROR\r\n");
		} else if (ret == 0) {
			length = sprintf(response, "OK\r\n");
		} else {
			length = sprintf(response, "EXIST\r\n");
		} 
		
		break;
	case KVS_CMD_GET: {
		char *result = g_engine.ops.get(g_engine.engine, key);
		if (result == NULL) {
			length = sprintf(response, "NO EXIST\r\n");
		} else {
			length = sprintf(response, "%s\r\n", result);
		}
		break;
	}
	case KVS_CMD_DEL:
		ret = g_engine.ops.del(g_engine.engine ,key);
		if (ret < 0) {
			length = sprintf(response, "ERROR\r\n");
 		} else if (ret == 0) {
			length = sprintf(response, "OK\r\n");
		} else {
			length = sprintf(response, "NO EXIST\r\n");
		}
		break;
	case KVS_CMD_MOD:
		ret = g_engine.ops.mod(g_engine.engine ,key, value);
		if (ret < 0) {
			length = sprintf(response, "ERROR\r\n");
 		} else if (ret == 0) {
			length = sprintf(response, "OK\r\n");
		} else {
			length = sprintf(response, "NO EXIST\r\n");
		}
		break;
	case KVS_CMD_EXIST:
		ret = g_engine.ops.exist(g_engine.engine ,key);
		if (ret == 0) {
			length = sprintf(response, "EXIST\r\n");
		} else {
			length = sprintf(response, "NO EXIST\r\n");
		}
		break;
	default: 
		printf("protocol error,please send again!\n");
		length = sprintf(response, "protocol error,please send again!\r\n");
	}
	return length;
}



// 协议统一入口：网络层只需要把完整请求交给这里处理即可
int kvs_protocol(char *msg, int length, char *response) {
#if 0//echo
	memcpy(response, msg, length);
	return length;
#endif
	if (msg == NULL || length <= 0 || response == NULL) return -1;

	//printf("recv %d : %s\n", length, msg);

	char *tokens[KVS_MAX_TOKENS] = {0};

	int count = kvs_split_token(msg, tokens);
	if (count == -1) return -1;

	return kvs_filter_protocol(tokens, count, response);
}


// 根据编译期开关选择当前引擎，并把对应操作注册到 g_engine 中
int init_kvengine(void) {

#if ENABLE_ARRAY
	g_engine.engine = &global_array;

	g_engine.ops.create=kvs_array_create;
	g_engine.ops.destroy=kvs_array_destroy;
	g_engine.ops.set=kvs_array_set;
	g_engine.ops.get=kvs_array_get;
	g_engine.ops.del=kvs_array_del;
	g_engine.ops.mod=kvs_array_mod;
	g_engine.ops.exist=kvs_array_exist;

	memset(g_engine.engine,0,sizeof(kvs_array_t));
	g_engine.ops.create(g_engine.engine);
#endif

#if ENABLE_HASH
	g_engine.engine = &global_hash;

	g_engine.ops.create=kvs_hash_create;
	g_engine.ops.destroy=kvs_hash_destroy;
	g_engine.ops.set=kvs_hash_set;
	g_engine.ops.get=kvs_hash_get;
	g_engine.ops.del=kvs_hash_del;
	g_engine.ops.mod=kvs_hash_mod;
	g_engine.ops.exist=kvs_hash_exist;

	memset(g_engine.engine,0,sizeof(kvs_hash_t));
	g_engine.ops.create(g_engine.engine);
#endif

#if ENABLE_RBTREE
	g_engine.engine = &global_rbtree;

	g_engine.ops.create=kvs_rbtree_create;
	g_engine.ops.destroy=kvs_rbtree_destroy;
	g_engine.ops.set=kvs_rbtree_set;
	g_engine.ops.get=kvs_rbtree_get;
	g_engine.ops.del=kvs_rbtree_del;
	g_engine.ops.mod=kvs_rbtree_mod;
	g_engine.ops.exist=kvs_rbtree_exist;

	memset(g_engine.engine,0,sizeof(kvs_rbtree_t));
	g_engine.ops.create(g_engine.engine);
#endif

	return 0;
}

void dest_kvengine(void) {
	g_engine.ops.destroy(g_engine.engine);
}


int main(int argc, char *argv[]) {

	if (argc != 2) return -1;

	int port = atoi(argv[1]);

	init_kvengine();

#if (NETWORK_SELECT == NETWORK_REACTOR)
	reactor_start(port, kvs_protocol);
#elif (NETWORK_SELECT == NETWORK_PROACTOR)
	proactor_start(port, kvs_protocol);
#elif (NETWORK_SELECT == NETWORK_NTYCO)
	ntyco_start(port, kvs_protocol);
#endif

	dest_kvengine();

	return 0;
}
