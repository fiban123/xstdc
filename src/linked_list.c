#define XLINKED_LIST_EXTERN_INLINE
#include "xstdc/linked_list.h"

void xllist_free(XLinkedList *llist) {
    XLinkedListNode *node = llist->head;
    while (node) {
        XLinkedListNode *next = node->next;
        xfree(node, sizeof(*node) + llist->elem_sz);
        node = next;
    }
    xllist_init(llist, llist->elem_sz);
}

void xdllist_free(XDoublyLinkedList *dllist) {
    XDoublyLinkedListNode *node = dllist->head;
    while (node) {
        XDoublyLinkedListNode *next = node->next;
        xfree(node, sizeof(*node) + dllist->elem_sz);
        node = next;
    }
    xdllist_init(dllist, dllist->elem_sz);
}
