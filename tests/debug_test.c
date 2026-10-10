#include "xstdc/assert.h"
#include "xstdc/debug.h"
#include "xstdc/mem.h"
#include "xstdc/print.h"

static bool contains(XString text, const char *name) {
    const usize length = __builtin_strlen(name);
    for (usize i = 0; i + length <= text.len; ++i)
        if (xmemeq(text.p + i, name, length))
            return true;
    return false;
}
// Exported, non-inlined calls with work after capture preserve test frames,
// even under -O3. No global compiler optimization settings need to change.
__attribute__((noinline)) XBacktrace debug_test_short_leaf(void) {
    XBacktrace trace = xbacktrace();
    __asm__ volatile("" : : "g"(trace.len) : "memory");
    return trace;
}
__attribute__((noinline)) XBacktrace debug_test_short_parent(void) {
    XBacktrace trace = debug_test_short_leaf();
    __asm__ volatile("" : : "g"(trace.len) : "memory");
    return trace;
}
__attribute__((noinline)) XFullBacktrace debug_test_full_leaf(void) {
    XFullBacktrace trace = xbacktrace_full();
    __asm__ volatile("" : : "g"(trace.len) : "memory");
    return trace;
}
__attribute__((noinline)) XFullBacktrace debug_test_deep(unsigned depth) {
    XFullBacktrace trace =
        depth ? debug_test_deep(depth - 1) : debug_test_full_leaf();
    __asm__ volatile("" : : "g"(trace.len) : "memory");
    return trace;
}
int main(void) {
    XBacktrace empty = {0};
    XFullBacktrace empty_full = {0};
    xbacktrace_symbolize(&empty);
    xbacktrace_full_symbolize(&empty_full);
    xbacktrace_free(&empty);
    xbacktrace_full_free(&empty_full);
    for (unsigned run = 0; run < 4; ++run) {
        XBacktrace short_trace = debug_test_short_parent();
        assert(short_trace.len == 2);
        assert(short_trace.frames[0].address && short_trace.frames[1].address);
        for (usize i = 0; i < short_trace.len; ++i)
            assert(!short_trace.frames[i].symbol.p &&
                   !short_trace.frames[i].symbol.len);
        void *short_address = short_trace.frames[0].address;
        xbacktrace_symbolize(&short_trace);
        assert(short_trace.frames[0].address == short_address);
        char *short_symbol = short_trace.frames[0].symbol.p;
        xbacktrace_symbolize(&short_trace);
        assert(short_trace.frames[0].symbol.p == short_symbol);
        assert(contains(short_trace.frames[0].symbol, "debug_test_short_leaf"));
        assert(
            contains(short_trace.frames[1].symbol, "debug_test_short_parent"));
        // Force multiple growth steps and verify self-frame exclusion/order.
        XFullBacktrace full = debug_test_deep(160);
        assert(full.len >= 162);
        for (usize i = 0; i < full.len; ++i)
            assert(full.frames[i].address && !full.frames[i].symbol.p &&
                   !full.frames[i].symbol.len);
        void *full_address = full.frames[0].address;
        xbacktrace_full_symbolize(&full);
        assert(full.frames[0].address == full_address);
        char *full_symbol = full.frames[0].symbol.p;
        xbacktrace_full_symbolize(&full);
        assert(full.frames[0].symbol.p == full_symbol);
        assert(contains(full.frames[0].symbol, "debug_test_full_leaf"));
        for (usize i = 1; i <= 161; ++i) {
            assert(full.frames[i].address);
            assert(contains(full.frames[i].symbol, "debug_test_deep"));
        }
        // Symbol descriptions are ordinary XStrings, suitable for xprint.
        char buffer[1024];
        XStringVw printed =
            xprint_buf(buffer, sizeof buffer, "%s", &full.frames[0].symbol);
        assert(printed.p && printed.len == full.frames[0].symbol.len);
        assert(xmemeq(printed.p, full.frames[0].symbol.p, printed.len));
        xbacktrace_free(&short_trace);
        xbacktrace_full_free(&full);
        assert(!short_trace.len && !short_trace.frames[0].symbol.p);
        assert(!full.len && !full.frames);
        xbacktrace_free(&short_trace);
        xbacktrace_full_free(&full);
    }
    // Raw captures own no symbols and can be freed without symbolization.
    XBacktrace raw = debug_test_short_parent();
    XFullBacktrace raw_full = debug_test_deep(0);
    xbacktrace_free(&raw);
    xbacktrace_full_free(&raw_full);
    xprint("debug tests passed\n");
}
