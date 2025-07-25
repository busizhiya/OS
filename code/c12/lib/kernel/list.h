#ifndef __LIB_KERBEL_LIST_H
#define __LIB_KERBEL_LIST_H
#include "../../kernel/global.h"
#define offset(struct_type, member) (int)(&(((struct_type*)0)->member))
//(a->b) 等价于(基址+变址), 基址为0, 得到的地址为变址
//用于general_tag变为task_struct线程对象(PCB基址)
#define elem2entry(struct_type, struct_member_name, elem_ptr) \
(struct_type*)((int)elem_ptr - offset(struct_type, struct_member_name))
//元素指针-相对偏移 = 基址

struct list_elem{
    struct list_elem* prev;
    struct list_elem* next;
};

struct list{
    struct list_elem head;
    struct list_elem tail;
};

typedef bool (function)(struct list_elem*, int arg);

uint32_t list_len(struct list* plist);
struct list_elem* list_traversal(struct list* plist, function func, int arg);
bool list_empty(struct list* plist);
bool elem_find(struct list* plist, struct list_elem* obj_elem);
struct list_elem* list_pop(struct list* plist);
void list_remove(struct list_elem* pelem);
void list_append(struct list* plist, struct list_elem* elem);
void list_push(struct list* plist, struct list_elem* elem);
void list_insert_before(struct list_elem* before, struct list_elem* elem);
void list_init(struct list* list);
#endif