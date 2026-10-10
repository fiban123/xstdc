#pragma once

#include "types.h"

// Report a failed assertion at file:line in func and terminate.
void xstdc_assert_fail(u64 line, const char *file, const char *func);
// Report an unreachable location at file:line in func and terminate.
void xstdc_unreachable_reached(u64 line, const char *file, const char *func);

#if defined(__OPTIMIZE__) && !defined(XSTDC_ALWAYS_ASSERT)
#define XSTDC_ASSERT_OPTIMIZE
#endif

#ifdef XSTDC_ASSERT_OPTIMIZE
#define assert(x)                                                              \
    if (!(x))                                                                  \
        __builtin_unreachable();
#else
#define assert(x)                                                              \
    if (!(x))                                                                  \
        xstdc_assert_fail(__LINE__, __FILE__, __func__);
#endif

#ifdef XSTDC_ASSERT_OPTIMIZE
#define unreachable() __builtin_unreachable()
#else
#define unreachable() xstdc_unreachable_reached(__LINE__, __FILE__, __func__);
#endif
