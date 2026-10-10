#include "xstdc/alloc.h"
#include "xstdc/mem.h"
#include "xstdc/print.h"
#include "xstdc/rng.h"
#include "xstdc/sort.h"
#include "xstdc/time.h"
#include <stdlib.h>

static u64 timestamp(void) {
    __asm__ volatile("lfence" ::: "memory");
    u64 t = xtime_cycles();
    __asm__ volatile("lfence" ::: "memory");
    return t;
}
#define CONSUME(p) __asm__ volatile("" : : "g"(p) : "memory")

#define SORT_BENCH(T)                                                          \
    static bool greater_##T(void *a, void *b) { return *(T *)a > *(T *)b; }    \
    static int compare_##T(const void *a, const void *b) {                     \
        T x = *(const T *)a, y = *(const T *)b;                                \
        return (x > y) - (x < y);                                              \
    }                                                                          \
    static void bench_##T(usize n, unsigned pattern) {                         \
        T *input = xalloc(n * sizeof *input),                                  \
          *work = xalloc(n * sizeof *work);                                    \
        XRngXoshiro256ppState rng;                                             \
        xrng_xoshiro256pp_seed(&rng, 1000);                                    \
        for (usize i = 0; i < n; ++i) {                                        \
            u64 bits = xrng_xoshiro256pp_u64(&rng);                            \
            xmemcp(input + i, &bits, sizeof(T));                               \
            if (pattern == 1)                                                  \
                input[i] = (T)i;                                               \
            if (pattern == 2)                                                  \
                input[i] = (T)(n - i);                                         \
            if (pattern == 3)                                                  \
                input[i] = (T)(bits % 8);                                      \
            if (pattern == 4)                                                  \
                input[i] = 7;                                                  \
        }                                                                      \
        const usize k = n / 16;                                                \
        double sum[6] = {0};                                                   \
        for (unsigned rep = 0; rep < 6; ++rep) {                               \
            /* Rotate execution order; restore identical inputs outside each   \
             * timer. */                                                       \
            for (unsigned j = 0; j < 6; ++j) {                                 \
                unsigned op = (j + rep) % 6;                                   \
                xmemcp(work, input, n * sizeof *work);                         \
                u64 start, elapsed;                                            \
                switch (op) {                                                  \
                case 0:                                                        \
                    start = timestamp();                                       \
                    qsort(work, n, sizeof *work, compare_##T);                 \
                    CONSUME(work);                                             \
                    elapsed = timestamp() - start;                             \
                    break;                                                     \
                case 1:                                                        \
                    start = timestamp();                                       \
                    xsort_cmp(work, sizeof *work, n, greater_##T);             \
                    CONSUME(work);                                             \
                    elapsed = timestamp() - start;                             \
                    break;                                                     \
                case 2:                                                        \
                    start = timestamp();                                       \
                    xsort_cmp_u(work, sizeof *work, n, greater_##T);           \
                    CONSUME(work);                                             \
                    elapsed = timestamp() - start;                             \
                    break;                                                     \
                case 3:                                                        \
                    start = timestamp();                                       \
                    xsort_radix_##T(work, n);                                  \
                    CONSUME(work);                                             \
                    elapsed = timestamp() - start;                             \
                    break;                                                     \
                case 4:                                                        \
                    start = timestamp();                                       \
                    xpsort_cmp(work, sizeof *work, n, k, greater_##T);         \
                    CONSUME(work);                                             \
                    elapsed = timestamp() - start;                             \
                    break;                                                     \
                case 5:                                                        \
                    start = timestamp();                                       \
                    xpsort_cmp_u(work, sizeof *work, n, k, greater_##T);       \
                    CONSUME(work);                                             \
                    elapsed = timestamp() - start;                             \
                    break;                                                     \
                }                                                              \
                if (rep)                                                       \
                    sum[op] += (double)elapsed / n;                            \
            }                                                                  \
        }                                                                      \
        const char *names[] = {"random", "sorted", "reverse", "duplicates",    \
                               "equal"};                                       \
        xprint("%z %z(e=10,rp=' ') %u64(e=6) %u64(e=6)", #T, names[pattern],   \
               &n, &k);                                          \
        for (unsigned op = 0; op < 6; ++op)                                    \
            xprint(" %f64(r=2,e=12)", &(f64){sum[op] / 5});                    \
        xprint("\n");                                           \
        xfree(input, n * sizeof *input);                                       \
        xfree(work, n * sizeof *work);                                         \
    }

SORT_BENCH(i32)
SORT_BENCH(i64)
SORT_BENCH(u32)
SORT_BENCH(u64)
int main(void) {
    xprint("%z\n", "Average TSC cycles/input element, five runs after warmup.");
    xprint("%z\n",
           "Generation and input copies excluded; sort allocations included.");
    xprint("%z\n", "Partial sorts select k=n/16; qsort fully sorts the input.");
    xprint(
        "%z\n",
        "type dataset     count      k        qsort       stable     unstable "
        "       radix  stable-part unstable-part");
    usize sizes[] = {256, 4096, 65536};
    for (unsigned s = 0; s < sizeof sizes / sizeof *sizes; ++s)
        for (unsigned p = 0; p < 5; ++p) {
            bench_i32(sizes[s], p);
            bench_i64(sizes[s], p);
            bench_u32(sizes[s], p);
            bench_u64(sizes[s], p);
        }
}
