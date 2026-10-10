#include "xstdc/assert.h"

#include "xstdc/print.h"
#include "xstdc/process.h"

void xstdc_assert_fail(u64 line, const char *file, const char *func) {
    xprint("Assertion failed in %z - %z:%u64\n", file, func, &line);
    xexit(1);
}

void xstdc_unreachable_reached(u64 line, const char *file, const char *func) {
    xprint("Unreachable code reached in %z - %z:%u64\n", file, func, &line);
    xexit(1);
}
