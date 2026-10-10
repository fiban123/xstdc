#pragma once

#include "alloc.h"
#include "assert.h"
#include "mem.h"
#include "types.h"

#ifndef XSORT_EXTERN_INLINE
#define XSORT_EXTERN_INLINE inline
#endif

typedef bool (*CompareFn)(void *a, void *b);

// Sizes count elements (elemsz is bytes). Comparators must define a strict
// ordering and return false for equal elements. Empty arrays may be NULL.
// Stable sorts may allocate O(arrsz) temporary storage. Unstable sorts use
// one element of scratch plus O(log(arrsz)) stack space. Radix sorts use an
// array-sized scratch buffer. All sorts write their result back to arr.

// Stable comparison sort; ascending when cmpfn returns a > b.
XSORT_EXTERN_INLINE void xsort_cmp(void *arr, usize elemsz, usize arrsz,
                                   CompareFn cmpfn);

// Unstable comparison sort; ascending when cmpfn returns a > b.
XSORT_EXTERN_INLINE void xsort_cmp_u(void *arr, usize elemsz, usize arrsz,
                                     CompareFn cmpfn);

// Stably sort the smallest partsz elements first, retaining suffix order.
// Requires partsz <= arrsz; ascending when cmpfn returns a > b.
XSORT_EXTERN_INLINE void xpsort_cmp(void *arr, usize elemsz, usize arrsz,
                                    usize partsz, CompareFn cmpfn);

// Unstably sort the smallest partsz elements first. Requires partsz <= arrsz;
// ascending when cmpfn returns a > b.
XSORT_EXTERN_INLINE void xpsort_cmp_u(void *arr, usize elemsz, usize arrsz,
                                      usize partsz, CompareFn cmpfn);

// radix sort int32 ascending
XSORT_EXTERN_INLINE void xsort_radix_i32(i32 *arr, usize arrsz);

// radix sort int64 ascending
XSORT_EXTERN_INLINE void xsort_radix_i64(i64 *arr, usize arrsz);

// radix sort uint32 ascending
XSORT_EXTERN_INLINE void xsort_radix_u32(u32 *arr, usize arrsz);

// radix sort uint64 ascending
XSORT_EXTERN_INLINE void xsort_radix_u64(u64 *arr, usize arrsz);

// Copy one element, specializing common element widths.
static inline void xsort_internal_copy_element(void *dst, const void *src,
                                               usize size);
// Swap two elements; identical pointers are allowed.
static inline void xsort_internal_swap(u8 *a, u8 *b, usize size);

// Check a run for monotonic order after its first pair; n must be at least two.
static inline bool xsort_internal_monotonic(u8 *a, usize size, usize n,
                                            CompareFn cmp, bool descending);

// Reversing a strictly descending run preserves equal-element stability.
static inline bool xsort_internal_ordered(u8 *a, usize size, usize n,
                                          CompareFn cmp);

// Handle an ordered run while preserving the unselected suffix order.
static inline bool xsort_internal_partial_ordered(u8 *a, usize size, usize n,
                                                  usize k, CompareFn cmp);

// Restore a max heap after replacing its root.
static inline void xsort_internal_sift(u8 *a, usize size, usize count,
                                       usize root, CompareFn cmp);

// Stably sort a small run using one element of scratch storage.
static inline void xsort_internal_insertion(u8 *a, usize size, usize n,
                                            CompareFn cmp, void *tmp);

// Sort using partitioning, with heap fallback at the depth limit.
static inline void xsort_internal_intro(u8 *a, usize size, usize n,
                                        unsigned depth, CompareFn cmp,
                                        void *pivot);

// Stably sort insertion runs and merge them using tmp as scratch storage.
static inline void xsort_internal_merge_sort(u8 *a, usize size, usize n,
                                             CompareFn cmp, u8 *tmp);

// Original indices break ties, so selection is stable even at the cutoff.
static inline bool xsort_internal_index_greater(u8 *a, usize size, usize i,
                                                usize j, CompareFn cmp);
// Restore an index max heap, breaking equal-value ties by original position.
static inline void xsort_internal_index_sift(usize *heap, usize n, usize root,
                                             u8 *a, usize size, CompareFn cmp);

// Read integer representations without aliasing signed and unsigned pointers.
static inline u64 xsort_internal_radix_key(const u8 *p, usize size);
// Stably radix-sort integers, flipping sign bits specified by sign.
static inline __attribute__((always_inline)) void
xsort_internal_radix(void *arr, usize n, usize size, u64 sign);

// Definitions

static inline void xsort_internal_copy_element(void *dst, const void *src,
                                               usize size) {
    if (size == 8) {
        u64 v;
        xmemcp(&v, (void *)src, 8);
        xmemcp(dst, &v, 8);
    } else if (size == 4) {
        u32 v;
        xmemcp(&v, (void *)src, 4);
        xmemcp(dst, &v, 4);
    } else
        xmemcp(dst, (void *)src, size);
}

static inline void xsort_internal_swap(u8 *a, u8 *b, usize size) {
    if (a == b)
        return;
    while (size >= 8) {
        u64 x, y;
        xmemcp(&x, a, 8);
        xmemcp(&y, b, 8);
        xmemcp(a, &y, 8);
        xmemcp(b, &x, 8);
        a += 8;
        b += 8;
        size -= 8;
    }
    if (size >= 4) {
        u32 x, y;
        xmemcp(&x, a, 4);
        xmemcp(&y, b, 4);
        xmemcp(a, &y, 4);
        xmemcp(b, &x, 4);
        a += 4;
        b += 4;
        size -= 4;
    }
    while (size--) {
        u8 t = *a;
        *a++ = *b;
        *b++ = t;
    }
}

static inline bool xsort_internal_monotonic(u8 *a, usize size, usize n,
                                            CompareFn cmp, bool descending) {
    usize i = 2;
    for (; n - i >= 4; i += 4) {
        const unsigned good =
            (unsigned)(cmp(a + (i - 1) * size, a + i * size) == descending) &
            (unsigned)(cmp(a + i * size, a + (i + 1) * size) == descending) &
            (unsigned)(cmp(a + (i + 1) * size, a + (i + 2) * size) ==
                       descending) &
            (unsigned)(cmp(a + (i + 2) * size, a + (i + 3) * size) ==
                       descending);
        if (!good)
            return false;
    }
    for (; i < n; ++i)
        if (cmp(a + (i - 1) * size, a + i * size) != descending)
            return false;
    return true;
}

static inline bool xsort_internal_ordered(u8 *a, usize size, usize n,
                                          CompareFn cmp) {
    if (!cmp(a, a + size)) {
        return xsort_internal_monotonic(a, size, n, cmp, false);
    }
    if (!xsort_internal_monotonic(a, size, n, cmp, true))
        return false;
    for (usize l = 0, r = n - 1; l < r; ++l, --r)
        xsort_internal_swap(a + l * size, a + r * size, size);
    return true;
}

static inline bool xsort_internal_partial_ordered(u8 *a, usize size, usize n,
                                                  usize k, CompareFn cmp) {
    if (!cmp(a, a + size)) {
        return xsort_internal_monotonic(a, size, n, cmp, false);
    }
    if (!xsort_internal_monotonic(a, size, n, cmp, true))
        return false;
    for (usize l = 0, r = n - 1; l < r; ++l, --r)
        xsort_internal_swap(a + l * size, a + r * size, size);
    // Restore the unselected suffix to its original order.
    for (usize l = k, r = n - 1; l < r; ++l, --r)
        xsort_internal_swap(a + l * size, a + r * size, size);
    return true;
}

static inline void xsort_internal_sift(u8 *a, usize size, usize count,
                                       usize root, CompareFn cmp) {
    while (root < count / 2) {
        usize child = root * 2 + 1;
        if (child + 1 < count && cmp(a + (child + 1) * size, a + child * size))
            ++child;
        if (!cmp(a + child * size, a + root * size))
            return;
        xsort_internal_swap(a + child * size, a + root * size, size);
        root = child;
    }
}

XSORT_EXTERN_INLINE void xpsort_cmp_u(void *arr, usize elemsz, usize arrsz,
                                      usize partsz, CompareFn cmpfn) {
    assert(partsz <= arrsz);
    if (!partsz || arrsz < 2)
        return;
    assert(arr && cmpfn && elemsz && arrsz <= (usize)-1 / elemsz);
    u8 *a = arr;
    if (xsort_internal_partial_ordered(a, elemsz, arrsz, partsz, cmpfn))
        return;
    for (usize i = partsz / 2; i; --i)
        xsort_internal_sift(a, elemsz, partsz, i - 1, cmpfn);
    for (usize i = partsz; i < arrsz; ++i) {
        if (cmpfn(a, a + i * elemsz)) {
            xsort_internal_swap(a, a + i * elemsz, elemsz);
            xsort_internal_sift(a, elemsz, partsz, 0, cmpfn);
        }
    }
    for (usize n = partsz; n > 1; --n) {
        xsort_internal_swap(a, a + (n - 1) * elemsz, elemsz);
        xsort_internal_sift(a, elemsz, n - 1, 0, cmpfn);
    }
}

static inline void xsort_internal_insertion(u8 *a, usize size, usize n,
                                            CompareFn cmp, void *tmp) {
    for (usize i = 1; i < n; ++i) {
        if (!cmp(a + (i - 1) * size, a + i * size))
            continue;
        xsort_internal_copy_element(tmp, a + i * size, size);
        usize j = i;
        do {
            --j;
        } while (j && cmp(a + (j - 1) * size, tmp));
        xmemmv(a + (j + 1) * size, a + j * size, (i - j) * size);
        xsort_internal_copy_element(a + j * size, tmp, size);
    }
}

static inline void xsort_internal_intro(u8 *a, usize size, usize n,
                                        unsigned depth, CompareFn cmp,
                                        void *pivot) {
    struct {
        u8 *a;
        usize n;
        unsigned depth;
    } pending[64]; // Smaller sides halve in size; usize has 64 bits.
    unsigned used = 0;
    for (;;) {
        while (n > 24) {
            if (!depth--) {
                xpsort_cmp_u(a, size, n, n, cmp);
                n = 0;
                break;
            }
            u8 *mid = a + (n / 2) * size, *last = a + (n - 1) * size;
            if (cmp(a, mid))
                xsort_internal_swap(a, mid, size);
            if (cmp(mid, last))
                xsort_internal_swap(mid, last, size);
            if (cmp(a, mid))
                xsort_internal_swap(a, mid, size);
            xsort_internal_copy_element(pivot, mid, size);
            usize lo, hi;
            if (!cmp(mid, a) || !cmp(last, mid)) {
                lo = 0;
                hi = n;
                usize i = 0;
                while (i < hi) {
                    if (cmp(pivot, a + i * size))
                        xsort_internal_swap(a + lo++ * size, a + i++ * size,
                                            size);
                    else if (cmp(a + i * size, pivot))
                        xsort_internal_swap(a + i * size, a + --hi * size,
                                            size);
                    else
                        ++i;
                }
            } else {
                // Hoare partition uses one directional comparison per scan.
                usize i = 0, j = n;
                for (;;) {
                    while (cmp(pivot, a + i * size))
                        ++i;
                    while (cmp(a + (j - 1) * size, pivot))
                        --j;
                    if (i >= j)
                        break;
                    xsort_internal_swap(a + i * size, a + (j - 1) * size, size);
                    ++i;
                    --j;
                }
                lo = hi = i;
            }
            // Process the smaller side first; defer the larger side on a
            // bounded stack.
            if (lo < n - hi) {
                pending[used].a = a + hi * size;
                pending[used].n = n - hi;
                pending[used++].depth = depth;
                n = lo;
            } else {
                pending[used].a = a;
                pending[used].n = lo;
                pending[used++].depth = depth;
                a += hi * size;
                n -= hi;
            }
        }
        xsort_internal_insertion(a, size, n, cmp, pivot);
        if (!used)
            break;
        --used;
        a = pending[used].a;
        n = pending[used].n;
        depth = pending[used].depth;
    }
}

XSORT_EXTERN_INLINE void xsort_cmp_u(void *arr, usize elemsz, usize arrsz,
                                     CompareFn cmpfn) {
    if (arrsz < 2)
        return;
    assert(arr && cmpfn && elemsz && arrsz <= (usize)-1 / elemsz);
    u8 *a = arr;
    if (xsort_internal_ordered(a, elemsz, arrsz, cmpfn))
        return;
    unsigned depth = 0;
    for (usize n = arrsz; n > 1; n >>= 1)
        ++depth;
    void *tmp = xalloc(elemsz);
    xsort_internal_intro(a, elemsz, arrsz, 2 * depth, cmpfn, tmp);
    xfree(tmp, elemsz);
}

static inline void xsort_internal_merge_sort(u8 *a, usize size, usize n,
                                             CompareFn cmp, u8 *tmp) {
    const usize run = 16;
    for (usize base = 0; base < n;) {
        const usize count = n - base < run ? n - base : run;
        xsort_internal_insertion(a + base * size, size, count, cmp, tmp);
        base += count;
    }
    for (usize width = run; width < n;) {
        for (usize base = 0; base < n - width;) {
            const usize mid = base + width;
            const usize count = n - mid < width ? n - mid : width;
            const usize end = mid + count;
            if (cmp(a + (mid - 1) * size, a + mid * size)) {
                xmemcp(tmp, a + base * size, width * size);
                usize l = 0, r = mid, out = base;
                while (l < width && r < end) {
                    const bool right = cmp(tmp + l * size, a + r * size);
                    xsort_internal_copy_element(
                        a + out++ * size, right ? a + r * size : tmp + l * size,
                        size);
                    r += right;
                    l += !right;
                }
                if (l < width)
                    xmemcp(a + out * size, tmp + l * size, (width - l) * size);
            }
            base = end;
        }
        if (width > n / 2)
            break;
        width *= 2;
    }
}

XSORT_EXTERN_INLINE void xsort_cmp(void *arr, usize elemsz, usize arrsz,
                                   CompareFn cmpfn) {
    if (arrsz < 2)
        return;
    assert(arr && cmpfn && elemsz && arrsz <= (usize)-1 / elemsz);
    u8 *a = arr;
    if (xsort_internal_ordered(a, elemsz, arrsz, cmpfn))
        return;
    // Largest left run that the bottom-up merge can copy. It is usually
    // half the array; an uneven final merge can require a larger left run.
    usize scratch = 1;
    if (arrsz > 16) {
        scratch = 16;
        while (scratch <= (arrsz - 1) / 2)
            scratch *= 2;
    }
    const usize bytes = scratch * elemsz;
    u8 *tmp = xalloc(bytes);
    xsort_internal_merge_sort(a, elemsz, arrsz, cmpfn, tmp);
    xfree(tmp, bytes);
}

static inline bool xsort_internal_index_greater(u8 *a, usize size, usize i,
                                                usize j, CompareFn cmp) {
    if (cmp(a + i * size, a + j * size))
        return true;
    if (cmp(a + j * size, a + i * size))
        return false;
    return i > j;
}

static inline void xsort_internal_index_sift(usize *heap, usize n, usize root,
                                             u8 *a, usize size, CompareFn cmp) {
    const usize value = heap[root];
    while (root < n / 2) {
        usize child = root * 2 + 1;
        if (child + 1 < n && xsort_internal_index_greater(
                                 a, size, heap[child + 1], heap[child], cmp))
            ++child;
        if (!xsort_internal_index_greater(a, size, heap[child], value, cmp))
            break;
        heap[root] = heap[child];
        root = child;
    }
    heap[root] = value;
}

XSORT_EXTERN_INLINE void xpsort_cmp(void *arr, usize elemsz, usize arrsz,
                                    usize partsz, CompareFn cmpfn) {
    assert(partsz <= arrsz);
    if (!partsz || arrsz < 2)
        return;
    if (partsz == arrsz) {
        xsort_cmp(arr, elemsz, arrsz, cmpfn);
        return;
    }
    assert(arr && cmpfn && elemsz && arrsz <= (usize)-1 / elemsz);
    assert(partsz <= (usize)-1 / sizeof(usize));
    u8 *a = arr;
    if (xsort_internal_partial_ordered(a, elemsz, arrsz, partsz, cmpfn))
        return;
    if (partsz <= 4) {
        void *tmp = xalloc(elemsz);
        for (usize i = 0; i < partsz; ++i) {
            usize best = i;
            for (usize j = i + 1; j < arrsz; ++j)
                if (cmpfn(a + best * elemsz, a + j * elemsz))
                    best = j;
            if (best == i)
                continue;
            xmemcp(tmp, a + best * elemsz, elemsz);
            xmemmv(a + (i + 1) * elemsz, a + i * elemsz, (best - i) * elemsz);
            xmemcp(a + i * elemsz, tmp, elemsz);
        }
        xfree(tmp, elemsz);
        return;
    }
    usize *heap = xalloc(partsz * sizeof *heap);
    for (usize i = 0; i < partsz; ++i)
        heap[i] = i;
    for (usize i = partsz / 2; i; --i)
        xsort_internal_index_sift(heap, partsz, i - 1, a, elemsz, cmpfn);
    for (usize i = partsz; i < arrsz; ++i) {
        if (xsort_internal_index_greater(a, elemsz, heap[0], i, cmpfn)) {
            heap[0] = i;
            xsort_internal_index_sift(heap, partsz, 0, a, elemsz, cmpfn);
        }
    }
    for (usize n = partsz; n > 1; --n) {
        usize t = heap[0];
        heap[0] = heap[n - 1];
        heap[n - 1] = t;
        xsort_internal_index_sift(heap, n - 1, 0, a, elemsz, cmpfn);
    }
    u8 *selected = xallocz(arrsz), *tmp = xalloc(arrsz * elemsz);
    for (usize i = 0; i < partsz; ++i) {
        selected[heap[i]] = 1;
        xmemcp(tmp + i * elemsz, a + heap[i] * elemsz, elemsz);
    }
    usize out = partsz;
    for (usize i = 0; i < arrsz; ++i)
        if (!selected[i])
            xmemcp(tmp + out++ * elemsz, a + i * elemsz, elemsz);
    xmemcp(a, tmp, arrsz * elemsz);
    xfree(tmp, arrsz * elemsz);
    xfree(selected, arrsz);
    xfree(heap, partsz * sizeof *heap);
}

static inline u64 xsort_internal_radix_key(const u8 *p, usize size) {
    if (size == 4) {
        u32 v;
        xmemcp(&v, (void *)p, 4);
        return v;
    }
    u64 v;
    xmemcp(&v, (void *)p, 8);
    return v;
}

static inline __attribute__((always_inline)) void
xsort_internal_radix(void *arr, usize n, usize size, u64 sign) {
    if (n < 2)
        return;
    assert(arr && n <= (usize)-1 / size);
    u8 *tmp = xalloc(n * size), *src = arr, *dst = tmp;
    u64 varying = 0, first = xsort_internal_radix_key(src, size);
    for (usize i = 1; i < n; ++i)
        varying |= first ^ xsort_internal_radix_key(src + i * size, size);
    for (usize pass = 0; pass < size; ++pass) {
        if (!((varying >> (pass * 8)) & 255))
            continue;
        usize counts[256] = {0};
        const unsigned shift = (unsigned)(pass * 8);
        for (usize i = 0; i < n; ++i)
            ++counts[((xsort_internal_radix_key(src + i * size, size) ^ sign) >>
                      shift) &
                     255];
        usize sum = 0;
        for (unsigned b = 0; b < 256; ++b) {
            usize count = counts[b];
            counts[b] = sum;
            sum += count;
        }
        for (usize i = 0; i < n; ++i) {
            const unsigned b =
                ((xsort_internal_radix_key(src + i * size, size) ^ sign) >>
                 shift) &
                255;
            xmemcp(dst + counts[b]++ * size, src + i * size, size);
        }
        u8 *p = src;
        src = dst;
        dst = p;
    }
    if (src != arr)
        xmemcp(arr, src, n * size);
    xfree(tmp, n * size);
}

XSORT_EXTERN_INLINE void xsort_radix_i32(i32 *arr, usize arrsz) {
    xsort_internal_radix(arr, arrsz, 4, 1ull << 31);
}

XSORT_EXTERN_INLINE void xsort_radix_i64(i64 *arr, usize arrsz) {
    xsort_internal_radix(arr, arrsz, 8, 1ull << 63);
}

XSORT_EXTERN_INLINE void xsort_radix_u32(u32 *arr, usize arrsz) {
    xsort_internal_radix(arr, arrsz, 4, 0);
}

XSORT_EXTERN_INLINE void xsort_radix_u64(u64 *arr, usize arrsz) {
    xsort_internal_radix(arr, arrsz, 8, 0);
}
