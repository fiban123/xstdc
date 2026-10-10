#pragma once

#include "types.h"

// unless otherwise specified, the following holds true for all functions:
//  * Floating point results that are mathematically undefined may return any
//    value, but may not cause a SIGSEGV or other fatal exception.
//  * For integers, the floor of the real value is returned. Integer domains
//    stated below are unchecked preconditions; violating them is undefined.
//  * Floating-point transcendental accuracy follows the platform libm.
//    Composite operations are not guaranteed to be correctly rounded.
//  * Overflows (e.g. 10^255) may return any value - but may not cause a SIGSEGV
//  or other fatal exception.

// Override with an empty definition in one translation unit to emit symbols.
#ifndef XMATH_EXTERN_INLINE
#define XMATH_EXTERN_INLINE inline
#endif

// Keep libm names out of unoptimized consumers and their language servers.
#if defined(__OPTIMIZE__)
// Natural logarithm from libm.
extern float logf(float x);
// Base-two logarithm from libm.
extern float log2f(float x);
// Base-ten logarithm from libm.
extern float log10f(float x);
// Exponential from libm.
extern float expf(float x);
// Sine in radians from libm.
extern float sinf(float x);
// Cosine in radians from libm.
extern float cosf(float x);
// Tangent in radians from libm.
extern float tanf(float x);
// Gamma function from libm.
extern float tgammaf(float x);
// Power from libm.
extern float powf(float x, float y);
// Real cube root from libm.
extern float cbrtf(float x);
// Natural logarithm from libm.
extern double log(double x);
// Base-two logarithm from libm.
extern double log2(double x);
// Base-ten logarithm from libm.
extern double log10(double x);
// Exponential from libm.
extern double exp(double x);
// Sine in radians from libm.
extern double sin(double x);
// Cosine in radians from libm.
extern double cos(double x);
// Tangent in radians from libm.
extern double tan(double x);
// Gamma function from libm.
extern double tgamma(double x);
// Power from libm.
extern double pow(double x, double y);
// Real cube root from libm.
extern double cbrt(double x);
// Hyperbolic sine from libm.
extern float sinhf(float x);
// Hyperbolic cosine from libm.
extern float coshf(float x);
// Hyperbolic tangent from libm.
extern float tanhf(float x);
// Inverse hyperbolic sine from libm.
extern float asinhf(float x);
// Inverse hyperbolic cosine from libm.
extern float acoshf(float x);
// Inverse hyperbolic tangent from libm.
extern float atanhf(float x);
// Euclidean length from libm, avoiding intermediate overflow/underflow.
extern float hypotf(float x, float y);
// Hyperbolic sine from libm.
extern double sinh(double x);
// Hyperbolic cosine from libm.
extern double cosh(double x);
// Hyperbolic tangent from libm.
extern double tanh(double x);
// Inverse hyperbolic sine from libm.
extern double asinh(double x);
// Inverse hyperbolic cosine from libm.
extern double acosh(double x);
// Inverse hyperbolic tangent from libm.
extern double atanh(double x);
// Euclidean length from libm, avoiding intermediate overflow/underflow.
extern double hypot(double x, double y);
// Extended-precision power from libm.
extern long double powl(long double x, long double y);
#endif

#if defined(__OPTIMIZE__) || defined(XMATH_IMPLEMENTATION)
#define XMATH_LIBM_INLINE XMATH_EXTERN_INLINE
#else
#define XMATH_LIBM_INLINE extern
#endif

#define XMATH_MIN(a, b) (a > b ? b : a)
#define XMATH_MAX(a, b) (a > b ? a : b)
#define XMATH_ABS(a) (a < 0 ? -a : a)

// Return the smaller value.
static inline u128 u128min(u128 a, u128 b);

// Return the smaller value.
static inline i128 i128min(i128 a, i128 b);

// Return the smaller value.
static inline u64 u64min(u64 a, u64 b);

// Return the smaller value.
static inline i64 i64min(i64 a, i64 b);

// Return the smaller value.
static inline u32 u32min(u32 a, u32 b);

// Return the smaller value.
static inline i32 i32min(i32 a, i32 b);

// Return the smaller value.
static inline u16 u16min(u16 a, u16 b);

// Return the smaller value.
static inline i16 i16min(i16 a, i16 b);

// Return the smaller value.
static inline u8 u8min(u8 a, u8 b);

// Return the smaller value.
static inline i8 i8min(i8 a, i8 b);

// Return the larger value.
static inline u128 u128max(u128 a, u128 b);

// Return the larger value.
static inline i128 i128max(i128 a, i128 b);

// Return the larger value.
static inline u64 u64max(u64 a, u64 b);

// Return the larger value.
static inline i64 i64max(i64 a, i64 b);

// Return the larger value.
static inline u32 u32max(u32 a, u32 b);

// Return the larger value.
static inline i32 i32max(i32 a, i32 b);

// Return the larger value.
static inline u16 u16max(u16 a, u16 b);

// Return the larger value.
static inline i16 i16max(i16 a, i16 b);

// Return the larger value.
static inline u8 u8max(u8 a, u8 b);

// Return the larger value.
static inline i8 i8max(i8 a, i8 b);

// Return the smaller value.
static inline f64 f64min(f64 a, f64 b);

// Return the smaller value.
static inline f32 f32min(f32 a, f32 b);

// Return the larger value.
static inline f64 f64max(f64 a, f64 b);

// Return the larger value.
static inline f32 f32max(f32 a, f32 b);

// Return the absolute value.
static inline u128 i128abs(i128 x);

// Return the absolute value.
static inline u64 i64abs(i64 x);

// Return the absolute value.
static inline u32 i32abs(i32 x);

// Return the absolute value.
static inline u16 i16abs(i16 x);

// Return the absolute value.
static inline u8 i8abs(i8 x);

// Return the absolute value.
static inline f64 f64abs(f64 x);

// Return the absolute value.
static inline f32 f32abs(f32 x);

// Return the square root of x.
XMATH_EXTERN_INLINE f64 f64sqrt(f64 x);
// Return the square root of x.
XMATH_EXTERN_INLINE f32 f32sqrt(f32 x);
// Return the floor of the square root of x.
XMATH_EXTERN_INLINE u64 u64sqrt(u64 x);
// Return the floor of the square root of x.
XMATH_EXTERN_INLINE u64 u128sqrt(u128 x);
// Return the floor of the square root of x.
XMATH_EXTERN_INLINE u32 u32sqrt(u32 x);
// Return the floor of the square root of x.
XMATH_EXTERN_INLINE u16 u16sqrt(u16 x);
// Return the floor of the square root of x.
XMATH_EXTERN_INLINE u8 u8sqrt(u8 x);

// Return floor(log2(x)); requires x > 0.
XMATH_EXTERN_INLINE u8 u64log2(u64 x);
// Return floor(log2(x)); requires x > 0.
XMATH_EXTERN_INLINE u8 u32log2(u32 x);
// Return floor(log2(x)); requires x > 0.
XMATH_EXTERN_INLINE u8 u16log2(u16 x);
// Return floor(log2(x)); requires x > 0.
XMATH_EXTERN_INLINE u8 u8log2(u8 x);
// Return floor(log2(x)); requires x > 0.
XMATH_EXTERN_INLINE u8 u128log2(u128 x);

// Return floor(logn(x)); requires x > 0 and n >= 2.
XMATH_EXTERN_INLINE u8 u64logn(u64 x, u64 n);
// Return floor(logn(x)); requires x > 0 and n >= 2.
XMATH_EXTERN_INLINE u8 u128logn(u128 x, u128 n);
// Return floor(logn(x)); requires x > 0 and n >= 2.
XMATH_EXTERN_INLINE u8 u32logn(u32 x, u32 n);
// Return floor(logn(x)); requires x > 0 and n >= 2.
XMATH_EXTERN_INLINE u8 u16logn(u16 x, u16 n);
// Return floor(logn(x)); requires x > 0 and n >= 2.
XMATH_EXTERN_INLINE u8 u8logn(u8 x, u8 n);
// Return floor(log10(x)); requires x > 0.
XMATH_EXTERN_INLINE u8 u64log10(u64 x);
// Return floor(log10(x)); requires x > 0.
XMATH_EXTERN_INLINE u8 u128log10(u128 x);
// Return floor(log10(x)); requires x > 0.
XMATH_EXTERN_INLINE u8 u32log10(u32 x);
// Return floor(log10(x)); requires x > 0.
XMATH_EXTERN_INLINE u8 u16log10(u16 x);
// Return floor(log10(x)); requires x > 0.
XMATH_EXTERN_INLINE u8 u8log10(u8 x);

// Return x raised to e.
XMATH_EXTERN_INLINE u128 u128pow(u128 x, u8 e);
// Return x raised to e.
XMATH_EXTERN_INLINE i128 i128pow(i128 x, u8 e);
// Return x raised to e.
XMATH_EXTERN_INLINE u64 u64pow(u64 x, u8 e);
// Return x raised to e.
XMATH_EXTERN_INLINE i64 i64pow(i64 x, u8 e);
// Return x raised to e.
XMATH_EXTERN_INLINE u32 u32pow(u32 x, u8 e);
// Return x raised to e.
XMATH_EXTERN_INLINE i32 i32pow(i32 x, u8 e);
// Return x raised to e.
XMATH_EXTERN_INLINE u16 u16pow(u16 x, u8 e);
// Return x raised to e.
XMATH_EXTERN_INLINE i16 i16pow(i16 x, u8 e);
// Return x raised to e.
XMATH_EXTERN_INLINE u8 u8pow(u8 x, u8 e);
// Return x raised to e.
XMATH_EXTERN_INLINE i8 i8pow(i8 x, u8 e);

// Transcendental functions use libm. Composite logn/nthroot operations are not
// guaranteed to be correctly rounded.

// Approximate sine of x in radians.
XMATH_LIBM_INLINE f32 f32sin(f32 x);
// Approximate cosine of x in radians.
XMATH_LIBM_INLINE f32 f32cos(f32 x);
// Approximate tangent of x in radians.
XMATH_LIBM_INLINE f32 f32tan(f32 x);

// Approximate the natural logarithm of x.
XMATH_LIBM_INLINE f32 f32ln(f32 x);
// Approximate the base-2 logarithm of x.
XMATH_LIBM_INLINE f32 f32log2(f32 x);
// Approximate the base-10 logarithm of x.
XMATH_LIBM_INLINE f32 f32log10(f32 x);
// Approximate e raised to x.
XMATH_LIBM_INLINE f32 f32exp(f32 x);
// Approximate the gamma function at x.
XMATH_LIBM_INLINE f32 f32gamma(f32 x);
// Approximate the base-n logarithm of x.
XMATH_LIBM_INLINE f32 f32logn(f32 x, f32 n);
// Return x raised to e.
XMATH_LIBM_INLINE f32 f32pow(f32 x, f32 e);
// Approximate the natural logarithm of x.
XMATH_LIBM_INLINE f64 f64ln(f64 x);
// Approximate the base-2 logarithm of x.
XMATH_LIBM_INLINE f64 f64log2(f64 x);
// Approximate the base-10 logarithm of x.
XMATH_LIBM_INLINE f64 f64log10(f64 x);
// Approximate sine of x in radians.
XMATH_LIBM_INLINE f64 f64sin(f64 x);
// Approximate cosine of x in radians.
XMATH_LIBM_INLINE f64 f64cos(f64 x);
// Approximate tangent of x in radians.
XMATH_LIBM_INLINE f64 f64tan(f64 x);
// Approximate e raised to x.
XMATH_LIBM_INLINE f64 f64exp(f64 x);
// Approximate the gamma function at x.
XMATH_LIBM_INLINE f64 f64gamma(f64 x);
// Return log(x) / log(n) in double precision.
XMATH_LIBM_INLINE f64 f64logn(f64 x, f64 n);
// Return x raised to e.
XMATH_LIBM_INLINE f64 f64pow(f64 x, f64 e);

// Compare a^n with limit without overflowing. Integer roots use this to correct
// a floating estimate; their final answers do not depend on its error.
XMATH_EXTERN_INLINE int xmath_power_compare(u128 a, unsigned n, u128 limit);

// Return the floor of the nth root; requires n > 0 and odd n for negative x.
XMATH_EXTERN_INLINE u128 u128nthroot(u128 x, u128 n);
// Return the floor of the real cube root of x.
XMATH_EXTERN_INLINE u128 u128cbrt(u128 x);
// Return the floor of the nth root; requires n > 0 and odd n for negative x.
XMATH_EXTERN_INLINE i128 i128nthroot(i128 x, i128 n);
// Return the floor of the real cube root of x.
XMATH_EXTERN_INLINE i128 i128cbrt(i128 x);
// Return the floor of the nth root; requires n > 0 and odd n for negative x.
XMATH_EXTERN_INLINE u64 u64nthroot(u64 x, u64 n);
// Return the floor of the real cube root of x.
XMATH_EXTERN_INLINE u64 u64cbrt(u64 x);
// Return the floor of the nth root; requires n > 0 and odd n for negative x.
XMATH_EXTERN_INLINE i64 i64nthroot(i64 x, i64 n);
// Return the floor of the real cube root of x.
XMATH_EXTERN_INLINE i64 i64cbrt(i64 x);
// Return the floor of the nth root; requires n > 0 and odd n for negative x.
XMATH_EXTERN_INLINE u32 u32nthroot(u32 x, u32 n);
// Return the floor of the real cube root of x.
XMATH_EXTERN_INLINE u32 u32cbrt(u32 x);
// Return the floor of the nth root; requires n > 0 and odd n for negative x.
XMATH_EXTERN_INLINE i32 i32nthroot(i32 x, i32 n);
// Return the floor of the real cube root of x.
XMATH_EXTERN_INLINE i32 i32cbrt(i32 x);
// Return the floor of the nth root; requires n > 0 and odd n for negative x.
XMATH_EXTERN_INLINE u16 u16nthroot(u16 x, u16 n);
// Return the floor of the real cube root of x.
XMATH_EXTERN_INLINE u16 u16cbrt(u16 x);
// Return the floor of the nth root; requires n > 0 and odd n for negative x.
XMATH_EXTERN_INLINE i16 i16nthroot(i16 x, i16 n);
// Return the floor of the real cube root of x.
XMATH_EXTERN_INLINE i16 i16cbrt(i16 x);
// Return the floor of the nth root; requires n > 0 and odd n for negative x.
XMATH_EXTERN_INLINE u8 u8nthroot(u8 x, u8 n);
// Return the floor of the real cube root of x.
XMATH_EXTERN_INLINE u8 u8cbrt(u8 x);
// Return the floor of the nth root; requires n > 0 and odd n for negative x.
XMATH_EXTERN_INLINE i8 i8nthroot(i8 x, i8 n);
// Return the floor of the real cube root of x.
XMATH_EXTERN_INLINE i8 i8cbrt(i8 x);

// Approximate the real cube root of x, including negative inputs.
XMATH_LIBM_INLINE f64 f64cbrt(f64 x);
// Approximate the real cube root of x, including negative inputs.
XMATH_LIBM_INLINE f32 f32cbrt(f32 x);

// Compute the nth root with reciprocal correction, targeting round-to-nearest.
// Requires finite nonzero n; negative x requires an odd integer n.
XMATH_LIBM_INLINE f64 f64nthroot(f64 x, f64 n);
// Approximate the nth root; negative x requires an odd integer n, and n != 0.
XMATH_LIBM_INLINE f32 f32nthroot(f32 x, f32 n);

// Return x - trunc(x / y) * y, retaining the sign of x, including signed zero.
XMATH_EXTERN_INLINE f64 f64mod(f64 x, f64 y);
// Return x - trunc(x / y) * y, retaining the sign of x, including signed zero.
XMATH_EXTERN_INLINE f32 f32mod(f32 x, f32 y);

// Hyperbolic sine.
XMATH_LIBM_INLINE f32 f32sinh(f32 x);
// Hyperbolic cosine.
XMATH_LIBM_INLINE f32 f32cosh(f32 x);
// Hyperbolic tangent.
XMATH_LIBM_INLINE f32 f32tanh(f32 x);
// Inverse hyperbolic sine.
XMATH_LIBM_INLINE f32 f32asinh(f32 x);
// Inverse hyperbolic cosine; real domain x >= 1.
XMATH_LIBM_INLINE f32 f32acosh(f32 x);
// Inverse hyperbolic tangent; real domain -1 < x < 1.
XMATH_LIBM_INLINE f32 f32atanh(f32 x);
// Euclidean length, avoiding intermediate overflow/underflow.
XMATH_LIBM_INLINE f32 f32hypot(f32 x, f32 y);

// Hyperbolic sine.
XMATH_LIBM_INLINE f64 f64sinh(f64 x);
// Hyperbolic cosine.
XMATH_LIBM_INLINE f64 f64cosh(f64 x);
// Hyperbolic tangent.
XMATH_LIBM_INLINE f64 f64tanh(f64 x);
// Inverse hyperbolic sine.
XMATH_LIBM_INLINE f64 f64asinh(f64 x);
// Inverse hyperbolic cosine; real domain x >= 1.
XMATH_LIBM_INLINE f64 f64acosh(f64 x);
// Inverse hyperbolic tangent; real domain -1 < x < 1.
XMATH_LIBM_INLINE f64 f64atanh(f64 x);
// Euclidean length, avoiding intermediate overflow/underflow.
XMATH_LIBM_INLINE f64 f64hypot(f64 x, f64 y);

// Return floor(sqrt(x*x + y*y)); the result must fit u8.
XMATH_EXTERN_INLINE u8 u8hypot(u8 x, u8 y);
// Return floor(sqrt(x*x + y*y)); all signed inputs fit the unsigned result.
XMATH_EXTERN_INLINE u8 i8hypot(i8 x, i8 y);
// Return floor(sqrt(x*x + y*y)); the result must fit u16.
XMATH_EXTERN_INLINE u16 u16hypot(u16 x, u16 y);
// Return floor(sqrt(x*x + y*y)); all signed inputs fit the unsigned result.
XMATH_EXTERN_INLINE u16 i16hypot(i16 x, i16 y);
// Return floor(sqrt(x*x + y*y)); the result must fit u32.
XMATH_EXTERN_INLINE u32 u32hypot(u32 x, u32 y);
// Return floor(sqrt(x*x + y*y)); all signed inputs fit the unsigned result.
XMATH_EXTERN_INLINE u32 i32hypot(i32 x, i32 y);
// Return floor(sqrt(x*x + y*y)); the result must fit u64.
XMATH_EXTERN_INLINE u64 u64hypot(u64 x, u64 y);
// Return floor(sqrt(x*x + y*y)); all signed inputs fit the unsigned result.
XMATH_EXTERN_INLINE u64 i64hypot(i64 x, i64 y);
// Return floor(sqrt(x*x + y*y)); the result must fit u128.
XMATH_EXTERN_INLINE u128 u128hypot(u128 x, u128 y);
// Return floor(sqrt(x*x + y*y)); all signed inputs fit the unsigned result.
XMATH_EXTERN_INLINE u128 i128hypot(i128 x, i128 y);

// Square a 128-bit value; return the low half and store the high half.
static inline u128 xmath_square128(u128 x, u128 *high);

// Definitions

static inline u128 u128min(u128 a, u128 b) { return XMATH_MIN(a, b); }

static inline i128 i128min(i128 a, i128 b) { return XMATH_MIN(a, b); }

static inline u64 u64min(u64 a, u64 b) { return XMATH_MIN(a, b); }

static inline i64 i64min(i64 a, i64 b) { return XMATH_MIN(a, b); }

static inline u32 u32min(u32 a, u32 b) { return XMATH_MIN(a, b); }

static inline i32 i32min(i32 a, i32 b) { return XMATH_MIN(a, b); }

static inline u16 u16min(u16 a, u16 b) { return XMATH_MIN(a, b); }

static inline i16 i16min(i16 a, i16 b) { return XMATH_MIN(a, b); }

static inline u8 u8min(u8 a, u8 b) { return XMATH_MIN(a, b); }

static inline i8 i8min(i8 a, i8 b) { return XMATH_MIN(a, b); }

static inline u128 u128max(u128 a, u128 b) { return XMATH_MAX(a, b); }

static inline i128 i128max(i128 a, i128 b) { return XMATH_MAX(a, b); }

static inline u64 u64max(u64 a, u64 b) { return XMATH_MAX(a, b); }

static inline i64 i64max(i64 a, i64 b) { return XMATH_MAX(a, b); }

static inline u32 u32max(u32 a, u32 b) { return XMATH_MAX(a, b); }

static inline i32 i32max(i32 a, i32 b) { return XMATH_MAX(a, b); }

static inline u16 u16max(u16 a, u16 b) { return XMATH_MAX(a, b); }

static inline i16 i16max(i16 a, i16 b) { return XMATH_MAX(a, b); }

static inline u8 u8max(u8 a, u8 b) { return XMATH_MAX(a, b); }

static inline i8 i8max(i8 a, i8 b) { return XMATH_MAX(a, b); }

static inline f64 f64min(f64 a, f64 b) { return __builtin_fmin(a, b); }

static inline f32 f32min(f32 a, f32 b) { return __builtin_fminf(a, b); }

static inline f64 f64max(f64 a, f64 b) { return __builtin_fmax(a, b); }

static inline f32 f32max(f32 a, f32 b) { return __builtin_fmaxf(a, b); }

static inline u128 i128abs(i128 x) { return XMATH_ABS(x); }

static inline u64 i64abs(i64 x) { return XMATH_ABS(x); }

static inline u32 i32abs(i32 x) { return XMATH_ABS(x); }

static inline u16 i16abs(i16 x) { return XMATH_ABS(x); }

static inline u8 i8abs(i8 x) { return XMATH_ABS(x); }

static inline f64 f64abs(f64 x) { return __builtin_fabs(x); }

static inline f32 f32abs(f32 x) { return __builtin_fabsf(x); }

XMATH_EXTERN_INLINE f64 f64sqrt(f64 x) {
#if defined(__x86_64__)
    __asm__("sqrtsd %0, %0" : "+x"(x));
    return x;
#else
    return __builtin_sqrt(x);
#endif
}

XMATH_EXTERN_INLINE f32 f32sqrt(f32 x) {
#if defined(__x86_64__)
    __asm__("sqrtss %0, %0" : "+x"(x));
    return x;
#else
    return __builtin_sqrtf(x);
#endif
}

XMATH_EXTERN_INLINE u64 u64sqrt(u64 x) {
    u64 r = (u64)f64sqrt((f64)x);
    // r may be 2^32, whose square wraps in u64.
    return r - ((u128)r * r > x);
}

XMATH_EXTERN_INLINE u64 u128sqrt(u128 x) {
#if defined(__x86_64__)
    u64 hi = x >> 64, lo = (u64)x;
    // Every input in this final 2^64-wide interval has root UINT64_MAX.
    // Handle it before DIV, whose quotient otherwise might not fit in u64.
    if (hi == (u64)-1)
        return (u64)-1;
    // Estimate half the root: even a result rounded up to 2^64 then has a
    // representable conversion. Saturate the doubled estimate at UINT64_MAX.
    f64 scaled = (f64)hi * 0x1p62 + (f64)lo * .25;
    u64 half = (u64)f64sqrt(scaled);
    u64 r = (half << 1) - (half >> 63);
    // r > hi guarantees a representable DIV quotient and also handles x=0.
    // Raising r this way cannot put it more than one above the true root.
    r = r > hi ? r : hi + 1;
    u64 quotient, remainder;
    __asm__("divq %[divisor]"
            : "=a"(quotient), "=d"(remainder)
            : "a"(lo), "d"(hi), [divisor] "r"(r)
            : "cc");
    // floor((r + floor(x/r))/2), without overflowing the addition.
    // The initial estimate is within 2 for small roots and has binary64
    // relative accuracy for large roots. One Newton step leaves floor(sqrt)
    // or floor(sqrt)+1, so a single exact square comparison finishes.
    r = (r & quotient) + ((r ^ quotient) >> 1);
    return r - ((u128)r * r > x);
#else

    if (!(x >> 64))
        return u64sqrt((u64)x);
    long double estimate = (long double)x;
#if defined(__x86_64__)
    __asm__("fsqrt" : "+t"(estimate));
#else
    estimate = __builtin_sqrtl(estimate);
#endif
    // 2^64 cannot be converted to u64. This is rounding correction, not an
    // input validity check: every u128, including its maximum, is valid.
    u64 r = estimate >= 0x1p64L ? (u64)-1 : (u64)estimate;
    while ((u128)r * r > x)
        --r;
    while (r != (u64)-1 && (u128)(r + 1) * (r + 1) <= x)
        ++r;
    return r;
#endif
}

XMATH_EXTERN_INLINE u32 u32sqrt(u32 x) {
    u32 r = (u32)f64sqrt(x);
    return r - ((u64)r * r > x);
}

XMATH_EXTERN_INLINE u16 u16sqrt(u16 x) { return (u16)f64sqrt(x); }

XMATH_EXTERN_INLINE u8 u8sqrt(u8 x) { return (u8)f64sqrt(x); }

static inline u128 xmath_square128(u128 x, u128 *high) {
    const u64 a = (u64)x, b = (u64)(x >> 64);
    const u128 low = (u128)a * a, cross = (u128)a * b;
    const u128 result = low + (cross << 65);
    *high = (u128)b * b + (cross >> 63) + (result < low);
    return result;
}

XMATH_EXTERN_INLINE u128 u128hypot(u128 x, u128 y) {
    u128 low, high;
    if (!((x | y) >> 64)) {
        const u128 xx = x * x;
        low = xx + y * y;
        high = low < xx;
        if (!high)
            return u128sqrt(low);
    } else {
        u128 xhi, yhi;
        const u128 xlo = xmath_square128(x, &xhi);
        const u128 ylo = xmath_square128(y, &yhi);
        low = xlo + ylo;
        high = xhi + yhi + (low < xlo);
    }

    // Normalize by an even shift so the first root digit is at least 2^63.
    const u64 top = (u64)(high >> 64);
    const unsigned shift =
        (top ? __builtin_clzll(top) : 64 + __builtin_clzll((u64)high)) & ~1u;
    if (shift) {
        high = (high << shift) | (low >> (128 - shift));
        low <<= shift;
    }
    const u64 a = u128sqrt(high);
    const u128 remainder = high - (u128)a * a;
    // floor((remainder * 2^64 + high64(low)) / (2*a)).
    // Dropping the low numerator bit makes this a 128/64 division.
    const u128 numerator = (remainder << 63) | (low >> 65);
    u64 b = (u64)-1;
    if ((u64)(numerator >> 64) < a) {
#if defined(__x86_64__)
        u64 unused;
        __asm__("divq %[divisor]"
                : "=a"(b), "=d"(unused)
                : "a"((u64)numerator),
                  "d"((u64)(numerator >> 64)), [divisor] "r"(a)
                : "cc");
#else
        b = (u64)(numerator / a);
#endif
    }
    // Ignoring the second digit's square overestimates by at most one:
    // b*b < 2^128 <= 2*a*2^64. One exact comparison completes the root.
    u128 result = ((u128)a << 64) | b, square_hi;
    const u128 square_lo = xmath_square128(result, &square_hi);
    result -= square_hi > high || (square_hi == high && square_lo > low);
    return result >> (shift / 2);
}

XMATH_EXTERN_INLINE u8 u8hypot(u8 x, u8 y) {
    return (u8)u16sqrt((u16)x * x + (u16)y * y);
}
XMATH_EXTERN_INLINE u8 i8hypot(i8 x, i8 y) {
    const u8 a = x < 0 ? (u8)0 - (u8)x : (u8)x;
    const u8 b = y < 0 ? (u8)0 - (u8)y : (u8)y;
    return u8hypot(a, b);
}
XMATH_EXTERN_INLINE u16 u16hypot(u16 x, u16 y) {
    return (u16)u32sqrt((u32)x * x + (u32)y * y);
}
XMATH_EXTERN_INLINE u16 i16hypot(i16 x, i16 y) {
    const u16 a = x < 0 ? (u16)0 - (u16)x : (u16)x;
    const u16 b = y < 0 ? (u16)0 - (u16)y : (u16)y;
    return u16hypot(a, b);
}
XMATH_EXTERN_INLINE u32 u32hypot(u32 x, u32 y) {
    return (u32)u64sqrt((u64)x * x + (u64)y * y);
}
XMATH_EXTERN_INLINE u32 i32hypot(i32 x, i32 y) {
    const u32 a = x < 0 ? (u32)0 - (u32)x : (u32)x;
    const u32 b = y < 0 ? (u32)0 - (u32)y : (u32)y;
    return u32hypot(a, b);
}
XMATH_EXTERN_INLINE u64 u64hypot(u64 x, u64 y) {
    return (u64)u128sqrt((u128)x * x + (u128)y * y);
}
XMATH_EXTERN_INLINE u64 i64hypot(i64 x, i64 y) {
    const u64 a = x < 0 ? (u64)0 - (u64)x : (u64)x;
    const u64 b = y < 0 ? (u64)0 - (u64)y : (u64)y;
    return u64hypot(a, b);
}
XMATH_EXTERN_INLINE u128 i128hypot(i128 x, i128 y) {
    const u128 a = x < 0 ? (u128)0 - (u128)x : (u128)x;
    const u128 b = y < 0 ? (u128)0 - (u128)y : (u128)y;
    return u128hypot(a, b);
}

XMATH_EXTERN_INLINE u8 u64log2(u64 x) { return 63 - __builtin_clzll(x); }

XMATH_EXTERN_INLINE u8 u32log2(u32 x) { return 31 - __builtin_clz(x); }

XMATH_EXTERN_INLINE u8 u16log2(u16 x) { return u32log2(x); }

XMATH_EXTERN_INLINE u8 u8log2(u8 x) { return u32log2(x); }

XMATH_EXTERN_INLINE u8 u128log2(u128 x) {
    u64 hi = x >> 64;
    return hi ? 64 + u64log2(hi) : u64log2((u64)x);
}

XMATH_EXTERN_INLINE u8 u64logn(u64 x, u64 n) {
    if (x < n)
        return 0;
    if ((n & (n - 1)) == 0)
        return u64log2(x) / u64log2(n);
    u64 powers[6], p = n;
    unsigned count = 0;
    for (;;) {
        powers[count++] = p;
        u64 next;
        if (__builtin_mul_overflow(p, p, &next) || next > x)
            break;
        p = next;
    }
    unsigned result = 0;
    while (count) {
        --count;
        if (x >= powers[count]) {
            x /= powers[count];
            result |= 1u << count;
        }
    }
    return result;
}

XMATH_EXTERN_INLINE u8 u128logn(u128 x, u128 n) {
    if (x < n)
        return 0;
    if (!(x >> 64))
        return u64logn((u64)x, (u64)n);
    if ((n & (n - 1)) == 0)
        return u128log2(x) / u128log2(n);
    u128 powers[7], p = n;
    unsigned count = 0;
    for (;;) {
        powers[count++] = p;
        u128 next;
        if (__builtin_mul_overflow(p, p, &next) || next > x)
            break;
        p = next;
    }
    unsigned result = 0;
    while (count) {
        --count;
        if (x >= powers[count]) {
            x /= powers[count];
            result |= 1u << count;
        }
    }
    return result;
}

XMATH_EXTERN_INLINE u8 u32logn(u32 x, u32 n) {
    u8 result = 0;
    while (x >= n) {
        x /= n;
        ++result;
    }
    return result;
}

XMATH_EXTERN_INLINE u8 u16logn(u16 x, u16 n) {
    u8 result = 0;
    while (x >= n) {
        x /= n;
        ++result;
    }
    return result;
}

XMATH_EXTERN_INLINE u8 u8logn(u8 x, u8 n) {
    u8 result = 0;
    while (x >= n) {
        x /= n;
        ++result;
    }
    return result;
}

XMATH_EXTERN_INLINE u8 u64log10(u64 x) {
    static const u64 powers[] = {1ull,
                                 10ull,
                                 100ull,
                                 1000ull,
                                 10000ull,
                                 100000ull,
                                 1000000ull,
                                 10000000ull,
                                 100000000ull,
                                 1000000000ull,
                                 10000000000ull,
                                 100000000000ull,
                                 1000000000000ull,
                                 10000000000000ull,
                                 100000000000000ull,
                                 1000000000000000ull,
                                 10000000000000000ull,
                                 100000000000000000ull,
                                 1000000000000000000ull,
                                 10000000000000000000ull};
    // 1233/4096 approximates log10(2) from below on bit lengths 1..128.
    unsigned e = (u64log2(x) + 1) * 1233 >> 12;
    return e - (x < powers[e]);
}

XMATH_EXTERN_INLINE u8 u128log10(u128 x) {
    static const u128 powers[] = {
        1ull,
        10ull,
        100ull,
        1000ull,
        10000ull,
        100000ull,
        1000000ull,
        10000000ull,
        100000000ull,
        1000000000ull,
        10000000000ull,
        100000000000ull,
        1000000000000ull,
        10000000000000ull,
        100000000000000ull,
        1000000000000000ull,
        10000000000000000ull,
        100000000000000000ull,
        1000000000000000000ull,
        10000000000000000000ull,
        ((u128)5ull << 64) | 7766279631452241920ull,
        ((u128)54ull << 64) | 3875820019684212736ull,
        ((u128)542ull << 64) | 1864712049423024128ull,
        ((u128)5421ull << 64) | 200376420520689664ull,
        ((u128)54210ull << 64) | 2003764205206896640ull,
        ((u128)542101ull << 64) | 1590897978359414784ull,
        ((u128)5421010ull << 64) | 15908979783594147840ull,
        ((u128)54210108ull << 64) | 11515845246265065472ull,
        ((u128)542101086ull << 64) | 4477988020393345024ull,
        ((u128)5421010862ull << 64) | 7886392056514347008ull,
        ((u128)54210108624ull << 64) | 5076944270305263616ull,
        ((u128)542101086242ull << 64) | 13875954555633532928ull,
        ((u128)5421010862427ull << 64) | 9632337040368467968ull,
        ((u128)54210108624275ull << 64) | 4089650035136921600ull,
        ((u128)542101086242752ull << 64) | 4003012203950112768ull,
        ((u128)5421010862427522ull << 64) | 3136633892082024448ull,
        ((u128)54210108624275221ull << 64) | 12919594847110692864ull,
        ((u128)542101086242752217ull << 64) | 68739955140067328ull,
        ((u128)5421010862427522170ull << 64) | 687399551400673280ull};
    // 1233/4096 approximates log10(2) from below on bit lengths 1..128.
    unsigned e = (u128log2(x) + 1) * 1233 >> 12;
    return e - (x < powers[e]);
}

XMATH_EXTERN_INLINE u8 u32log10(u32 x) { return u64log10(x); }

XMATH_EXTERN_INLINE u8 u16log10(u16 x) { return u64log10(x); }

XMATH_EXTERN_INLINE u8 u8log10(u8 x) { return u64log10(x); }

XMATH_EXTERN_INLINE u128 u128pow(u128 x, u8 e) {
    u128 result = 1;
    while (e) {
        if (e & 1)
            result = (u128)((u128)result * x);
        e >>= 1;
        x = (u128)((u128)x * x);
    }
    return result;
}

XMATH_EXTERN_INLINE i128 i128pow(i128 x, u8 e) {
    return (i128)u128pow((u128)x, e);
}

XMATH_EXTERN_INLINE u64 u64pow(u64 x, u8 e) {
    u64 result = 1;
    while (e) {
        if (e & 1)
            result = (u64)((u64)result * x);
        e >>= 1;
        x = (u64)((u64)x * x);
    }
    return result;
}

XMATH_EXTERN_INLINE i64 i64pow(i64 x, u8 e) { return (i64)u64pow((u64)x, e); }

XMATH_EXTERN_INLINE u32 u32pow(u32 x, u8 e) {
    u32 result = 1;
    while (e) {
        if (e & 1)
            result = (u32)((u32)result * x);
        e >>= 1;
        x = (u32)((u32)x * x);
    }
    return result;
}

XMATH_EXTERN_INLINE i32 i32pow(i32 x, u8 e) { return (i32)u32pow((u32)x, e); }

XMATH_EXTERN_INLINE u16 u16pow(u16 x, u8 e) {
    u16 result = 1;
    while (e) {
        if (e & 1)
            result = (u16)((u32)result * x);
        e >>= 1;
        x = (u16)((u32)x * x);
    }
    return result;
}

XMATH_EXTERN_INLINE i16 i16pow(i16 x, u8 e) { return (i16)u16pow((u16)x, e); }

XMATH_EXTERN_INLINE u8 u8pow(u8 x, u8 e) {
    u8 result = 1;
    while (e) {
        if (e & 1)
            result = (u8)((u32)result * x);
        e >>= 1;
        x = (u8)((u32)x * x);
    }
    return result;
}

XMATH_EXTERN_INLINE i8 i8pow(i8 x, u8 e) { return (i8)u8pow((u8)x, e); }

XMATH_EXTERN_INLINE int xmath_power_compare(u128 a, unsigned n, u128 limit) {
    u128 result = 1;
    while (n) {
        if (n & 1) {
            if (__builtin_mul_overflow(result, a, &result) || result > limit)
                return 1;
        }
        n >>= 1;
        if (n && (__builtin_mul_overflow(a, a, &a) || a > limit))
            return 1;
    }
    return (result > limit) - (result < limit);
}

XMATH_EXTERN_INLINE u128 u128nthroot(u128 x, u128 n) {
    if (n == 1 || x <= 1)
        return x;
    if (n == 2)
        return u128sqrt(x);
    if (n >= 128)
        return 1;
    u128 r = (u128)f64exp(f64ln((f64)x) / (f64)n);
    while (xmath_power_compare(r, (unsigned)n, x) > 0)
        --r;
    while (xmath_power_compare(r + 1, (unsigned)n, x) <= 0)
        ++r;
    return r;
}

XMATH_EXTERN_INLINE u128 u128cbrt(u128 x) { return u128nthroot(x, 3); }

XMATH_EXTERN_INLINE i128 i128nthroot(i128 x, i128 n) {
    u128 magnitude = x < 0 ? -(u128)x : (u128)x;
    u128 r = u128nthroot(magnitude, (u128)n);
    if (x >= 0)
        return (i128)r;
    // For n >= 128 the root of a magnitude > 1 is strictly between 1 and 2.
    bool exact =
        magnitude <= 1 || n == 1 ||
        (n < 128 && xmath_power_compare(r, (unsigned)n, magnitude) == 0);
    return (i128)(-(r + !exact));
}

XMATH_EXTERN_INLINE i128 i128cbrt(i128 x) { return i128nthroot(x, 3); }

XMATH_EXTERN_INLINE u64 u64nthroot(u64 x, u64 n) {
    return (u64)u128nthroot(x, n);
}

XMATH_EXTERN_INLINE u64 u64cbrt(u64 x) { return u64nthroot(x, 3); }

XMATH_EXTERN_INLINE i64 i64nthroot(i64 x, i64 n) {
    return (i64)i128nthroot(x, n);
}

XMATH_EXTERN_INLINE i64 i64cbrt(i64 x) { return i64nthroot(x, 3); }

XMATH_EXTERN_INLINE u32 u32nthroot(u32 x, u32 n) {
    return (u32)u128nthroot(x, n);
}

XMATH_EXTERN_INLINE u32 u32cbrt(u32 x) { return u32nthroot(x, 3); }

XMATH_EXTERN_INLINE i32 i32nthroot(i32 x, i32 n) {
    return (i32)i128nthroot(x, n);
}

XMATH_EXTERN_INLINE i32 i32cbrt(i32 x) { return i32nthroot(x, 3); }

XMATH_EXTERN_INLINE u16 u16nthroot(u16 x, u16 n) {
    return (u16)u128nthroot(x, n);
}

XMATH_EXTERN_INLINE u16 u16cbrt(u16 x) { return u16nthroot(x, 3); }

XMATH_EXTERN_INLINE i16 i16nthroot(i16 x, i16 n) {
    return (i16)i128nthroot(x, n);
}

XMATH_EXTERN_INLINE i16 i16cbrt(i16 x) { return i16nthroot(x, 3); }

XMATH_EXTERN_INLINE u8 u8nthroot(u8 x, u8 n) { return (u8)u128nthroot(x, n); }

XMATH_EXTERN_INLINE u8 u8cbrt(u8 x) { return u8nthroot(x, 3); }

XMATH_EXTERN_INLINE i8 i8nthroot(i8 x, i8 n) { return (i8)i128nthroot(x, n); }

XMATH_EXTERN_INLINE i8 i8cbrt(i8 x) { return i8nthroot(x, 3); }

XMATH_EXTERN_INLINE f64 f64mod(f64 x, f64 y) {
    union {
        f64 f;
        u64 u;
    } a = {x}, b = {y}, result;
    u64 sign = a.u & 0x8000000000000000ull;
    a.u &= 0x7fffffffffffffffull;
    b.u &= 0x7fffffffffffffffull;
    if (a.u >= 0x7ff0000000000000ull || b.u > 0x7ff0000000000000ull || b.u == 0)
        return __builtin_nan("");
    if (a.u < b.u)
        return x;
    if (a.u == b.u) {
        result.u = sign;
        return result.f;
    }
    int ex = a.u >> 52, ey = b.u >> 52;
    u64 mx = a.u & 0xfffffffffffffull, my = b.u & 0xfffffffffffffull;
    if (ex)
        mx |= 1ull << 52;
    else {
        unsigned shift = __builtin_clzll(mx) - 11;
        mx <<= shift;
        ex = 1 - (int)shift;
    }
    if (ey)
        my |= 1ull << 52;
    else {
        unsigned shift = __builtin_clzll(my) - 11;
        my <<= shift;
        ey = 1 - (int)shift;
    }
    if (mx >= my)
        mx -= my;
    // A normalized significand has 53 bits. Shift up to 11 bits at once,
    // then take an integer remainder; each step fits in u64 exactly.
    unsigned distance = (unsigned)(ex - ey);
    while (distance > 11) {
        mx = (mx << 11) % my;
        distance -= 11;
    }
    if (distance)
        mx = (mx << distance) % my;
    ex = ey;
    if (!mx) {
        result.u = sign;
        return result.f;
    }
    unsigned shift = __builtin_clzll(mx) - 11;
    mx <<= shift;
    ex -= (int)shift;
    if (ex > 0)
        result.u = sign | ((u64)ex << 52) | (mx & 0xfffffffffffffull);
    else
        result.u = sign | (mx >> (1 - ex));
    return result.f;
}

XMATH_EXTERN_INLINE f32 f32mod(f32 x, f32 y) { return (f32)f64mod(x, y); }

#if defined(__OPTIMIZE__) || defined(XMATH_IMPLEMENTATION)

XMATH_LIBM_INLINE f32 f32sin(f32 x) { return sinf(x); }

XMATH_LIBM_INLINE f32 f32cos(f32 x) { return cosf(x); }

XMATH_LIBM_INLINE f32 f32tan(f32 x) { return tanf(x); }

XMATH_LIBM_INLINE f32 f32ln(f32 x) { return logf(x); }

XMATH_LIBM_INLINE f32 f32log2(f32 x) { return log2f(x); }

XMATH_LIBM_INLINE f32 f32log10(f32 x) { return log10f(x); }

XMATH_LIBM_INLINE f32 f32exp(f32 x) { return expf(x); }

XMATH_LIBM_INLINE f32 f32gamma(f32 x) { return tgammaf(x); }

XMATH_LIBM_INLINE f32 f32logn(f32 x, f32 n) {
    return (f32)(log((f64)x) / log((f64)n));
}

XMATH_LIBM_INLINE f32 f32pow(f32 x, f32 e) { return powf(x, e); }

XMATH_LIBM_INLINE f64 f64ln(f64 x) { return log(x); }

XMATH_LIBM_INLINE f64 f64log2(f64 x) { return log2(x); }

XMATH_LIBM_INLINE f64 f64log10(f64 x) { return log10(x); }

XMATH_LIBM_INLINE f64 f64sin(f64 x) { return sin(x); }

XMATH_LIBM_INLINE f64 f64cos(f64 x) { return cos(x); }

XMATH_LIBM_INLINE f64 f64tan(f64 x) { return tan(x); }

XMATH_LIBM_INLINE f64 f64exp(f64 x) { return exp(x); }

XMATH_LIBM_INLINE f64 f64gamma(f64 x) { return tgamma(x); }

XMATH_LIBM_INLINE f64 f64logn(f64 x, f64 n) { return log(x) / log(n); }

XMATH_LIBM_INLINE f64 f64pow(f64 x, f64 e) { return pow(x, e); }

XMATH_LIBM_INLINE f64 f64cbrt(f64 x) { return cbrt(x); }

XMATH_LIBM_INLINE f32 f32cbrt(f32 x) { return cbrtf(x); }

XMATH_LIBM_INLINE f64 f64nthroot(f64 x, f64 n) {
    if (!__builtin_isfinite(n) || n == 0)
        return __builtin_nan("");
    if (n == 1)
        return x;
    if (n == 2)
        return f64sqrt(x);
    if (n == 3)
        return f64cbrt(x);
    if (n == -1)
        return 1 / x;
    if (__builtin_isnan(x))
        return x;
    bool negative = false;
    if (__builtin_signbit(x)) {
        const f64 parity = f64mod(n, 2);
        negative = parity == 1 || parity == -1;
        if (x < 0 && !negative)
            return __builtin_nan("");
    }
    const f64 magnitude = __builtin_fabs(x);
    const f64 reciprocal = 1 / n;
    f64 result = pow(magnitude, reciprocal);
    if (__builtin_isnormal(result) && __builtin_isfinite(reciprocal)) {
        // If r = round(1/n), residual = 1-n*r recovers its rounding error.
        // Correct pow(x,r) by exp(log(result)*residual), to first order.
        const f64 residual = __builtin_fma(-n, reciprocal, 1);
        union {
            f64 f;
            u64 u;
        } reduced = {result};
        const int exponent = (int)(reduced.u >> 52) - 1023;
        reduced.u = (reduced.u & 0xfffffffffffffull) | 0x3ff0000000000000ull;
        const f64 t = __builtin_fma(reduced.f, 2.0 / 3, -1);
        // Only the tiny correction needs this log estimate. |t| <= 1/3;
        // the omitted Taylor tail is < 1/810 before multiplication by residual.
        const f64 logarithm = exponent * .69314718055994530942 +
                              .40546510810816438198 +
                              t * (1 + t * (-.5 + t * (1.0 / 3 - .25 * t)));
        result = __builtin_fma(result, logarithm * residual, result);
        if (__builtin_isnormal(result))
            return negative ? -result : result;
    }
    // Extended precision resolves rounding across underflow/overflow boundaries
    // and reciprocal overflow for extremely small degrees.
    result = (f64)powl((long double)magnitude, 1.0L / (long double)n);
    return negative ? -result : result;
}

XMATH_LIBM_INLINE f32 f32nthroot(f32 x, f32 n) {
    if (!__builtin_isfinite(n) || n == 0)
        return __builtin_nanf("");
    if (n == 1)
        return x;
    if (n == 2)
        return f32sqrt(x);
    if (n == 3)
        return f32cbrt(x);
    if (n == -1)
        return 1 / x;
    if (__builtin_isnan(x))
        return x;
    const f64 parity = f64mod(n, 2);
    const bool odd = parity == 1 || parity == -1;
    if (x < 0 && !odd)
        return __builtin_nanf("");
    const f32 result = (f32)pow(__builtin_fabs((f64)x), 1.0 / (f64)n);
    return __builtin_signbit(x) && odd ? -result : result;
}

XMATH_LIBM_INLINE f32 f32sinh(f32 x) { return sinhf(x); }
XMATH_LIBM_INLINE f32 f32cosh(f32 x) { return coshf(x); }
XMATH_LIBM_INLINE f32 f32tanh(f32 x) { return tanhf(x); }
XMATH_LIBM_INLINE f32 f32asinh(f32 x) { return asinhf(x); }
XMATH_LIBM_INLINE f32 f32acosh(f32 x) { return acoshf(x); }
XMATH_LIBM_INLINE f32 f32atanh(f32 x) { return atanhf(x); }
XMATH_LIBM_INLINE f32 f32hypot(f32 x, f32 y) { return hypotf(x, y); }

XMATH_LIBM_INLINE f64 f64sinh(f64 x) { return sinh(x); }
XMATH_LIBM_INLINE f64 f64cosh(f64 x) { return cosh(x); }
XMATH_LIBM_INLINE f64 f64tanh(f64 x) { return tanh(x); }
XMATH_LIBM_INLINE f64 f64asinh(f64 x) { return asinh(x); }
XMATH_LIBM_INLINE f64 f64acosh(f64 x) { return acosh(x); }
XMATH_LIBM_INLINE f64 f64atanh(f64 x) { return atanh(x); }
XMATH_LIBM_INLINE f64 f64hypot(f64 x, f64 y) { return hypot(x, y); }

#endif

#undef XMATH_LIBM_INLINE
