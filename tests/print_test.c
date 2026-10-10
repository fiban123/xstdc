#include "xstdc/assert.h"
#include "xstdc/print.h"
#include "xstdc/rng.h"
#include <fenv.h>
#include <stdio.h>
#include <unistd.h>

#define CHECK(expected, fmt, ...)                                              \
    do {                                                                       \
        const char *printcheck_want = (expected);                              \
        const usize printcheck_n = __builtin_strlen(printcheck_want);          \
        assert(xprint_sz(fmt, ##__VA_ARGS__) == printcheck_n);                 \
        XString printcheck_owned = xprint_str(fmt, ##__VA_ARGS__);             \
        assert(printcheck_owned.p && printcheck_owned.len == printcheck_n &&   \
               (!printcheck_n ||                                               \
                xmemeq(printcheck_owned.p, printcheck_want, printcheck_n)));   \
        xfree(printcheck_owned.p, printcheck_owned.len);                       \
        char *printcheck_buf = xalloc(printcheck_n + 4);                       \
        XBufWriter printcheck_writer =                                         \
            xbuf_writer(printcheck_buf, printcheck_n);                         \
        assert(xprint_writer(&printcheck_writer.base, fmt, ##__VA_ARGS__) ==   \
               printcheck_n);                                                  \
        assert(printcheck_writer.pos == printcheck_n);                         \
        assert(!printcheck_n ||                                                \
               xmemeq(printcheck_buf, printcheck_want, printcheck_n));         \
        for (usize printcheck_cap = 0; printcheck_cap <= printcheck_n + 2;     \
             ++printcheck_cap) {                                               \
            for (usize printcheck_j = 0; printcheck_j < printcheck_n + 4;      \
                 ++printcheck_j)                                               \
                printcheck_buf[printcheck_j] = '!';                            \
            XStringVw printcheck_got = xprint_buf(                             \
                printcheck_buf + 1, printcheck_cap, fmt, ##__VA_ARGS__);       \
            const usize printcheck_count =                                     \
                printcheck_cap < printcheck_n ? printcheck_cap : printcheck_n; \
            assert(printcheck_got.p == printcheck_buf + 1 &&                   \
                   printcheck_got.len == printcheck_count);                    \
            assert(                                                            \
                !printcheck_count ||                                           \
                xmemeq(printcheck_got.p, printcheck_want, printcheck_count));  \
            assert(printcheck_buf[0] == '!' &&                                 \
                   printcheck_buf[printcheck_count + 1] == '!' &&              \
                   printcheck_buf[printcheck_n + 3] == '!');                   \
        }                                                                      \
        xfree(printcheck_buf, printcheck_n + 4);                               \
    } while (0)

#define INTEGER_TEST(T, SPEC, FMT)                                             \
    do {                                                                       \
        T values[] = {0, 1, (T) - 1, (T)((u128)1 << (sizeof(T) * 8 - 1)),      \
                      (T)(((u128)1 << (sizeof(T) * 8 - 1)) - 1)};              \
        for (usize i = 0; i < sizeof values / sizeof *values; ++i) {           \
            char expected[100];                                                \
            snprintf(expected, sizeof expected, FMT, values[i]);               \
            CHECK(expected, SPEC, &values[i]);                                 \
        }                                                                      \
    } while (0)
static void test_integers(void) {
    INTEGER_TEST(u8, "%u8", "%hhu");
    INTEGER_TEST(i8, "%i8", "%hhd");
    INTEGER_TEST(u16, "%u16", "%hu");
    INTEGER_TEST(i16, "%i16", "%hd");
    INTEGER_TEST(u32, "%u32", "%u");
    INTEGER_TEST(i32, "%i32", "%d");
    INTEGER_TEST(u64, "%u64", "%llu");
    INTEGER_TEST(i64, "%i64", "%lld");
    INTEGER_TEST(usize, "%usz", "%llu");
    INTEGER_TEST(ssize, "%isz", "%lld");
    u128 u = (u128)-1;
    CHECK("340282366920938463463374607431768211455", "%u128", &u);
    CHECK("ffffffffffffffffffffffffffffffff", "%u128(x)", &u);
    i128 i = (i128)((u128)1 << 127);
    CHECK("-170141183460469231731687303715884105728", "%i128", &i);
    CHECK("-80000000000000000000000000000000", "%i128(x)", &i);
    i = (i128)(((u128)1 << 127) - 1);
    CHECK("170141183460469231731687303715884105727", "%i128", &i);
    u = 0;
    CHECK("0", "%u128", &u);
    XRngXoshiro256ppState rng;
    xrng_xoshiro256pp_seed(&rng, 1000);
    for (unsigned j = 0; j < 1000; ++j) {
        u64 n = xrng_xoshiro256pp_u64(&rng);
        char expected[64], buf[64];
        int len = snprintf(expected, sizeof expected, "%llu/%llx", n, n);
        XStringVw got = xprint_buf(buf, sizeof buf, "%u64/%u64(x)", &n, &n);
        assert(got.len == (usize)len && xmemeq(got.p, expected, got.len));
    }
}
static void test_booleans(void) {
    bool yes = true, no = false;
    CHECK("true false", "%b %b", &yes, &no);
    CHECK("  true", "%b(e=6)", &yes);
    CHECK("false..", "%b(rp=.,e=7)", &no);
    CHECK("tru", "%b(t=3)", &yes);
    CHECK("", "%b(t=0)", &no);
    // Read exactly one byte, including noncanonical nonzero byte values.
    const u8 bytes[] = {0, 1, 2, 255};
    CHECK("false true true true", "%b %b %b %b", &bytes[0], &bytes[1],
          &bytes[2], &bytes[3]);
    assert(xprint_sz("%b", (void *)0) == (usize)-1);
}
static void test_strings(void) {
    CHECK("", "");
    CHECK("hello % world", "hello %% world");
    char c = 'A';
    XStringVw v = strl("hello");
    CHECK("A hello world!", "%c %s %z", &c, &v, "world!");
    CHECK("41 68656c6c6f ff", "%c(x) %s(x) %z(x)", &c, &v, "\xff");
    XString owned = xstr_fromz("owned");
    CHECK("owned", "%s", &owned);
    xfree(owned.p, owned.len);
    XStringVw null = {0};
    CHECK("", "%s", &null);
    const char bytes[] = {'a', 0, (char)255};
    XStringVw binary = {bytes, 3};
    char buf[4] = {1, 1, 1, 1};
    XStringVw got = xprint_buf(buf, 3, "%s", &binary);
    assert(got.len == 3 && xmemeq(buf, bytes, 3) && buf[3] == 1);
    assert(xprint_sz("%s", &binary) == 3);
    CHECK("6100ff", "%s(x)", &binary);
    CHECK("610", "%s(x,t=3)", &binary);
    c = 0;
    got = xprint_buf(buf, 1, "%c", &c);
    assert(got.len == 1 && !buf[0]);
    got = xprint_buf(0, 0, "abc");
    assert(!got.p && !got.len);
}
static void test_attributes(void) {
    u32 n = 42;
    CHECK("00042", "%u32(lp=0,e=5)", &n);
    CHECK("42000", "%u32(rp=0,e=5)", &n);
    CHECK("_42..", "%u32(lp=_,rp=.,e=5)", &n);
    CHECK("   42", "%u32(e=5)", &n);
    CHECK("000", "%u32(lp=0,e=5,t=3)", &n);
    CHECK("42", "%u32(e=1,t=10)", &n);
    CHECK("", "%u32(e=100,t=0)", &n);
    CHECK(" 42 ", "%u32( lp = ' ', rp = ' ', e = 4 )", &n);
    CHECK(",42)", "%u32(lp=',',rp=')',e=4)", &n);
    CHECK("\t42", "%u32(lp='\\t',e=3)", &n);
    CHECK("42", "%u32()", &n);
    CHECK("42 suffix", "%u32 suffix", &n);
    // Huge widths with truncation must not cause allocation or long loops.
    CHECK("000", "%u32(lp=0,e=18446744073709551615,t=3)", &n);
}
static void test_floats(void) {
    f64 values[] = {0.0,
                    -0.0,
                    1.23456789,
                    -12345.25,
                    1e300,
                    1e-300,
                    __builtin_inf(),
                    -__builtin_inf(),
                    __builtin_nan("")};
    for (usize i = 0; i < sizeof values / sizeof *values; ++i) {
        char expected[2048];
        f64 d = values[i];
        f32 f = (f32)d;
        snprintf(expected, sizeof expected, "%g", d);
        CHECK(expected, "%f64", &d);
        snprintf(expected, sizeof expected, "%.4f", d);
        CHECK(expected, "%f64(r=4)", &d);
        snprintf(expected, sizeof expected, "%g", (double)f);
        CHECK(expected, "%f32", &f);
    }
    f32 f = 1.25f;
    CHECK("331.250033", "%f32(r=4,lp=3,rp=3,e=10)", &f);
    f64 d = 0.5;
    char expected[1100];
    snprintf(expected, sizeof expected, "%.900f", d);
    CHECK(expected, "%f64(r=900)", &d);
    d = 2.5;
    snprintf(expected, sizeof expected, "%.0f", d);
    CHECK(expected, "%f64(r=0)", &d);
}
static void test_errors(void) {
    const char *bad[] = {"%",
                         "%b(x)",
                         "%b(r=2)",
                         "%q",
                         "%u",
                         "%f",
                         "%u64(r=2)",
                         "%u64(x=1)",
                         "%u64(e=-1)",
                         "%u64(e=18446744073709551616)",
                         "%u64(e=1,e=2)",
                         "%u64(lp=ab)",
                         "%u64(lp='')",
                         "%u64(lp='x)",
                         "%u64(lp=)",
                         "%u64(e=1,)",
                         "%u64(e=1",
                         "%u64(no=1)",
                         "%f64(x)",
                         "%f64(r=2147483648)"};
    u64 n = 0;
    char buf[16];
    for (usize i = 0; i < sizeof bad / sizeof *bad; ++i) {
        assert(xprint_sz(bad[i], &n) == (usize)-1);
        XStringVw v = xprint_buf(buf, sizeof buf, bad[i], &n);
        assert(!v.p && !v.len);
        XString s = xprint_str(bad[i], &n);
        assert(!s.p && !s.len);
    }
    assert(xprint_sz(0) == (usize)-1);
    assert(xprint_sz("%u64", (void *)0) == (usize)-1);
    assert(xprint_sz("%z", (void *)0) == (usize)-1);
    XStringVw invalid = {0, 1};
    assert(xprint_sz("%s", &invalid) == (usize)-1);
    XStringVw got = xprint_buf(0, 1, "abc");
    assert(!got.p && !got.len);
    assert(xprint_sz("%z(e=18446744073709551615)!", "") == (usize)-1);
}
// Independent digit-by-digit oracle for the optimized 128-bit conversion.
static void test_wide_random(void) {
    XRngXoshiro256ppState rng;
    xrng_xoshiro256pp_seed(&rng, 1234);
    for (unsigned i = 0; i < 2000; ++i) {
        u128 value = ((u128)xrng_xoshiro256pp_u64(&rng) << 64) |
                     xrng_xoshiro256pp_u64(&rng);
        if (i < 128)
            value = (u128)1 << i;
        if (i >= 128 && i < 256)
            value = ((u128)1 << (i - 128)) - 1;
        for (unsigned hex = 0; hex < 2; ++hex) {
            for (unsigned sign = 0; sign < 2; ++sign) {
                const bool negative = sign && (value >> 127);
                u128 magnitude = negative ? (u128)0 - value : value;
                char reverse[40], expected[40], result[40];
                usize count = 0, length = 0;
                const unsigned base = hex ? 16 : 10;
                do {
                    reverse[count++] =
                        "0123456789abcdef"[(unsigned)(magnitude % base)];
                    magnitude /= base;
                } while (magnitude);
                if (negative)
                    expected[length++] = '-';
                while (count)
                    expected[length++] = reverse[--count];
                const char *fmt = sign ? (hex ? "%i128(x)" : "%i128")
                                       : (hex ? "%u128(x)" : "%u128");
                XStringVw v = xprint_buf(result, sizeof result, fmt, &value);
                assert(v.p && v.len == length &&
                       xmemeq(result, expected, length));
            }
        }
    }
}

static void test_owned_boundaries(void) {
    char text[1025];
    for (usize i = 0; i < sizeof text; ++i)
        text[i] = (char)i;
    const usize sizes[] = {0, 1, 511, 512, 513, 1024, 1025};
    for (usize i = 0; i < sizeof sizes / sizeof *sizes; ++i) {
        XStringVw input = {text, sizes[i]};
        XString s = xprint_str("%s", &input);
        assert(s.p && s.len == input.len &&
               (!s.len || xmemeq(s.p, text, s.len)));
        xfree(s.p, s.len);
    }
}

// Capture stdout/stderr so wrapper tests do not flood the terminal.
static void test_stream(FILE *stream, bool error_stream) {
    FILE *capture = tmpfile();
    assert(capture);
    assert(fflush(stream) == 0);
    const int saved = dup(fileno(stream));
    assert(saved >= 0 && dup2(fileno(capture), fileno(stream)) >= 0);
    char input[1025];
    for (usize i = 0; i < sizeof input; ++i)
        input[i] = (char)i;
    const usize sizes[] = {0, 1, 511, 512, 513, 1025};
    usize total = 0;
    for (usize i = 0; i < sizeof sizes / sizeof *sizes; ++i) {
        XStringVw view = {input, sizes[i]};
        const usize written =
            error_stream ? xprint_err("%s", &view) : xprint("%s", &view);
        assert(written == view.len);
        total += view.len;
    }
    assert((error_stream ? xprint_err("bad%q") : xprint("bad%q")) == (usize)-1);
    assert(fflush(stream) == 0);
    assert(dup2(saved, fileno(stream)) >= 0);
    close(saved);
    assert(ftell(capture) == (long)total);
    rewind(capture);
    char output[1025];
    for (usize i = 0; i < sizeof sizes / sizeof *sizes; ++i) {
        assert(fread(output, 1, sizes[i], capture) == sizes[i]);
        assert(!sizes[i] || xmemeq(output, input, sizes[i]));
    }
    fclose(capture);
}

static void compare_float_reference(f64 value, const char *fmt, int precision) {
    char expected[2048], actual[2048];
    const int n =
        precision < 0
            ? snprintf(expected, sizeof expected, "%g", value)
            : snprintf(expected, sizeof expected, "%.*f", precision, value);
    const XStringVw got = xprint_buf(actual, sizeof actual, fmt, &value);
    if (!got.p || n < 0 || got.len != (usize)n ||
        !xmemeq(actual, expected, got.len)) {
        u64 bits;
        xmemcp(&bits, &value, sizeof bits);
        xprint_err("float mismatch: bits=%u64(x,e=16,lp=0) mode=%i32 "
                   "precision=%i32\nexpected=%z\nactual=%s\n",
                   &bits, &(i32){fegetround()}, &precision, expected, &got);
        assert(false);
    }
}
static void test_float_reference(void) {
    const int saved = fegetround();
    const int modes[] = {FE_TONEAREST, FE_UPWARD, FE_DOWNWARD, FE_TOWARDZERO};
    const int precisions[] = {0, 1, 2, 4, 6, 17, 18, 19, 30, 100, 324, 900};
    const f64 boundaries[] = {0.0,
                              -0.0,
                              0.5,
                              -0.5,
                              1.5,
                              2.5,
                              0.125,
                              9.999995e-5,
                              9.999995e-4,
                              9.999995,
                              999999.5,
                              1e20,
                              1e-300,
                              0x1p-1074,
                              0x1.fffffffffffffp-1023,
                              0x1p-1022,
                              0x1.fffffffffffffp1023};
    for (unsigned mode = 0; mode < 4; ++mode) {
        assert(fesetround(modes[mode]) == 0);
        XRngXoshiro256ppState rng;
        xrng_xoshiro256pp_seed(&rng, 2026);
        for (usize i = 0; i < 5000; ++i) {
            u64 bits = xrng_xoshiro256pp_u64(&rng);
            f64 value;
            xmemcp(&value, &bits, sizeof value);
            compare_float_reference(value, "%f64", -1);
            const int precision =
                precisions[i % (sizeof precisions / sizeof *precisions)];
            char fmt[32];
            xprint_buf(fmt, sizeof fmt, "%%f64(r=%i32)%c", &precision,
                       &(char){0});
            compare_float_reference(value, fmt, precision);
            u32 smallbits = (u32)bits;
            f32 small;
            xmemcp(&small, &smallbits, sizeof small);
            char expected[2048], actual[2048];
            int n = snprintf(expected, sizeof expected, "%g", (f64)small);
            XStringVw got = xprint_buf(actual, sizeof actual, "%f32", &small);
            assert(got.p && got.len == (usize)n &&
                   xmemeq(actual, expected, got.len));
            xprint_buf(fmt, sizeof fmt, "%%f32(r=%i32)%c", &precision,
                       &(char){0});
            n = snprintf(expected, sizeof expected, "%.*f", precision,
                         (f64)small);
            got = xprint_buf(actual, sizeof actual, fmt, &small);
            assert(got.p && got.len == (usize)n &&
                   xmemeq(actual, expected, got.len));
        }
        for (usize i = 0; i < sizeof boundaries / sizeof *boundaries; ++i) {
            u64 bits;
            xmemcp(&bits, (void *)&boundaries[i], sizeof bits);
            for (int adjacent = -1; adjacent <= 1; ++adjacent) {
                u64 nearby = bits + adjacent;
                f64 value;
                xmemcp(&value, &nearby, sizeof value);
                compare_float_reference(value, "%f64", -1);
                for (usize p = 0; p < sizeof precisions / sizeof *precisions;
                     ++p) {
                    char fmt[32];
                    xprint_buf(fmt, sizeof fmt, "%%f64(r=%i32)%c",
                               &precisions[p], &(char){0});
                    compare_float_reference(value, fmt, precisions[p]);
                }
            }
        }
    }
    assert(fesetround(saved) == 0);
}

typedef struct {
    XWriter base;
    unsigned calls;
} RejectWriter;
static bool reject_write(XWriter *base, const void *src, usize n) {
    RejectWriter *writer = (RejectWriter *)base;
    assert(src && n);
    ++writer->calls;
    return false;
}
static void test_writer(void) {
    RejectWriter reject = {.base = {.vtable = {.write = reject_write}}};
    assert(xprint_writer(&reject.base, "") == 0 && reject.calls == 0);
    assert(xprint_writer(&reject.base, "bad%q") == (usize)-1 &&
           reject.calls == 0);
    assert(xprint_writer(&reject.base, "text") == (usize)-1 &&
           reject.calls == 1);
    u32 n = 7;
    assert(xprint_writer(&reject.base, "%u32(e=2000)", &n) == (usize)-1 &&
           reject.calls == 2);
    char data[4] = {'!', '!', '!', '!'};
    XBufWriter writer = xbuf_writer(data, 3);
    assert(xprint_writer(&writer.base, "four") == (usize)-1);
    assert(writer.pos == 0 && data[0] == '!');
    XStringVw binary = {"a\0b", 3};
    assert(xprint_writer(&writer.base, "%s", &binary) == 3);
    assert(xmemeq(data, binary.p, 3) && data[3] == '!');
}
int main(void) {
    test_writer();
    test_booleans();
    test_integers();
    test_wide_random();
    test_owned_boundaries();
    test_strings();
    test_attributes();
    test_floats();
    test_float_reference();
    test_errors();
    test_stream(stdout, false);
    test_stream(stderr, true);
    xprint("%z\n", "print tests passed");
}
