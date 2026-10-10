#include "xstdc/debug.h"
#include "xstdc/alloc.h"
#include "xstdc/assert.h"
#include "xstdc/print.h"

#include <execinfo.h>
#include <limits.h>
#include <stdlib.h>

static int caller_index(void **addresses, int count, void *caller) {
    // Locate our caller explicitly: an instrumented backtrace implementation
    // can insert an additional frame ahead of the capture function.
    for (int i = 0; i < count; ++i)
        if (addresses[i] == caller)
            return i;
    return count;
}

static void symbolize(XBacktraceFrame *frames, usize count) {
    // Resolve bounded batches without a temporary heap allocation.
    for (usize first = 0; first < count;) {
        void *addresses[64];
        XBacktraceFrame *missing[64];
        int len = 0;
        while (first < count && len < 64) {
            XBacktraceFrame *frame = &frames[first++];
            if (!frame->symbol.p) {
                addresses[len] = frame->address;
                missing[len++] = frame;
            }
        }
        if (!len)
            continue;
        char **symbols = backtrace_symbols(addresses, len);
        if (!symbols)
            return;
        for (int i = 0; i < len; ++i)
            missing[i]->symbol = xstr_fromz(symbols[i]);
        // backtrace_symbols uses libc's allocator, not xalloc.
        free(symbols);
    }
}

void xbacktrace_symbolize(XBacktrace *trace) {
    assert(trace && trace->len <= 2);
    symbolize(trace->frames, trace->len);
}
void xbacktrace_full_symbolize(XFullBacktrace *trace) {
    assert(trace && (trace->frames || !trace->len));
    symbolize(trace->frames, trace->len);
}

__attribute__((noinline)) XFullBacktrace xbacktrace_full(void) {
    int capacity = 64;
    void *local[64];
    void **addresses = local;
    int count;
    for (;;) {
        count = backtrace(addresses, capacity);
        if (count < capacity)
            break;
        if (capacity > INT_MAX / 2) {
            if (addresses != local)
                xfree(addresses, (usize)capacity * sizeof(*addresses));
            xstdc_assert_fail(__LINE__, __FILE__, __func__);
            return (XFullBacktrace){0};
        }
        const int next = capacity * 2;
        addresses =
            addresses == local
                ? xalloc((usize)next * sizeof(*addresses))
                : xrealloc(addresses, (usize)capacity * sizeof(*addresses),
                           (usize)next * sizeof(*addresses));
        capacity = next;
    }
    const int first = caller_index(
        addresses, count,
        __builtin_extract_return_addr(__builtin_return_address(0)));
    XFullBacktrace trace = {0};
    if (count > first) {
        trace.len = (usize)(count - first);
        trace.frames = xalloc(trace.len * sizeof(*trace.frames));
        for (usize i = 0; i < trace.len; ++i)
            trace.frames[i] =
                (XBacktraceFrame){.address = addresses[first + i]};
    }
    if (addresses != local)
        xfree(addresses, (usize)capacity * sizeof(*addresses));
    return trace;
}

__attribute__((noinline)) XBacktrace xbacktrace(void) {
    void *addresses[4];
    const int count = backtrace(addresses, 4);
    const int first = caller_index(
        addresses, count,
        __builtin_extract_return_addr(__builtin_return_address(0)));
    XBacktrace trace = {0};
    if (count > first) {
        trace.len = count - first > 2 ? 2 : (usize)(count - first);
        for (usize i = 0; i < trace.len; ++i)
            trace.frames[i].address = addresses[first + i];
    }
    return trace;
}

static void free_frames(XBacktraceFrame *frames, usize count) {
    for (usize i = 0; i < count; ++i)
        if (frames[i].symbol.p)
            xfree(frames[i].symbol.p, frames[i].symbol.len);
}
void xbacktrace_full_free(XFullBacktrace *trace) {
    assert(trace);
    free_frames(trace->frames, trace->len);
    if (trace->frames)
        xfree(trace->frames, trace->len * sizeof(*trace->frames));
    *trace = (XFullBacktrace){0};
}
void xbacktrace_free(XBacktrace *trace) {
    assert(trace && trace->len <= 2);
    free_frames(trace->frames, trace->len);
    *trace = (XBacktrace){0};
}

static void print_frames(const XBacktraceFrame *frames, usize count) {
    for (usize i = 0; i < count; ++i) {
        if (frames[i].symbol.p)
            xprint_err("%s\n", &frames[i].symbol);
        else {
            const usize address = (usize)frames[i].address;
            xprint_err("Unknown - 0x%usz(x)\n", &address);
        }
    }
}
void xbacktrace_print(const XBacktrace *trace) {
    assert(trace && trace->len <= 2);
    print_frames(trace->frames, trace->len);
}
void xbacktrace_full_print(const XFullBacktrace *trace) {
    assert(trace && (trace->frames || !trace->len));
    print_frames(trace->frames, trace->len);
}
