#pragma once

#include "xstdc/types.h"

// Round capacity up by doubling to chunk_bytes, then adding whole-element
// chunks. Use the exact requested size if rounding would overflow.
static inline usize storage_capacity(usize capacity, usize elem_sz, usize n,
                                     usize chunk_bytes);

// Definitions

static inline usize storage_capacity(usize capacity, usize elem_sz, usize n,
                                     usize chunk_bytes) {
    const usize limit = (usize)-1 / elem_sz;
    const usize chunk = chunk_bytes / elem_sz ? chunk_bytes / elem_sz : 1;
    while (capacity < n && capacity < chunk) {
        capacity = capacity > chunk / 2 ? chunk : capacity * 2;
    }
    if (capacity < n) {
        const usize missing = n - capacity;
        const usize steps = missing / chunk + (missing % chunk != 0);
        if (steps > (limit - capacity) / chunk)
            return n;
        capacity += steps * chunk;
    }
    return capacity;
}
