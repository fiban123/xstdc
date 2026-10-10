#include "xstdc/alloc.h"
#include "xstdc/assert.h"
#include "xstdc/mem.h"
#include "xstdc/print.h"
#include "xstdc/rng.h"
#include "xstdc/sort.h"
#include <stdlib.h>

typedef struct {
    i32 key;
    u32 id;
    u8 payload[17];
} Record;
static bool greater(void *a, void *b) {
    return ((Record *)a)->key > ((Record *)b)->key;
}
static bool less(void *a, void *b) {
    return ((Record *)a)->key < ((Record *)b)->key;
}
static int ordered(const void *a, const void *b) {
    const Record *x = a, *y = b;
    if (x->key != y->key)
        return (x->key > y->key) - (x->key < y->key);
    return (x->id > y->id) - (x->id < y->id);
}
static void check(const Record *a, const Record *input, const Record *expected,
                  usize n, usize k, bool stable) {
    bool *seen = xallocz(n);
    for (usize i = 0; i < n; ++i) {
        assert(a[i].id < n && !seen[a[i].id]);
        seen[a[i].id] = true;
        assert(xmemeq(&a[i], &input[a[i].id], sizeof *a));
        if (i < k) {
            assert(a[i].key == expected[i].key);
            if (stable)
                assert(a[i].id == expected[i].id);
        } else if (stable && i > k)
            assert(a[i - 1].id < a[i].id);
    }
    xfree(seen, n);
}
static void comparisons(usize n, u64 seed) {
    Record input[257] = {0}, a[257], expected[257];
    XRngXoshiro256ppState rng;
    xrng_xoshiro256pp_seed(&rng, seed);
    for (usize i = 0; i < n; ++i) {
        input[i].key = (i32)(xrng_xoshiro256pp_u64(&rng) % 11) - 5;
        input[i].id = i;
        for (unsigned j = 0; j < 17; ++j)
            input[i].payload[j] = (u8)(i + j);
    }
    xmemcp(expected, input, n * sizeof *a);
    qsort(expected, n, sizeof *a, ordered);
    xmemcp(a, input, n * sizeof *a);
    xsort_cmp(a, sizeof *a, n, greater);
    check(a, input, expected, n, n, true);
    xmemcp(a, input, n * sizeof *a);
    xsort_cmp_u(a, sizeof *a, n, greater);
    check(a, input, expected, n, n, false);
    for (usize k = 0; k <= n; ++k) {
        xmemcp(a, input, n * sizeof *a);
        xpsort_cmp(a, sizeof *a, n, k, greater);
        check(a, input, expected, n, k, true);
        xmemcp(a, input, n * sizeof *a);
        xpsort_cmp_u(a, sizeof *a, n, k, greater);
        check(a, input, expected, n, k, false);
        if (!k)
            assert(xmemeq(a, input, n * sizeof *a));
    }
    xsort_cmp(a, sizeof *a, n, less);
    for (usize i = 1; i < n; ++i)
        assert(a[i - 1].key >= a[i].key);
    xsort_cmp_u(a, sizeof *a, n, greater);
    check(a, input, expected, n, n, false);
}

#define RADIX_TEST(T)                                                          \
    static int cmp_##T(const void *a, const void *b) {                         \
        T x = *(const T *)a, y = *(const T *)b;                                \
        return (x > y) - (x < y);                                              \
    }                                                                          \
    static void test_##T(void) {                                               \
        T a[1025], expected[1025];                                             \
        XRngXoshiro256ppState rng;                                             \
        xrng_xoshiro256pp_seed(&rng, 123);                                     \
        for (usize n = 0; n <= 1025; n = n < 17 ? n + 1 : n * 2 - 1) {         \
            for (usize i = 0; i < n; ++i) {                                    \
                u64 bits = xrng_xoshiro256pp_u64(&rng);                        \
                xmemcp(a + i, &bits, sizeof(T));                               \
            }                                                                  \
            if (n >= 4) {                                                      \
                a[0] = 0;                                                      \
                a[1] = (T) - 1;                                                \
                u64 bits = 1ull << (sizeof(T) * 8 - 1);                        \
                xmemcp(a + 2, &bits, sizeof(T));                               \
                --bits;                                                        \
                xmemcp(a + 3, &bits, sizeof(T));                               \
            }                                                                  \
            xmemcp(expected, a, n * sizeof(T));                                \
            qsort(expected, n, sizeof(T), cmp_##T);                            \
            xsort_radix_##T(a, n);                                             \
            assert(xmemeq(a, expected, n * sizeof(T)));                        \
            xsort_radix_##T(a, n);                                             \
            assert(xmemeq(a, expected, n * sizeof(T)));                        \
            for (usize i = 0; i < n; ++i)                                      \
                a[i] = (T)7;                                                   \
            xsort_radix_##T(a, n);                                             \
            for (usize i = 0; i < n; ++i)                                      \
                assert(a[i] == (T)7);                                          \
        }                                                                      \
        xsort_radix_##T(0, 0);                                                 \
    }
RADIX_TEST(i32)
RADIX_TEST(i64)
RADIX_TEST(u32)
RADIX_TEST(u64)
static bool byte_greater(void *a, void *b) { return *(u8 *)a > *(u8 *)b; }
static void test_odd_size(void) {
    // Three-byte records starting at an unaligned address, with end guards.
    u8 bytes[] = {99, 3, 0, 7, 1, 1, 7, 3, 2, 7, 0, 3, 7, 99};
    xsort_cmp(bytes + 1, 3, 4, byte_greater);
    const u8 expected[] = {99, 0, 3, 7, 1, 1, 7, 3, 0, 7, 3, 2, 7, 99};
    assert(xmemeq(bytes, expected, sizeof bytes));
    xsort_cmp_u(bytes + 1, 3, 4, byte_greater);
    assert(bytes[0] == 99 && bytes[13] == 99);
    assert(bytes[1] == 0 && bytes[4] == 1 && bytes[7] == 3 && bytes[10] == 3);
}

static void test_patterns(void) {
    const usize n = 4097;
    Record *input = xalloc(n * sizeof *input), *work = xalloc(n * sizeof *work);
    Record *expected = xalloc(n * sizeof *expected);
    usize ks[] = {0, 1, 4, 5, 24, 256, 2048, 4096, 4097};
    for (unsigned pattern = 0; pattern < 7; ++pattern) {
        xmemzero(input, n * sizeof *input);
        for (usize i = 0; i < n; ++i) {
            input[i].id = i;
            if (pattern == 0)
                input[i].key = (i32)i;
            if (pattern == 1)
                input[i].key = (i32)(n - i);
            if (pattern == 2)
                input[i].key = 1;
            if (pattern == 3)
                input[i].key = (i32)(i < n / 2 ? i : n - i);
            if (pattern == 4)
                input[i].key = (i32)(i % 13);
            if (pattern == 5)
                input[i].key = (i32)((i * 373) % n);
            if (pattern == 6)
                input[i].key = (i32)((n - i) / 3);
        }
        xmemcp(expected, input, n * sizeof *input);
        qsort(expected, n, sizeof *expected, ordered);
        xmemcp(work, input, n * sizeof *input);
        xsort_cmp(work, sizeof *work, n, greater);
        check(work, input, expected, n, n, true);
        xmemcp(work, input, n * sizeof *input);
        xsort_cmp_u(work, sizeof *work, n, greater);
        check(work, input, expected, n, n, false);
        for (unsigned j = 0; j < sizeof ks / sizeof *ks; ++j) {
            xmemcp(work, input, n * sizeof *input);
            xpsort_cmp(work, sizeof *work, n, ks[j], greater);
            check(work, input, expected, n, ks[j], true);
            xmemcp(work, input, n * sizeof *input);
            xpsort_cmp_u(work, sizeof *work, n, ks[j], greater);
            check(work, input, expected, n, ks[j], false);
        }
    }
    xfree(input, n * sizeof *input);
    xfree(work, n * sizeof *work);
    xfree(expected, n * sizeof *expected);
}

static void test_nearly_ordered(void) {
    enum { N = 37 };
    Record input[N], work[N], expected[N];
    for (unsigned descending = 0; descending < 2; ++descending) {
        for (usize defect = 1; defect < N; ++defect) {
            xmemzero(input, sizeof input);
            for (usize i = 0; i < N; ++i) {
                input[i].key = (i32)(descending ? N - i : i);
                input[i].id = i;
            }
            if (descending)
                input[defect].key = input[defect - 1].key;
            else {
                i32 t = input[defect].key;
                input[defect].key = input[defect - 1].key;
                input[defect - 1].key = t;
            }
            xmemcp(expected, input, sizeof input);
            qsort(expected, N, sizeof *expected, ordered);
            xmemcp(work, input, sizeof input);
            xsort_cmp(work, sizeof *work, N, greater);
            check(work, input, expected, N, N, true);
            xmemcp(work, input, sizeof input);
            xsort_cmp_u(work, sizeof *work, N, greater);
            check(work, input, expected, N, N, false);
            xmemcp(work, input, sizeof input);
            xpsort_cmp(work, sizeof *work, N, defect, greater);
            check(work, input, expected, N, defect, true);
            xmemcp(work, input, sizeof input);
            xpsort_cmp_u(work, sizeof *work, N, defect, greater);
            check(work, input, expected, N, defect, false);
        }
    }
}

int main(void) {
    xsort_cmp(0, 0, 0, 0);
    xsort_cmp_u(0, 0, 0, 0);
    xpsort_cmp(0, 0, 0, 0, 0);
    xpsort_cmp_u(0, 0, 0, 0, 0);
    for (usize n = 0; n < 33; ++n)
        comparisons(n, n + 1);
    const usize boundaries[] = {33, 63, 64, 65, 127, 128, 129, 255, 256, 257};
    for (usize i = 0; i < sizeof boundaries / sizeof *boundaries; ++i)
        comparisons(boundaries[i], 1000 + i);
    test_nearly_ordered();
    test_patterns();
    test_odd_size();
    test_i32();
    test_i64();
    test_u32();
    test_u64();
    xprint("%z\n", "sort tests passed");
}
