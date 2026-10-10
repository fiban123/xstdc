#include "xstdc/assert.h"
#include "xstdc/math.h"
#include "xstdc/print.h"
#include "xstdc/rng.h"
#include <float.h>
#include <math.h>

static void close_to(const char *name, double x, double actual, double expected,
                     double relative, double absolute) {
    if (actual == expected)
        return;
    if (isnan(actual) && isnan(expected))
        return;
    if (!isfinite(actual) || !isfinite(expected) ||
        fabs(actual - expected) > absolute + relative * fabs(expected)) {
        xprint("%z(%f64): %f64(r=17), libm %f64(r=17)\n", name, &x, &actual,
               &expected);
        assert(false);
    }
}

// Libm is not an exact oracle for wide integer inputs. Check the defining
// inequalities instead, using division to avoid overflow.
#define TEST_UNSIGNED(T)                                                       \
    static void test_##T(T x) {                                                \
        T root = T##sqrt(x);                                                   \
        assert(root == 0 || root <= x / root);                                 \
        u128 next = (u128)root + 1;                                            \
        assert(next > (u128)x / next);                                         \
        for (unsigned base = 2; base <= 10; ++base) {                          \
            unsigned exponent = T##logn(x, base);                              \
            u128 power = 1;                                                    \
            for (unsigned i = 0; i < exponent; ++i)                            \
                power *= base;                                                 \
            assert(x == 0 || power <= x);                                      \
            assert(power > (u128)x / base);                                    \
            if (x && base == 2)                                                \
                assert(T##log2(x) == exponent);                                \
            if (x && base == 10)                                               \
                assert(T##log10(x) == exponent);                               \
        }                                                                      \
    }

TEST_UNSIGNED(u8)
TEST_UNSIGNED(u16)
TEST_UNSIGNED(u32)
TEST_UNSIGNED(u64)
TEST_UNSIGNED(u128)

static void test_integers(void) {
    XRngXoshiro256ppState rng;
    xrng_xoshiro256pp_seed(&rng, 1000);
    for (u32 i = 0; i <= 65535; ++i) {
        test_u16(i);
        if (i <= 255)
            test_u8(i);
        assert(u16sqrt(i) == (u16)sqrt(i));
    }
    test_u32(0);
    test_u32((u32)-1);
    for (unsigned i = 0; i < sizeof(u32) * 8; ++i) {
        u32 x = (u32)1 << i;
        test_u32(x - 1);
        test_u32(x);
        test_u32(x + 1);
    }
    for (u32 x = 1; x <= (u32)-1 / 10;) {
        x *= 10;
        test_u32(x - 1);
        test_u32(x);
        test_u32(x + 1);
    }
    test_u64(0);
    test_u64((u64)-1);
    for (unsigned i = 0; i < sizeof(u64) * 8; ++i) {
        u64 x = (u64)1 << i;
        test_u64(x - 1);
        test_u64(x);
        test_u64(x + 1);
    }
    for (u64 x = 1; x <= (u64)-1 / 10;) {
        x *= 10;
        test_u64(x - 1);
        test_u64(x);
        test_u64(x + 1);
    }
    test_u128(0);
    test_u128((u128)-1);
    for (unsigned i = 0; i < sizeof(u128) * 8; ++i) {
        u128 x = (u128)1 << i;
        test_u128(x - 1);
        test_u128(x);
        test_u128(x + 1);
    }
    for (u128 x = 1; x <= (u128)-1 / 10;) {
        x *= 10;
        test_u128(x - 1);
        test_u128(x);
        test_u128(x + 1);
    }
    for (unsigned i = 0; i < 2000; ++i) {
        u128 x = (u128)xrng_xoshiro256pp_u64(&rng) << 64;
        x |= xrng_xoshiro256pp_u64(&rng);
        test_u128(x);
        test_u64(x);
        test_u32(x);
        u128 root = (u64)x;
        u128 square = root * root;
        test_u128(square);
        if (square)
            test_u128(square - 1);
        test_u128(square + 1);
    }
    assert(u8pow(0, 0) == 1);
    assert(u8pow(0, 255) == 0);
    assert(u8pow(1, 255) == 1);
    for (unsigned e = 0; e < 8; ++e) {
        assert(u8pow(2, e) == (u8)((u128)1 << e));
    }
    assert(u8pow(3, 3) == (u8)pow(3, 3));
    assert(i8pow(0, 0) == 1);
    assert(i8pow(0, 255) == 0);
    assert(i8pow(1, 255) == 1);
    for (unsigned e = 0; e < 7; ++e) {
        assert(i8pow(2, e) == (i8)((u128)1 << e));
        assert(i8pow(-2, e) ==
               (e & 1 ? -(i8)((u128)1 << e) : (i8)((u128)1 << e)));
    }
    assert(i8pow(-2, 7) == (i8)((u128)1 << 7));
    assert(i8pow(3, 3) == (i8)pow(3, 3));
    assert(u16pow(0, 0) == 1);
    assert(u16pow(0, 255) == 0);
    assert(u16pow(1, 255) == 1);
    for (unsigned e = 0; e < 16; ++e) {
        assert(u16pow(2, e) == (u16)((u128)1 << e));
    }
    assert(u16pow(3, 3) == (u16)pow(3, 3));
    assert(i16pow(0, 0) == 1);
    assert(i16pow(0, 255) == 0);
    assert(i16pow(1, 255) == 1);
    for (unsigned e = 0; e < 15; ++e) {
        assert(i16pow(2, e) == (i16)((u128)1 << e));
        assert(i16pow(-2, e) ==
               (e & 1 ? -(i16)((u128)1 << e) : (i16)((u128)1 << e)));
    }
    assert(i16pow(-2, 15) == (i16)((u128)1 << 15));
    assert(i16pow(3, 3) == (i16)pow(3, 3));
    assert(u32pow(0, 0) == 1);
    assert(u32pow(0, 255) == 0);
    assert(u32pow(1, 255) == 1);
    for (unsigned e = 0; e < 32; ++e) {
        assert(u32pow(2, e) == (u32)((u128)1 << e));
    }
    assert(u32pow(3, 3) == (u32)pow(3, 3));
    assert(i32pow(0, 0) == 1);
    assert(i32pow(0, 255) == 0);
    assert(i32pow(1, 255) == 1);
    for (unsigned e = 0; e < 31; ++e) {
        assert(i32pow(2, e) == (i32)((u128)1 << e));
        assert(i32pow(-2, e) ==
               (e & 1 ? -(i32)((u128)1 << e) : (i32)((u128)1 << e)));
    }
    assert(i32pow(-2, 31) == (i32)((u128)1 << 31));
    assert(i32pow(3, 3) == (i32)pow(3, 3));
    assert(u64pow(0, 0) == 1);
    assert(u64pow(0, 255) == 0);
    assert(u64pow(1, 255) == 1);
    for (unsigned e = 0; e < 64; ++e) {
        assert(u64pow(2, e) == (u64)((u128)1 << e));
    }
    assert(u64pow(3, 3) == (u64)pow(3, 3));
    assert(i64pow(0, 0) == 1);
    assert(i64pow(0, 255) == 0);
    assert(i64pow(1, 255) == 1);
    for (unsigned e = 0; e < 63; ++e) {
        assert(i64pow(2, e) == (i64)((u128)1 << e));
        assert(i64pow(-2, e) ==
               (e & 1 ? -(i64)((u128)1 << e) : (i64)((u128)1 << e)));
    }
    assert(i64pow(-2, 63) == (i64)((u128)1 << 63));
    assert(i64pow(3, 3) == (i64)pow(3, 3));
    assert(u128pow(0, 0) == 1);
    assert(u128pow(0, 255) == 0);
    assert(u128pow(1, 255) == 1);
    for (unsigned e = 0; e < 128; ++e) {
        assert(u128pow(2, e) == (u128)((u128)1 << e));
    }
    assert(u128pow(3, 3) == (u128)pow(3, 3));
    assert(i128pow(0, 0) == 1);
    assert(i128pow(0, 255) == 0);
    assert(i128pow(1, 255) == 1);
    for (unsigned e = 0; e < 127; ++e) {
        assert(i128pow(2, e) == (i128)((u128)1 << e));
        assert(i128pow(-2, e) ==
               (e & 1 ? -(i128)((u128)1 << e) : (i128)((u128)1 << e)));
    }
    assert(i128pow(-2, 127) == (i128)((u128)1 << 127));
    assert(i128pow(3, 3) == (i128)pow(3, 3));
}

static void test_f32_floats(void) {
    XRngXoshiro256ppState rng;
    xrng_xoshiro256pp_seed(&rng, 1001);
    const f32 edges[] = {0,        -0., 1,       2,       .5,
                         INFINITY, NAN, FLT_MIN, FLT_MAX, 0x1p-149f};
    for (unsigned i = 0; i < sizeof(edges) / sizeof(edges[0]); ++i) {
        f32 x = edges[i];
        close_to("f32sqrt edge", x, f32sqrt(x), sqrtf(x), 2e-5, 0);
        close_to("f32ln edge", x, f32ln(x), logf(x), 2e-5, 2e-5);
    }
    assert(signbit(f32sqrt(-0.)));
    assert(signbit(f32sin(-0.)));
    assert(f32ln(1) == 0);
    assert(f32exp(0) == 1);
    assert(f32pow(-2, 3) == -8);
    assert(f32pow(-2, 2) == 4);
    assert(isnan(f32pow(-2, .5)));
    for (unsigned i = 0; i < 3000; ++i) {
        double unit = xrng_f64range(xrng_xoshiro256pp_u64(&rng), 0, 1);
        f32 x = (f32)exp2(-120 + unit * 240);
        close_to("f32sqrt", x, f32sqrt(x), sqrtf(x), 2e-5, 0);
        close_to("f32ln", x, f32ln(x), logf(x), 2e-5, 2e-5);
        close_to("f32log2", x, f32log2(x), log2f(x), 2e-5, 2e-5);
        close_to("f32log10", x, f32log10(x), log10f(x), 2e-5, 2e-5);
        f32 base = (f32)(1.25 + unit * 10);
        close_to("f32logn", x, f32logn(x, base), logf(x) / logf(base), 2e-5,
                 2e-5);
        x = (f32)(-1e6 + 2e6 * unit);
        close_to("f32sin", x, f32sin(x), sinf(x), 2e-5, 2e-5);
        close_to("f32cos", x, f32cos(x), cosf(x), 2e-5, 2e-5);
        if (fabs(cos(x)) >= .01)
            close_to("f32tan", x, f32tan(x), tanf(x), 2e-5, 2e-5);
        x = (f32)(-80 + 160 * unit);
        close_to("f32exp", x, f32exp(x), expf(x), 2e-5, 0);
        x = (f32)(.25 + 19.75 * unit);
        close_to("f32gamma", x, f32gamma(x), tgammaf(x), 2e-5, 0);
        x = (f32)(.125 + 15.875 * unit);
        f32 e = (f32)xrng_f64range(xrng_xoshiro256pp_u64(&rng), -8, 8);
        close_to("f32pow", x, f32pow(x, e), powf(x, e), 2e-5, 0);
    }
    close_to("f32gamma negative", -.5, f32gamma(-.5), tgammaf(-.5), 2e-5, 0);
    assert(f32sqrt(4) == 2);
    assert(f32log2(8) == 3);
}

static void test_f64_floats(void) {
    XRngXoshiro256ppState rng;
    xrng_xoshiro256pp_seed(&rng, 1001);
    const f64 edges[] = {0,        -0., 1,       2,       .5,
                         INFINITY, NAN, DBL_MIN, DBL_MAX, 0x1p-1074};
    for (unsigned i = 0; i < sizeof(edges) / sizeof(edges[0]); ++i) {
        f64 x = edges[i];
        close_to("f64sqrt edge", x, f64sqrt(x), sqrt(x), 2e-12, 0);
        close_to("f64ln edge", x, f64ln(x), log(x), 2e-12, 2e-12);
    }
    assert(signbit(f64sqrt(-0.)));
    assert(signbit(f64sin(-0.)));
    assert(f64ln(1) == 0);
    assert(f64exp(0) == 1);
    assert(f64pow(-2, 3) == -8);
    assert(f64pow(-2, 2) == 4);
    assert(isnan(f64pow(-2, .5)));
    for (unsigned i = 0; i < 3000; ++i) {
        double unit = xrng_f64range(xrng_xoshiro256pp_u64(&rng), 0, 1);
        f64 x = (f64)exp2(-1000 + unit * 2000);
        close_to("f64sqrt", x, f64sqrt(x), sqrt(x), 2e-12, 0);
        close_to("f64ln", x, f64ln(x), log(x), 2e-12, 2e-12);
        close_to("f64log2", x, f64log2(x), log2(x), 2e-12, 2e-12);
        close_to("f64log10", x, f64log10(x), log10(x), 2e-12, 2e-12);
        f64 base = (f64)(1.25 + unit * 10);
        close_to("f64logn", x, f64logn(x, base), log(x) / log(base), 2e-12,
                 2e-12);
        x = (f64)(-1e6 + 2e6 * unit);
        close_to("f64sin", x, f64sin(x), sin(x), 2e-12, 2e-12);
        close_to("f64cos", x, f64cos(x), cos(x), 2e-12, 2e-12);
        if (fabs(cos(x)) >= .01)
            close_to("f64tan", x, f64tan(x), tan(x), 2e-12, 2e-12);
        x = (f64)(-700 + 1400 * unit);
        close_to("f64exp", x, f64exp(x), exp(x), 2e-12, 0);
        x = (f64)(.25 + 19.75 * unit);
        close_to("f64gamma", x, f64gamma(x), tgamma(x), 2e-12, 0);
        x = (f64)(.125 + 15.875 * unit);
        f64 e = (f64)xrng_f64range(xrng_xoshiro256pp_u64(&rng), -8, 8);
        close_to("f64pow", x, f64pow(x, e), pow(x, e), 2e-12, 0);
    }
    close_to("f64gamma negative", -.5, f64gamma(-.5), tgamma(-.5), 2e-12, 0);
    assert(f64sqrt(4) == 2);
    assert(f64log2(8) == 3);
}

// Check both neighbors of range-reduction boundaries, not just random points.
static void check_reduction_point(f64 x) {
    close_to("ln boundary", x, f64ln(x), log(x), 2e-12, 2e-12);
    close_to("log2 boundary", x, f64log2(x), log2(x), 2e-12, 2e-12);
    close_to("log10 boundary", x, f64log10(x), log10(x), 2e-12, 2e-12);
    close_to("pow boundary", x, f64pow(x, 8), pow(x, 8), 2e-12, 0);
    close_to("powf boundary", x, f32pow((f32)x, 8), powf((f32)x, 8), 2e-5, 0);
}
static void test_reduction_boundaries(void) {
    for (unsigned i = 0; i <= 128; ++i) {
        double x = 1 + i / 128.;
        check_reduction_point(nextafter(x, 0));
        check_reduction_point(x);
        check_reduction_point(nextafter(x, INFINITY));
    }
    for (int i = -64000; i <= 64000; i += 7) {
        double x = (i + .5) * .69314718055994530942 / 64;
        const double points[] = {nextafter(x, -INFINITY), x,
                                 nextafter(x, INFINITY)};
        for (unsigned j = 0; j < 3; ++j)
            close_to("exp boundary", points[j], f64exp(points[j]),
                     exp(points[j]), 2e-12, 0);
    }
    for (int i = -600000; i <= 600000; i += 101) {
        double x = i * 1.57079632679489661923;
        const double points[] = {nextafter(x, -INFINITY), x,
                                 nextafter(x, INFINITY)};
        for (unsigned j = 0; j < 3; ++j) {
            double y = points[j];
            close_to("sin boundary", y, f64sin(y), sin(y), 2e-12, 2e-12);
            close_to("cos boundary", y, f64cos(y), cos(y), 2e-12, 2e-12);
            if (fabs(cos(y)) >= .01)
                close_to("tan boundary", y, f64tan(y), tan(y), 2e-12, 2e-12);
            close_to("sinf boundary", y, f32sin((f32)y), sinf((f32)y), 2e-5,
                     2e-5);
            close_to("cosf boundary", y, f32cos((f32)y), cosf((f32)y), 2e-5,
                     2e-5);
            if (fabsf(cosf((f32)y)) >= .01)
                close_to("tanf boundary", y, f32tan((f32)y), tanf((f32)y), 2e-5,
                         2e-5);
        }
    }
    for (unsigned i = 0; i <= 20000; ++i) {
        double x = .25 + i * (19.75 / 20000);
        close_to("gamma grid", x, f64gamma(x), tgamma(x), 2e-12, 0);
        close_to("gammaf grid", x, f32gamma((f32)x), tgammaf((f32)x), 2e-5, 0);
    }
    const double exp_edges[] = {-INFINITY, -750, -745, -710,     -700, 0,
                                700,       709,  710,  INFINITY, NAN};
    for (unsigned i = 0; i < sizeof(exp_edges) / sizeof(exp_edges[0]); ++i) {
        double x = exp_edges[i];
        close_to("exp edge", x, f64exp(x), exp(x), 2e-12, 0x1p-1074);
        close_to("expf edge", x, f32exp(x), expf(x), 2e-5, 0x1p-149);
    }
}

static void test_integer_rounding(void) {
    XRngXoshiro256ppState rng;
    xrng_xoshiro256pp_seed(&rng, 2001);
    for (unsigned i = 0; i < 10000; ++i) {
        u64 r = (u32)xrng_xoshiro256pp_u64(&rng);
        u64 square = r * r;
        assert(u64sqrt(square) == r);
        if (r)
            assert(u64sqrt(square - 1) == r - 1);
        assert(u64sqrt(square + 1) == r + (r == 0));
        u128 x = (u128)xrng_xoshiro256pp_u64(&rng) << 64;
        x |= xrng_xoshiro256pp_u64(&rng);
        u128 base = (u128)2 + xrng_xoshiro256pp_u64(&rng);
        if (i & 1)
            base <<= 64;
        unsigned reference = 0;
        for (u128 v = x; v >= base; v /= base)
            ++reference;
        assert(u128logn(x, base) == reference);
        u64 n = 2ull + (u32)base;
        reference = 0;
        for (u64 v = (u64)x; v >= n; v /= n)
            ++reference;
        assert(u64logn((u64)x, n) == reference);
    }
    // Rounding to 2^64 must not turn the largest u128 root into zero.
    u128 square = (u128)(u64)-1 * (u64)-1;
    assert(u128sqrt(square - 1) == (u64)-2);
    assert(u128sqrt(square) == (u64)-1);
    assert(u128sqrt((u128)-1) == (u64)-1);
}

static void test_f32_special(void) {
    assert(isnan(f32sqrt(-1)));
    assert(isnan(f32ln(-1)));
    assert(isnan(f32sin(INFINITY)));
    assert(isnan(f32cos(-INFINITY)));
    assert(isnan(f32tan(INFINITY)));
    assert(isnan(f32gamma(NAN)));
    assert(isnan(f32gamma(-INFINITY)));
    assert(f32gamma(INFINITY) == INFINITY);
    assert(f32ln(INFINITY) == INFINITY);
    assert(f32ln(0) == -INFINITY);
    assert(f32pow(INFINITY, 0) == 1);
    assert(f32pow(1, NAN) == 1);
    assert(f32pow(NAN, 0) == 1);
    assert(isnan(f32pow(NAN, 1)));
    assert(f32pow(INFINITY, 2) == INFINITY);
    assert(f32pow(INFINITY, -2) == 0);
    assert(f32pow(0, -2) == INFINITY);
    assert(f32pow(2, INFINITY) == INFINITY);
    assert(f32pow(2, -INFINITY) == 0);
}

static void test_f64_special(void) {
    assert(isnan(f64sqrt(-1)));
    assert(isnan(f64ln(-1)));
    assert(isnan(f64sin(INFINITY)));
    assert(isnan(f64cos(-INFINITY)));
    assert(isnan(f64tan(INFINITY)));
    assert(isnan(f64gamma(NAN)));
    assert(isnan(f64gamma(-INFINITY)));
    assert(f64gamma(INFINITY) == INFINITY);
    assert(f64ln(INFINITY) == INFINITY);
    assert(f64ln(0) == -INFINITY);
    assert(f64pow(INFINITY, 0) == 1);
    assert(f64pow(1, NAN) == 1);
    assert(f64pow(NAN, 0) == 1);
    assert(isnan(f64pow(NAN, 1)));
    assert(f64pow(INFINITY, 2) == INFINITY);
    assert(f64pow(INFINITY, -2) == 0);
    assert(f64pow(0, -2) == INFINITY);
    assert(f64pow(2, INFINITY) == INFINITY);
    assert(f64pow(2, -INFINITY) == 0);
}

// Exercise f32pow's log table, rounded exp2 table index, and slow paths.
static void test_f32pow_kernel(void) {
    for (unsigned i = 0; i <= 64; ++i) {
        f32 center = 1 + i / 64.f;
        f32 points[] = {nextafterf(center, 0), center,
                        nextafterf(center, INFINITY)};
        for (unsigned j = 0; j < 3; ++j)
            for (int n = -32; n <= 32; ++n) {
                f32 e = n * .25f;
                close_to("powf log boundary", points[j], f32pow(points[j], e),
                         powf(points[j], e), 2e-5, 0);
            }
    }
    for (int n = -4800; n <= 4096; ++n) {
        f32 midpoint = (n + .5f) / 32;
        f32 points[] = {nextafterf(midpoint, -INFINITY), midpoint,
                        nextafterf(midpoint, INFINITY)};
        for (unsigned j = 0; j < 3; ++j)
            close_to("powf exp boundary", points[j], f32pow(2, points[j]),
                     powf(2, points[j]), 2e-5, 0x1p-149);
    }
    const f32 bases[] = {0x1p-149f, 0x1p-126f, FLT_MAX,  -.5f,      -2,
                         -0.f,      0.f,       INFINITY, -INFINITY, NAN};
    for (unsigned i = 0; i < sizeof(bases) / sizeof(bases[0]); ++i)
        for (int e = -2; e <= 2; ++e) {
            // The existing zero-base pole contract does not specify its sign.
            if (bases[i] == 0 && e < 0) {
                assert(isinf(f32pow(bases[i], e)));
            } else
                close_to("powf fallback", bases[i], f32pow(bases[i], e),
                         powf(bases[i], e), 2e-5, 0x1p-149);
        }
    XRngXoshiro256ppState rng;
    xrng_xoshiro256pp_seed(&rng, 3017);
    for (unsigned i = 0; i < 100000; ++i) {
        f32 x = xrng_f32range(xrng_xoshiro256pp_u64(&rng), .125f, 16.f);
        f32 e = xrng_f32range(xrng_xoshiro256pp_u64(&rng), -8.f, 8.f);
        close_to("powf kernel", x, f32pow(x, e), powf(x, e), 2e-5, 0);
    }
}

static void check_u128_root(u128 x) {
    u64 r = u128sqrt(x);
    assert((u128)r * r <= x);
    if (r != (u64)-1) {
        u128 next = (u128)r + 1;
        assert(next * next > x);
    }
}

static void test_u128sqrt_kernel(void) {
    for (u32 x = 0; x < 65536; ++x)
        check_u128_root(x);
    XRngXoshiro256ppState rng;
    xrng_xoshiro256pp_seed(&rng, 4019);
    for (unsigned i = 0; i < 200000; ++i) {
        u128 x = (u128)xrng_xoshiro256pp_u64(&rng) << 64;
        x |= xrng_xoshiro256pp_u64(&rng);
        check_u128_root(x);
        check_u128_root(x >> (i % 128));
        u64 root = xrng_xoshiro256pp_u64(&rng);
        u128 square = (u128)root * root;
        assert(u128sqrt(square) == root);
        if (root) {
            assert(u128sqrt(square - 1) == root - 1);
        }
        check_u128_root(square + 1);
    }
    // Conversion saturation and the DIV quotient limit both occur here.
    for (u64 i = 0; i < 8192; ++i) {
        u64 root = (u64)-1 - i;
        u128 square = (u128)root * root;
        assert(u128sqrt(square) == root);
        assert(u128sqrt(square - 1) == root - 1);
        check_u128_root(square + 1);
        check_u128_root((u128)root << 64);
        check_u128_root(((u128)root << 64) | (u64)-1);
    }
}

// Independent integer oracle: linear multiplication with a division guard.
static int root_power_compare(u128 a, unsigned n, u128 x) {
    u128 p = 1;
    for (unsigned i = 0; i < n; ++i) {
        if (a && p > x / a)
            return 1;
        p *= a;
    }
    return (p > x) - (p < x);
}
static void check_unsigned_root(u128 x, unsigned n, u128 r) {
    assert(root_power_compare(r, n, x) <= 0);
    if (r != (u128)-1) {
        assert(root_power_compare(r + 1, n, x) > 0);
    }
}
static void check_signed_root(i128 x, unsigned n, i128 r) {
    if (x >= 0) {
        assert(r >= 0);
        check_unsigned_root(x, n, r);
    } else {
        assert(r < 0);
        u128 a = -(u128)x, magnitude = -(u128)r;
        assert(root_power_compare(magnitude, n, a) >= 0);
        assert(root_power_compare(magnitude - 1, n, a) < 0);
    }
}

#define CHECK_UNSIGNED_ROOTS(T, X, N)                                          \
    do {                                                                       \
        T root_input = (X);                                                    \
        unsigned root_degree = (N);                                            \
        check_unsigned_root(root_input, 3, T##cbrt(root_input));               \
        check_unsigned_root(root_input, root_degree,                           \
                            T##nthroot(root_input, root_degree));              \
    } while (0)
#define CHECK_SIGNED_ROOTS(T, X, N)                                            \
    do {                                                                       \
        T root_input = (X);                                                    \
        unsigned root_degree = (N);                                            \
        if (root_input < 0)                                                    \
            root_degree |= 1;                                                  \
        check_signed_root(root_input, 3, T##cbrt(root_input));                 \
        check_signed_root(root_input, root_degree,                             \
                          T##nthroot(root_input, root_degree));                \
    } while (0)

static void test_integer_roots(void) {
    for (u32 x = 0; x <= 65535; ++x) {
        check_unsigned_root(x, 3, u16cbrt(x));
        check_signed_root((i16)x, 3, i16cbrt((i16)x));
        if (x <= 255) {
            for (unsigned n = 1; n <= 8; ++n) {
                CHECK_UNSIGNED_ROOTS(u8, x, n);
                CHECK_SIGNED_ROOTS(i8, x, n);
            }
        }
    }
    XRngXoshiro256ppState rng;
    xrng_xoshiro256pp_seed(&rng, 5011);
    for (unsigned i = 0; i < 2000; ++i) {
        u128 bits = (u128)xrng_xoshiro256pp_u64(&rng) << 64;
        bits |= xrng_xoshiro256pp_u64(&rng);
        unsigned degree = 1 + i % 127;
        CHECK_UNSIGNED_ROOTS(u8, bits, degree);
        CHECK_SIGNED_ROOTS(i8, bits, degree);
        CHECK_UNSIGNED_ROOTS(u16, bits, degree);
        CHECK_SIGNED_ROOTS(i16, bits, degree);
        CHECK_UNSIGNED_ROOTS(u32, bits, degree);
        CHECK_SIGNED_ROOTS(i32, bits, degree);
        CHECK_UNSIGNED_ROOTS(u64, bits, degree);
        CHECK_SIGNED_ROOTS(i64, bits, degree);
        CHECK_UNSIGNED_ROOTS(u128, bits, degree);
        CHECK_SIGNED_ROOTS(i128, bits, degree);
    }
    CHECK_UNSIGNED_ROOTS(u8, (u8)-1, 3);
    CHECK_SIGNED_ROOTS(i8, (i8)((u8)1 << 7), 1);
    CHECK_SIGNED_ROOTS(i8, (i8)((u8)1 << 7), 3);
    CHECK_SIGNED_ROOTS(i8, (i8)((u8)1 << 7), 127);
    assert(i8cbrt(-9) == -3);
    assert(i8cbrt(-8) == -2);
    assert(u8nthroot(0, 3) == 0);
    assert(u8nthroot(1, 127) == 1);
    CHECK_UNSIGNED_ROOTS(u16, (u16)-1, 3);
    CHECK_SIGNED_ROOTS(i16, (i16)((u16)1 << 15), 1);
    CHECK_SIGNED_ROOTS(i16, (i16)((u16)1 << 15), 3);
    CHECK_SIGNED_ROOTS(i16, (i16)((u16)1 << 15), 127);
    assert(i16cbrt(-9) == -3);
    assert(i16cbrt(-8) == -2);
    assert(u16nthroot(0, 3) == 0);
    assert(u16nthroot(1, 127) == 1);
    CHECK_UNSIGNED_ROOTS(u32, (u32)-1, 3);
    CHECK_SIGNED_ROOTS(i32, (i32)((u32)1 << 31), 1);
    CHECK_SIGNED_ROOTS(i32, (i32)((u32)1 << 31), 3);
    CHECK_SIGNED_ROOTS(i32, (i32)((u32)1 << 31), 127);
    assert(i32cbrt(-9) == -3);
    assert(i32cbrt(-8) == -2);
    assert(u32nthroot(0, 3) == 0);
    assert(u32nthroot(1, 127) == 1);
    CHECK_UNSIGNED_ROOTS(u64, (u64)-1, 3);
    CHECK_SIGNED_ROOTS(i64, (i64)((u64)1 << 63), 1);
    CHECK_SIGNED_ROOTS(i64, (i64)((u64)1 << 63), 3);
    CHECK_SIGNED_ROOTS(i64, (i64)((u64)1 << 63), 127);
    assert(i64cbrt(-9) == -3);
    assert(i64cbrt(-8) == -2);
    assert(u64nthroot(0, 3) == 0);
    assert(u64nthroot(1, 127) == 1);
    CHECK_UNSIGNED_ROOTS(u128, (u128)-1, 3);
    CHECK_SIGNED_ROOTS(i128, (i128)((u128)1 << 127), 1);
    CHECK_SIGNED_ROOTS(i128, (i128)((u128)1 << 127), 3);
    CHECK_SIGNED_ROOTS(i128, (i128)((u128)1 << 127), 127);
    assert(i128cbrt(-9) == -3);
    assert(i128cbrt(-8) == -2);
    assert(u128nthroot(0, 3) == 0);
    assert(u128nthroot(1, 127) == 1);
    assert(u128nthroot((u128)-1, (u128)-1) == 1);
    assert(i128nthroot(-9, (i128)(((u128)1 << 127) - 1)) == -2);
    assert(i128nthroot((i128)((u128)1 << 127), 127) == -2);
    for (unsigned n = 3; n <= 127; ++n) {
        u128 power = (u128)1 << n;
        assert(u128nthroot(power, n) == 2);
        assert(u128nthroot(power - 1, n) == 1);
        assert(u128nthroot(power + 1, n) == 2);
    }
}

static void test_f32_roots_mod(void) {
    XRngXoshiro256ppState rng;
    xrng_xoshiro256pp_seed(&rng, 5013);
    for (unsigned i = 0; i < 10000; ++i) {
        double unit = xrng_f64range(xrng_xoshiro256pp_u64(&rng), 0, 1);
        f32 x = (f32)exp2(-149 + (276) * unit);
        if (i & 1)
            x = -x;
        close_to("f32cbrt", x, f32cbrt(x), cbrtf(x), 2e-5, 0);
        f32 n = 1 + i % 31;
        if (x < 0 && (int)n % 2 == 0)
            n += 1;
        if (i & 2)
            n = -n;
        double expected = pow(fabs((double)x), 1. / n);
        if (x < 0)
            expected = -expected;
        close_to("f32nthroot", x, f32nthroot(x, n), (f32)expected, 2e-5,
                 0x1p-149);
        f32 y = (f32)exp2(
            -149 + (276) * xrng_f64range(xrng_xoshiro256pp_u64(&rng), 0, 1));
        if (i & 2)
            y = -y;
        f32 fractional_n = (f32)(1 + unit * 64);
        if (i & 4)
            fractional_n = -fractional_n;
        close_to("f32 fractional nthroot", x,
                 f32nthroot((f32)fabs(x), fractional_n),
                 (f32)pow(fabs(x), 1. / fractional_n), 2e-5, 0x1p-149);
        f32 actual = f32mod(x, y), reference = fmodf(x, y);
        assert(actual == reference);
        assert(!!signbit(actual) == !!signbit(reference));
    }
    const f32 values[] = {0,       -0.,       1,        -1,        8,
                          -8,      9,         -9,       FLT_MAX,   -FLT_MAX,
                          FLT_MIN, 0x1p-149f, INFINITY, -INFINITY, NAN};
    for (unsigned i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
        f32 x = values[i];
        close_to("f32cbrt edge", x, f32cbrt(x), cbrtf(x), 2e-5, 0);
        if (x == 0)
            assert(!!signbit(f32cbrt(x)) == !!signbit(x));
        for (unsigned j = 0; j < sizeof(values) / sizeof(values[0]); ++j) {
            f32 y = values[j];
            f32 actual = f32mod(x, y), reference = fmodf(x, y);
            if (isnan(reference)) {
                assert(isnan(actual));
            } else {
                assert(actual == reference);
                assert(!!signbit(actual) == !!signbit(reference));
            }
        }
    }
    close_to("f32 fractional degree", 9, f32nthroot(9, 1.5), pow(9, 1. / 1.5),
             2e-5, 0);
    close_to("f32 negative degree", -8, f32nthroot(-8, -3), -.5, 2e-5, 0);
    assert(isnan(f32nthroot(-8, 2)));
    assert(isnan(f32nthroot(-8, 2.5)));
    assert(isnan(f32nthroot(8, 0)));
    assert(isnan(f32nthroot(8, NAN)));
    assert(isnan(f32nthroot(8, INFINITY)));
    assert(isnan(f32nthroot(NAN, 3)));
    assert(f32nthroot(INFINITY, 3) == INFINITY);
    assert(f32nthroot(-INFINITY, 3) == -INFINITY);
    assert(f32nthroot(INFINITY, -3) == 0);
    assert(f32nthroot(0, -3) == INFINITY);
    assert(f32nthroot(-0., -3) == -INFINITY);
    assert(signbit(f32nthroot(-0., 3)));
    assert(f32mod(-5.5, 2) == -1.5);
}

static void test_f64_roots_mod(void) {
    XRngXoshiro256ppState rng;
    xrng_xoshiro256pp_seed(&rng, 5013);
    for (unsigned i = 0; i < 10000; ++i) {
        double unit = xrng_f64range(xrng_xoshiro256pp_u64(&rng), 0, 1);
        f64 x = (f64)exp2(-1074 + (2097) * unit);
        if (i & 1)
            x = -x;
        close_to("f64cbrt", x, f64cbrt(x), cbrt(x), 2e-12, 0);
        f64 n = 1 + i % 31;
        if (x < 0 && (int)n % 2 == 0)
            n += 1;
        if (i & 2)
            n = -n;
        double expected = pow(fabs((double)x), 1. / n);
        if (x < 0)
            expected = -expected;
        close_to("f64nthroot", x, f64nthroot(x, n), (f64)expected, 2e-12,
                 0x1p-1074);
        f64 y = (f64)exp2(
            -1074 + (2097) * xrng_f64range(xrng_xoshiro256pp_u64(&rng), 0, 1));
        if (i & 2)
            y = -y;
        f64 fractional_n = (f64)(1 + unit * 64);
        if (i & 4)
            fractional_n = -fractional_n;
        close_to("f64 fractional nthroot", x,
                 f64nthroot((f64)fabs(x), fractional_n),
                 (f64)pow(fabs(x), 1. / fractional_n), 2e-12, 0x1p-1074);
        f64 actual = f64mod(x, y), reference = fmod(x, y);
        assert(actual == reference);
        assert(!!signbit(actual) == !!signbit(reference));
    }
    const f64 values[] = {0,       -0.,       1,        -1,        8,
                          -8,      9,         -9,       DBL_MAX,   -DBL_MAX,
                          DBL_MIN, 0x1p-1074, INFINITY, -INFINITY, NAN};
    for (unsigned i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
        f64 x = values[i];
        close_to("f64cbrt edge", x, f64cbrt(x), cbrt(x), 2e-12, 0);
        if (x == 0)
            assert(!!signbit(f64cbrt(x)) == !!signbit(x));
        for (unsigned j = 0; j < sizeof(values) / sizeof(values[0]); ++j) {
            f64 y = values[j];
            f64 actual = f64mod(x, y), reference = fmod(x, y);
            if (isnan(reference)) {
                assert(isnan(actual));
            } else {
                assert(actual == reference);
                assert(!!signbit(actual) == !!signbit(reference));
            }
        }
    }
    close_to("f64 fractional degree", 9, f64nthroot(9, 1.5), pow(9, 1. / 1.5),
             2e-12, 0);
    close_to("f64 negative degree", -8, f64nthroot(-8, -3), -.5, 2e-12, 0);
    assert(isnan(f64nthroot(-8, 2)));
    assert(isnan(f64nthroot(-8, 2.5)));
    assert(isnan(f64nthroot(8, 0)));
    assert(isnan(f64nthroot(8, NAN)));
    assert(isnan(f64nthroot(8, INFINITY)));
    assert(isnan(f64nthroot(NAN, 3)));
    assert(f64nthroot(INFINITY, 3) == INFINITY);
    assert(f64nthroot(-INFINITY, 3) == -INFINITY);
    assert(f64nthroot(INFINITY, -3) == 0);
    assert(f64nthroot(0, -3) == INFINITY);
    assert(f64nthroot(-0., -3) == -INFINITY);
    assert(signbit(f64nthroot(-0., 3)));
    assert(f64mod(-5.5, 2) == -1.5);
}

static void test_libm_regressions(void) {
    // Cancellation immediately around one used to round double logs to zero.
    volatile f64 near_one[] = {0x1.fffffffffffffp-1, 0x1.0000000000001p+0};
    volatile f32 near_one_f[] = {0x1.fffffep-1f, 0x1.000002p+0f};
    for (unsigned i = 0; i < 2; ++i) {
        f64 x = near_one[i];
        f32 y = near_one_f[i];
        assert(f64ln(x) == log(x) && f64ln(x) != 0);
        assert(f64log2(x) == log2(x));
        assert(f64log10(x) == log10(x));
        assert(f32ln(y) == logf(y) && f32ln(y) != 0);
        assert(f32log2(y) == log2f(y));
        assert(f32log10(y) == log10f(y));
    }
    // Exercise full-range reduction, gamma poles and signed-zero powers.
    volatile f64 huge = 1e200;
    volatile f32 huge_f = 1e30f;
    assert(f64sin(huge) == sin(huge));
    assert(f64cos(huge) == cos(huge));
    assert(f64tan(huge) == tan(huge));
    assert(f32sin(huge_f) == sinf(huge_f));
    assert(f32cos(huge_f) == cosf(huge_f));
    assert(f32tan(huge_f) == tanf(huge_f));
    assert(f64gamma(0.) == INFINITY && f64gamma(-0.) == -INFINITY);
    assert(f32gamma(0.) == INFINITY && f32gamma(-0.) == -INFINITY);
    assert(isnan(f64gamma(-2)) && isnan(f32gamma(-2)));
    assert(f64pow(-0., -3) == -INFINITY);
    assert(f32pow(-0., -3) == -INFINITY);
    assert(signbit(f64pow(-0., 3)) && signbit(f32pow(-0., 3)));
}

static void test_nthroot_correction(void) {
    const f64 pairs[][2] = {{0x1p-1000, 7.1},
                            {0x1p1000, 7.1},
                            {0x1p-1000, .95},
                            {0x1p1000, .95},
                            {0x1p-1000, .9},
                            {0x1p1000, .98},
                            {0x1.0000000000001p0, 0x1p-52},
                            {0x1.fffffffffffffp-1, 0x1p-52},
                            {2, 0x1p-1074},
                            {.5, 0x1p-1074},
                            {1, 0x1p-1074},
                            {16, 4},
                            {256, -8},
                            {DBL_MAX, DBL_MAX}};
    for (usize i = 0; i < sizeof pairs / sizeof *pairs; ++i) {
        f64 x = pairs[i][0], n = pairs[i][1];
        f64 expected = (f64)powl((long double)x, 1.0L / (long double)n);
        f64 actual = f64nthroot(x, n);
        if (actual == expected)
            continue;
        assert(isfinite(actual) && isfinite(expected));
        const f64 ulp = nextafter(expected, INFINITY) - expected;
        assert(fabs(actual - expected) <= 4 * ulp);
    }
}

// A separate base-2^32 schoolbook product checks wide roots without relying
// on libm rounding or the implementation's 64-bit squaring formula.
static void hypot_square(u128 x, u32 out[9]) {
    for (unsigned i = 0; i < 9; ++i)
        out[i] = 0;
    for (unsigned i = 0; i < 4; ++i) {
        u64 carry = 0;
        for (unsigned j = 0; j < 4; ++j) {
            const u64 v = (u64)(u32)(x >> (32 * i)) * (u32)(x >> (32 * j)) +
                          out[i + j] + carry;
            out[i + j] = (u32)v;
            carry = v >> 32;
        }
        out[i + 4] = (u32)carry;
    }
}

static int hypot_compare(const u32 a[9], const u32 b[9]) {
    for (unsigned i = 9; i-- > 0;)
        if (a[i] != b[i])
            return a[i] > b[i] ? 1 : -1;
    return 0;
}

static void check_integer_hypot(u128 x, u128 y, u128 root) {
    u32 sum[9], other[9], square[9], next[9];
    hypot_square(x, sum);
    hypot_square(y, other);
    u64 carry = 0;
    for (unsigned i = 0; i < 9; ++i) {
        const u64 v = (u64)sum[i] + other[i] + carry;
        sum[i] = (u32)v;
        carry = v >> 32;
    }
    hypot_square(root, square);
    hypot_square(root + 1, next);
    if (root == (u128)-1)
        next[8] = 1; // (2^128)^2, without wrapping the next root.
    assert(hypot_compare(square, sum) <= 0);
    assert(hypot_compare(next, sum) > 0);
}

#define CHECK_HYPOT_WIDTH(U, I)                                                \
    do {                                                                       \
        const U max = (U) - 1;                                                 \
        assert(U##hypot(max, 0) == max);                                       \
        assert(U##hypot(max, 1) == max);                                       \
        assert(U##hypot(0, 0) == 0);                                           \
        U a = (U)x >> 1, b = (U)y >> 1;                                        \
        const U h = U##hypot(a, b);                                            \
        check_integer_hypot(a, b, h);                                          \
        assert(U##hypot(b, a) == h);                                           \
        const I sa = (I)a, sb = (I)b;                                          \
        const U sh = I##hypot(-sa, sb);                                        \
        check_integer_hypot(a, b, (U)sh);                                      \
        assert(I##hypot(sa, -sb) == sh);                                       \
        assert(I##hypot(-sa, -sb) == sh);                                      \
        const I imax = (I)(max >> 1);                                          \
        assert(I##hypot(-imax, 0) == imax);                                    \
        const I imin = -imax - 1;                                              \
        const U magnitude = (U)imax + 1;                                       \
        assert(I##hypot(imin, 0) == magnitude);                                \
        check_integer_hypot(magnitude, magnitude, I##hypot(imin, imin));       \
        check_integer_hypot((U)imax, magnitude, I##hypot(imax, imin));         \
        _Static_assert(                                                        \
            __builtin_types_compatible_p(__typeof__(I##hypot(0, 0)), U),       \
            "signed hypot returns unsigned");                                  \
        assert(I##hypot(imax, 1) == (U)imax);                                  \
    } while (0)

static void test_integer_hypot(void) {
    // Exhaust all representable u8 results, including near-square boundaries.
    for (u32 a = 0; a <= 255; ++a)
        for (u32 b = 0; b <= 255; ++b) {
            const u32 sum = a * a + b * b;
            if (sum >= 256 * 256)
                continue;
            const u32 h = u8hypot(a, b);
            assert(h * h <= sum && (h + 1) * (h + 1) > sum);
            if (a <= 127 && b <= 127)
                assert(i8hypot(-(i8)a, (i8)b) == h);
        }
    XRngXoshiro256ppState rng;
    xrng_xoshiro256pp_seed(&rng, 731);
    for (unsigned i = 0; i < 10000; ++i) {
        const u128 x = (((u128)xrng_xoshiro256pp_u64(&rng) << 64) |
                        xrng_xoshiro256pp_u64(&rng)) >>
                       (i % 3 ? i % 128 : 0);
        const u128 y = (((u128)xrng_xoshiro256pp_u64(&rng) << 64) |
                        xrng_xoshiro256pp_u64(&rng)) >>
                       (i % 3 ? (i / 128) % 128 : 0);
        CHECK_HYPOT_WIDTH(u8, i8);
        CHECK_HYPOT_WIDTH(u16, i16);
        CHECK_HYPOT_WIDTH(u32, i32);
        CHECK_HYPOT_WIDTH(u64, i64);
        CHECK_HYPOT_WIDTH(u128, i128);
    }
    // Large exact Pythagorean triples and neighbors exercise both correction
    // directions at every scale, including carries between 128-bit halves.
    for (unsigned bit = 0; bit < 126; ++bit) {
        const u128 k = (u128)1 << bit;
        assert(u128hypot(3 * k, 4 * k) == 5 * k);
        assert(u128hypot(3 * k, 4 * k - 1) == 5 * k - 1);
        assert(u128hypot(3 * k, 4 * k + 1) == 5 * k);
    }
    const u128 k = (u128)-1 / 5;
    assert(u128hypot(3 * k, 4 * k) == 5 * k);
    check_integer_hypot((u64)-1, (u64)-1, u128hypot((u64)-1, (u64)-1));
    check_integer_hypot((u128)-1, (u128)1 << 64,
                        u128hypot((u128)-1, (u128)1 << 64));
}

static void same_float_result(f64 actual, f64 expected) {
    assert((isnan(actual) && isnan(expected)) || actual == expected);
    if (actual == 0)
        assert(!!signbit(actual) == !!signbit(expected));
}

#define CHECK_HYPERBOLIC(T, NAME, LIB, X)                                      \
    do {                                                                       \
        T (*volatile reference)(T) = LIB;                                      \
        same_float_result(T##NAME(X), reference(X));                           \
    } while (0)

#define TEST_HYPERBOLIC(T, SUFFIX)                                             \
    static void check_##T##_hyperbolic(T x, T y) {                             \
        CHECK_HYPERBOLIC(T, sinh, sinh##SUFFIX, x);                            \
        CHECK_HYPERBOLIC(T, cosh, cosh##SUFFIX, x);                            \
        CHECK_HYPERBOLIC(T, tanh, tanh##SUFFIX, x);                            \
        CHECK_HYPERBOLIC(T, asinh, asinh##SUFFIX, x);                          \
        CHECK_HYPERBOLIC(T, acosh, acosh##SUFFIX, x);                          \
        CHECK_HYPERBOLIC(T, atanh, atanh##SUFFIX, x);                          \
        T (*volatile reference)(T, T) = hypot##SUFFIX;                         \
        same_float_result(T##hypot(x, y), reference(x, y));                    \
    }

TEST_HYPERBOLIC(f32, f)
TEST_HYPERBOLIC(f64, )

static void test_hyperbolic(void) {
    const f64 edges[] = {0.,
                         -0.,
                         1.,
                         -1.,
                         .5,
                         -.5,
                         2.,
                         -2.,
                         20.,
                         -20.,
                         89.,
                         -89.,
                         710.,
                         -710.,
                         711.,
                         -711.,
                         0x1.fffffffffffffp-1,
                         0x1.0000000000001p0,
                         0x1.fffffep-1,
                         0x1.000002p0,
                         0x1p-1074,
                         -0x1p-1074,
                         DBL_MIN,
                         -DBL_MIN,
                         0x1p-149,
                         -0x1p-149,
                         FLT_MIN,
                         -FLT_MIN,
                         FLT_MAX,
                         -FLT_MAX,
                         DBL_MAX,
                         -DBL_MAX,
                         INFINITY,
                         -INFINITY,
                         NAN};
    for (usize i = 0; i < sizeof(edges) / sizeof(*edges); ++i)
        for (usize j = 0; j < sizeof(edges) / sizeof(*edges); ++j) {
            check_f32_hyperbolic((f32)edges[i], (f32)edges[j]);
            check_f64_hyperbolic(edges[i], edges[j]);
        }
    XRngXoshiro256ppState rng;
    xrng_xoshiro256pp_seed(&rng, 733);
    for (unsigned i = 0; i < 10000; ++i) {
        union {
            u64 bits;
            f64 value;
        } x = {xrng_xoshiro256pp_u64(&rng)}, y = {xrng_xoshiro256pp_u64(&rng)};
        union {
            u32 bits;
            f32 value;
        } a = {(u32)x.bits}, b = {(u32)y.bits};
        check_f32_hyperbolic(a.value, b.value);
        check_f64_hyperbolic(x.value, y.value);
    }
    assert(f32hypot(INFINITY, NAN) == INFINITY);
    assert(f64hypot(NAN, -INFINITY) == INFINITY);
}

int main(void) {
    test_integer_hypot();
    test_hyperbolic();
    test_nthroot_correction();
    test_libm_regressions();
    test_f32pow_kernel();
    test_u128sqrt_kernel();
    test_integer_roots();
    test_f32_roots_mod();
    test_f64_roots_mod();
    test_integers();
    test_integer_rounding();
    test_reduction_boundaries();
    test_f32_special();
    test_f64_special();
    test_f32_floats();
    test_f64_floats();
    return 0;
}
