// os/src/lib/list.c
#include "list.h"
#include "interrupt.h"

void list_init(struct list *list) {
    list->head.prev = NULL;
    list->head.next = &list->tail;
    list->tail.prev = &list->head;
    list->tail.next = NULL;
}

void list_insert_before(struct list_elem *before, struct list_elem *elem) {
    enum intr_status old_status = intr_disable();

    /* 将before前驱元素的后继元素更新为elem, 暂时使before脱离链表*/
    before->prev->next = elem;
    /* 更新elem自己的前驱结点为before的前驱,
     * 更新elem自己的后继结点为before, 于是before又回到链表 */
    elem->prev = before->prev;
    elem->next = before;
    /* 更新before的前驱结点为elem */
    before->prev = elem;
    intr_set_status(old_status);
}

void list_push(struct list *plist, struct list_elem *elem) {
    list_insert_before(plist->head.next, elem); // 在队头插入elem
}

void list_append(struct list *plist, struct list_elem *elem) {
    list_insert_before(&plist->tail, elem); // 在队尾的前面插入
}

void list_remove(struct list_elem *pelem) {
    enum intr_status old_status = intr_disable();

    pelem->prev->next = pelem->next;
    pelem->next->prev = pelem->prev;

    intr_set_status(old_status);
}

struct list_elem *list_pop(struct list *plist) {
    struct list_elem *elem = plist->head.next;
    list_remove(elem);
    return elem;
}

struct list_elem *list_traversal(struct list *plist, function func, int arg) {
    struct list_elem *elem = plist->head.next;

    if (list_empty(plist)) return NULL; // 队列为空则直接返回

    while (elem != &plist->tail) {
        if (func(elem, arg)) { // func返回ture则认为符合条件
            return elem;
        } // 若回调函数func返回true,则继续遍历
        elem = elem->next;
    }
    return NULL;
}

uint32_t list_len(struct list *plist) {
    struct list_elem *elem = plist->head.next;
    uint32_t length = 0;
    while (elem != &plist->tail) {
        length++;
        elem = elem->next;
    }
    return length;
}

bool list_empty(struct list *plist) {
    return (plist->head.next == &plist->tail ? true : false);
}

bool elem_find(struct list *plist, struct list_elem *obj_elem) {
    struct list_elem *elem = plist->head.next;
    while (elem != &plist->tail) {
        if (elem == obj_elem) {
            return true;
        }
        elem = elem->next;
    }
    return false;
}