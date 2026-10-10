#define _GNU_SOURCE
#include <errno.h>
#include <locale.h>
#include <stdlib.h>

#define XSTRING_EXTERN_INLINE
#include "xstdc/string.h"

bool xstr_parse_float(XStringVw text, bool single, void *out) {
    if (text.len == (usize)-1)
        return false;
    char local[256];
    char *buffer = text.len < sizeof(local) ? local : xalloc(text.len + 1);
    xmemcp(buffer, (void *)text.p, text.len);
    buffer[text.len] = 0;
    locale_t locale = newlocale(LC_NUMERIC_MASK, "C", (locale_t)0);
    if (!locale) {
        if (buffer != local)
            xfree(buffer, text.len + 1);
        return false;
    }
    const int saved_errno = errno;
    errno = 0;
    bool success;
    if (single) {
        const f32 value = strtof_l(buffer, 0, locale);
        success = __builtin_isfinite(value) &&
                  !(errno == ERANGE && __builtin_fabsf(value) >= 1);
        if (success)
            *(f32 *)out = value;
    } else {
        const f64 value = strtod_l(buffer, 0, locale);
        success = __builtin_isfinite(value) &&
                  !(errno == ERANGE && __builtin_fabs(value) >= 1);
        if (success)
            *(f64 *)out = value;
    }
    errno = saved_errno;
    freelocale(locale);
    if (buffer != local)
        xfree(buffer, text.len + 1);
    return success;
}
