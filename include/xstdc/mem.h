#pragma once

#include "types.h"

// Copy n bytes; source and destination must not overlap.
static inline void xmemcp(void *restrict dst, const void *restrict src,
                          usize n);

// Copy n bytes, allowing overlapping source and destination.
static inline void xmemmv(void *dst, const void *src, usize n);

// Return whether the first n bytes are equal.
static inline bool xmemeq(const void *a, const void *b, usize n);

// Set n bytes to zero.
static inline void xmemzero(void *dst, usize n);

// Definitions

static inline void xmemcp(void *restrict dst, const void *restrict src,
                          usize n) {
    __builtin_memcpy(dst, src, n);
}

static inline void xmemmv(void *dst, const void *src, usize n) {
    __builtin_memmove(dst, src, n);
}

static inline bool xmemeq(const void *a, const void *b, usize n) {
    return __builtin_memcmp(a, b, n) == 0;
}

static inline void xmemzero(void *dst, usize n) { __builtin_memset(dst, 0, n); }
