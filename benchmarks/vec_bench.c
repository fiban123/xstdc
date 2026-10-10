#include "xstdc/print.h"
#include "xstdc/rng.h"
#include "xstdc/time.h"
#include "xstdc/vec.h"

// Conventional typed macro vector; same allocator and doubling policy as XVec.
#define MV(T)                                                                  \
    struct {                                                                   \
        T *p;                                                                  \
        usize sz, reserved;                                                    \
    }
#define MV_RESERVE(v, n)                                                       \
    do {                                                                       \
        usize mv_n = (n);                                                      \
        if (mv_n > (v)->reserved) {                                            \
            usize cap = (v)->reserved ? (v)->reserved : 8;                     \
            while (cap < mv_n)                                                 \
                cap *= 2;                                                      \
            (v)->p = xrealloc((v)->p, (v)->reserved * sizeof *(v)->p,          \
                              cap * sizeof *(v)->p);                           \
            (v)->reserved = cap;                                               \
        }                                                                      \
    } while (0)
#define MV_APPEND(v, val)                                                      \
    do {                                                                       \
        MV_RESERVE(v, (v)->sz + 1);                                            \
        (v)->p[(v)->sz++] = *(val);                                            \
    } while (0)
#define MV_INSERT(v, idx, val)                                                 \
    do {                                                                       \
        usize mv_idx = (idx);                                                  \
        MV_RESERVE(v, (v)->sz + 1);                                            \
        xmemmv((v)->p + mv_idx + 1, (v)->p + mv_idx,                           \
               ((v)->sz - mv_idx) * sizeof *(v)->p);                           \
        (v)->p[mv_idx] = *(val);                                               \
        ++(v)->sz;                                                             \
    } while (0)
#define MV_REMOVE(v, idx)                                                      \
    do {                                                                       \
        usize mv_idx = (idx);                                                  \
        xmemmv((v)->p + mv_idx, (v)->p + mv_idx + 1,                           \
               ((v)->sz - mv_idx - 1) * sizeof *(v)->p);                       \
        --(v)->sz;                                                             \
    } while (0)
#define MV_FREE(v) xfree((v)->p, (v)->reserved * sizeof *(v)->p)
#define MV_AT(v, i) ((v)->p[i])
#define XV_AT(v, i) xvat(v, u64, i)
#define CONSUME(x) __asm__ volatile("" : : "g"(x) : "memory")

static u64 timestamp(void) {
    __asm__ volatile("lfence" ::: "memory");
    u64 t = xtime_cycles();
    __asm__ volatile("lfence" ::: "memory");
    return t;
}
typedef MV(u64) MacroVec;
static inline void macro_init(MacroVec *v) { *v = (MacroVec){0}; }
static inline void struct_init(XVec *v) { xvec_init(v, sizeof(u64)); }

// Compile-time direct calls, as in the hashmap benchmark. No function pointers.
#define BENCH(NAME, TYPE, INIT, RESERVE, APPEND, INSERT, REMOVE, AT, FREE)     \
    static void NAME(const u64 *data, const usize *indices, usize n) {         \
        double sum[5] = {0};                                                   \
        for (unsigned rep = 0; rep < 8; ++rep) {                               \
            TYPE v;                                                            \
            INIT(&v);                                                          \
            u64 t = timestamp();                                               \
            for (usize i = 0; i < n; ++i) {                                    \
                APPEND(&v, data + i);                                          \
                CONSUME(v.p);                                                  \
            }                                                                  \
            u64 elapsed = timestamp() - t;                                     \
            if (rep)                                                           \
                sum[0] += (double)elapsed / n;                                 \
            FREE(&v);                                                          \
            INIT(&v);                                                          \
            RESERVE(&v, n);                                                    \
            t = timestamp();                                                   \
            for (usize i = 0; i < n; ++i) {                                    \
                APPEND(&v, data + i);                                          \
                CONSUME(v.p);                                                  \
            }                                                                  \
            elapsed = timestamp() - t;                                         \
            if (rep)                                                           \
                sum[1] += (double)elapsed / n;                                 \
            u64 checksum = 0;                                                  \
            t = timestamp();                                                   \
            for (usize i = 0; i < n; ++i)                                      \
                checksum += AT(&v, indices[i] % n);                            \
            CONSUME(checksum);                                                 \
            elapsed = timestamp() - t;                                         \
            if (rep)                                                           \
                sum[2] += (double)elapsed / n;                                 \
            FREE(&v);                                                          \
            INIT(&v);                                                          \
            RESERVE(&v, n);                                                    \
            t = timestamp();                                                   \
            for (usize i = 0; i < n; ++i) {                                    \
                INSERT(&v, indices[i] % (i + 1), data + i);                    \
                CONSUME(v.p);                                                  \
            }                                                                  \
            elapsed = timestamp() - t;                                         \
            if (rep)                                                           \
                sum[3] += (double)elapsed / n;                                 \
            t = timestamp();                                                   \
            for (usize i = n; i; --i) {                                        \
                REMOVE(&v, indices[i - 1] % i);                                \
                CONSUME(v.p);                                                  \
            }                                                                  \
            elapsed = timestamp() - t;                                         \
            if (rep)                                                           \
                sum[4] += (double)elapsed / n;                                 \
            FREE(&v);                                                          \
        }                                                                      \
        xprint("%z(e=12,rp=' ') %u64(e=6)", #NAME, &n);                 \
        for (unsigned i = 0; i < 5; ++i)                                       \
            xprint(" %f64(r=2,e=14)", &(f64){sum[i] / 7});                     \
        xprint("\n");                                           \
    }
BENCH(macro_vec, MacroVec, macro_init, MV_RESERVE, MV_APPEND, MV_INSERT,
      MV_REMOVE, MV_AT, MV_FREE)
BENCH(struct_vec, XVec, struct_init, xvec_reserve, xvec_append, xvec_insert,
      xvec_remove, XV_AT, xvec_free)

int main(void) {
    enum { N = 16384 };
    u64 *data = xalloc(N * sizeof *data);
    usize *indices = xalloc(N * sizeof *indices);
    XRngXoshiro256ppState rng;
    xrng_xoshiro256pp_seed(&rng, 1000);
    for (usize i = 0; i < N; ++i) {
        data[i] = xrng_xoshiro256pp_u64(&rng);
        indices[i] = xrng_xoshiro256pp_u64(&rng);
    }
    xprint(
        "%z\n",
        "Average TSC cycles/operation; u64 elements, seven runs after warmup.");
    xprint("%z\n",
           "Inputs generated outside timers; random insertion/removal includes "
           "shifting.");
    xprint("%z\n",
           "backend       count    append-grow append-reserved    random-read  "
           "random-insert  random-remove");
    usize sizes[] = {256, 4096, N};
    for (unsigned i = 0; i < 3; ++i) {
        macro_vec(data, indices, sizes[i]);
        struct_vec(data, indices, sizes[i]);
    }
    xfree(data, N * sizeof *data);
    xfree(indices, N * sizeof *indices);
}
