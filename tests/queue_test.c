#include "xstdc/assert.h"
#include "xstdc/print.h"
#include "xstdc/queue.h"
#include "xstdc/rng.h"

static void test_ring(void) {
    XQueue q;
    xqueue_init(&q, sizeof(u64));
    assert(!xqueue_pop(&q) && !xqueue_peek(&q));
    assert(!xqueue_shrink(&q));
    xqueue_reserve(&q, 16);
    assert(q.reserved == 16);
    for (u64 i = 0; i < 16; ++i)
        xqueue_push(&q, &i);
    u64 *popped[16];
    for (u64 i = 0; i < 12; ++i) {
        popped[i] = xqueue_pop(&q);
        assert(*popped[i] == i);
    }
    for (u64 i = 0; i < 12; ++i)
        assert(*popped[i] == i);
    xqueue_reserve(&q, 8); // No-op: pointers remain intact.
    for (u64 i = 0; i < 12; ++i)
        assert(*popped[i] == i);
    for (u64 i = 16; i < 20; ++i)
        xqueue_push(&q, &i);
    assert(q.sz == 8 && q.head == 12);
    assert(xqueue_shrink(&q) == q.p && q.reserved == 8);
    for (u64 i = 12; i < 20; ++i) {
        assert(*(u64 *)xqueue_peek(&q) == i);
        popped[i - 12] = xqueue_pop(&q);
    }
    assert(!q.sz && !xqueue_pop(&q));
    for (u64 i = 0; i < 8; ++i)
        assert(*popped[i] == i + 12);
    assert(!xqueue_shrink(&q) && !q.reserved);
    // Growth with wrapped live elements.
    for (u64 i = 0; i < 8; ++i)
        xqueue_push(&q, &i);
    for (u64 i = 0; i < 6; ++i)
        assert(*(u64 *)xqueue_pop(&q) == i);
    for (u64 i = 8; i < 15; ++i)
        xqueue_push(&q, &i);
    assert(q.reserved == 16 && q.sz == 9);
    void *before = q.p;
    assert(xqueue_shrink(&q) == before && q.reserved == 16);
    for (u64 i = 6; i < 15; ++i)
        assert(*(u64 *)xqueue_pop(&q) == i);
    xqueue_free(&q);
    assert(!q.p && !q.sz && q.elem_sz == sizeof(u64));
    u64 value = 123;
    xqueue_push(&q, &value);
    assert(*(u64 *)xqueue_pop(&q) == value);
    xqueue_free(&q);
    xqueue_free(&q);
}
static void test_linked(void) {
    XLinkedQueue q;
    xlqueue_init(&q, sizeof(u64));
    assert(!xlqueue_pop(&q) && !xlqueue_peek(&q));
    u64 *pointers[64];
    for (u64 i = 0; i < 64; ++i) {
        xlqueue_push(&q, &i);
        pointers[i] = (u64 *)q.tail->data;
    }
    for (u64 i = 0; i < 64; ++i) {
        assert(xlqueue_pop(&q) == pointers[i]);
        for (u64 j = 0; j < 64; ++j)
            assert(*pointers[j] == j);
    }
    assert(!q.head && !q.tail && !q.sz);
    // Reuse a retired node, including when it is the push source.
    xlqueue_push(&q, pointers[63]);
    assert(*(u64 *)xlqueue_peek(&q) == 63);
    xlqueue_push(&q, xlqueue_peek(&q));
    assert(*(u64 *)xlqueue_pop(&q) == 63);
    assert(*(u64 *)xlqueue_pop(&q) == 63);
    xlqueue_free(&q);
    assert(!q.head && !q.retired && q.elem_sz == sizeof(u64));
    u64 value = 7;
    xlqueue_push(&q, &value);
    xlqueue_free(&q); // Live and retired lists both have cleanup paths.
    xlqueue_free(&q);
}
static void test_model(void) {
    XQueue ring;
    XLinkedQueue list;
    xqueue_init(&ring, 3);
    xlqueue_init(&list, 3);
    u8 expected[256][3];
    usize count = 0;
    XRngXoshiro256ppState rng;
    xrng_xoshiro256pp_seed(&rng, 42);
    for (unsigned step = 0; step < 10000; ++step) {
        u64 random = xrng_xoshiro256pp_u64(&rng);
        if (count < 256 && (random & 1)) {
            u8 value[3] = {(u8)random, (u8)(random >> 8), (u8)(random >> 16)};
            xqueue_push(&ring, value);
            xlqueue_push(&list, value);
            xmemcp(expected[count++], value, 3);
        } else if (count) {
            assert(xmemeq(xqueue_pop(&ring), expected[0], 3));
            assert(xmemeq(xlqueue_pop(&list), expected[0], 3));
            --count;
            xmemmv(expected, expected + 1, count * 3);
        } else {
            assert(!xqueue_pop(&ring) && !xlqueue_pop(&list));
        }
        if (step % 17 == 0)
            xqueue_shrink(&ring);
        if (step % 31 == 0)
            xqueue_reserve(&ring, count + 20);
        assert(ring.sz == count && list.sz == count);
        if (count) {
            assert(xmemeq(xqueue_peek(&ring), expected[0], 3));
            assert(xmemeq(xlqueue_peek(&list), expected[0], 3));
        }
    }
    xqueue_free(&ring);
    xlqueue_free(&list);
}
int main(void) {
    test_ring();
    test_linked();
    test_model();
    xprint("queue tests passed\n");
}
