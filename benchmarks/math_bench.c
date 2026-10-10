#include "xstdc/math.h"
#include "xstdc/print.h"
#include "xstdc/rng.h"
#include "xstdc/time.h"
#include <math.h>
#include <string.h>

enum { SAMPLES = 2048, REPEATS = 8 };

// RDTSCP counts TSC ticks (reference cycles on invariant-TSC machines).
// Fence subsequent work as well as prior work at each timestamp.
static u64 timestamp(void) {
    __asm__ volatile("lfence" ::: "memory");
    u64 result = xtime_cycles();
    __asm__ volatile("lfence" ::: "memory");
    return result;
}

// Static expansion gives every operation a direct call, including at -O0.
// Input generation is outside the timer. Consuming each result prevents dead
// code elimination, without a dependency chain through a checksum.
// Timings include array loads, the loop, and consuming the result.
#define TIME_OPERATION(R, EXPR, OUT)                                           \
    do {                                                                       \
        for (unsigned i = 0; i < SAMPLES; ++i) {                               \
            R result = (EXPR);                                                 \
            __asm__ volatile("" : : "m"(result) : "memory");                   \
        }                                                                      \
        u64 start = timestamp();                                               \
        for (unsigned repeat = 0; repeat < REPEATS; ++repeat)                  \
            for (unsigned i = 0; i < SAMPLES; ++i) {                           \
                R result = (EXPR);                                             \
                __asm__ volatile("" : : "m"(result) : "memory");               \
            }                                                                  \
        OUT = (double)(timestamp() - start) / (SAMPLES * REPEATS);             \
    } while (0)

// Two data sets and one timing loop serve Xmath and libm alike. Xoshiro256++
// supplies all unpredictable inputs; predictable inputs repeat a small band.
// Integer libm rows measure floating point equivalents, including conversion
// of the integer inputs; they do not provide exact integer arithmetic.
#define DEFINE_BENCH(NAME, T, E, R, CALL, LR, LIBCALL, INPUT, ARGUMENT)        \
    static void bench_##NAME(void) {                                           \
        T xs[2][SAMPLES];                                                      \
        E ys[2][SAMPLES];                                                      \
        XRngXoshiro256ppState rng;                                             \
        xrng_xoshiro256pp_seed(&rng, 1000);                                    \
        for (unsigned predictable = 0; predictable < 2; ++predictable)         \
            for (unsigned i = 0; i < SAMPLES; ++i) {                           \
                u64 r = xrng_xoshiro256pp_u64(&rng);                           \
                u128 wide = ((u128)r << 64) | xrng_xoshiro256pp_u64(&rng);     \
                double unit = xrng_f64range(r, 0, 1);                          \
                double other =                                                 \
                    xrng_f64range(xrng_xoshiro256pp_u64(&rng), 0, 1);          \
                (void)wide;                                                    \
                (void)unit;                                                    \
                (void)other;                                                   \
                xs[predictable][i] = (INPUT);                                  \
                ys[predictable][i] = (ARGUMENT);                               \
            }                                                                  \
        double ours[2], libm[2];                                               \
        for (unsigned mode = 0; mode < 2; ++mode) {                            \
            T *x = xs[mode];                                                   \
            E *y = ys[mode];                                                   \
            (void)y;                                                           \
            TIME_OPERATION(R, CALL, ours[mode]);                               \
            TIME_OPERATION(LR, LIBCALL, libm[mode]);                           \
        }                                                                      \
        xprint(                                                                \
            "%z(e=12,rp=' ') %z(e=7,rp=' ') %f64(r=2,e=16) %f64(r=2,e=16)\n",  \
            #NAME, "xmath", &ours[1], &ours[0]);                               \
        xprint(                                                                \
            "%z(e=12,rp=' ') %z(e=7,rp=' ') %f64(r=2,e=16) %f64(r=2,e=16)\n",  \
            #NAME, "libm", &libm[1], &libm[0]);                                \
    }

// Random bit lengths exercise different paths in integer roots/logarithms.
#define INTEGER_INPUT(T)                                                       \
    ((T)(predictable ? 64 + i % 8 : ((T)wide >> (r % (sizeof(T) * 8)))))
#define REAL_INPUT(LO, HI)                                                     \
    ((LO) + ((HI) - (LO)) * (predictable ? .5 + (i % 8) / 1024. : unit))
#define REAL_ARGUMENT(LO, HI)                                                  \
    ((LO) + ((HI) - (LO)) * (predictable ? .5 + (i % 8) / 1024. : other))

// The wrapper expression is expanded at compile time, never via a vtable.
#define INTEGER_BENCHES(T, ROOT)                                               \
    DEFINE_BENCH(T##sqrt, T, T, ROOT, T##sqrt(x[i]), long double, sqrtl(x[i]), \
                 INTEGER_INPUT(T), 0)                                          \
    DEFINE_BENCH(T##log2, T, T, u8, T##log2(x[i]), long double, log2l(x[i]),   \
                 INTEGER_INPUT(T) | 1, 0)                                      \
    DEFINE_BENCH(T##log10, T, T, u8, T##log10(x[i]), long double,              \
                 log10l(x[i]), INTEGER_INPUT(T) | 1, 0)                        \
    DEFINE_BENCH(T##logn, T, T, u8, T##logn(x[i], y[i]), long double,          \
                 logl(x[i]) / logl(y[i]), INTEGER_INPUT(T) | 1,                \
                 predictable ? 3 : 2 + r % 15)

#define POWER_BENCH(T, SIGNED)                                                 \
    DEFINE_BENCH(                                                              \
        T##pow, T, u8, T, T##pow(x[i], y[i]), long double, powl(x[i], y[i]),   \
        predictable ? 2 : (SIGNED && (r & 1) ? -3 : (i64)(1 + r % 3)),         \
        predictable ? 2 : r % (sizeof(T) * 4))

INTEGER_BENCHES(u8, u8)
INTEGER_BENCHES(u16, u16)
INTEGER_BENCHES(u32, u32)
INTEGER_BENCHES(u64, u64)
INTEGER_BENCHES(u128, u64)
POWER_BENCH(u8, 0)
POWER_BENCH(u16, 0)
POWER_BENCH(u32, 0)
POWER_BENCH(u64, 0)
POWER_BENCH(u128, 0)
POWER_BENCH(i8, 1)
POWER_BENCH(i16, 1)
POWER_BENCH(i32, 1)
POWER_BENCH(i64, 1)
POWER_BENCH(i128, 1)

#define SIGNED_ROOT_INPUT(T, U)                                                \
    (predictable ? (T)(64 + i % 8)                                             \
                 : ((r & 1) ? -(T)(INTEGER_INPUT(U) >> 1)                      \
                            : (T)(INTEGER_INPUT(U) >> 1)))

// libm has no generic nthroot; use sign-preserving pow as its equivalent.
// Like the other integer libm rows, this returns a floating point result.
#define ROOT_BENCHES(T, INPUT, DEGREE)                                         \
    DEFINE_BENCH(T##cbrt, T, T, T, T##cbrt(x[i]), long double, cbrtl(x[i]),    \
                 INPUT, 0)                                                     \
    DEFINE_BENCH(T##nthroot, T, T, T, T##nthroot(x[i], y[i]), long double,     \
                 copysignl(powl(fabsl((long double)x[i]), 1.L / y[i]),         \
                           (long double)x[i]),                                 \
                 INPUT, DEGREE)

ROOT_BENCHES(u8, INTEGER_INPUT(u8), predictable ? 3 : 2 + r % 15)
ROOT_BENCHES(i8, SIGNED_ROOT_INPUT(i8, u8), predictable ? 3 : 3 + 2 * (r % 7))
ROOT_BENCHES(u16, INTEGER_INPUT(u16), predictable ? 3 : 2 + r % 15)
ROOT_BENCHES(i16, SIGNED_ROOT_INPUT(i16, u16),
             predictable ? 3 : 3 + 2 * (r % 7))
ROOT_BENCHES(u32, INTEGER_INPUT(u32), predictable ? 3 : 2 + r % 15)
ROOT_BENCHES(i32, SIGNED_ROOT_INPUT(i32, u32),
             predictable ? 3 : 3 + 2 * (r % 7))
ROOT_BENCHES(u64, INTEGER_INPUT(u64), predictable ? 3 : 2 + r % 15)
ROOT_BENCHES(i64, SIGNED_ROOT_INPUT(i64, u64),
             predictable ? 3 : 3 + 2 * (r % 7))
ROOT_BENCHES(u128, INTEGER_INPUT(u128), predictable ? 3 : 2 + r % 15)
ROOT_BENCHES(i128, SIGNED_ROOT_INPUT(i128, u128),
             predictable ? 3 : 3 + 2 * (r % 7))

#define REAL_UNARY(T, NAME, LIB, LO, HI)                                       \
    DEFINE_BENCH(T##NAME, T, T, T, T##NAME(x[i]), T, LIB(x[i]),                \
                 REAL_INPUT(LO, HI), 0)

REAL_UNARY(f32, sqrt, sqrtf, 0.01, 100)
REAL_UNARY(f32, log2, log2f, 0.01, 100)
REAL_UNARY(f32, log10, log10f, 0.01, 100)
REAL_UNARY(f32, ln, logf, 0.01, 100)
REAL_UNARY(f32, sin, sinf, -32, 32)
REAL_UNARY(f32, cos, cosf, -32, 32)
REAL_UNARY(f32, tan, tanf, -1.5, 1.5)
REAL_UNARY(f32, exp, expf, -20, 20)
REAL_UNARY(f32, gamma, tgammaf, 0.25, 20)
DEFINE_BENCH(f32logn, f32, f32, f32, f32logn(x[i], y[i]), f32,
             logf(x[i]) / logf(y[i]), REAL_INPUT(.01, 100),
             REAL_ARGUMENT(1.25, 10))
DEFINE_BENCH(f32pow, f32, f32, f32, f32pow(x[i], y[i]), f32, powf(x[i], y[i]),
             REAL_INPUT(.125, 16), REAL_ARGUMENT(-8, 8))
REAL_UNARY(f64, sqrt, sqrt, 0.01, 100)
REAL_UNARY(f64, log2, log2, 0.01, 100)
REAL_UNARY(f64, log10, log10, 0.01, 100)
REAL_UNARY(f64, ln, log, 0.01, 100)
REAL_UNARY(f64, sin, sin, -32, 32)
REAL_UNARY(f64, cos, cos, -32, 32)
REAL_UNARY(f64, tan, tan, -1.5, 1.5)
REAL_UNARY(f64, exp, exp, -20, 20)
REAL_UNARY(f64, gamma, tgamma, 0.25, 20)
DEFINE_BENCH(f64logn, f64, f64, f64, f64logn(x[i], y[i]), f64,
             log(x[i]) / log(y[i]), REAL_INPUT(.01, 100),
             REAL_ARGUMENT(1.25, 10))
DEFINE_BENCH(f64pow, f64, f64, f64, f64pow(x[i], y[i]), f64, pow(x[i], y[i]),
             REAL_INPUT(.125, 16), REAL_ARGUMENT(-8, 8))

REAL_UNARY(f32, cbrt, cbrtf, -1e6, 1e6)
DEFINE_BENCH(f32nthroot, f32, f32, f32, f32nthroot(x[i], y[i]), f32,
             powf(x[i], 1 / y[i]), REAL_INPUT(.01, 100),
             REAL_ARGUMENT(1.25, 12))
DEFINE_BENCH(f32mod, f32, f32, f32, f32mod(x[i], y[i]), f32, fmodf(x[i], y[i]),
             REAL_INPUT(-1000, 1000), REAL_ARGUMENT(.125, 16))
REAL_UNARY(f64, cbrt, cbrt, -1e6, 1e6)
DEFINE_BENCH(f64nthroot, f64, f64, f64, f64nthroot(x[i], y[i]), f64,
             pow(x[i], 1 / y[i]), REAL_INPUT(.01, 100), REAL_ARGUMENT(1.25, 12))
DEFINE_BENCH(f64mod, f64, f64, f64, f64mod(x[i], y[i]), f64, fmod(x[i], y[i]),
             REAL_INPUT(-1000, 1000), REAL_ARGUMENT(.125, 16))

REAL_UNARY(f32, sinh, sinhf, -80, 80)
REAL_UNARY(f32, cosh, coshf, -80, 80)
REAL_UNARY(f32, tanh, tanhf, -24, 24)
REAL_UNARY(f32, asinh, asinhf, -1000000.0, 1000000.0)
REAL_UNARY(f32, acosh, acoshf, 1, 1000000.0)
REAL_UNARY(f32, atanh, atanhf, -0.999, 0.999)
DEFINE_BENCH(f32hypot, f32, f32, f32, f32hypot(x[i], y[i]), f32,
             hypotf(x[i], y[i]),
             predictable ? 3 + (i % 8) / 16.
                         : ldexp(unit - .5, (int)(r % 254) - 126),
             predictable ? 4 + (i % 8) / 16.
                         : ldexp(other - .5, (int)((r >> 32) % 254) - 126))
REAL_UNARY(f64, sinh, sinh, -700, 700)
REAL_UNARY(f64, cosh, cosh, -700, 700)
REAL_UNARY(f64, tanh, tanh, -24, 24)
REAL_UNARY(f64, asinh, asinh, -1000000.0, 1000000.0)
REAL_UNARY(f64, acosh, acosh, 1, 1000000.0)
REAL_UNARY(f64, atanh, atanh, -0.999, 0.999)
DEFINE_BENCH(f64hypot, f64, f64, f64, f64hypot(x[i], y[i]), f64,
             hypot(x[i], y[i]),
             predictable ? 3 + (i % 8) / 16.
                         : ldexp(unit - .5, (int)(r % 2046) - 1022),
             predictable ? 4 + (i % 8) / 16.
                         : ldexp(other - .5, (int)((r >> 32) % 2046) - 1022))
DEFINE_BENCH(u8hypot, u8, u8, u8, u8hypot(x[i], y[i]), long double,
             hypotl(x[i], y[i]),
             predictable ? 64 + i % 8 : (((u8)wide >> 1) >> (r % 8)),
             predictable ? 64 + i % 8
                         : (((u8)(wide ^ (wide >> 64)) >> 1) >> ((r >> 8) % 8)))
DEFINE_BENCH(
    i8hypot, i8, i8, u8, i8hypot(x[i], y[i]), long double, hypotl(x[i], y[i]),
    predictable ? 64 + i % 8
                : ((r & 1) ? -(i8)(((u8)wide >> 1) >> (r % 8))
                           : (i8)(((u8)wide >> 1) >> (r % 8))),
    predictable
        ? 64 + i % 8
        : ((r & 2) ? -(i8)(((u8)(wide ^ (wide >> 64)) >> 1) >> ((r >> 8) % 8))
                   : (i8)(((u8)(wide ^ (wide >> 64)) >> 1) >> ((r >> 8) % 8))))
DEFINE_BENCH(u16hypot, u16, u16, u16, u16hypot(x[i], y[i]), long double,
             hypotl(x[i], y[i]),
             predictable ? 64 + i % 8 : (((u16)wide >> 1) >> (r % 16)),
             predictable
                 ? 64 + i % 8
                 : (((u16)(wide ^ (wide >> 64)) >> 1) >> ((r >> 8) % 16)))
DEFINE_BENCH(i16hypot, i16, i16, u16, i16hypot(x[i], y[i]), long double,
             hypotl(x[i], y[i]),
             predictable ? 64 + i % 8
                         : ((r & 1) ? -(i16)(((u16)wide >> 1) >> (r % 16))
                                    : (i16)(((u16)wide >> 1) >> (r % 16))),
             predictable
                 ? 64 + i % 8
                 : ((r & 2) ? -(i16)(((u16)(wide ^ (wide >> 64)) >> 1) >>
                                     ((r >> 8) % 16))
                            : (i16)(((u16)(wide ^ (wide >> 64)) >> 1) >>
                                    ((r >> 8) % 16))))
DEFINE_BENCH(u32hypot, u32, u32, u32, u32hypot(x[i], y[i]), long double,
             hypotl(x[i], y[i]),
             predictable ? 64 + i % 8 : (((u32)wide >> 1) >> (r % 32)),
             predictable
                 ? 64 + i % 8
                 : (((u32)(wide ^ (wide >> 64)) >> 1) >> ((r >> 8) % 32)))
DEFINE_BENCH(i32hypot, i32, i32, u32, i32hypot(x[i], y[i]), long double,
             hypotl(x[i], y[i]),
             predictable ? 64 + i % 8
                         : ((r & 1) ? -(i32)(((u32)wide >> 1) >> (r % 32))
                                    : (i32)(((u32)wide >> 1) >> (r % 32))),
             predictable
                 ? 64 + i % 8
                 : ((r & 2) ? -(i32)(((u32)(wide ^ (wide >> 64)) >> 1) >>
                                     ((r >> 8) % 32))
                            : (i32)(((u32)(wide ^ (wide >> 64)) >> 1) >>
                                    ((r >> 8) % 32))))
DEFINE_BENCH(u64hypot, u64, u64, u64, u64hypot(x[i], y[i]), long double,
             hypotl(x[i], y[i]),
             predictable ? 64 + i % 8 : (((u64)wide >> 1) >> (r % 64)),
             predictable
                 ? 64 + i % 8
                 : (((u64)(wide ^ (wide >> 64)) >> 1) >> ((r >> 8) % 64)))
DEFINE_BENCH(i64hypot, i64, i64, u64, i64hypot(x[i], y[i]), long double,
             hypotl(x[i], y[i]),
             predictable ? 64 + i % 8
                         : ((r & 1) ? -(i64)(((u64)wide >> 1) >> (r % 64))
                                    : (i64)(((u64)wide >> 1) >> (r % 64))),
             predictable
                 ? 64 + i % 8
                 : ((r & 2) ? -(i64)(((u64)(wide ^ (wide >> 64)) >> 1) >>
                                     ((r >> 8) % 64))
                            : (i64)(((u64)(wide ^ (wide >> 64)) >> 1) >>
                                    ((r >> 8) % 64))))
DEFINE_BENCH(u128hypot, u128, u128, u128, u128hypot(x[i], y[i]), long double,
             hypotl(x[i], y[i]),
             predictable ? 64 + i % 8 : (((u128)wide >> 1) >> (r % 128)),
             predictable
                 ? 64 + i % 8
                 : (((u128)(wide ^ (wide >> 64)) >> 1) >> ((r >> 8) % 128)))
DEFINE_BENCH(i128hypot, i128, i128, u128, i128hypot(x[i], y[i]), long double,
             hypotl(x[i], y[i]),
             predictable ? 64 + i % 8
                         : ((r & 1) ? -(i128)(((u128)wide >> 1) >> (r % 128))
                                    : (i128)(((u128)wide >> 1) >> (r % 128))),
             predictable
                 ? 64 + i % 8
                 : ((r & 2) ? -(i128)(((u128)(wide ^ (wide >> 64)) >> 1) >>
                                      ((r >> 8) % 128))
                            : (i128)(((u128)(wide ^ (wide >> 64)) >> 1) >>
                                     ((r >> 8) % 128))))

#define RUN(NAME)                                                              \
    do {                                                                       \
        if (argc == 1 || strcmp(argv[1], #NAME) == 0) {                        \
            bench_##NAME();                                                    \
            found = true;                                                      \
        }                                                                      \
    } while (0)

#define RUN_UNSIGNED(T)                                                        \
    do {                                                                       \
        RUN(T##hypot);                                                         \
        RUN(T##cbrt);                                                          \
        RUN(T##nthroot);                                                       \
        RUN(T##sqrt);                                                          \
        RUN(T##log2);                                                          \
        RUN(T##log10);                                                         \
        RUN(T##logn);                                                          \
        RUN(T##pow);                                                           \
    } while (0)

#define RUN_REAL(T)                                                            \
    do {                                                                       \
        RUN(T##hypot);                                                         \
        RUN(T##cbrt);                                                          \
        RUN(T##nthroot);                                                       \
        RUN(T##mod);                                                           \
        RUN(T##sqrt);                                                          \
        RUN(T##log2);                                                          \
        RUN(T##log10);                                                         \
        RUN(T##ln);                                                            \
        RUN(T##logn);                                                          \
        RUN(T##sin);                                                           \
        RUN(T##cos);                                                           \
        RUN(T##tan);                                                           \
        RUN(T##sinh);                                                          \
        RUN(T##cosh);                                                          \
        RUN(T##tanh);                                                          \
        RUN(T##asinh);                                                         \
        RUN(T##acosh);                                                         \
        RUN(T##atanh);                                                         \
        RUN(T##exp);                                                           \
        RUN(T##gamma);                                                         \
        RUN(T##pow);                                                           \
    } while (0)

int main(int argc, char **argv) {
    if (argc > 2) {
        xprint("Usage: %z [function, e.g. f64sin]\n", argv[0]);
        return 1;
    }
    xprint("%z\n",
           "Average cycles/op (TSC ticks); libm integer rows use long double.");
    xprint("%z(e=12,rp=' ') %z(e=7,rp=' ') %z(e=16) %z(e=16)\n", "function",
           "library", "maybe predictable", "unpredictable");
    bool found = false;
    RUN_UNSIGNED(u8);
    RUN_UNSIGNED(u16);
    RUN_UNSIGNED(u32);
    RUN_UNSIGNED(u64);
    RUN_UNSIGNED(u128);
    RUN(i8hypot);
    RUN(i8pow);
    RUN(i8cbrt);
    RUN(i8nthroot);
    RUN(i16hypot);
    RUN(i16pow);
    RUN(i16cbrt);
    RUN(i16nthroot);
    RUN(i32hypot);
    RUN(i32pow);
    RUN(i32cbrt);
    RUN(i32nthroot);
    RUN(i64hypot);
    RUN(i64pow);
    RUN(i64cbrt);
    RUN(i64nthroot);
    RUN(i128hypot);
    RUN(i128pow);
    RUN(i128cbrt);
    RUN(i128nthroot);
    RUN_REAL(f32);
    RUN_REAL(f64);
    if (!found) {
        xprint("Unknown function: %z\n", argv[1]);
        return 1;
    }
    return 0;
}
