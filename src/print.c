#include "xstdc/print.h"
#include "xstdc/alloc.h"
#include "xstdc/mem.h"
#include <fenv.h>
#include <limits.h>
#include <locale.h>
#include <stdarg.h>
#include <stdio.h>

// Public implementations intentionally live only in this translation unit.
typedef struct {
    char *p;
    usize cap, total;
    bool error;
} Output;
typedef struct {
    bool hex, left, right, truncate, round;
    char lp, rp;
    usize extend, limit, precision;
} Attributes;
typedef enum {
    UINT,
    SINT,
    FLOAT,
    STRING,
    ZERO_STRING,
    CHARACTER,
    BOOLEAN
} Kind;
typedef struct {
    unsigned bytes;
    Kind kind;
} Spec;
static bool specifier(const char **format, Spec *spec) {
    const char *p = *format;
    switch (*p++) {
    case 'u':
    case 'i':
        spec->kind = **format == 'u' ? UINT : SINT;
        if (*p == '8') {
            spec->bytes = 1;
            ++p;
        } else if (p[0] == '1' && p[1] == '6') {
            spec->bytes = 2;
            p += 2;
        } else if (p[0] == '3' && p[1] == '2') {
            spec->bytes = 4;
            p += 2;
        } else if (p[0] == '6' && p[1] == '4') {
            spec->bytes = 8;
            p += 2;
        } else if (p[0] == '1' && p[1] == '2' && p[2] == '8') {
            spec->bytes = 16;
            p += 3;
        } else if (p[0] == 's' && p[1] == 'z') {
            spec->bytes = sizeof(usize);
            p += 2;
        } else
            return false;
        break;
    case 'f':
        spec->kind = FLOAT;
        if (p[0] == '3' && p[1] == '2') {
            spec->bytes = 4;
            p += 2;
        } else if (p[0] == '6' && p[1] == '4') {
            spec->bytes = 8;
            p += 2;
        } else
            return false;
        break;
    case 's':
        spec->kind = STRING;
        spec->bytes = 0;
        break;
    case 'z':
        spec->kind = ZERO_STRING;
        spec->bytes = 0;
        break;
    case 'c':
        spec->kind = CHARACTER;
        spec->bytes = 1;
        break;
    case 'b':
        spec->kind = BOOLEAN;
        spec->bytes = 1;
        break;
    default:
        return false;
    }
    *format = p;
    return true;
}
static bool starts(const char *p, const char *text, unsigned n) {
    for (unsigned i = 0; i < n; ++i)
        if (!p[i] || p[i] != text[i])
            return false;
    return true;
}
static void spaces(const char **p) {
    while (**p == ' ' || **p == '\t' || **p == '\n' || **p == '\r')
        ++*p;
}
static bool number(const char **p, usize *n) {
    *n = 0;
    if (**p < '0' || **p > '9')
        return false;
    while (**p >= '0' && **p <= '9') {
        unsigned d = (unsigned)(*(*p)++ - '0');
        if (*n > ((usize)-1 - d) / 10)
            return false;
        *n = *n * 10 + d;
    }
    return true;
}
static bool padchar(const char **p, char *c) {
    if (**p == '\'' || **p == '"') {
        const char quote = *(*p)++;
        if (!**p || **p == quote)
            return false;
        *c = *(*p)++;
        if (*c == '\\') {
            switch (**p) {
            case 'n':
                *c = '\n';
                break;
            case 'r':
                *c = '\r';
                break;
            case 't':
                *c = '\t';
                break;
            case '\\':
            case '\'':
            case '"':
                *c = **p;
                break;
            default:
                return false;
            }
            ++*p;
        }
        if (**p != quote)
            return false;
        ++*p;
        return true;
    }
    if (!**p || **p == ',' || **p == ')')
        return false;
    *c = *(*p)++;
    return true;
}
static bool attributes(const char **p, Attributes *a) {
    *a = (Attributes){.lp = ' ', .rp = ' '};
    if (**p != '(')
        return true;
    ++*p;
    spaces(p);
    unsigned seen = 0;
    if (**p == ')') {
        ++*p;
        return true;
    }
    for (;;) {
        unsigned field;
        if (starts(*p, "lp", 2)) {
            field = 1;
            *p += 2;
        } else if (starts(*p, "rp", 2)) {
            field = 2;
            *p += 2;
        } else {
            switch (**p) {
            case 'x':
                field = 0;
                break;
            case 'e':
                field = 3;
                break;
            case 't':
                field = 4;
                break;
            case 'r':
                field = 5;
                break;
            default:
                return false;
            }
            ++*p;
        }
        if (seen & (1u << field))
            return false;
        seen |= 1u << field;
        spaces(p);
        if (!field)
            a->hex = true;
        else {
            if (**p != '=')
                return false;
            ++*p;
            spaces(p);
            if (field <= 2) {
                if (!padchar(p, field == 1 ? &a->lp : &a->rp))
                    return false;
                if (field == 1)
                    a->left = true;
                else
                    a->right = true;
            } else {
                usize value;
                if (!number(p, &value))
                    return false;
                if (field == 3)
                    a->extend = value;
                if (field == 4) {
                    a->truncate = true;
                    a->limit = value;
                }
                if (field == 5) {
                    a->round = true;
                    a->precision = value;
                }
            }
        }
        spaces(p);
        if (**p == ')') {
            ++*p;
            return true;
        }
        if (**p != ',')
            return false;
        ++*p;
        spaces(p);
    }
}
static usize room(Output *out, usize n) {
    if (n > (usize)-1 - out->total) {
        out->error = true;
        return 0;
    }
    const usize available = out->total < out->cap ? out->cap - out->total : 0;
    return n < available ? n : available;
}
static void emit(Output *out, const char *p, usize n) {
    const usize copy = room(out, n);
    if (out->error)
        return;
    if (copy)
        xmemcp(out->p + out->total, (void *)p, copy);
    out->total += n;
}
static void repeat(Output *out, char c, usize n) {
    const usize copy = room(out, n);
    if (out->error)
        return;
    for (usize i = 0; i < copy; ++i)
        out->p[out->total + i] = c;
    out->total += n;
}
static usize minimum(usize a, usize b) { return a < b ? a : b; }
static void field(Output *out, const char *p, usize n, bool hex, Attributes a) {
    if (hex && n > (usize)-1 / 2) {
        out->error = true;
        return;
    }
    const usize content = hex ? n * 2 : n;
    const usize length = content > a.extend ? content : a.extend;
    usize remaining = a.truncate ? minimum(length, a.limit) : length;
    const usize padding = length - content;
    const usize left = a.left && a.right ? padding / 2 : a.right ? 0 : padding;
    usize take = minimum(left, remaining);
    repeat(out, a.lp, take);
    remaining -= take;
    take = minimum(content, remaining);
    if (hex) {
        static const char digits[] = "0123456789abcdef";
        const usize copy = room(out, take);
        if (out->error)
            return;
        for (usize i = 0; i < copy; ++i) {
            const u8 byte = (u8)p[i / 2];
            out->p[out->total + i] = digits[i & 1 ? byte & 15 : byte >> 4];
        }
        out->total += take;
    } else
        emit(out, p, take);
    remaining -= take;
    repeat(out, a.rp, remaining);
}
static const char decimal_pairs[] = "0001020304050607080910111213141516171819"
                                    "2021222324252627282930313233343536373839"
                                    "4041424344454647484950515253545556575859"
                                    "6061626364656667686970717273747576777879"
                                    "8081828384858687888990919293949596979899";

// Write backwards, two decimal digits at a time. Ordinary integers never
// enter the 128-bit division path; hexadecimal needs only shifts and masks.
static usize integer(char *buf, const void *arg, unsigned bytes, bool sign,
                     bool hex, const char **begin) {
    u64 value = 0;
    bool negative;
    char *end = buf + 40, *p = end;
    if (bytes == 16) {
        u128 wide;
        xmemcp(&wide, (void *)arg, 16);
        negative = sign && (wide >> 127);
        if (negative)
            wide = (u128)0 - wide;
        if (hex) {
            do {
                *--p = "0123456789abcdef"[(unsigned)wide & 15];
                wide >>= 4;
            } while (wide);
            if (negative)
                *--p = '-';
            *begin = p;
            return (usize)(end - p);
        }
        while (wide > (u64)-1) {
            const u128 quotient = wide / 100;
            const unsigned remainder = (unsigned)(wide - quotient * 100);
            p -= 2;
            xmemcp(p, (void *)(decimal_pairs + remainder * 2), 2);
            wide = quotient;
        }
        value = (u64)wide;
    } else {
        switch (bytes) {
        case 1: {
            u8 v;
            xmemcp(&v, (void *)arg, 1);
            value = v;
            break;
        }
        case 2: {
            u16 v;
            xmemcp(&v, (void *)arg, 2);
            value = v;
            break;
        }
        case 4: {
            u32 v;
            xmemcp(&v, (void *)arg, 4);
            value = v;
            break;
        }
        case 8:
            xmemcp(&value, (void *)arg, 8);
            break;
        }
        const unsigned bits = bytes * 8;
        negative = sign && (value >> (bits - 1));
        if (negative) {
            value = (u64)0 - value;
            if (bits < 64)
                value &= ((u64)1 << bits) - 1;
        }
    }
    if (hex) {
        do {
            *--p = "0123456789abcdef"[value & 15];
            value >>= 4;
        } while (value);
    } else {
        while (value >= 100) {
            const u64 quotient = value / 100;
            const unsigned remainder = (unsigned)(value - quotient * 100);
            p -= 2;
            xmemcp(p, (void *)(decimal_pairs + remainder * 2), 2);
            value = quotient;
        }
        if (value >= 10) {
            p -= 2;
            xmemcp(p, (void *)(decimal_pairs + value * 2), 2);
        } else
            *--p = (char)('0' + value);
    }
    if (negative)
        *--p = '-';
    *begin = p;
    return (usize)(end - p);
}
// Exact base-10 expansion of a binary64 significand. Multipliers fit in u32
// and each base-1e9 limb product plus carry fits in u64.
typedef struct {
    u32 limb[128];
    unsigned used;
} Decimal;
static void decimal_multiply(Decimal *d, u32 factor) {
    u64 carry = 0;
    for (unsigned i = 0; i < d->used; ++i) {
        const u64 product = (u64)d->limb[i] * factor + carry;
        d->limb[i] = product % 1000000000;
        carry = product / 1000000000;
    }
    while (carry) {
        d->limb[d->used++] = (u32)(carry % 1000000000);
        carry /= 1000000000;
    }
}
static usize decimal_expand(char *digits, u64 mantissa, int exponent,
                            int *point) {
    if (!mantissa) {
        digits[0] = '0';
        *point = 1;
        return 1;
    }
    while (exponent < 0 && !(mantissa & 1)) {
        mantissa >>= 1;
        ++exponent;
    }
    Decimal d = {0};
    do {
        d.limb[d.used++] = mantissa % 1000000000;
        mantissa /= 1000000000;
    } while (mantissa);
    const int scale = exponent < 0 ? -exponent : 0;
    while (exponent > 0) {
        const unsigned step = exponent > 29 ? 29 : (unsigned)exponent;
        decimal_multiply(&d, 1u << step);
        exponent -= step;
    }
    while (exponent < 0) {
        const unsigned step = exponent < -13 ? 13 : (unsigned)-exponent;
        u32 power = 1;
        for (unsigned i = 0; i < step; ++i)
            power *= 5;
        decimal_multiply(&d, power);
        exponent += step;
    }
    char scratch[40];
    const char *p;
    usize n = integer(scratch, &d.limb[d.used - 1], 4, false, false, &p);
    xmemcp(digits, (void *)p, n);
    for (unsigned i = d.used - 1; i; --i) {
        u32 value = d.limb[i - 1];
        char *end = digits + n + 9;
        for (unsigned j = 0; j < 4; ++j) {
            const unsigned pair = value % 100;
            value /= 100;
            end -= 2;
            xmemcp(end, (void *)(decimal_pairs + pair * 2), 2);
        }
        *--end = (char)('0' + value);
        n += 9;
    }
    *point = (int)n - scale;
    return n;
}
static bool rounding_up(bool negative, int mode, bool discarded, int half,
                        bool odd) {
    if (!discarded)
        return false;
    if (mode == FE_UPWARD)
        return !negative;
    if (mode == FE_DOWNWARD)
        return negative;
    if (mode == FE_TOWARDZERO)
        return false;
    return half > 0 || (half == 0 && odd);
}
static void decimal_round(char *digits, usize *length, int *point, i64 keep,
                          bool negative, int mode) {
    const usize n = *length;
    if (keep >= (i64)n)
        return;
    const usize first = keep > 0 ? (usize)keep : 0;
    bool discarded = false, tail = false;
    for (usize i = first; i < n; ++i)
        discarded |= digits[i] != '0';
    for (usize i = first + 1; i < n; ++i)
        tail |= digits[i] != '0';
    int half = -1;
    if (keep >= 0)
        half = digits[first] > '5'   ? 1
               : digits[first] < '5' ? -1
               : tail                ? 1
                                     : 0;
    const bool up = rounding_up(negative, mode, discarded, half,
                                keep > 0 && ((digits[first - 1] - '0') & 1));
    if (keep <= 0) {
        digits[0] = up ? '1' : '0';
        *length = 1;
        *point = up ? *point - (int)keep + 1 : 1;
        return;
    }
    *length = (usize)keep;
    if (!up)
        return;
    usize i = (usize)keep;
    while (i && digits[i - 1] == '9')
        digits[--i] = '0';
    if (i)
        ++digits[i - 1];
    else {
        digits[0] = '1';
        ++*point;
    }
}
// For common precisions, round mantissa * 5^precision * 2^(exponent+precision)
// exactly in 128 bits, without ever multiplying floating-point values.
static bool fixed_scaled(u64 mantissa, int exponent, unsigned precision,
                         bool negative, int mode, u128 *result) {
    u128 value = mantissa;
    for (unsigned i = 0; i < precision; ++i)
        value *= 5;
    const int shift = exponent + (int)precision;
    if (shift >= 0) {
        if (shift >= 128 || value > ((u128)-1 >> shift))
            return false;
        *result = value << shift;
        return true;
    }
    const unsigned right = (unsigned)-shift;
    u128 quotient = 0, remainder = value;
    int half = -1;
    if (right < 128) {
        quotient = value >> right;
        remainder = value & (((u128)1 << right) - 1);
        const u128 midpoint = (u128)1 << (right - 1);
        half = remainder > midpoint ? 1 : remainder < midpoint ? -1 : 0;
    } else if (right == 128) {
        const u128 midpoint = (u128)1 << 127;
        half = remainder > midpoint ? 1 : remainder < midpoint ? -1 : 0;
    }
    *result = quotient + rounding_up(negative, mode, remainder != 0, half,
                                     (bool)(quotient & 1));
    return true;
}
static bool float_fixed_text(char *scratch, usize capacity, const char *digits,
                             usize count, int point, usize precision,
                             bool negative, const char **text, usize *length,
                             char **allocated) {
    const char *dot = precision ? localeconv()->decimal_point : "";
    const usize dotlen = __builtin_strlen(dot);
    const usize whole = point > 0 ? (usize)point : 1;
    const usize total = (usize)negative + whole + dotlen + precision;
    // Match the former snprintf backend's representable output length.
    if (total > INT_MAX)
        return false;
    char *out = total <= capacity ? scratch : xalloc(total);
    if (out != scratch)
        *allocated = out;
    *text = out;
    *length = total;
    if (negative)
        *out++ = '-';
    if (point > 0) {
        const usize take = minimum(count, (usize)point);
        xmemcp(out, (void *)digits, take);
        out += take;
        for (usize i = take; i < (usize)point; ++i)
            *out++ = '0';
    } else
        *out++ = '0';
    if (!precision)
        return true;
    xmemcp(out, (void *)dot, dotlen);
    out += dotlen;
    const usize leading = point < 0 ? minimum(precision, (usize)-point) : 0;
    for (usize i = 0; i < leading; ++i)
        *out++ = '0';
    precision -= leading;
    const usize start = point > 0 ? (usize)point : 0;
    const usize take = start < count ? minimum(precision, count - start) : 0;
    if (take) {
        xmemcp(out, (void *)(digits + start), take);
        out += take;
    }
    for (usize i = take; i < precision; ++i)
        *out++ = '0';
    return true;
}
static bool floating(char *scratch, usize capacity, double value, Attributes a,
                     const char **text, usize *length, char **allocated) {
    u64 bits;
    xmemcp(&bits, &value, 8);
    const bool negative = (bool)(bits >> 63);
    const unsigned encoded = (unsigned)(bits >> 52) & 2047;
    u64 mantissa = bits & 0xfffffffffffffull;
    if (a.precision > INT_MAX)
        return false;
    if (encoded == 2047) {
        char *out = scratch;
        if (negative)
            *out++ = '-';
        xmemcp(out, mantissa ? "nan" : "inf", 3);
        *text = scratch;
        *length = 3 + negative;
        return true;
    }
    const int exponent = encoded ? (int)encoded - 1023 - 52 : -1074;
    if (encoded)
        mantissa |= 1ull << 52;
    const int mode = fegetround();
    char digits[1100];
    usize n;
    int point;
    u128 scaled;
    if (a.round && a.precision <= 18 &&
        fixed_scaled(mantissa, exponent, (unsigned)a.precision, negative, mode,
                     &scaled)) {
        char small[40];
        const char *p;
        n = integer(small, &scaled, 16, false, false, &p);
        xmemcp(digits, (void *)p, n);
        point = (int)n - (int)a.precision;
    } else {
        n = decimal_expand(digits, mantissa, exponent, &point);
        decimal_round(digits, &n, &point,
                      a.round ? (i64)point + (i64)a.precision : 6, negative,
                      mode);
    }
    if (a.round)
        return float_fixed_text(scratch, capacity, digits, n, point,
                                a.precision, negative, text, length, allocated);
    // %g has six significant digits and removes trailing fractional zeros.
    while (n > 1 && digits[n - 1] == '0')
        --n;
    if (point > -4 && point <= 6) {
        const usize precision = (int)n > point ? (usize)((int)n - point) : 0;
        return float_fixed_text(scratch, capacity, digits, n, point, precision,
                                negative, text, length, allocated);
    }
    int exp = point - 1;
    const unsigned magnitude = (unsigned)(exp < 0 ? -exp : exp);
    const char *dot = n > 1 ? localeconv()->decimal_point : "";
    const usize dotlen = __builtin_strlen(dot);
    const usize total = negative + n + dotlen + 2 + (magnitude >= 100 ? 3 : 2);
    char *out = total <= capacity ? scratch : xalloc(total);
    if (out != scratch)
        *allocated = out;
    *text = out;
    *length = total;
    if (negative)
        *out++ = '-';
    *out++ = digits[0];
    if (n > 1) {
        xmemcp(out, (void *)dot, dotlen);
        out += dotlen;
        xmemcp(out, digits + 1, n - 1);
        out += n - 1;
    }
    *out++ = 'e';
    *out++ = exp < 0 ? '-' : '+';
    unsigned tail = magnitude;
    if (tail >= 100) {
        *out++ = (char)('0' + tail / 100);
        tail %= 100;
    }
    xmemcp(out, (void *)(decimal_pairs + 2 * tail), 2);
    return true;
}

static bool format(Output *out, const char *fmt, va_list args) {
    if (!fmt)
        return false;
    while (*fmt && !out->error) {
        const char *begin = fmt;
        while (*fmt && *fmt != '%')
            ++fmt;
        if (fmt != begin)
            emit(out, begin, (usize)(fmt - begin));
        if (!*fmt)
            break;
        ++fmt;
        if (*fmt == '%') {
            emit(out, "%", 1);
            ++fmt;
            continue;
        }
        Spec parsed;
        if (!specifier(&fmt, &parsed))
            return false;
        const Spec *spec = &parsed;
        Attributes a;
        if (!attributes(&fmt, &a) || (a.round && spec->kind != FLOAT) ||
            (a.hex && (spec->kind == FLOAT || spec->kind == BOOLEAN)))
            return false;
        const void *arg = va_arg(args, const void *);
        if (!arg)
            return false;
        char scratch[768];
        const char *p = scratch;
        usize n = 0;
        char *allocated = 0;
        usize allocation = 0;
        bool hexbytes = false;
        switch (spec->kind) {
        case BOOLEAN: {
            const bool value = *(const unsigned char *)arg != 0;
            p = value ? "true" : "false";
            n = value ? 4 : 5;
            break;
        }
        case UINT:
        case SINT:
            n = integer(scratch, arg, spec->bytes, spec->kind == SINT, a.hex,
                        &p);
            break;
        case CHARACTER:
            p = arg;
            n = 1;
            hexbytes = a.hex;
            break;
        case ZERO_STRING:
            p = arg;
            n = __builtin_strlen(p);
            hexbytes = a.hex;
            break;
        case STRING: {
            // Copy the common representation, avoiding aliasing XString as
            // XStringVw.
            XStringVw view;
            xmemcp(&view, (void *)arg, sizeof view);
            if (!view.p && view.len)
                return false;
            p = view.p;
            n = view.len;
            hexbytes = a.hex;
            break;
        }
        case FLOAT: {
            double value;
            if (spec->bytes == 4) {
                f32 v;
                xmemcp(&v, (void *)arg, 4);
                value = v;
            } else
                xmemcp(&value, (void *)arg, 8);
            if (!floating(scratch, sizeof scratch, value, a, &p, &n,
                          &allocated))
                return false;
            allocation = n;
            break;
        }
        }
        if (!a.extend && !a.truncate && !hexbytes)
            emit(out, p, n);
        else
            field(out, p, n, hexbytes, a);
        if (allocated)
            xfree(allocated, allocation);
    }
    return !out->error;
}
static XString make_string(const char *fmt, va_list args) {
    va_list copy;
    va_copy(copy, args);
    char local[512];
    Output count = {.p = local, .cap = sizeof local};
    const bool valid = format(&count, fmt, copy);
    va_end(copy);
    if (!valid)
        return (XString){0};
    char *p = xalloc(count.total);
    if (count.total <= sizeof local) {
        if (count.total)
            xmemcp(p, local, count.total);
        return (XString){p, count.total};
    }
    Output out = {.p = p, .cap = count.total};
    va_copy(copy, args);
    const bool success = format(&out, fmt, copy);
    va_end(copy);
    if (!success || out.total != count.total) {
        xfree(p, count.total);
        return (XString){0};
    }
    return (XString){p, count.total};
}
usize xprint_sz(const char *zstr, ...) {
    va_list args;
    va_start(args, zstr);
    Output out = {0};
    bool valid = format(&out, zstr, args);
    va_end(args);
    return valid ? out.total : (usize)-1;
}
XStringVw xprint_buf(char *buf, usize len, const char *zstr, ...) {
    if (!buf && len)
        return (XStringVw){0};
    va_list args;
    va_start(args, zstr);
    Output out = {.p = buf, .cap = len};
    bool valid = format(&out, zstr, args);
    va_end(args);
    return valid ? (XStringVw){buf, minimum(out.total, len)} : (XStringVw){0};
}
XString xprint_str(const char *zstr, ...) {
    va_list args;
    va_start(args, zstr);
    XString result = make_string(zstr, args);
    va_end(args);
    return result;
}
static usize stream_print(FILE *stream, XWriter *writer, const char *fmt,
                          va_list args) {
    char local[512];
    va_list copy;
    va_copy(copy, args);
    Output first = {.p = local, .cap = sizeof local};
    const bool valid = format(&first, fmt, copy);
    va_end(copy);
    if (!valid)
        return (usize)-1;
    char *p = local;
    if (first.total > sizeof local) {
        p = xalloc(first.total);
        Output full = {.p = p, .cap = first.total};
        va_copy(copy, args);
        const bool success = format(&full, fmt, copy);
        va_end(copy);
        if (!success || full.total != first.total) {
            xfree(p, first.total);
            return (usize)-1;
        }
    }
    const bool success =
        writer ? (!first.total || xwriter_write(writer, p, first.total))
               : fwrite(p, 1, first.total, stream) == first.total;
    if (p != local)
        xfree(p, first.total);
    return success ? first.total : (usize)-1;
}
usize xprint(const char *zstr, ...) {
    va_list args;
    va_start(args, zstr);
    usize n = stream_print(stdout, 0, zstr, args);
    va_end(args);
    return n;
}
usize xprint_err(const char *zstr, ...) {
    va_list args;
    va_start(args, zstr);
    usize n = stream_print(stderr, 0, zstr, args);
    va_end(args);
    return n;
}

usize xprint_writer(XWriter *writer, const char *zstr, ...) {
    va_list args;
    va_start(args, zstr);
    usize n = stream_print(0, writer, zstr, args);
    va_end(args);
    return n;
}
