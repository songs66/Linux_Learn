#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define INFO printf

/* 无尾指针双向链表无尾指针宏 */
#define LIST_INSERT_HEAD(item, list) do {       \
    (item)->prev = NULL;                        \
    (item)->next = (list);                      \
    if ((list) != NULL)                         \
        (list)->prev = (item);                  \
    (list) = (item);                            \
} while (0)

#define LIST_INSERT_TAIL(item, list) do {       \
    (item)->next = NULL;                        \
    (item)->prev = NULL;                        \
    if ((list) == NULL)                         \
        (list) = (item);                        \
    else {                                      \
        typeof(item) tail_ = (list);            \
        while (tail_->next) tail_ = tail_->next;\
        tail_->next = (item);                   \
        (item)->prev = tail_;                   \
    }                                           \
} while (0)

#define LIST_REMOVE(item, list) do {            \
    if ((item)->prev != NULL)                   \
        (item)->prev->next = (item)->next;      \
    if ((item)->next != NULL)                   \
        (item)->next->prev = (item)->prev;      \
    if ((list) == (item))                       \
        (list) = (item)->next;                  \
    (item)->prev = (item)->next = NULL;         \
} while (0)


typedef struct car{
    int id;

    struct car *prev;
    struct car *next;
}car;



static void dump_list(const char *title, struct car *head)
{
    printf("%s: ", title);
    for (struct car *p = head; p; p = p->next)
        printf("%d ", p->id);
    putchar('\n');
}

int main(){
    struct car *head = NULL;

    /* 新建 3 个节点 */
    struct car *c1 = malloc(sizeof *c1);
    struct car *c2 = malloc(sizeof *c2);
    struct car *c3 = malloc(sizeof *c3);
    struct car *c4 = malloc(sizeof *c4);
    c1->id = 1; c1->prev = c1->next = NULL;
    c2->id = 2; c2->prev = c2->next = NULL;
    c3->id = 3; c3->prev = c3->next = NULL;
    c4->id = 4; c4->prev = c4->next = NULL;
    /* 依次头插：3 2 1 */
    LIST_INSERT_TAIL(c1,head);
    LIST_INSERT_HEAD(c4, head);
    LIST_INSERT_HEAD(c2, head);
    LIST_INSERT_HEAD(c3, head);

    dump_list("after insert 3,2,1", head);   /* 期望 3 2 1 */

    /* 把中间节点 2 摘掉 */
    LIST_REMOVE(c2, head);
    dump_list("after remove 2", head);       /* 期望 3 1 */

    /* 再把 2 插到尾部（手动找到尾再插） */
    struct car *tail = head;
    while (tail->next) tail = tail->next;
    tail->next = c2;
    c2->prev = tail;
    c2->next = NULL;
    dump_list("after append 2 to tail", head); /* 期望 3 1 2 */

    /* 清理 */
    while (head) {
        struct car *tmp = head;
        head = head->next;
        free(tmp);
    }
    return 0;
}
