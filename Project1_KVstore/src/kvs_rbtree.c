#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stddef.h>

#include "kvs_rbtree.h"
#include "kvstore.h"


// 当前红黑树引擎的全局实例，由上层在 init_kvengine() 中统一注册
kvs_rbtree_t global_rbtree = {0};


// 返回以 x 为根的子树中的最小节点
rbtree_node *rbtree_mini(rbtree *T, rbtree_node *x) {
	while (x->left != T->nil) {
		x = x->left;
	}
	return x;
}

// 返回以 x 为根的子树中的最大节点
rbtree_node *rbtree_maxi(rbtree *T, rbtree_node *x) {
	while (x->right != T->nil) {
		x = x->right;
	}
	return x;
}

// 查找中序遍历意义下的后继节点，删除时会用到
rbtree_node *rbtree_successor(rbtree *T, rbtree_node *x) {
	rbtree_node *y = x->parent;

	if (x->right != T->nil) {
		return rbtree_mini(T, x->right);
	}

	while ((y != T->nil) && (x == y->right)) {
		x = y;
		y = y->parent;
	}
	return y;
}


// 左旋：把 x 的右孩子提上来，局部重排父子关系
void rbtree_left_rotate(rbtree *T, rbtree_node *x) {

	rbtree_node *y = x->right;  // x  --> y  ,  y --> x,   right --> left,  left --> right

	x->right = y->left; //1 1
	if (y->left != T->nil) { //1 2
		y->left->parent = x;
	}

	y->parent = x->parent; //1 3
	if (x->parent == T->nil) { //1 4
		T->root = y;
	} else if (x == x->parent->left) {
		x->parent->left = y;
	} else {
		x->parent->right = y;
	}

	y->left = x; //1 5
	x->parent = y; //1 6
}


// 右旋：和左旋完全对称，用于另一侧失衡的修复
void rbtree_right_rotate(rbtree *T, rbtree_node *y) {

	rbtree_node *x = y->left;

	y->left = x->right;
	if (x->right != T->nil) {
		x->right->parent = y;
	}

	x->parent = y->parent;
	if (y->parent == T->nil) {
		T->root = x;
	} else if (y == y->parent->right) {
		y->parent->right = x;
	} else {
		y->parent->left = x;
	}

	x->right = y;
	y->parent = x;
}

// 插入修复：通过变色和旋转重新满足红黑树 5 条性质
void rbtree_insert_fixup(rbtree *T, rbtree_node *z) {

	while (z->parent->color == RED) { //z ---> RED
		if (z->parent == z->parent->parent->left) {
			rbtree_node *y = z->parent->parent->right;
			if (y->color == RED) {
				z->parent->color = BLACK;
				y->color = BLACK;
				z->parent->parent->color = RED;

				z = z->parent->parent; //z --> RED
			} else {

				if (z == z->parent->right) {
					z = z->parent;
					rbtree_left_rotate(T, z);
				}

				z->parent->color = BLACK;
				z->parent->parent->color = RED;
				rbtree_right_rotate(T, z->parent->parent);
			}
		}else {
			rbtree_node *y = z->parent->parent->left;
			if (y->color == RED) {
				z->parent->color = BLACK;
				y->color = BLACK;
				z->parent->parent->color = RED;

				z = z->parent->parent; //z --> RED
			} else {
				if (z == z->parent->left) {
					z = z->parent;
					rbtree_right_rotate(T, z);
				}

				z->parent->color = BLACK;
				z->parent->parent->color = RED;
				rbtree_left_rotate(T, z->parent->parent);
			}
		}
		
	}

	T->root->color = BLACK;
}


// 先按普通 BST 规则插入，再交给 fixup 完成平衡修复
void rbtree_insert(rbtree *T, rbtree_node *z) {

	rbtree_node *y = T->nil;
	rbtree_node *x = T->root;

	while (x != T->nil) {
		y = x;
#if ENABLE_KEY_CHAR

		if (strcmp(z->key, x->key) < 0) {
			x = x->left;
		} else if (strcmp(z->key, x->key) > 0) {
			x = x->right;
		} else {
			return ;
		}

#else
		if (z->key < x->key) {
			x = x->left;
		} else if (z->key > x->key) {
			x = x->right;
		} else { //Exist
			return ;
		}
#endif
	}

	z->parent = y;
	if (y == T->nil) {
		T->root = z;
#if ENABLE_KEY_CHAR
	} else if (strcmp(z->key, y->key) < 0) {
#else
	} else if (z->key < y->key) {
#endif
		y->left = z;
	} else {
		y->right = z;
	}

	z->left = T->nil;
	z->right = T->nil;
	z->color = RED;

	rbtree_insert_fixup(T, z);
}

// 删除修复：当删除黑节点破坏黑高时，通过旋转和变色恢复平衡
void rbtree_delete_fixup(rbtree *T, rbtree_node *x) {

	while ((x != T->root) && (x->color == BLACK)) {
		if (x == x->parent->left) {

			rbtree_node *w= x->parent->right;
			if (w->color == RED) {
				w->color = BLACK;
				x->parent->color = RED;

				rbtree_left_rotate(T, x->parent);
				w = x->parent->right;
			}

			if ((w->left->color == BLACK) && (w->right->color == BLACK)) {
				w->color = RED;
				x = x->parent;
			} else {

				if (w->right->color == BLACK) {
					w->left->color = BLACK;
					w->color = RED;
					rbtree_right_rotate(T, w);
					w = x->parent->right;
				}

				w->color = x->parent->color;
				x->parent->color = BLACK;
				w->right->color = BLACK;
				rbtree_left_rotate(T, x->parent);

				x = T->root;
			}

		} else {

			rbtree_node *w = x->parent->left;
			if (w->color == RED) {
				w->color = BLACK;
				x->parent->color = RED;
				rbtree_right_rotate(T, x->parent);
				w = x->parent->left;
			}

			if ((w->left->color == BLACK) && (w->right->color == BLACK)) {
				w->color = RED;
				x = x->parent;
			} else {

				if (w->left->color == BLACK) {
					w->right->color = BLACK;
					w->color = RED;
					rbtree_left_rotate(T, w);
					w = x->parent->left;
				}

				w->color = x->parent->color;
				x->parent->color = BLACK;
				w->left->color = BLACK;
				rbtree_right_rotate(T, x->parent);

				x = T->root;
			}

		}
	}

	x->color = BLACK;
}

// 删除核心流程：先找到真正被摘掉的节点，再决定是否需要 fixup
rbtree_node *rbtree_delete(rbtree *T, rbtree_node *z) {

	rbtree_node *y = T->nil;
	rbtree_node *x = T->nil;

	if ((z->left == T->nil) || (z->right == T->nil)) {
		y = z;
	} else {
		y = rbtree_successor(T, z);
	}

	if (y->left != T->nil) {
		x = y->left;
	} else if (y->right != T->nil) {
		x = y->right;
	}

	x->parent = y->parent;
	if (y->parent == T->nil) {
		T->root = x;
	} else if (y == y->parent->left) {
		y->parent->left = x;
	} else {
		y->parent->right = x;
	}

	if (y != z) {
#if ENABLE_KEY_CHAR

		void *tmp = z->key;
		z->key = y->key;
		y->key = tmp;

		tmp = z->value;
		z->value= y->value;
		y->value = tmp;

#else
		z->key = y->key;
		z->value = y->value;
#endif
	}

	if (y->color == BLACK) {
		rbtree_delete_fixup(T, x);
	}

	return y;
}

// 红黑树搜索：本质上仍然是标准二叉搜索树查找
rbtree_node *rbtree_search(rbtree *T, KEY_TYPE key) {

	rbtree_node *node = T->root;
	while (node != T->nil) {
#if ENABLE_KEY_CHAR

		if (strcmp(key, node->key) < 0) {
			node = node->left;
		} else if (strcmp(key, node->key) > 0) {
			node = node->right;
		} else {
			return node;
		}

#else
		if (key < node->key) {
			node = node->left;
		} else if (key > node->key) {
			node = node->right;
		} else {
			return node;
		}	
#endif
	}
	return T->nil;
}


// 中序遍历打印，便于调试树中当前的有序内容
void rbtree_traversal(rbtree *T, rbtree_node *node) {
	if (node != T->nil) {
		rbtree_traversal(T, node->left);
#if ENABLE_KEY_CHAR
		printf("key:%s, value:%s\n", node->key, (char *)node->value);
#else
		printf("key:%d, color:%d\n", node->key, node->color);
#endif
		rbtree_traversal(T, node->right);
	}
}





//============================================================================


// 创建红黑树：初始化 nil 哨兵，并让 root 指向 nil
int kvs_rbtree_create(kvs_rbtree_t *inst) {

	if (inst == NULL) return 1;

	inst->nil = (rbtree_node*)kvs_malloc(sizeof(rbtree_node));
	if (inst->nil == NULL) return -1;

	memset(inst->nil, 0, sizeof(rbtree_node));
	inst->nil->color = BLACK;
	inst->nil->left = inst->nil;
	inst->nil->right = inst->nil;
	inst->nil->parent = inst->nil;
	inst->root = inst->nil;

	return 0;

}

// 销毁红黑树：持续删除最小节点，直到整棵树被清空
void kvs_rbtree_destroy(kvs_rbtree_t *inst) {

	if (inst == NULL || inst->nil == NULL) return ;

	rbtree_node *node = NULL;

	while ((node = inst->root) != inst->nil) {
		
		rbtree_node *mini = rbtree_mini(inst, node);
		
		rbtree_node *cur = rbtree_delete(inst, mini);
		kvs_free(cur->key);
		kvs_free(cur->value);
		kvs_free(cur);
		
	}

	kvs_free(inst->nil);
	inst->nil = NULL;
	inst->root = NULL;

	return ;

}


// 红黑树版 SET：创建新节点，复制 key/value，再执行插入
int kvs_rbtree_set(kvs_rbtree_t *inst, char *key, char *value) {

	if (!inst || !key || !value) return -1;
	if (rbtree_search(inst, key) != inst->nil) return 1;

	rbtree_node *node = (rbtree_node*)kvs_malloc(sizeof(rbtree_node));
	if (!node) return -2;
	memset(node, 0, sizeof(rbtree_node));
		
	node->key = kvs_malloc(strlen(key) + 1);
	if (!node->key) {
		kvs_free(node);
		return -2;
	}
 	memset(node->key, 0, strlen(key) + 1);
	strcpy(node->key, key);
	
	node->value = kvs_malloc(strlen(value) + 1);
	if (!node->value) {
		kvs_free(node->key);
		kvs_free(node);
		return -2;
	}
	memset(node->value, 0, strlen(value) + 1);
	strcpy(node->value, value);

	rbtree_insert(inst, node);

	return 0;
}


// 红黑树版 GET：命中节点后直接返回它保存的 value
char* kvs_rbtree_get(kvs_rbtree_t *inst, char *key)  {

	if (!inst || !key) return NULL;
	rbtree_node *node = rbtree_search(inst, key);
	if (!node) return NULL; // no exist
	if (node == inst->nil) return NULL;

	return node->value;
	
}

// 红黑树版 DEL：先定位节点，再走统一删除流程
int kvs_rbtree_del(kvs_rbtree_t *inst, char *key) {

	if (!inst || !key) return -1;

	rbtree_node *node = rbtree_search(inst, key);
	if (!node || node == inst->nil) return 1; // no exist
	
	rbtree_node *cur = rbtree_delete(inst, node);
	kvs_free(cur->key);
	kvs_free(cur->value);
	kvs_free(cur);

	return 0;
}

// 红黑树版 MOD：不改树形结构，只替换节点 value
int kvs_rbtree_mod(kvs_rbtree_t *inst, char *key, char *value) {

	if (!inst || !key || !value) return -1;

	rbtree_node *node = rbtree_search(inst, key);
	if (!node) return 1; // no exist
	if (node == inst->nil) return 1;
	
	kvs_free(node->value);

	node->value = kvs_malloc(strlen(value) + 1);
	if (!node->value) return -2;
	
	memset(node->value, 0, strlen(value) + 1);
	strcpy(node->value, value);

	return 0;

}

// EXIST 只是搜索结果的布尔化包装
int kvs_rbtree_exist(kvs_rbtree_t *inst, char *key) {

	if (!inst || !key) return -1;

	rbtree_node *node = rbtree_search(inst, key);
	if (!node) return 1; // no exist
	if (node == inst->nil) return 1;

	return 0;
}
