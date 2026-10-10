#define XVEC_EXTERN_INLINE
#include "xstdc/vec.h"
#include "growth.h"

// Keep allocation out of callers even in builds using link-time optimization.
__attribute__((noinline)) XVecStorage xvec_grow(void *p, usize reserved,
                                                usize elem_sz, usize n) {
    assert(elem_sz && n <= (usize)-1 / elem_sz);
    if (n <= reserved)
        return (XVecStorage){p, reserved};
    const usize limit = (usize)-1 / elem_sz;
    const usize chunk = XVEC_GROWTH_BYTES / elem_sz;
    const usize initial = chunk < 8 ? (chunk ? chunk : 1) : 8;
    const usize capacity = storage_capacity(
        reserved ? reserved : (limit < initial ? limit : initial),
        elem_sz, n, XVEC_GROWTH_BYTES);
    p = xrealloc(p, reserved * elem_sz, capacity * elem_sz);
    return (XVecStorage){p, capacity};
}

__attribute__((noinline)) XVecStorage xvec_trim(void *p, usize reserved,
                                                usize elem_sz, usize sz) {
    assert(elem_sz && sz <= reserved);
    if (!sz) {
        if (p)
            xfree(p, reserved * elem_sz);
        return (XVecStorage){0};
    }
    usize capacity = storage_capacity(1, elem_sz, sz, XVEC_GROWTH_BYTES);
    if (capacity > reserved)
        capacity = reserved;
    if (capacity < reserved)
        p = xrealloc(p, reserved * elem_sz, capacity * elem_sz);
    return (XVecStorage){p, capacity};
}

__attribute__((noinline)) XVecStorage xvec_insert_alias(void *p, usize reserved,
                                                        usize elem_sz, usize sz,
                                                        usize idx,
                                                        const void *vals,
                                                        usize n) {
    const usize bytes = n * elem_sz;
    void *copy = xalloc(bytes);
    xmemcp(copy, (void *)vals, bytes);
    const XVecStorage storage = xvec_grow(p, reserved, elem_sz, sz + n);
    u8 *at = (u8 *)storage.p + idx * elem_sz;
    xmemmv(at + bytes, at, (sz - idx) * elem_sz);
    xmemcp(at, copy, bytes);
    xfree(copy, bytes);
    return storage;
}
