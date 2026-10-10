#pragma once

#include "xstdc/alloc.h"
#include "xstdc/assert.h"
#include "xstdc/mem.h"
#include "xstdc/types.h"

#ifndef XLINKED_LIST_EXTERN_INLINE
#define XLINKED_LIST_EXTERN_INLINE inline
#endif

// Lists own copies of elem_sz bytes. Element pointers remain valid until their
// node is removed or the list is freed. Structural mutations invalidate
// iterators. node arguments are element pointers belonging to the list, never
// NULL. Elements have 8-byte alignment; do not copy live lists as separate
// owners.
typedef struct XLinkedListNode {
    struct XLinkedListNode *next;
    u8 data[];
} XLinkedListNode;
typedef struct XDoublyLinkedListNode {
    struct XDoublyLinkedListNode *prev, *next;
    u8 data[];
} XDoublyLinkedListNode;
_Static_assert(sizeof(XLinkedListNode) == 8,
               "single node metadata must be 8 bytes");
_Static_assert(sizeof(XDoublyLinkedListNode) == 16,
               "double node metadata must be 16 bytes");

typedef struct {
    usize elem_sz, sz;
    XLinkedListNode *head, *tail;
} XLinkedList;
typedef struct {
    XLinkedListNode *next;
} XLinkedListIter;

typedef struct {
    usize elem_sz, sz;
    XDoublyLinkedListNode *head, *tail;
} XDoublyLinkedList;
// Cursor between prev and next: reversing direction returns the last element
// again.
typedef struct {
    XDoublyLinkedListNode *prev, *next;
} XDoublyLinkedListIter;

// Initialize an empty list; elem_sz must be nonzero.
static inline void xllist_init(XLinkedList *llist, usize elem_sz);

// Recover node metadata from a non-NULL element pointer.
static inline XLinkedListNode *xllist_node(const void *elem);

// Allocate one link plus elem_sz bytes and copy the element.
static inline XLinkedListNode *xllist_new(usize elem_sz, const void *elem);

// Remove the successor of node; node and its successor must exist.
XLINKED_LIST_EXTERN_INLINE void xllist_removeafter(XLinkedList *llist,
                                                   const void *node);

// Append a copy of elem, including to an empty list.
XLINKED_LIST_EXTERN_INLINE void xllist_append(XLinkedList *llist,
                                              const void *elem);

// Prepend a copy of elem, including to an empty list.
XLINKED_LIST_EXTERN_INLINE void xllist_prepend(XLinkedList *llist,
                                               const void *elem);

// Remove the first element; the list must not be empty.
XLINKED_LIST_EXTERN_INLINE void xllist_pop_front(XLinkedList *llist);

// Insert a copy of elem after the non-NULL node.
XLINKED_LIST_EXTERN_INLINE void
xllist_insertafter(XLinkedList *llist, void *node, const void *elem);

// Free all nodes and empty the list; retain elem_sz for reuse.
void xllist_free(XLinkedList *llist);

// Start iteration before the first element.
static inline void xllit_init(XLinkedListIter *llit, XLinkedList *llist);

// Return the next element and advance; return NULL at the end.
static inline void *xllit_forward(XLinkedListIter *llit);

// Initialize an empty doubly linked list; elem_sz must be nonzero.
static inline void xdllist_init(XDoublyLinkedList *dllist, usize elem_sz);

// Recover node metadata from a non-NULL element pointer.
static inline XDoublyLinkedListNode *xdllist_node(const void *elem);

// Copy elem into a new node between adjacent neighbors; NULL marks an
// endpoint.
static inline void xdllist_insert_between(XDoublyLinkedList *list,
                                          XDoublyLinkedListNode *prev,
                                          XDoublyLinkedListNode *next,
                                          const void *elem);

// Append a copy of elem, including to an empty list.
XLINKED_LIST_EXTERN_INLINE void xdllist_append(XDoublyLinkedList *dllist,
                                               const void *elem);

// Prepend a copy of elem, including to an empty list.
XLINKED_LIST_EXTERN_INLINE void xdllist_prepend(XDoublyLinkedList *dllist,
                                                const void *elem);

// Remove and free the non-NULL node belonging to this list.
XLINKED_LIST_EXTERN_INLINE void xdllist_remove(XDoublyLinkedList *dllist,
                                               void *node);

// Remove the first element; the list must not be empty.
XLINKED_LIST_EXTERN_INLINE void xdllist_pop_front(XDoublyLinkedList *dllist);

// Insert a copy of elem after the non-NULL node.
XLINKED_LIST_EXTERN_INLINE void
xdllist_insertafter(XDoublyLinkedList *dllist, void *node, const void *elem);

// Insert a copy of elem before the non-NULL node.
XLINKED_LIST_EXTERN_INLINE void xdllist_insertb4(XDoublyLinkedList *dllist,
                                                 void *node, const void *elem);

// Free all nodes and empty the list; retain elem_sz for reuse.
void xdllist_free(XDoublyLinkedList *dllist);

// Position before the head; the first forward call returns the head.
static inline void xdllit_init_head(XDoublyLinkedListIter *dllit,
                                    XDoublyLinkedList *llist);

// Position after the tail; the first backward call returns the tail.
static inline void xdllit_init_tail(XDoublyLinkedListIter *dllit,
                                    XDoublyLinkedList *llist);

// Return the next element and advance; NULL at the end leaves the cursor
// unchanged.
static inline void *xdllit_forward(XDoublyLinkedListIter *dllit);

// Return the previous element and retreat; NULL at the start leaves the cursor
// unchanged.
static inline void *xdllit_backward(XDoublyLinkedListIter *dllit);

// Definitions

static inline void xllist_init(XLinkedList *llist, usize elem_sz) {
    assert(elem_sz && elem_sz <= (usize)-1 - sizeof(XLinkedListNode));
    *llist = (XLinkedList){.elem_sz = elem_sz};
}

static inline XLinkedListNode *xllist_node(const void *elem) {
    assert(elem);
    return (XLinkedListNode *)((u8 *)elem - sizeof(XLinkedListNode));
}

static inline XLinkedListNode *xllist_new(usize elem_sz, const void *elem) {
    XLinkedListNode *node = xalloc(sizeof(*node) + elem_sz);
    node->next = 0;
    xmemcp(node->data, (void *)elem, elem_sz);
    return node;
}

XLINKED_LIST_EXTERN_INLINE void xllist_removeafter(XLinkedList *llist,
                                                   const void *node) {
    XLinkedListNode *prev = xllist_node(node), *removed = prev->next;
    assert(removed);
    prev->next = removed->next;
    if (llist->tail == removed)
        llist->tail = prev;
    --llist->sz;
    xfree(removed, sizeof(*removed) + llist->elem_sz);
}

XLINKED_LIST_EXTERN_INLINE void xllist_append(XLinkedList *llist,
                                              const void *elem) {
    XLinkedListNode *node = xllist_new(llist->elem_sz, elem);
    if (llist->tail)
        llist->tail->next = node;
    else
        llist->head = node;
    llist->tail = node;
    ++llist->sz;
}

XLINKED_LIST_EXTERN_INLINE void xllist_prepend(XLinkedList *llist,
                                               const void *elem) {
    XLinkedListNode *node = xllist_new(llist->elem_sz, elem);
    node->next = llist->head;
    llist->head = node;
    if (!llist->tail)
        llist->tail = node;
    ++llist->sz;
}

XLINKED_LIST_EXTERN_INLINE void xllist_pop_front(XLinkedList *llist) {
    XLinkedListNode *node = llist->head;
    assert(node);
    llist->head = node->next;
    if (!llist->head)
        llist->tail = 0;
    --llist->sz;
    xfree(node, sizeof(*node) + llist->elem_sz);
}

XLINKED_LIST_EXTERN_INLINE void
xllist_insertafter(XLinkedList *llist, void *node, const void *elem) {
    XLinkedListNode *prev = xllist_node(node);
    XLinkedListNode *added = xllist_new(llist->elem_sz, elem);
    added->next = prev->next;
    prev->next = added;
    if (llist->tail == prev)
        llist->tail = added;
    ++llist->sz;
}

static inline void xllit_init(XLinkedListIter *llit, XLinkedList *llist) {
    *llit = (XLinkedListIter){.next = llist->head};
}

static inline void *xllit_forward(XLinkedListIter *llit) {
    XLinkedListNode *node = llit->next;
    if (!node)
        return 0;
    llit->next = node->next;
    return node->data;
}

static inline void xdllist_init(XDoublyLinkedList *dllist, usize elem_sz) {
    assert(elem_sz && elem_sz <= (usize)-1 - sizeof(XDoublyLinkedListNode));
    *dllist = (XDoublyLinkedList){.elem_sz = elem_sz};
}

static inline XDoublyLinkedListNode *xdllist_node(const void *elem) {
    assert(elem);
    return (XDoublyLinkedListNode *)((u8 *)elem -
                                     sizeof(XDoublyLinkedListNode));
}

static inline void xdllist_insert_between(XDoublyLinkedList *list,
                                          XDoublyLinkedListNode *prev,
                                          XDoublyLinkedListNode *next,
                                          const void *elem) {
    XDoublyLinkedListNode *node = xalloc(sizeof(*node) + list->elem_sz);
    node->prev = prev;
    node->next = next;
    xmemcp(node->data, (void *)elem, list->elem_sz);
    if (prev)
        prev->next = node;
    else
        list->head = node;
    if (next)
        next->prev = node;
    else
        list->tail = node;
    ++list->sz;
}

XLINKED_LIST_EXTERN_INLINE void xdllist_append(XDoublyLinkedList *dllist,
                                               const void *elem) {
    xdllist_insert_between(dllist, dllist->tail, 0, elem);
}

XLINKED_LIST_EXTERN_INLINE void xdllist_prepend(XDoublyLinkedList *dllist,
                                                const void *elem) {
    xdllist_insert_between(dllist, 0, dllist->head, elem);
}

XLINKED_LIST_EXTERN_INLINE void xdllist_remove(XDoublyLinkedList *dllist,
                                               void *node) {
    XDoublyLinkedListNode *removed = xdllist_node(node);
    if (removed->prev)
        removed->prev->next = removed->next;
    else
        dllist->head = removed->next;
    if (removed->next)
        removed->next->prev = removed->prev;
    else
        dllist->tail = removed->prev;
    --dllist->sz;
    xfree(removed, sizeof(*removed) + dllist->elem_sz);
}

XLINKED_LIST_EXTERN_INLINE void xdllist_pop_front(XDoublyLinkedList *dllist) {
    assert(dllist->head);
    xdllist_remove(dllist, dllist->head->data);
}

XLINKED_LIST_EXTERN_INLINE void
xdllist_insertafter(XDoublyLinkedList *dllist, void *node, const void *elem) {
    XDoublyLinkedListNode *prev = xdllist_node(node);
    xdllist_insert_between(dllist, prev, prev->next, elem);
}

XLINKED_LIST_EXTERN_INLINE void xdllist_insertb4(XDoublyLinkedList *dllist,
                                                 void *node, const void *elem) {
    XDoublyLinkedListNode *next = xdllist_node(node);
    xdllist_insert_between(dllist, next->prev, next, elem);
}

static inline void xdllit_init_head(XDoublyLinkedListIter *dllit,
                                    XDoublyLinkedList *llist) {
    *dllit = (XDoublyLinkedListIter){.next = llist->head};
}

static inline void xdllit_init_tail(XDoublyLinkedListIter *dllit,
                                    XDoublyLinkedList *llist) {
    *dllit = (XDoublyLinkedListIter){.prev = llist->tail};
}

static inline void *xdllit_forward(XDoublyLinkedListIter *dllit) {
    XDoublyLinkedListNode *node = dllit->next;
    if (!node)
        return 0;
    dllit->prev = node;
    dllit->next = node->next;
    return node->data;
}

static inline void *xdllit_backward(XDoublyLinkedListIter *dllit) {
    XDoublyLinkedListNode *node = dllit->prev;
    if (!node)
        return 0;
    dllit->next = node;
    dllit->prev = node->prev;
    return node->data;
}
