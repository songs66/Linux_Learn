#include <stdio.h>
#include <stdlib.h>
#include <string.h>



#define LIST_INSERT_HEAD(item,list)do{      \
    item->prev=NULL;                        \
    item->next=list;                        \
    if(list!=NULL){                         \
        list->prev=item;                    \
    }                                       \
    list=item;                              \
}while (0)

#define LIST_INSERT_TAIL(item,list)do{      \
    item->next=item->prev=NULL              \
    if(list==NULL){                         \
        list=item;                          \
    }                                       \
    else{                                   \
        typeof(item) tail_= list;           \
        while(tail_->next){                 \
            tail_=tali_->next;              \
        }                                   \
        tail_->next=item;                   \
        item->prev=tail_;                   \
    }                                       \
}while(0)

#define LIST_REMOVE(item,list)do{           \
    if (item->prev!=NULL) {                 \
        item->prev->next=item->next;        \
    }                                       \
    if (item->next!=NULL) {                 \
        item->next->prev=item->prev;        \
    }                                       \
    if (item==list) {                       \
        list=item->next;                    \
        item->prev=->next=NULL;             \
    }                                       \
}while (0)




typedef struct student {
    int id;
    char name[20];

    struct student *prev;
    struct student *next;
}student;








