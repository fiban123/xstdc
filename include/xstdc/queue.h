#pragma once

#include "alloc.h"
#include "assert.h"
#include "mem.h"
#include "types.h"

// Byte threshold and increment; override when building the library.
// Rounded down to whole elements, with a minimum increment of one element.
#ifndef XQUEUE_GROWTH_BYTES
#define XQUEUE_GROWTH_BYTES (64u * 1024u * 1024u)
#endif

#ifndef XQUEUE_EXTERN_INLINE
#define XQUEUE_EXTERN_INLINE inline
#endif

// FIFO ring buffer. Element size must be nonzero. Elements are copied by value;
// queues own their storage, not resources referenced by their elements.
typedef struct {
    usize elem_sz;
    usize sz;
    void *p;
    usize reserved, head;
} XQueue;

// Allocation/relocation stays outside callers. Helpers receive storage and
// read-only size metadata by value, never write access to the queue itself.
typedef struct {
    void *p;
    usize reserved, head;
} XQueueStorage;
// Grow and unwrap storage for n elements; return storage with head zero.
extern XQueueStorage xqueue_grow(void *p, usize reserved, usize elem_sz,
                                 usize sz, usize head, usize n);
// Halve storage if live elements fit; free storage if empty.
extern XQueueStorage xqueue_trim(void *p, usize reserved, usize elem_sz,
                                 usize sz, usize head);

// Initialize an empty ring queue with a nonzero element size.
static inline void xqueue_init(XQueue *q, usize elem_sz);

// Reserve n elements, doubling up to XQUEUE_GROWTH_BYTES, then adding chunks.
// Growth invalidates element pointers.
XQUEUE_EXTERN_INLINE void xqueue_reserve(XQueue *q, usize n);

// elem must not be in the queue's allocation. May invalidate all elem pointers.
XQUEUE_EXTERN_INLINE void xqueue_push(XQueue *q, const void *elem);

// Get the oldest element and pop it. NULL if empty. Pop never moves, frees or
// overwrites storage: pointers survive subsequent pops, including emptying.
XQUEUE_EXTERN_INLINE void *xqueue_pop(XQueue *q);
// Get the oldest element without removing it. NULL if empty.
XQUEUE_EXTERN_INLINE void *xqueue_peek(XQueue *q);

// Halve capacity if live elements fit; free if empty. Return the backing
// buffer. Relocation invalidates all element pointers, including popped ones.
XQUEUE_EXTERN_INLINE void *xqueue_shrink(XQueue *q);
// Frees storage and resets the queue, retaining elem_sz for reuse.
void xqueue_free(XQueue *q);

// Linked FIFO. Live element pointers survive pushes and pops. Popped nodes are
// retained until reused by a later push or released by free. Thus pointers to
// popped elements survive all subsequent pops, but may be invalidated by push.
typedef struct XQueueNode {
    struct XQueueNode *next;
    u8 data[] __attribute__((aligned(8)));
} XQueueNode;
typedef struct {
    usize elem_sz;
    usize sz;
    XQueueNode *head, *tail, *retired;
} XLinkedQueue;

// Initialize an empty linked queue with a nonzero element size.
static inline void xlqueue_init(XLinkedQueue *q, usize elem_sz);

// Append a copy of elem; may reuse a popped node. Live pointers remain valid.
XQUEUE_EXTERN_INLINE void xlqueue_push(XLinkedQueue *q, const void *elem);

// Get the oldest element and pop it. NULL if empty. Does not free its node.
XQUEUE_EXTERN_INLINE void *xlqueue_pop(XLinkedQueue *q);
// Get the oldest element without removing it. NULL if empty.
XQUEUE_EXTERN_INLINE void *xlqueue_peek(XLinkedQueue *q);
// Also releases retired nodes. Preserves elem_sz for reuse.
void xlqueue_free(XLinkedQueue *q);

// Definitions

static inline void xqueue_init(XQueue *q, usize elem_sz) {
    assert(elem_sz);
    *q = (XQueue){.elem_sz = elem_sz};
}

XQUEUE_EXTERN_INLINE void xqueue_reserve(XQueue *q, usize n) {
    if (n > q->reserved) {
        const XQueueStorage storage =
            xqueue_grow(q->p, q->reserved, q->elem_sz, q->sz, q->head, n);
        q->p = storage.p;
        q->reserved = storage.reserved;
        q->head = storage.head;
    }
}

XQUEUE_EXTERN_INLINE void xqueue_push(XQueue *q, const void *elem) {
    assert(q->sz < (usize)-1 / q->elem_sz);
    xqueue_reserve(q, q->sz + 1);
    const usize remaining = q->reserved - q->head;
    const usize tail = q->sz >= remaining ? q->sz - remaining : q->head + q->sz;
    xmemcp((u8 *)q->p + tail * q->elem_sz, (void *)elem, q->elem_sz);
    ++q->sz;
}

XQUEUE_EXTERN_INLINE void *xqueue_pop(XQueue *q) {
    if (!q->sz)
        return 0;
    void *p = (u8 *)q->p + q->head * q->elem_sz;
    if (++q->head == q->reserved)
        q->head = 0;
    --q->sz;
    return p;
}

XQUEUE_EXTERN_INLINE void *xqueue_peek(XQueue *q) {
    return q->sz ? (u8 *)q->p + q->head * q->elem_sz : 0;
}

XQUEUE_EXTERN_INLINE void *xqueue_shrink(XQueue *q) {
    if (q->reserved && (!q->sz || q->sz <= q->reserved / 2)) {
        const XQueueStorage storage =
            xqueue_trim(q->p, q->reserved, q->elem_sz, q->sz, q->head);
        q->p = storage.p;
        q->reserved = storage.reserved;
        q->head = storage.head;
    }
    return q->p;
}

static inline void xlqueue_init(XLinkedQueue *q, usize elem_sz) {
    assert(elem_sz && elem_sz <= (usize)-1 - sizeof(XQueueNode));
    *q = (XLinkedQueue){.elem_sz = elem_sz};
}

XQUEUE_EXTERN_INLINE void xlqueue_push(XLinkedQueue *q, const void *elem) {
    assert(q->sz != (usize)-1);
    XQueueNode *node = q->retired;
    if (node)
        q->retired = node->next;
    else
        node = xalloc(sizeof(*node) + q->elem_sz);
    // memmove permits pushing a value obtained from peek, or a retired node.
    xmemmv(node->data, elem, q->elem_sz);
    node->next = 0;
    if (q->tail)
        q->tail->next = node;
    else
        q->head = node;
    q->tail = node;
    ++q->sz;
}

XQUEUE_EXTERN_INLINE void *xlqueue_pop(XLinkedQueue *q) {
    XQueueNode *node = q->head;
    if (!node)
        return 0;
    q->head = node->next;
    if (!q->head)
        q->tail = 0;
    node->next = q->retired;
    q->retired = node;
    --q->sz;
    return node->data;
}

XQUEUE_EXTERN_INLINE void *xlqueue_peek(XLinkedQueue *q) {
    return q->head ? q->head->data : 0;
}
