#pragma once

#include "alloc.h"
#include "assert.h"
#include "mem.h"
#include "types.h"

#ifndef XSTRING_EXTERN_INLINE
#define XSTRING_EXTERN_INLINE inline
#endif

#ifndef XSTRING_NO_SHORT_NAMES
#define stov(s) xstrv_froms(s)
#define strl(s) (XStringVw){.p = s, .len = sizeof(s) - 1}
#endif

#if defined(__OPTIMIZE__) && !defined(XSTRING_NO_ALWAYS_INLINE)
#define XSTRING_ALWAYS_INLINE __attribute__((always_inline))
#else
#define XSTRING_ALWAYS_INLINE
#endif

// Strings are *not* null terminated.
// A String is NULL if its pointer is nullptr
// XString owns its bytes; release with xfree(str.p, str.len) when p is
// non-NULL. XStringVw borrows bytes and must not outlive their storage. Empty
// owned strings use xalloc(0), just like other allocations, and must also be
// freed.
typedef struct {
    char *p;
    usize len;
} XString;

typedef struct {
    const char *p;
    usize len;
} XStringVw;

// Borrows bytes up to the first NUL; NULL produces a null, empty view.
XSTRING_EXTERN_INLINE XStringVw xstrv_fromz(const char *zstr);

// Copies bytes up to the first NUL; NULL produces a null string.
XSTRING_EXTERN_INLINE XString xstr_fromz(const char *zstr);

// Return a borrowed view of the string bytes.
static inline XSTRING_ALWAYS_INLINE XStringVw xstrv_froms(XString str);

// Compares length and bytes, including embedded NULs.
XSTRING_EXTERN_INLINE bool xstrv_eq(XStringVw a, XStringVw b);

// Return whether text begins with prefix; an empty prefix always matches.
XSTRING_EXTERN_INLINE bool xstrv_starts_with(XStringVw text, XStringVw prefix);
// Return whether text ends with suffix; an empty suffix always matches.
XSTRING_EXTERN_INLINE bool xstrv_end_with(XStringVw text, XStringVw suffix);

// Allocates an independent copy of both views, without a trailing NUL.
XSTRING_EXTERN_INLINE XString xstrv_concat(XStringVw a, XStringVw b);

// Parsers consume a decimal prefix from byte zero, without skipping whitespace.
// Optional '+' is accepted; '-' is accepted only for signed integers/floats.
// Floats accept a decimal point and an optional complete e/E exponent.
// Return bytes consumed, including sign, decimal point and complete exponent.
// At least one digit is required. Overflow/no number returns zero unchanged;
// float underflow is allowed. out must point to writable storage.
// Parse a decimal prefix into u8; leave out unchanged on failure.
XSTRING_EXTERN_INLINE usize u8parse(XStringVw text, u8 *out);
// Parse a decimal prefix into i8; leave out unchanged on failure.
XSTRING_EXTERN_INLINE usize i8parse(XStringVw text, i8 *out);
// Parse a decimal prefix into u16; leave out unchanged on failure.
XSTRING_EXTERN_INLINE usize u16parse(XStringVw text, u16 *out);
// Parse a decimal prefix into i16; leave out unchanged on failure.
XSTRING_EXTERN_INLINE usize i16parse(XStringVw text, i16 *out);
// Parse a decimal prefix into u32; leave out unchanged on failure.
XSTRING_EXTERN_INLINE usize u32parse(XStringVw text, u32 *out);
// Parse a decimal prefix into i32; leave out unchanged on failure.
XSTRING_EXTERN_INLINE usize i32parse(XStringVw text, i32 *out);
// Parse a decimal prefix into u64; leave out unchanged on failure.
XSTRING_EXTERN_INLINE usize u64parse(XStringVw text, u64 *out);
// Parse a decimal prefix into i64; leave out unchanged on failure.
XSTRING_EXTERN_INLINE usize i64parse(XStringVw text, i64 *out);
// Parse a decimal prefix into u128; leave out unchanged on failure.
XSTRING_EXTERN_INLINE usize u128parse(XStringVw text, u128 *out);
// Parse a decimal prefix into i128; leave out unchanged on failure.
XSTRING_EXTERN_INLINE usize i128parse(XStringVw text, i128 *out);
// Parse a decimal prefix into usize; leave out unchanged on failure.
XSTRING_EXTERN_INLINE usize usizeparse(XStringVw text, usize *out);
// Parse a decimal prefix into ssize; leave out unchanged on failure.
XSTRING_EXTERN_INLINE usize ssizeparse(XStringVw text, ssize *out);
// Parse a decimal float prefix; underflow succeeds, overflow leaves out
// unchanged.
XSTRING_EXTERN_INLINE usize f32parse(XStringVw text, f32 *out);
// Parse a decimal float prefix; underflow succeeds, overflow leaves out
// unchanged.
XSTRING_EXTERN_INLINE usize f64parse(XStringVw text, f64 *out);

// Parse true/false/1/0 prefixes, case-sensitively, without skipping whitespace.
// Return bytes consumed, or zero on failure with out unchanged.
XSTRING_EXTERN_INLINE usize xboolparse(XStringVw text, bool *out);

// Parse a magnitude with separate sign-dependent limits; zero disables minus.
static inline usize xstr_parse_magnitude(XStringVw text, u128 positive_limit,
                                         u128 negative_limit, u128 *out,
                                         bool *negative);
// Find the valid decimal float prefix, including only a complete exponent.
static inline usize xstr_float_prefix(XStringVw text);
// Convert a validated prefix using a bounded copy and the C numeric locale.
bool xstr_parse_float(XStringVw text, bool single, void *out);

// Definitions

XSTRING_EXTERN_INLINE XStringVw xstrv_fromz(const char *zstr) {
    return (XStringVw){zstr, zstr ? __builtin_strlen(zstr) : 0};
}

XSTRING_EXTERN_INLINE XString xstr_fromz(const char *zstr) {
    if (!zstr)
        return (XString){0};
    const usize len = __builtin_strlen(zstr);
    char *const p = xalloc(len);
    if (len)
        xmemcp(p, (void *)zstr, len);
    return (XString){p, len};
}

static inline XSTRING_ALWAYS_INLINE XStringVw xstrv_froms(XString str) {
    return (XStringVw){str.p, str.len};
}

XSTRING_EXTERN_INLINE bool xstrv_eq(XStringVw a, XStringVw b) {
    return a.len == b.len && xmemeq(a.p, b.p, a.len);
}

XSTRING_EXTERN_INLINE bool xstrv_starts_with(XStringVw text, XStringVw prefix) {
    return !prefix.len ||
           (prefix.len <= text.len && xmemeq(text.p, prefix.p, prefix.len));
}

XSTRING_EXTERN_INLINE bool xstrv_end_with(XStringVw text, XStringVw suffix) {
    return !suffix.len ||
           (suffix.len <= text.len &&
            xmemeq(text.p + text.len - suffix.len, suffix.p, suffix.len));
}

XSTRING_EXTERN_INLINE XString xstrv_concat(XStringVw a, XStringVw b) {
    assert(b.len <= (usize)-1 - a.len);
    const usize len = a.len + b.len;
    char *const p = xalloc(len);
    if (a.len)
        xmemcp(p, (void *)a.p, a.len);
    if (b.len)
        xmemcp(p + a.len, (void *)b.p, b.len);
    return (XString){p, len};
}

static inline usize xstr_parse_magnitude(XStringVw text, u128 positive_limit,
                                         u128 negative_limit, u128 *out,
                                         bool *negative) {
    usize i = 0;
    *negative = false;
    if (i < text.len && (text.p[i] == '+' || text.p[i] == '-')) {
        *negative = text.p[i++] == '-';
        if (*negative && !negative_limit)
            return 0;
    }
    const usize first = i;
    const u128 limit = *negative ? negative_limit : positive_limit;
    const u128 cutoff = limit / 10;
    const unsigned last = limit % 10;
    u128 value = 0;
    for (; i < text.len; ++i) {
        const unsigned digit = (unsigned)(unsigned char)text.p[i] - '0';
        if (digit > 9)
            break;
        if (value > cutoff || (value == cutoff && digit > last))
            return 0;
        value = value * 10 + digit;
    }
    if (i == first)
        return 0;
    *out = value;
    return i;
}

static inline usize xstr_float_prefix(XStringVw text) {
    usize i = 0;
    if (i < text.len && (text.p[i] == '+' || text.p[i] == '-'))
        ++i;
    bool digits = false;
    while (i < text.len && (unsigned)(unsigned char)text.p[i] - '0' <= 9) {
        digits = true;
        ++i;
    }
    if (i < text.len && text.p[i] == '.') {
        ++i;
        while (i < text.len && (unsigned)(unsigned char)text.p[i] - '0' <= 9) {
            digits = true;
            ++i;
        }
    }
    if (!digits)
        return 0;
    if (i < text.len && (text.p[i] == 'e' || text.p[i] == 'E')) {
        usize end = i + 1;
        if (end < text.len && (text.p[end] == '+' || text.p[end] == '-'))
            ++end;
        const usize first = end;
        while (end < text.len &&
               (unsigned)(unsigned char)text.p[end] - '0' <= 9)
            ++end;
        if (end != first)
            i = end;
    }
    return i;
}

XSTRING_EXTERN_INLINE usize u8parse(XStringVw text, u8 *out) {
    u128 value;
    bool negative;
    const usize length =
        xstr_parse_magnitude(text, (u8)-1, 0, &value, &negative);
    if (!length)
        return 0;
    *out = (u8)value;
    return length;
}

XSTRING_EXTERN_INLINE usize i8parse(XStringVw text, i8 *out) {
    const u128 limit = (u128)1 << 7;
    u128 value;
    bool negative;
    const usize length =
        xstr_parse_magnitude(text, limit - 1, limit, &value, &negative);
    if (!length)
        return 0;
    *out = negative && value ? -(i8)(value - 1) - 1 : (i8)value;
    return length;
}

XSTRING_EXTERN_INLINE usize u16parse(XStringVw text, u16 *out) {
    u128 value;
    bool negative;
    const usize length =
        xstr_parse_magnitude(text, (u16)-1, 0, &value, &negative);
    if (!length)
        return 0;
    *out = (u16)value;
    return length;
}

XSTRING_EXTERN_INLINE usize i16parse(XStringVw text, i16 *out) {
    const u128 limit = (u128)1 << 15;
    u128 value;
    bool negative;
    const usize length =
        xstr_parse_magnitude(text, limit - 1, limit, &value, &negative);
    if (!length)
        return 0;
    *out = negative && value ? -(i16)(value - 1) - 1 : (i16)value;
    return length;
}

XSTRING_EXTERN_INLINE usize u32parse(XStringVw text, u32 *out) {
    u128 value;
    bool negative;
    const usize length =
        xstr_parse_magnitude(text, (u32)-1, 0, &value, &negative);
    if (!length)
        return 0;
    *out = (u32)value;
    return length;
}

XSTRING_EXTERN_INLINE usize i32parse(XStringVw text, i32 *out) {
    const u128 limit = (u128)1 << 31;
    u128 value;
    bool negative;
    const usize length =
        xstr_parse_magnitude(text, limit - 1, limit, &value, &negative);
    if (!length)
        return 0;
    *out = negative && value ? -(i32)(value - 1) - 1 : (i32)value;
    return length;
}

XSTRING_EXTERN_INLINE usize u64parse(XStringVw text, u64 *out) {
    u128 value;
    bool negative;
    const usize length =
        xstr_parse_magnitude(text, (u64)-1, 0, &value, &negative);
    if (!length)
        return 0;
    *out = (u64)value;
    return length;
}

XSTRING_EXTERN_INLINE usize i64parse(XStringVw text, i64 *out) {
    const u128 limit = (u128)1 << 63;
    u128 value;
    bool negative;
    const usize length =
        xstr_parse_magnitude(text, limit - 1, limit, &value, &negative);
    if (!length)
        return 0;
    *out = negative && value ? -(i64)(value - 1) - 1 : (i64)value;
    return length;
}

XSTRING_EXTERN_INLINE usize u128parse(XStringVw text, u128 *out) {
    u128 value;
    bool negative;
    const usize length =
        xstr_parse_magnitude(text, (u128)-1, 0, &value, &negative);
    if (!length)
        return 0;
    *out = (u128)value;
    return length;
}

XSTRING_EXTERN_INLINE usize i128parse(XStringVw text, i128 *out) {
    const u128 limit = (u128)1 << 127;
    u128 value;
    bool negative;
    const usize length =
        xstr_parse_magnitude(text, limit - 1, limit, &value, &negative);
    if (!length)
        return 0;
    *out = negative && value ? -(i128)(value - 1) - 1 : (i128)value;
    return length;
}

XSTRING_EXTERN_INLINE usize usizeparse(XStringVw text, usize *out) {
    return u64parse(text, out);
}

XSTRING_EXTERN_INLINE usize ssizeparse(XStringVw text, ssize *out) {
    return i64parse(text, out);
}

XSTRING_EXTERN_INLINE usize xboolparse(XStringVw text, bool *out) {
    if (!text.len)
        return 0;
    if (text.p[0] == '1' || text.p[0] == '0') {
        *out = text.p[0] == '1';
        return 1;
    }
    if (text.len >= 4 && xmemeq(text.p, "true", 4)) {
        *out = true;
        return 4;
    }
    if (text.len >= 5 && xmemeq(text.p, "false", 5)) {
        *out = false;
        return 5;
    }
    return 0;
}

XSTRING_EXTERN_INLINE usize f32parse(XStringVw text, f32 *out) {
    const usize length = xstr_float_prefix(text);
    return length && xstr_parse_float((XStringVw){text.p, length}, true, out)
               ? length
               : 0;
}

XSTRING_EXTERN_INLINE usize f64parse(XStringVw text, f64 *out) {
    const usize length = xstr_float_prefix(text);
    return length && xstr_parse_float((XStringVw){text.p, length}, false, out)
               ? length
               : 0;
}
