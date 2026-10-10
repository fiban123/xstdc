#include "xstdc/print.h"
#include "xstdc/string.h"
#include <fenv.h>

static void test_conversions(void) {
    char input[] = "hello\0unused";
    XStringVw view = xstrv_fromz(input);
    assert(view.p == input && view.len == 5);
    XString owned = xstr_fromz(input);
    assert(owned.p != input && owned.len == 5);
    assert(xstrv_eq(stov(owned), strl("hello")));
    input[0] = 'j';
    assert(view.p[0] == 'j' && owned.p[0] == 'h');
    XStringVw borrowed = xstrv_froms(owned);
    assert(borrowed.p == owned.p && borrowed.len == owned.len);
    xfree(owned.p, owned.len);

    XString null = xstr_fromz(0);
    assert(!null.p && !null.len);
    view = xstrv_fromz(0);
    assert(!view.p && !view.len);
    owned = xstr_fromz("");
    assert(owned.p && !owned.len);
    assert(xstrv_eq(stov(owned), strl("")));
    xfree(owned.p, owned.len);
    assert(strl("a\0b").len == 3);
}

static void test_bytes(void) {
    // Exact-sized buffers ensure operations do not require a trailing NUL.
    for (usize n = 0; n <= 513; ++n) {
        char *a = xalloc(n), *b = xalloc(n);
        for (usize i = 0; i < n; ++i)
            a[i] = b[i] = (char)(i * 37);
        XStringVw av = {a, n}, bv = {b, n};
        assert(xstrv_eq(av, av));
        assert(xstrv_eq(av, bv));
        if (n) {
            assert(!xstrv_eq(av, (XStringVw){b, n - 1}));
            for (usize i = 0; i < n; ++i) {
                b[i] ^= 1;
                assert(!xstrv_eq(av, bv));
                b[i] ^= 1;
            }
        }
        // Includes overlapping views into the same input allocation.
        const usize split = n / 2;
        XString c =
            xstrv_concat((XStringVw){a, n}, (XStringVw){a + split, n - split});
        assert(c.len == 2 * n - split && c.p != a);
        for (usize i = 0; i < n; ++i)
            assert(c.p[i] == a[i]);
        for (usize i = split; i < n; ++i)
            assert(c.p[n + i - split] == a[i]);
        xfree(c.p, c.len);
        c = xstrv_concat((XStringVw){0}, av);
        assert(xstrv_eq(stov(c), av));
        xfree(c.p, c.len);
        c = xstrv_concat(av, (XStringVw){0});
        assert(xstrv_eq(stov(c), av));
        xfree(c.p, c.len);
        xfree(a, n);
        xfree(b, n);
    }
    XString c = xstrv_concat((XStringVw){0}, (XStringVw){0});
    assert(c.p && !c.len);
    xfree(c.p, c.len);
}

static void test_zlengths(void) {
    // Different lengths and alignments, with the terminator at the allocation
    // end.
    for (usize n = 0; n <= 1024; ++n) {
        const usize offset = n % 16;
        char *base = xalloc(offset + n + 1), *p = base + offset;
        for (usize i = 0; i < n; ++i)
            p[i] = (char)(1 + i % 127);
        p[n] = 0;
        XStringVw v = xstrv_fromz(p);
        XString s = xstr_fromz(p);
        assert(v.p == p && v.len == n && s.len == n);
        assert(xstrv_eq(v, stov(s)));
        xfree(s.p, s.len);
        xfree(base, offset + n + 1);
    }
}

static void test_integer_parse(void) {
    {
        u8 u = 42;
        i8 i = 42;
        const i8 imax = (i8)(((u8)-1) >> 1);
        assert(u8parse(strl("255tail"), &u) == 3 && u == (u8)-1);
        assert(!u8parse(strl("256"), &u) && u == (u8)-1);
        assert(!u8parse(strl("-1"), &u) && u == (u8)-1);
        assert(u8parse(strl("+00123.5"), &u) == 6 && u == 123);
        assert(!u8parse(strl(""), &u) && u == 123);
        assert(!u8parse((XStringVw){0}, &u) && u == 123);
        assert(!u8parse(strl("+"), &u) && u == 123);
        assert(!u8parse(strl(" 1"), &u) && u == 123);
        assert(!u8parse(strl("a1"), &u) && u == 123);
        assert(i8parse(strl("127x"), &i) == 3 && i == imax);
        assert(!i8parse(strl("128"), &i) && i == imax);
        assert(i8parse(strl("-128x"), &i) == 4 && i == -imax - 1);
        assert(!i8parse(strl("-129"), &i) && i == -imax - 1);
        assert(i8parse(strl("-000"), &i) == 4 && i == 0);
        assert(!i8parse(strl("-"), &i) && i == 0);
        assert(i8parse(strl("-123a"), &i) == 4 && i == -123);
    }
    {
        u16 u = 42;
        i16 i = 42;
        const i16 imax = (i16)(((u16)-1) >> 1);
        assert(u16parse(strl("65535tail"), &u) == 5 && u == (u16)-1);
        assert(!u16parse(strl("65536"), &u) && u == (u16)-1);
        assert(!u16parse(strl("-1"), &u) && u == (u16)-1);
        assert(u16parse(strl("+00123.5"), &u) == 6 && u == 123);
        assert(!u16parse(strl(""), &u) && u == 123);
        assert(!u16parse((XStringVw){0}, &u) && u == 123);
        assert(!u16parse(strl("+"), &u) && u == 123);
        assert(!u16parse(strl(" 1"), &u) && u == 123);
        assert(!u16parse(strl("a1"), &u) && u == 123);
        assert(i16parse(strl("32767x"), &i) == 5 && i == imax);
        assert(!i16parse(strl("32768"), &i) && i == imax);
        assert(i16parse(strl("-32768x"), &i) == 6 && i == -imax - 1);
        assert(!i16parse(strl("-32769"), &i) && i == -imax - 1);
        assert(i16parse(strl("-000"), &i) == 4 && i == 0);
        assert(!i16parse(strl("-"), &i) && i == 0);
        assert(i16parse(strl("-123a"), &i) == 4 && i == -123);
    }
    {
        u32 u = 42;
        i32 i = 42;
        const i32 imax = (i32)(((u32)-1) >> 1);
        assert(u32parse(strl("4294967295tail"), &u) == 10 && u == (u32)-1);
        assert(!u32parse(strl("4294967296"), &u) && u == (u32)-1);
        assert(!u32parse(strl("-1"), &u) && u == (u32)-1);
        assert(u32parse(strl("+00123.5"), &u) == 6 && u == 123);
        assert(!u32parse(strl(""), &u) && u == 123);
        assert(!u32parse((XStringVw){0}, &u) && u == 123);
        assert(!u32parse(strl("+"), &u) && u == 123);
        assert(!u32parse(strl(" 1"), &u) && u == 123);
        assert(!u32parse(strl("a1"), &u) && u == 123);
        assert(i32parse(strl("2147483647x"), &i) == 10 && i == imax);
        assert(!i32parse(strl("2147483648"), &i) && i == imax);
        assert(i32parse(strl("-2147483648x"), &i) == 11 && i == -imax - 1);
        assert(!i32parse(strl("-2147483649"), &i) && i == -imax - 1);
        assert(i32parse(strl("-000"), &i) == 4 && i == 0);
        assert(!i32parse(strl("-"), &i) && i == 0);
        assert(i32parse(strl("-123a"), &i) == 4 && i == -123);
    }
    {
        u64 u = 42;
        i64 i = 42;
        const i64 imax = (i64)(((u64)-1) >> 1);
        assert(u64parse(strl("18446744073709551615tail"), &u) == 20 &&
               u == (u64)-1);
        assert(!u64parse(strl("18446744073709551616"), &u) && u == (u64)-1);
        assert(!u64parse(strl("-1"), &u) && u == (u64)-1);
        assert(u64parse(strl("+00123.5"), &u) == 6 && u == 123);
        assert(!u64parse(strl(""), &u) && u == 123);
        assert(!u64parse((XStringVw){0}, &u) && u == 123);
        assert(!u64parse(strl("+"), &u) && u == 123);
        assert(!u64parse(strl(" 1"), &u) && u == 123);
        assert(!u64parse(strl("a1"), &u) && u == 123);
        assert(i64parse(strl("9223372036854775807x"), &i) == 19 && i == imax);
        assert(!i64parse(strl("9223372036854775808"), &i) && i == imax);
        assert(i64parse(strl("-9223372036854775808x"), &i) == 20 &&
               i == -imax - 1);
        assert(!i64parse(strl("-9223372036854775809"), &i) && i == -imax - 1);
        assert(i64parse(strl("-000"), &i) == 4 && i == 0);
        assert(!i64parse(strl("-"), &i) && i == 0);
        assert(i64parse(strl("-123a"), &i) == 4 && i == -123);
    }
    {
        u128 u = 42;
        i128 i = 42;
        const i128 imax = (i128)(((u128)-1) >> 1);
        assert(u128parse(strl("340282366920938463463374607431768211455tail"),
                         &u) &&
               u == (u128)-1);
        assert(
            !u128parse(strl("340282366920938463463374607431768211456"), &u) &&
            u == (u128)-1);
        assert(!u128parse(strl("-1"), &u) && u == (u128)-1);
        assert(u128parse(strl("+00123.5"), &u) == 6 && u == 123);
        assert(!u128parse(strl(""), &u) && u == 123);
        assert(!u128parse((XStringVw){0}, &u) && u == 123);
        assert(!u128parse(strl("+"), &u) && u == 123);
        assert(!u128parse(strl(" 1"), &u) && u == 123);
        assert(!u128parse(strl("a1"), &u) && u == 123);
        assert(i128parse(strl("170141183460469231731687303715884105727x"),
                         &i) == 39 &&
               i == imax);
        assert(
            !i128parse(strl("170141183460469231731687303715884105728"), &i) &&
            i == imax);
        assert(i128parse(strl("-170141183460469231731687303715884105728x"),
                         &i) == 40 &&
               i == -imax - 1);
        assert(
            !i128parse(strl("-170141183460469231731687303715884105729"), &i) &&
            i == -imax - 1);
        assert(i128parse(strl("-000"), &i) == 4 && i == 0);
        assert(!i128parse(strl("-"), &i) && i == 0);
        assert(i128parse(strl("-123a"), &i) == 4 && i == -123);
    }
    usize size;
    ssize delta;
    assert(usizeparse(strl("123"), &size) == 3 && size == 123);
    assert(ssizeparse(strl("-123"), &delta) == 4 && delta == -123);
    const char exact[] = {'1', '2', '3'};
    u64 u;
    assert(u64parse((XStringVw){exact, sizeof exact}, &u) == sizeof exact &&
           u == 123);
    assert(u64parse((XStringVw){exact, 2}, &u) == 2 && u == 12);
    assert(u64parse(strl("12\0"
                         "34"),
                    &u) &&
           u == 12);
    char zeros[1024];
    xmemzero(zeros, sizeof zeros);
    for (usize j = 0; j < sizeof zeros - 1; ++j)
        zeros[j] = '0';
    zeros[sizeof zeros - 1] = '7';
    assert(u64parse((XStringVw){zeros, sizeof zeros}, &u) == sizeof zeros &&
           u == 7);
}

static void test_float_parse(void) {
    const XStringVw valid[] = {strl("123a"),
                               strl("123.1......"),
                               strl("123..1"),
                               strl(".5"),
                               strl("-.5"),
                               strl("+1.25e+2tail"),
                               strl("12e+"),
                               strl("12e"),
                               strl("1.e2"),
                               strl("1e-2"),
                               strl("0x12"),
                               strl("12\0"
                                    "34")};
    const f64 expected[] = {123, 123.1, 123, .5,  -.5, 125,
                            12,  12,    100, .01, 0,   12};
    const usize consumed[] = {3, 5, 4, 2, 3, 8, 2, 2, 4, 4, 1, 2};
    for (usize i = 0; i < sizeof valid / sizeof *valid; ++i) {
        f32 f;
        f64 d;
        assert(f32parse(valid[i], &f) == consumed[i] && f == (f32)expected[i]);
        assert(f64parse(valid[i], &d) == consumed[i] && d == expected[i]);
    }
    const XStringVw invalid[] = {
        {0},
        strl(""),
        strl("+"),
        strl("-"),
        strl("."),
        strl("+."),
        strl("e2"),
        strl(" 12"),
        strl("nan"),
        strl("inf"),
        strl("1e999999999999999999999999999999999999")};
    for (usize i = 0; i < sizeof invalid / sizeof *invalid; ++i) {
        f32 f = 42;
        f64 d = 42;
        assert(!f32parse(invalid[i], &f) && f == 42);
        assert(!f64parse(invalid[i], &d) && d == 42);
    }
    f32 f;
    f64 d;
    assert(f32parse(strl("3.4028234663852886e38"), &f) && f == __FLT_MAX__);
    assert(!f32parse(strl("3.402824e38"), &f) && f == __FLT_MAX__);
    assert(f64parse(strl("1.7976931348623157e308"), &d) && d == __DBL_MAX__);
    assert(!f64parse(strl("1.7976931348623159e308"), &d) && d == __DBL_MAX__);
    assert(f32parse(strl("1.401298464324817e-45"), &f) && f == 0x1p-149f);
    assert(f64parse(strl("4.9406564584124654e-324"), &d) && d == 0x1p-1074);
    assert(f32parse(strl("-1e-99999"), &f) && f == 0 && __builtin_signbit(f));
    assert(f64parse(strl("-0"), &d) && d == 0 && __builtin_signbit(d));
    assert(f64parse(strl("0e999999999999999999999999"), &d) && d == 0);
    // Overflow can round to a finite maximum under directed rounding.
    const int rounding = fegetround();
    assert(fesetround(FE_DOWNWARD) == 0);
    f = 42;
    d = 42;
    assert(!f32parse(strl("1e999"), &f) && f == 42);
    assert(!f64parse(strl("1e999"), &d) && d == 42);
    assert(fesetround(rounding) == 0);
    // Length bounds apply even when a numeric suffix exists beyond the view.
    const char exact[] = {'1', '.', '2', '5'};
    assert(f32parse((XStringVw){exact, 3}, &f) == 3 && f == 1.2f);
    assert(f64parse((XStringVw){exact, sizeof exact}, &d) == sizeof exact &&
           d == 1.25);
    // Large mantissas must not overflow before a compensating exponent.
    char long_number[1007];
    long_number[0] = '1';
    for (usize i = 1; i <= 1000; ++i)
        long_number[i] = '0';
    xmemcp(long_number + 1001, "e-1000", 6);
    assert(f32parse((XStringVw){long_number, sizeof long_number}, &f) ==
               sizeof long_number &&
           f == 1);
    assert(f64parse((XStringVw){long_number, sizeof long_number}, &d) ==
               sizeof long_number &&
           d == 1);
}

static void test_prefix_suffix(void) {
    assert(xstrv_starts_with(strl("hello"), strl("he")));
    assert(xstrv_end_with(strl("hello"), strl("lo")));
    assert(!xstrv_starts_with(strl("hello"), strl("He")));
    assert(!xstrv_end_with(strl("hello"), strl("Lo")));
    assert(!xstrv_starts_with(strl("hi"), strl("high")));
    assert(!xstrv_end_with(strl("hi"), strl("oh hi")));
    const XStringVw empty = {0};
    assert(xstrv_starts_with(empty, empty));
    assert(xstrv_end_with(empty, empty));
    assert(xstrv_starts_with(strl("hi"), empty));
    assert(xstrv_end_with(strl("hi"), empty));
    assert(xstrv_starts_with(empty, strl("")));
    assert(xstrv_end_with(empty, strl("")));
    assert(!xstrv_starts_with(empty, strl("x")));
    assert(!xstrv_end_with(empty, strl("x")));
    // Nonterminated storage, embedded NUL, and overlapping views.
    const char bytes[] = {'a', '\0', 'b'};
    const XStringVw text = {bytes, sizeof bytes};
    assert(xstrv_starts_with(text, text));
    assert(xstrv_end_with(text, text));
    assert(xstrv_starts_with(text, (XStringVw){bytes, 2}));
    assert(xstrv_end_with(text, (XStringVw){bytes + 1, 2}));
    assert(!xstrv_starts_with(text, strl("ab")));
    assert(!xstrv_end_with(text, strl("ab")));
}

static void test_bool_parse(void) {
    bool value;
    assert(xboolparse(strl("true"), &value) == 4 && value);
    assert(xboolparse(strl("false"), &value) == 5 && !value);
    assert(xboolparse(strl("1"), &value) == 1 && value);
    assert(xboolparse(strl("0"), &value) == 1 && !value);
    assert(xboolparse(strl("true!"), &value) == 4 && value);
    assert(xboolparse(strl("falsehood"), &value) == 5 && !value);
    assert(xboolparse(strl("123"), &value) == 1 && value);
    assert(xboolparse(strl("01"), &value) == 1 && !value);
    const char exact[] = {'t', 'r', 'u', 'e'};
    assert(xboolparse((XStringVw){exact, sizeof exact}, &value) == 4 && value);
    const XStringVw invalid[] = {
        {0},           strl(""),      strl("tru"),   strl("fals"), strl("True"),
        strl("FALSE"), strl(" true"), strl("+1"),    strl("-0"),   strl("2"),
        strl("yes"),   {exact, 3},    strl("tr\0ue")};
    for (usize i = 0; i < sizeof invalid / sizeof *invalid; ++i) {
        value = true;
        assert(xboolparse(invalid[i], &value) == 0 && value);
        value = false;
        assert(xboolparse(invalid[i], &value) == 0 && !value);
    }
}

int main(void) {
    test_bool_parse();
    test_prefix_suffix();
    test_integer_parse();
    test_float_parse();
    test_conversions();
    test_bytes();
    test_zlengths();
    xprint("%z\n", "string tests passed");
}
