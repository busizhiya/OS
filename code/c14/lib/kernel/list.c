#include "list.h"
#include "../../kernel/interrupt.h"
#include "stdint.h"
#include "print.h"
void list_init(struct list* list){
    list->head.prev = NULL;
    list->head.next = &list->tail;
    list->tail.prev = &list->head;
    list->tail.next = NULL;
}

void list_insert_before(struct list_elem* before, struct list_elem* elem){
    /*
    A <->  before
    A <- before & A->elem
    A<->elem
    elem<->before
    */
   enum intr_status old_status = intr_disable();
   before->prev->next = elem;
   elem->prev = before->prev;
   elem->next = before;
   before->prev = elem;
   intr_set_status(old_status);
}
//从头插入
void list_push(struct list* plist, struct list_elem* elem){
    list_insert_before(plist->head.next, elem);
}
//从尾插入
void list_append(struct list* plist, struct list_elem* elem){
    list_insert_before(&plist->tail, elem);
}

void list_remove(struct list_elem* pelem){ 
    enum intr_status old_status = intr_disable();
    pelem->next->prev = pelem->prev;
    pelem->prev->next = pelem->next;
    intr_set_status(old_status);
}
//弹出队列首
struct list_elem* list_pop(struct list* plist){
    struct list_elem* elem = plist->head.next;
    list_remove(elem);
    return elem;
}

bool elem_find(struct list* plist, struct list_elem* obj_elem){
    struct list_elem* elem = plist->head.next;
    while(elem != &plist->tail){
        if(elem == obj_elem)
            return true;
        elem = elem->next;
    }
    return false;
}
bool list_empty(struct list* plist){
    return (plist->head.next == &plist->tail ? true : false);
}
//寻找符合筛选函数func的节点
struct list_elem* list_traversal(struct list* plist, function func, int arg){
    struct list_elem* elem = plist->head.next;
    if(list_empty(plist))
        return NULL;
    while(elem != &plist->tail){
        if(func(elem, arg))
            return elem;
        elem = elem->next;
    }
    return NULL;
}

uint32_t list_len(struct list* plist){
    if(list_empty(plist))
        return 0;
    struct list_elem* elem = plist->head.next;
    uint32_t length = 0;
    
    while(elem != &plist->tail)
    {
        length++;
        elem = elem->next;
    }
    return length;
}