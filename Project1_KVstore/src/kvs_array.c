#include "kvstore.h"


kvs_array_t global_array = {0};

int kvs_array_create(kvs_array_t *inst){
    if (!inst) return -1;
	if (inst->table) {
		printf("table has alloc\n");
		return -1;
	}	

	inst->table = kvs_malloc(KVS_ARRAY_SIZE * sizeof(kvs_array_item_t));
	if (!inst->table) {
		return -1;
	}

	inst->total = 0;

	return 0;
}


void kvs_array_destory(kvs_array_t *inst) {
	if (!inst) return ;

	if (inst->table) {
		kvs_free(inst->table);
	}
}


/*
 * @return: <0, error; =0, success; >0, exist
 */
int kvs_array_set(kvs_array_t *inst, char *key, char *value) {

	if (inst == NULL || key == NULL || value == NULL) return -1;
	if (inst->total == KVS_ARRAY_SIZE) {
		printf("maximum capacity reached\n");
		return -1;//满
	}
	
	char *str = kvs_array_get(inst, key);
	if (str) {
		return 1;//已存在
	}

	char *kcopy = kvs_malloc(strlen(key) + 1);
	if (kcopy == NULL) return -2;
	memset(kcopy, 0, strlen(key) + 1);
	strncpy(kcopy, key, strlen(key));

	char *kvalue = kvs_malloc(strlen(value) + 1);
	if (kvalue == NULL) return -2;
	memset(kvalue, 0, strlen(value) + 1);
	strncpy(kvalue, value, strlen(value));

	inst->table[inst->total].key = kcopy;
	inst->table[inst->total].value = kvalue;
	inst->total ++;

	return 0;
}


char* kvs_array_get(kvs_array_t *inst, char *key) {

	if (inst == NULL || key == NULL) return NULL;

	int i = 0;
	for (i = 0;i < inst->total;i ++) {
		if (inst->table[i].key == NULL) {
			continue;
		}

		if (strcmp(inst->table[i].key, key) == 0) {
			return inst->table[i].value;
		}
	}

	return NULL;
}


/*
 * @return < 0, error;  =0,  success; >0, no exist
 */
int kvs_array_del(kvs_array_t *inst, char *key) {

    if (inst == NULL || key == NULL) return -1;

    int i = 0;
    for (i = 0; i < inst->total; i++) {

        if (inst->table[i].key == NULL) {
            continue;
        }

        if (strcmp(inst->table[i].key, key) == 0) {

            kvs_free(inst->table[i].key);
            kvs_free(inst->table[i].value);

            int j = 0;
            for (j = i; j < inst->total - 1; j++) {
                inst->table[j] = inst->table[j + 1];
            }

            inst->table[inst->total - 1].key = NULL;
            inst->table[inst->total - 1].value = NULL;
            inst->total--;

            return 0;
        }
    }
    return i;
}


/*
 * @return : < 0, error; =0, success; >0, no exist 
 */
int kvs_array_mod(kvs_array_t *inst, char *key, char *value) {

	if (inst == NULL || key == NULL || value == NULL) return -1;
	if (inst->total == 0) {
		return KVS_ARRAY_SIZE;
	}
	

	int i = 0;
	for (i = 0;i < inst->total;i ++) {

		if (inst->table[i].key == NULL) {
			continue;
		}

		if (strcmp(inst->table[i].key, key) == 0) {

			kvs_free(inst->table[i].value);

			char *kvalue = kvs_malloc(strlen(value) + 1);
			if (kvalue == NULL) return -2;
			memset(kvalue, 0, strlen(value) + 1);
			strncpy(kvalue, value, strlen(value));

			inst->table[i].value = kvalue;

			return 0;
		}
	}
	return i;
}


/*
 * @return 0: exist, 1: no exist
 */
int kvs_array_exist(kvs_array_t *inst, char *key) {

	if (!inst || !key) return -1;
	
	char *str = kvs_array_get(inst, key);
	if (!str) {
		return 1;
	}
	return 0;
}
