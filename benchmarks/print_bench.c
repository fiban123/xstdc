#include "xstdc/print.h"
#include "xstdc/rng.h"
#include "xstdc/time.h"
#include <stdio.h>
#include <stdlib.h>

enum { SAMPLES = 256, REPEATS = 32, RUNS = 7 };
typedef struct {
    u64 u;
    i64 i;
    f64 f;
    XStringVw s;
} Input;
static u64 timestamp(void) {
    __asm__ volatile("lfence" ::: "memory");
    u64 t = xtime_cycles();
    __asm__ volatile("lfence" ::: "memory");
    return t;
}
#define CONSUME(p, n) __asm__ volatile("" : : "g"(p), "g"(n) : "memory")
// Direct formatting calls, no dispatch through a table of function pointers.
#define BENCH(NAME, XCALL, CCALL)                                              \
    static void bench_##NAME(const Input *inputs) {                            \
        char xbuf[256], cbuf[257];                                             \
        for (usize j = 0; j < SAMPLES; ++j) {                                  \
            const Input *v = &inputs[j];                                       \
            XStringVw x = (XCALL);                                             \
            int c = (CCALL);                                                   \
            if (!x.p || c < 0 || x.len != (usize)c ||                          \
                !xmemeq(xbuf, cbuf, x.len)) {                                  \
                xprint_err("%z", "print benchmark: output mismatch\n");        \
                exit(1);                                                       \
            }                                                                  \
        }                                                                      \
        double sums[2] = {0};                                                  \
        for (unsigned run = 0; run <= RUNS; ++run) {                           \
            for (unsigned order = 0; order < 2; ++order) {                     \
                const unsigned backend = (order + run) & 1;                    \
                u64 begin = timestamp();                                       \
                if (!backend) {                                                \
                    for (unsigned r = 0; r < REPEATS; ++r)                     \
                        for (usize j = 0; j < SAMPLES; ++j) {                  \
                            const Input *v = &inputs[j];                       \
                            XStringVw x = (XCALL);                             \
                            CONSUME(xbuf, x.len);                              \
                        }                                                      \
                } else {                                                       \
                    for (unsigned r = 0; r < REPEATS; ++r)                     \
                        for (usize j = 0; j < SAMPLES; ++j) {                  \
                            const Input *v = &inputs[j];                       \
                            int c = (CCALL);                                   \
                            CONSUME(cbuf, c);                                  \
                        }                                                      \
                }                                                              \
                u64 elapsed = timestamp() - begin;                             \
                if (run)                                                       \
                    sums[backend] += (double)elapsed / (SAMPLES * REPEATS);    \
            }                                                                  \
        }                                                                      \
        xprint("%z(e=16,rp=' ') %f64(r=2,e=14) %f64(r=2,e=14)\n", #NAME,       \
               &(f64){sums[0] / RUNS}, &(f64){sums[1] / RUNS});                \
    }
BENCH(decimal, xprint_buf(xbuf, sizeof xbuf, "%u64/%i64", &v->u, &v->i),
      snprintf(cbuf, sizeof cbuf, "%llu/%lld", v->u, v->i))
BENCH(hex, xprint_buf(xbuf, sizeof xbuf, "%u64(x)", &v->u),
      snprintf(cbuf, sizeof cbuf, "%llx", v->u))
BENCH(string, xprint_buf(xbuf, sizeof xbuf, "[%s]", &v->s),
      snprintf(cbuf, sizeof cbuf, "[%.*s]", (int)v->s.len, v->s.p))
BENCH(padded, xprint_buf(xbuf, sizeof xbuf, "%u64(lp=0,e=24)", &v->u),
      snprintf(cbuf, sizeof cbuf, "%024llu", v->u))
BENCH(floating, xprint_buf(xbuf, sizeof xbuf, "%f64(r=4)", &v->f),
      snprintf(cbuf, sizeof cbuf, "%.4f", v->f))
BENCH(general, xprint_buf(xbuf, sizeof xbuf, "%f64", &v->f),
      snprintf(cbuf, sizeof cbuf, "%g", v->f))
BENCH(mixed,
      xprint_buf(xbuf, sizeof xbuf, "id=%u64 name=%s value=%f64(r=2)", &v->u,
                 &v->s, &v->f),
      snprintf(cbuf, sizeof cbuf, "id=%llu name=%.*s value=%.2f", v->u,
               (int)v->s.len, v->s.p, v->f))
int main(void) {
    Input inputs[SAMPLES];
    XRngXoshiro256ppState rng;
    xrng_xoshiro256pp_seed(&rng, 1000);
    for (usize i = 0; i < SAMPLES; ++i) {
        inputs[i].u = xrng_xoshiro256pp_u64(&rng);
        inputs[i].i = (i64)xrng_xoshiro256pp_u64(&rng);
        inputs[i].f = (f64)(i64)(inputs[i].u % 20000001) - 10000000.0;
        inputs[i].f /= 37.0;
        inputs[i].s = i & 1 ? strl("sample string")
                            : strl("another value with more bytes");
    }
    xprint("%z\n",
           "Average TSC cycles/format; buffer output only, inputs generated "
           "outside timers.");
    xprint("%z\n",
           "snprintf is the buffer-based printf baseline; xprint uses custom "
           "float conversion.");
    xprint("%z\n", "case                     xprint       snprintf");
    bench_decimal(inputs);
    bench_hex(inputs);
    bench_string(inputs);
    bench_padded(inputs);
    bench_floating(inputs);
    bench_general(inputs);
    bench_mixed(inputs);
}
