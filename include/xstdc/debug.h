#pragma once

#include "string.h"
#include "types.h"

// Traces own their storage; release once with the matching free. Capturing and
// symbolizing may allocate; allocator instrumentation must suppress recursion.

// Frames run from the caller outward; inlined/tail-called frames may be absent.
// On ELF, link with -rdynamic for executable symbols. Not safe in signal
// handlers.

// A return address and its owned, non-NUL-terminated symbol description.
// Descriptions include the module, symbol and offset when available; otherwise
// libc provides an address description. symbol is NULL until symbolized.
typedef struct {
    void *address;
    XString symbol;
} XBacktraceFrame;

// Arbitrary-depth backtrace with optional symbol names.
typedef struct {
    XBacktraceFrame *frames;
    usize len;
} XFullBacktrace;

// Depth-2 backtrace with optional symbol names.
// len may be less than 2 if fewer callers can be unwound.
typedef struct {
    XBacktraceFrame frames[2];
    usize len;
} XBacktrace;

// Capture addresses starting at the caller; release with xbacktrace_full_free.
XFullBacktrace xbacktrace_full(void);
// Capture up to two caller addresses without resolving symbols.
XBacktrace xbacktrace(void);

// Resolve missing symbols, retaining existing ones; safe to call repeatedly.
void xbacktrace_symbolize(XBacktrace *trace);
// Resolve missing symbols, retaining existing ones; safe to call repeatedly.
void xbacktrace_full_symbolize(XFullBacktrace *trace);

// Free owned storage and clear the trace. A zero-initialized trace is valid.
void xbacktrace_full_free(XFullBacktrace *trace);
// Free owned symbols and clear the trace; a zero-initialized trace is valid.
void xbacktrace_free(XBacktrace *trace);

// Print symbols or unresolved addresses to stderr without consuming the trace.
void xbacktrace_print(const XBacktrace *trace);
// Print symbols or unresolved addresses to stderr without consuming the trace.
void xbacktrace_full_print(const XFullBacktrace *trace);
