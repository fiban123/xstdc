#include "xstdc/print.h"
#include "xstdc/rng.h"
#include "xstdc/vec.h"

static void test_sizes(usize width) {
    XVec v;
    xvec_init(&v, width);
    u8 model[256 * 129], input[8 * 129];
    usize count = 0;
    XRngXoshiro256ppState rng;
    xrng_xoshiro256pp_seed(&rng, width);
    xvec_appendn(&v, 0, 0);
    xvec_insertn(&v, 0, 0, 0);
    xvec_removen(&v, 0, 0);
    for (unsigned step = 0; step < 3000; ++step) {
        u64 r = xrng_xoshiro256pp_u64(&rng);
        usize n = 1 + r % 8, idx = (r >> 8) % (count + 1);
        for (usize i = 0; i < n * width; ++i)
            input[i] = (u8)(r + i);
        if (count + n <= 256 && (r & 1)) {
            if (r & 2)
                idx = count;
            xmemmv(model + (idx + n) * width, model + idx * width,
                   (count - idx) * width);
            xmemcp(model + idx * width, input, n * width);
            if (idx == count) {
                if (n == 1)
                    xvec_append(&v, input);
                else
                    xvec_appendn(&v, input, n);
            } else if (n == 1)
                xvec_insert(&v, idx, input);
            else
                xvec_insertn(&v, idx, input, n);
            count += n;
        } else if (count) {
            idx %= count;
            n = 1 + (r >> 16) % (count - idx);
            if (n == 1 && idx == count - 1)
                xvec_pop(&v);
            else if (n == 1)
                xvec_remove(&v, idx);
            else
                xvec_removen(&v, idx, idx + n);
            xmemmv(model + idx * width, model + (idx + n) * width,
                   (count - idx - n) * width);
            count -= n;
        }
        assert(v.sz == count && v.reserved >= count);
        if (count)
            assert(xmemeq(v.p, model, count * width));
        for (usize i = 0; i < count; ++i)
            assert(xmemeq(xvec_at(&v, i), model + i * width, width));
    }
    usize capacity = v.reserved;
    void *p = v.p;
    xvec_reserve(&v, capacity);
    assert(v.p == p && v.reserved == capacity);
    xvec_reserve(&v, capacity + 17);
    assert(v.reserved >= capacity + 17 && v.sz == count);
    if (count)
        assert(xmemeq(v.p, model, count * width));
    xvec_free(&v);
    xvec_free(&v);
    assert(!v.p && !v.sz && !v.reserved && v.elem_sz == width);
    xvec_append(&v, input);
    assert(xmemeq(v.p, input, width));
    xvec_free(&v);
}

static void test_alias(void) {
    for (unsigned grow = 0; grow < 2; ++grow) {
        XVec v;
        xvec_init(&v, sizeof(u64));
        xvec_reserve(&v, grow ? 8 : 32);
        for (u64 i = 0; i < 8; ++i)
            xvec_append(&v, &i);
        xvec_insertn(&v, 2, xvec_at(&v, 1), 4);
        u64 expected[] = {0, 1, 1, 2, 3, 4, 2, 3, 4, 5, 6, 7};
        assert(v.sz == 12 && xmemeq(v.p, expected, sizeof expected));
        xvec_appendn(&v, v.p, v.sz);
        assert(v.sz == 24);
        for (usize i = 0; i < v.sz; ++i)
            assert(xvat(&v, u64, i) == expected[i % 12]);
        xvat(&v, u64, 0) = 99;
        assert(*(u64 *)xvec_at(&v, 0) == 99);
        xvec_removen(&v, 0, v.sz);
        assert(!v.sz);
        xvec_free(&v);
    }
}

// Exercise the append fast path and relocation with a source in live storage.
static void test_append_alias(void) {
    for (unsigned grow = 0; grow < 2; ++grow) {
        XVec v;
        xvec_init(&v, sizeof(u64));
        xvec_reserve(&v, grow ? 8 : 32);
        for (u64 i = 0; i < 8; ++i)
            xvec_append(&v, &i);
        const usize capacity = v.reserved;
        xvec_append(&v, xvec_at(&v, 3));
        assert(v.sz == 9 && v.elem_sz == sizeof(u64));
        assert(xvat(&v, u64, 8) == 3);
        assert(grow ? v.reserved > capacity : v.reserved == capacity);
        for (usize i = 0; i < 8; ++i)
            assert(xvat(&v, u64, i) == i);
        xvec_free(&v);
    }
}

static void test_shrink(void) {
    XVec v;
    xvec_init(&v, sizeof(u64));
    xvec_shrink(&v);
    assert(!v.p && !v.reserved && !v.sz);
    for (usize n = 0; n <= 65; ++n) {
        xvec_reserve(&v, 256);
        for (u64 i = 0; i < n; ++i)
            xvec_append(&v, &i);
        xvec_shrink(&v);
        const usize capacity = v.reserved;
        assert(v.sz == n && v.elem_sz == sizeof(u64));
        if (n) {
            assert(capacity >= n && capacity / 2 < n);
            assert(!(capacity & (capacity - 1)));
        } else {
            assert(!v.p && !capacity);
        }
        void *p = v.p;
        xvec_shrink(&v);
        assert(v.p == p && v.reserved == capacity);
        u64 last = n;
        xvec_append(&v, &last);
        for (usize i = 0; i <= n; ++i)
            assert(xvat(&v, u64, i) == i);
        xvec_removen(&v, 0, v.sz);
        xvec_shrink(&v);
        assert(!v.p && !v.reserved && !v.sz);
    }
    xvec_free(&v);
}

int main(void) {
    usize widths[] = {1, 3, 4, 8, 16, 129};
    for (usize i = 0; i < sizeof widths / sizeof *widths; ++i)
        test_sizes(widths[i]);
    test_alias();
    test_append_alias();
    test_shrink();
    xprint("%z\n", "vector tests passed");
}
