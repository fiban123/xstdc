#pragma once

#include "alloc.h"
#include "assert.h"
#include "mem.h"
#include "types.h"

// Byte threshold and increment; override when building the library.
// Rounded down to whole elements, with a minimum increment of one element.
#ifndef XVEC_GROWTH_BYTES
#define XVEC_GROWTH_BYTES (64u * 1024u * 1024u)
#endif

#if defined(__OPTIMIZE__) && !defined(XVEC_NO_ALWAYS_INLINE)
#define XVEC_ALWAYS_INLINE __attribute__((always_inline))
#else
#define XVEC_ALWAYS_INLINE
#endif

#ifndef XVEC_EXTERN_INLINE
#define XVEC_EXTERN_INLINE inline
#endif

#ifndef XVEC_NO_SHORT_NAMES
#define xvat(v, T, i) (*(T *)xvec_at((v), (i)))
#endif

typedef struct {
    void *p;
    usize elem_sz;
    usize sz;
    usize reserved;
} XVec;

// Element size must be nonzero. Indices/ranges must be within the live
// elements. Growth and shrinking may invalidate element pointers. free preserves
// elem_sz for reuse. Allocation paths are deliberately defined only in src/vec.c.
typedef struct {
    void *p;
    usize reserved;
} XVecStorage;

// Grow storage for at least n elements; return the new pointer and capacity.
XVecStorage xvec_grow(void *p, usize reserved, usize elem_sz, usize n);
// Shrink storage to fit sz elements using the growth steps; free if empty.
XVecStorage xvec_trim(void *p, usize reserved, usize elem_sz, usize sz);
// Copy aliased input before growing and inserting n elements at idx.
XVecStorage xvec_insert_alias(void *p, usize reserved, usize elem_sz, usize sz,
                              usize idx, const void *vals, usize n);

// Initialize an empty vector with a nonzero element size.
static inline void xvec_init(XVec *v, usize elem_sz);

// Expect N elements. Double capacity up to XVEC_GROWTH_BYTES, then add chunks.
static inline void xvec_reserve(XVec *v, usize n);

// Shrink to the smallest growth capacity fitting sz; free if empty.
static inline void xvec_shrink(XVec *v);

// append elements to the end
static inline XVEC_ALWAYS_INLINE void xvec_appendn(XVec *v, const void *vals,
                                                   usize n);

// append element to the end
static inline XVEC_ALWAYS_INLINE void xvec_append(XVec *v, const void *val);

// pop the end
static inline XVEC_ALWAYS_INLINE void xvec_pop(XVec *v);

// insert such that v[idx] = vals[0], ... after the insertion
XVEC_EXTERN_INLINE void xvec_insertn(XVec *v, usize idx, const void *vals,
                                     usize n);

// insert such that v[idx] = val after the insertion
XVEC_EXTERN_INLINE void xvec_insert(XVec *v, usize idx, const void *val);

// Equivalent to calling xvec_remove(begin) end - begin times.
XVEC_EXTERN_INLINE void xvec_removen(XVec *v, usize begin, usize end);

// Remove the element at idx and shift subsequent elements left.
XVEC_EXTERN_INLINE void xvec_remove(XVec *v, usize idx);

// Return a pointer to the element at idx; idx must be less than sz.
static inline XVEC_ALWAYS_INLINE void *xvec_at(XVec *v, usize idx);

// Free storage and empty the vector, preserving elem_sz for reuse.
XVEC_EXTERN_INLINE void xvec_free(XVec *v);

// Definitions

static inline void xvec_init(XVec *v, usize elem_sz) {
    assert(elem_sz);
    *v = (XVec){.elem_sz = elem_sz};
}

static inline void xvec_reserve(XVec *v, usize n) {
    if (n > v->reserved) {
        const XVecStorage storage = xvec_grow(v->p, v->reserved, v->elem_sz, n);
        v->p = storage.p;
        v->reserved = storage.reserved;
    }
}

static inline void xvec_shrink(XVec *v) {
    const XVecStorage storage = xvec_trim(v->p, v->reserved, v->elem_sz, v->sz);
    v->p = storage.p;
    v->reserved = storage.reserved;
}

static inline XVEC_ALWAYS_INLINE void xvec_appendn(XVec *v, const void *vals,
                                                   usize n) {
    if (!n)
        return;
    const usize elem_sz = v->elem_sz, sz = v->sz;
    assert(n <= (usize)-1 / elem_sz - sz);
    if (sz + n > v->reserved) {
        // Only relocation needs to translate a source in the old allocation.
        const usize offset = (usize)vals - (usize)v->p;
        const bool alias = v->p && offset < sz * elem_sz;
        assert(!alias || n * elem_sz <= sz * elem_sz - offset);
        xvec_reserve(v, sz + n);
        if (alias)
            vals = (u8 *)v->p + offset;
    }
    xmemmv((u8 *)v->p + sz * elem_sz, vals, n * elem_sz);
    v->sz = sz + n;
}

static inline XVEC_ALWAYS_INLINE void xvec_append(XVec *v, const void *val) {
    xvec_appendn(v, val, 1);
}

static inline XVEC_ALWAYS_INLINE void xvec_pop(XVec *v) {
    assert(v->sz);
    --v->sz;
}

XVEC_EXTERN_INLINE void xvec_insertn(XVec *v, usize idx, const void *vals,
                                     usize n) {
    assert(idx <= v->sz);
    if (!n)
        return;
    assert(n <= (usize)-1 / v->elem_sz - v->sz);
    const usize offset = (usize)vals - (usize)v->p;
    if (v->p && offset < v->sz * v->elem_sz) {
        assert(n * v->elem_sz <= v->sz * v->elem_sz - offset);
        const XVecStorage storage = xvec_insert_alias(
            v->p, v->reserved, v->elem_sz, v->sz, idx, vals, n);
        v->p = storage.p;
        v->reserved = storage.reserved;
        v->sz += n;
        return;
    }
    xvec_reserve(v, v->sz + n);
    u8 *at = (u8 *)v->p + idx * v->elem_sz;
    xmemmv(at + n * v->elem_sz, at, (v->sz - idx) * v->elem_sz);
    xmemcp(at, (void *)vals, n * v->elem_sz);
    v->sz += n;
}

XVEC_EXTERN_INLINE void xvec_insert(XVec *v, usize idx, const void *val) {
    xvec_insertn(v, idx, val, 1);
}

XVEC_EXTERN_INLINE void xvec_removen(XVec *v, usize begin, usize end) {
    assert(begin <= end && end <= v->sz);
    if (begin == end)
        return;
    xmemmv((u8 *)v->p + begin * v->elem_sz, (u8 *)v->p + end * v->elem_sz,
           (v->sz - end) * v->elem_sz);
    v->sz -= end - begin;
}

XVEC_EXTERN_INLINE void xvec_remove(XVec *v, usize idx) {
    assert(idx < v->sz);
    xvec_removen(v, idx, idx + 1);
}

static inline XVEC_ALWAYS_INLINE void *xvec_at(XVec *v, usize idx) {
    assert(idx < v->sz);
    return (u8 *)v->p + idx * v->elem_sz;
}

XVEC_EXTERN_INLINE void xvec_free(XVec *v) {
    if (v->p)
        xfree(v->p, v->reserved * v->elem_sz);
    xvec_init(v, v->elem_sz);
}
