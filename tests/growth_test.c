#include "xstdc/print.h"
#include "xstdc/queue.h"
#include "xstdc/vec.h"
#include "../src/growth.h"

static void test_capacity(usize bytes, usize width) {
    const usize chunk = bytes / width ? bytes / width : 1;
    if (chunk > 16) {
        assert(storage_capacity(8, width, 9, bytes) == 16);
        assert(storage_capacity(8, width, chunk - 1, bytes) == chunk);
    }
    assert(storage_capacity(1, width, chunk, bytes) == chunk);
    assert(storage_capacity(chunk, width, chunk + 1, bytes) == 2 * chunk);
    assert(storage_capacity(2 * chunk, width, 2 * chunk + 1, bytes) == 3 * chunk);
    assert(storage_capacity(chunk, width, 7 * chunk + 1, bytes) == 8 * chunk);
    const usize limit = (usize)-1 / width;
    assert(storage_capacity(1, width, limit, bytes) == limit);
}

static void test_vector(void) {
    XVec v;
    xvec_init(&v, 3);
    const usize chunk = XVEC_GROWTH_BYTES / v.elem_sz;
    const u8 value[3] = {17, 29, 41};
    xvec_append(&v, value);
    xvec_reserve(&v, chunk);
    assert(v.reserved == chunk);
    xvec_reserve(&v, chunk + 1);
    assert(v.reserved == 2 * chunk);
    xvec_reserve(&v, 2 * chunk + 1);
    assert(v.reserved == 3 * chunk);
    assert(v.sz == 1 && xmemeq(v.p, value, sizeof value));
    // A live range in the third chunk must not round up to four chunks.
    v.sz = 2 * chunk + 1;
    void *p = v.p;
    xvec_shrink(&v);
    assert(v.p == p && v.reserved == 3 * chunk);
    v.sz = 1;
    xvec_shrink(&v);
    assert(v.reserved == 1 && xmemeq(v.p, value, sizeof value));
    xvec_free(&v);
}

static void test_queue(void) {
    XQueue q;
    xqueue_init(&q, sizeof(u64));
    const usize chunk = XQUEUE_GROWTH_BYTES / q.elem_sz;
    xqueue_reserve(&q, chunk);
    assert(q.reserved == chunk);
    // Construct an empty ring near the end to exercise wrapped relocation.
    q.head = chunk - 2;
    for (u64 i = 0; i < 4; ++i)
        xqueue_push(&q, &i);
    xqueue_reserve(&q, chunk + 1);
    assert(q.reserved == 2 * chunk && q.head == 0);
    xqueue_reserve(&q, 2 * chunk + 1);
    assert(q.reserved == 3 * chunk);
    for (u64 i = 0; i < 4; ++i)
        assert(*(u64 *)xqueue_pop(&q) == i);
    xqueue_free(&q);
}

int main(void) {
    const usize widths[] = {1, 3, 8, 129};
    for (usize i = 0; i < sizeof widths / sizeof *widths; ++i) {
        test_capacity(XVEC_GROWTH_BYTES, widths[i]);
        test_capacity(XQUEUE_GROWTH_BYTES, widths[i]);
    }
    // Elements larger than the byte increment still grow one at a time.
    assert(storage_capacity(1, 129, 3, 64) == 3);
    test_vector();
    test_queue();
    xprint("growth tests passed\n");
}
