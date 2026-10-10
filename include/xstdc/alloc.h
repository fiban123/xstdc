#pragma once

#include "xstdc/types.h"

// Allocate n bytes; exit on failure.
void *xalloc(usize n);
// Allocate n bytes, aligned to align; exit on failure.
void *xalloca(usize n, usize align);
// Allocate n bytes; exit on failure.
void *xallocm(usize n);
// Allocate n bytes, aligned to align; exit on failure.
void *xallocma(usize n, usize align);

// Allocate n zeroed bytes; exit on failure.
void *xallocz(usize n);
// Allocate n zeroed bytes, aligned to align; exit on failure.
void *xallocza(usize n, usize align);
// Allocate n zeroed bytes; exit on failure.
void *xalloczm(usize n);
// Allocate n zeroed bytes, aligned to align; exit on failure.
void *xalloczma(usize n, usize align);

// Resize to new bytes; exit on failure.
void *xrealloc(void *mem, usize old, usize new);
// Resize to new bytes, aligned to align; exit on failure.
void *xrealloca(void *mem, usize old, usize new, usize align);
// Resize to new bytes; exit on failure.
void *xreallocm(void *mem, usize new);
// Resize to new bytes, aligned to align; exit on failure.
void *xreallocma(void *mem, usize new, usize align);

// Try to allocate n bytes. Return NULL on failure.
void *xtry_alloc(usize n);
// Try to allocate n bytes, aligned to align. Return NULL on failure.
void *xtry_alloca(usize n, usize align);
// Try to allocate n bytes. Return NULL on failure.
void *xtry_allocm(usize n);
// Try to allocate n bytes, aligned to align. Return NULL on failure.
void *xtry_allocma(usize n, usize align);

// Try to allocate n zeroed bytes. Return NULL on failure.
void *xtry_allocz(usize n);
// Try to allocate n zeroed bytes, aligned to align. Return NULL on failure.
void *xtry_allocza(usize n, usize align);
// Try to allocate n zeroed bytes. Return NULL on failure.
void *xtry_alloczm(usize n);
// Try to allocate n zeroed bytes, aligned to align. Return NULL on failure.
void *xtry_alloczma(usize n, usize align);

// Resize to new bytes. Return NULL on failure, keeping the old allocation.
void *xtry_realloc(void *mem, usize old, usize new);
// Resize to new bytes, aligned to align. Return NULL on failure, keeping the
// old allocation.
void *xtry_realloca(void *mem, usize old, usize new, usize align);
// Resize to new bytes. Return NULL on failure, keeping the old allocation.
void *xtry_reallocm(void *mem, usize new);
// Resize to new bytes, aligned to align. Return NULL on failure, keeping the
// old allocation.
void *xtry_reallocma(void *mem, usize new, usize align);

// Free mem using the exact allocated size n; NULL is allowed.
void xfree(void *mem, usize n);
// Free mem using the exact allocated size n and alignment align; NULL is
// allowed.
void xfreea(void *mem, usize n, usize align);
// Free mem using its stored size; NULL is allowed.
void xfreem(void *mem);
// Free mem using its stored size and alignment align; NULL is allowed.
void xfreema(void *mem, usize align);

// Print live allocations and their last valid operation backtraces to stderr.
// Silent without debug allocation tracking or when no allocations remain.
void xalloc_debug_unfreed();

// Return whether this library was built with debug allocation tracking.
bool xalloc_has_debug();
