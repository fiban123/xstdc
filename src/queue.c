#define XQUEUE_EXTERN_INLINE
#include "xstdc/queue.h"
#include "growth.h"

static XQueueStorage relocate(void *p, usize reserved, usize elem_sz, usize sz,
                              usize head, usize capacity) {
    void *next = capacity ? xalloc(capacity * elem_sz) : 0;
    if (sz) {
        const usize first = sz < reserved - head ? sz : reserved - head;
        xmemcp(next, (u8 *)p + head * elem_sz, first * elem_sz);
        if (sz > first)
            xmemcp((u8 *)next + first * elem_sz, p, (sz - first) * elem_sz);
    }
    if (p)
        xfree(p, reserved * elem_sz);
    return (XQueueStorage){next, capacity, 0};
}

__attribute__((noinline)) XQueueStorage xqueue_grow(void *p, usize reserved,
                                                    usize elem_sz, usize sz,
                                                    usize head, usize n) {
    assert(elem_sz && n <= (usize)-1 / elem_sz);
    if (n <= reserved)
        return (XQueueStorage){p, reserved, head};
    const usize limit = (usize)-1 / elem_sz;
    const usize chunk = XQUEUE_GROWTH_BYTES / elem_sz;
    const usize initial = chunk < 8 ? (chunk ? chunk : 1) : 8;
    const usize capacity = storage_capacity(
        reserved ? reserved : (limit < initial ? limit : initial),
        elem_sz, n, XQUEUE_GROWTH_BYTES);
    return relocate(p, reserved, elem_sz, sz, head, capacity);
}

__attribute__((noinline)) XQueueStorage xqueue_trim(void *p, usize reserved,
                                                    usize elem_sz, usize sz,
                                                    usize head) {
    if (!reserved || (sz && sz > reserved / 2))
        return (XQueueStorage){p, reserved, head};
    return relocate(p, reserved, elem_sz, sz, head, sz ? reserved / 2 : 0);
}

void xqueue_free(XQueue *q) {
    if (q->p)
        xfree(q->p, q->reserved * q->elem_sz);
    xqueue_init(q, q->elem_sz);
}
void xlqueue_free(XLinkedQueue *q) {
    XQueueNode *node = q->head;
    while (node) {
        XQueueNode *next = node->next;
        xfree(node, sizeof(*node) + q->elem_sz);
        node = next;
    }
    node = q->retired;
    while (node) {
        XQueueNode *next = node->next;
        xfree(node, sizeof(*node) + q->elem_sz);
        node = next;
    }
    xlqueue_init(q, q->elem_sz);
}
