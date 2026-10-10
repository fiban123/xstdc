#include "../extern/khash/khash.h"
#include "xstdc/hashmap.h"
#include "xstdc/mem.h"
#include "xstdc/print.h"
#include "xstdc/rng.h"
#include "xstdc/time.h"
#include <math.h>
#include <string.h>

enum { HASH_SAMPLES = 256, HASH_REPEATS = 32, MAP_REPEATS = 5 };

// As in math_bench: TSC ticks, or reference cycles on invariant-TSC CPUs.
static u64 timestamp(void) {
    __asm__ volatile("lfence" ::: "memory");
    u64 t = xtime_cycles();
    __asm__ volatile("lfence" ::: "memory");
    return t;
}
#define CONSUME(x) __asm__ volatile("" : : "g"(x) : "memory")

// khash's default 64-bit integer hash returns a 32-bit hash.
static inline __attribute__((always_inline)) u64 khash_native(const void *key,
                                                              usize length) {
    (void)length;
    u64 value;
    xmemcp(&value, (void *)key, sizeof value);
    return kh_int64_hash_func(value);
}

// Direct calls generated at compile time; no benchmark dispatch table.
#define HASH_BENCH(HASH)                                                       \
    static void bench_##HASH(const u8 *data, usize length) {                   \
        for (unsigned i = 0; i < HASH_SAMPLES; ++i) {                          \
            u64 h = HASH(data + i * length, length);                           \
            CONSUME(h);                                                        \
        }                                                                      \
        u64 start = timestamp();                                               \
        for (unsigned r = 0; r < HASH_REPEATS; ++r)                            \
            for (unsigned i = 0; i < HASH_SAMPLES; ++i) {                      \
                u64 h = HASH(data + i * length, length);                       \
                CONSUME(h);                                                    \
            }                                                                  \
        double cycles =                                                        \
            (double)(timestamp() - start) / (HASH_SAMPLES * HASH_REPEATS);     \
        xprint("%z(e=18,rp=' ') %u64(e=6) %f64(r=2,e=14) %f64(r=3,e=14)\n",    \
               #HASH, &length, &cycles,                          \
               &(f64){cycles / length});                                       \
    }
HASH_BENCH(khash_native)
HASH_BENCH(xhash_fnv1a)
HASH_BENCH(xhash_zx)
HASH_BENCH(xhash_zxmix)
HASH_BENCH(xhash_xxh3)
HASH_BENCH(xhash_rapidhash)

static void bench_hashes(void) {
    usize lengths[] = {1, 2, 4, 8, 16, 32, 64, 256, 1024, 4096};
    u8 *data = xalloc(HASH_SAMPLES * 4096);
    XRngXoshiro256ppState rng;
    xrng_xoshiro256pp_seed(&rng, 1000);
    for (usize i = 0; i < HASH_SAMPLES * 4096; i += 8) {
        u64 r = xrng_xoshiro256pp_u64(&rng);
        xmemcp(data + i, &r, 8);
    }
    xprint("%z\n", "hash                bytes    cycles/hash    cycles/byte");
    for (usize i = 0; i < sizeof lengths / sizeof *lengths; ++i) {
        usize n = lengths[i];
        if (n == 8)
            bench_khash_native(data, n);
        bench_xhash_fnv1a(data, n);
        if (n <= 8) {
            bench_xhash_zx(data, n);
            bench_xhash_zxmix(data, n);
        }
        bench_xhash_xxh3(data, n);
        bench_xhash_rapidhash(data, n);
    }
    xfree(data, HASH_SAMPLES * 4096);
}

// Adapters disappear at compile time; the shared timed loops call each backend
// directly. Fixed maps establish the 8-byte key size once at initialization.
#define FIXED_ADAPTERS(P, MAP, SET, M, S)                                      \
    static inline __attribute__((always_inline)) void P##_init(MAP *m,         \
                                                               usize v) {      \
        M##_init(m, 8, v);                                                     \
    }                                                                          \
    static inline __attribute__((always_inline)) bool P##_insert(              \
        MAP *m, HashFn h, const void *k, usize n, void *v) {                   \
        (void)n;                                                               \
        return M##_insert(m, h, k, v);                                         \
    }                                                                          \
    static inline __attribute__((always_inline)) void *P##_get(                \
        MAP *m, HashFn h, const void *k, usize n) {                            \
        (void)n;                                                               \
        return M##_get(m, h, k);                                               \
    }                                                                          \
    static inline __attribute__((always_inline)) void P##_set_init(SET *s,     \
                                                                   usize v) {  \
        (void)v;                                                               \
        S##_init(s, 8);                                                        \
    }                                                                          \
    static inline __attribute__((always_inline)) bool P##_set_insert(          \
        SET *s, HashFn h, const void *k, usize n, void *v) {                   \
        (void)n;                                                               \
        (void)v;                                                               \
        return S##_insert(s, h, k);                                            \
    }                                                                          \
    static inline __attribute__((always_inline)) bool P##_set_get(             \
        SET *s, HashFn h, const void *k, usize n) {                            \
        (void)n;                                                               \
        return S##_contains(s, h, k);                                          \
    }
FIXED_ADAPTERS(node, XNodeHashMap, XNodeHashSet, xnhm, xnhs)
FIXED_ADAPTERS(array, XArrayHashMap, XArrayHashSet, xahm, xahs)

#define DYN_SET_ADAPTERS(P, SET, S)                                            \
    static inline __attribute__((always_inline)) void P##_init(SET *s,         \
                                                               usize v) {      \
        (void)v;                                                               \
        S##_init(s);                                                           \
    }                                                                          \
    static inline __attribute__((always_inline)) bool P##_insert(              \
        SET *s, HashFn h, const void *k, usize n, void *v) {                   \
        (void)v;                                                               \
        return S##_insert(s, h, k, n);                                         \
    }                                                                          \
    static inline __attribute__((always_inline)) bool P##_get(                 \
        SET *s, HashFn h, const void *k, usize n) {                            \
        return S##_contains(s, h, k, n);                                       \
    }
DYN_SET_ADAPTERS(dyn_node_set, XDynNodeHashSet, xdnhs)
DYN_SET_ADAPTERS(dyn_array_set, XDynArrayHashSet, xdahs)

// khash specializes each hash at compile time. It stores u64 keys/values
// inline and uses its default libc allocator and 0.77 maximum load factor.
// Node containers copy arbitrary byte keys/values and use xalloc instead.
#define KHASH_BACKEND(NAME, HASH, IS_MAP)                                      \
    static inline __attribute__((always_inline)) khint_t NAME##_hash(          \
        u64 key) {                                                             \
        return (khint_t)HASH(&key, sizeof key);                                \
    }                                                                          \
    KHASH_INIT(NAME, u64, u64, IS_MAP, NAME##_hash, kh_int64_hash_equal)       \
    static inline __attribute__((always_inline)) void NAME##_init(             \
        khash_t(NAME) * *map, usize size) {                                    \
        (void)size;                                                            \
        *map = kh_init(NAME);                                                  \
        if (!*map) {                                                           \
            xprint_err("%z", "khash allocation failed\n");                     \
            exit(1);                                                           \
        }                                                                      \
    }                                                                          \
    static inline __attribute__((always_inline)) void NAME##_reserve(          \
        khash_t(NAME) * *map, HashFn hash, usize n) {                          \
        (void)hash;                                                            \
        /* kh_resize takes buckets, not an element count. */                   \
        khint_t buckets = 4;                                                   \
        while ((usize)(buckets * __ac_HASH_UPPER + 0.5) < n)                   \
            buckets *= 2;                                                      \
        if (kh_resize(NAME, *map, buckets) < 0) {                              \
            xprint_err("%z", "khash allocation failed\n");                     \
            exit(1);                                                           \
        }                                                                      \
    }                                                                          \
    static inline __attribute__((always_inline)) bool NAME##_insert(           \
        khash_t(NAME) * *map, HashFn hash, const void *key, usize len,         \
        void *val) {                                                           \
        (void)hash;                                                            \
        (void)len;                                                             \
        u64 k;                                                                 \
        xmemcp(&k, (void *)key, sizeof k);                                     \
        int result;                                                            \
        khint_t i = kh_put(NAME, *map, k, &result);                            \
        if (result < 0) {                                                      \
            xprint_err("%z", "khash allocation failed\n");                     \
            exit(1);                                                           \
        }                                                                      \
        if (IS_MAP)                                                            \
            xmemcp(&kh_val(*map, i), val, sizeof(u64));                        \
        return result == 0;                                                    \
    }                                                                          \
    static inline __attribute__((always_inline)) void *NAME##_get(             \
        khash_t(NAME) * *map, HashFn hash, const void *key, usize len) {       \
        (void)hash;                                                            \
        (void)len;                                                             \
        u64 k;                                                                 \
        xmemcp(&k, (void *)key, sizeof k);                                     \
        khint_t i = kh_get(NAME, *map, k);                                     \
        if (i == kh_end(*map))                                                 \
            return 0;                                                          \
        if (IS_MAP)                                                            \
            return &kh_val(*map, i);                                           \
        return &kh_key(*map, i);                                               \
    }                                                                          \
    static inline __attribute__((always_inline)) void NAME##_free(             \
        khash_t(NAME) * *map) {                                                \
        kh_destroy(NAME, *map);                                                \
        *map = 0;                                                              \
    }

typedef struct {
    double cycles[5];
} BenchResult;
typedef struct {
    unsigned wins, ties, losses;
    double log_speedup;
} Summary;
static Summary map_summary, set_summary, dyn_map_summary, dyn_set_summary;
static bool run_maps, run_sets, run_dynamic, run_fixed, quick;

static void compare(Summary *s, BenchResult ours, BenchResult reference) {
    for (unsigned i = 0; i < 5; ++i) {
        const double ratio = reference.cycles[i] / ours.cycles[i];
        s->log_speedup += log(ratio);
        if (ratio > 1.05)
            ++s->wins;
        else if (ratio < 1.0 / 1.05)
            ++s->losses;
        else
            ++s->ties;
    }
}
static void summary(const char *name, Summary s) {
    const unsigned total = s.wins + s.ties + s.losses;
    if (!total)
        return;
    xprint("%z(e=19,rp=' ') %u32(e=4) faster, %u32(e=4) within 5%%, %u32(e=4) "
           "slower; geomean %f64(r=2)x khash speed\n",
           name, &s.wins, &s.ties, &s.losses,
           &(f64){exp(s.log_speedup / total)});
}

// Insert includes allocation and, for "grow", rehashing. For "reserved", the
// initial reserve is outside the timer. Lookup/update use shuffled queries.
// One full untimed-result warmup precedes the measured repetitions.
// Free is outside the timer. Results include loop and result-consumption cost.
#define CONTAINER_BENCH(NAME, TYPE, INIT, RESERVE, INSERT, GET, FREE, HASH)    \
    static BenchResult NAME(const u64 *keys, const u64 *misses,                \
                            const usize *order, usize n) {                     \
        double totals[5] = {0};                                                \
        for (unsigned r = 0; r <= MAP_REPEATS; ++r) {                          \
            TYPE map;                                                          \
            INIT(&map, sizeof(u64));                                           \
            u64 start = timestamp();                                           \
            for (usize i = 0; i < n; ++i) {                                    \
                u64 value = i;                                                 \
                bool existed = INSERT(&map, HASH, keys + i, 8, &value);        \
                CONSUME(existed);                                              \
            }                                                                  \
            totals[0] += timestamp() - start;                                  \
            FREE(&map);                                                        \
            INIT(&map, sizeof(u64));                                           \
            RESERVE(&map, HASH, n);                                            \
            start = timestamp();                                               \
            for (usize i = 0; i < n; ++i) {                                    \
                u64 value = i;                                                 \
                bool existed = INSERT(&map, HASH, keys + i, 8, &value);        \
                CONSUME(existed);                                              \
            }                                                                  \
            totals[1] += timestamp() - start;                                  \
            start = timestamp();                                               \
            for (usize i = 0; i < n; ++i) {                                    \
                __auto_type found = GET(&map, HASH, keys + order[i], 8);       \
                CONSUME(found);                                                \
            }                                                                  \
            totals[2] += timestamp() - start;                                  \
            start = timestamp();                                               \
            for (usize i = 0; i < n; ++i) {                                    \
                __auto_type found = GET(&map, HASH, misses + order[i], 8);     \
                CONSUME(found);                                                \
            }                                                                  \
            totals[3] += timestamp() - start;                                  \
            start = timestamp();                                               \
            for (usize i = 0; i < n; ++i) {                                    \
                u64 value = i;                                                 \
                bool existed = INSERT(&map, HASH, keys + order[i], 8, &value); \
                CONSUME(existed);                                              \
            }                                                                  \
            totals[4] += timestamp() - start;                                  \
            FREE(&map);                                                        \
            if (!r)                                                            \
                xmemzero(totals, sizeof totals); /* warmup */                  \
        }                                                                      \
        xprint("%z(e=30,rp=' ') %u64(e=7)", #NAME, &n);                 \
        for (unsigned i = 0; i < 5; ++i)                                       \
            xprint(" %f64(r=2,e=12)", &(f64){totals[i] / (n * MAP_REPEATS)});  \
        xprint("\n");                                           \
        BenchResult result;                                                    \
        for (unsigned i = 0; i < 5; ++i)                                       \
            result.cycles[i] = totals[i] / (n * MAP_REPEATS);                  \
        return result;                                                         \
    }
#define XHASH_BENCH(HASH)                                                      \
    CONTAINER_BENCH(node_map_##HASH, XNodeHashMap, node_init, xnhm_reserve,    \
                    node_insert, node_get, xnhm_free, HASH)                    \
    CONTAINER_BENCH(node_set_##HASH, XNodeHashSet, node_set_init,              \
                    xnhs_reserve, node_set_insert, node_set_get, xnhs_free,    \
                    HASH)                                                      \
    CONTAINER_BENCH(array_map_##HASH, XArrayHashMap, array_init, xahm_reserve, \
                    array_insert, array_get, xahm_free, HASH)                  \
    CONTAINER_BENCH(array_set_##HASH, XArrayHashSet, array_set_init,           \
                    xahs_reserve, array_set_insert, array_set_get, xahs_free,  \
                    HASH)                                                      \
    CONTAINER_BENCH(dyn_node_map_##HASH, XDynNodeHashMap, xdnhm_init,          \
                    xdnhm_reserve, xdnhm_insert, xdnhm_get, xdnhm_free, HASH)  \
    CONTAINER_BENCH(dyn_node_set_##HASH, XDynNodeHashSet, dyn_node_set_init,   \
                    xdnhs_reserve, dyn_node_set_insert, dyn_node_set_get,      \
                    xdnhs_free, HASH)                                          \
    CONTAINER_BENCH(dyn_array_map_##HASH, XDynArrayHashMap, xdahm_init,        \
                    xdahm_reserve, xdahm_insert, xdahm_get, xdahm_free, HASH)  \
    CONTAINER_BENCH(dyn_array_set_##HASH, XDynArrayHashSet,                    \
                    dyn_array_set_init, xdahs_reserve, dyn_array_set_insert,   \
                    dyn_array_set_get, xdahs_free, HASH)
XHASH_BENCH(xhash_fnv1a)
XHASH_BENCH(xhash_zx)
XHASH_BENCH(xhash_zxmix)
XHASH_BENCH(xhash_xxh3)
XHASH_BENCH(xhash_rapidhash)
XHASH_BENCH(khash_native)

#define KHASH_BENCH_ONE(NAME, HASH, IS_MAP)                                    \
    KHASH_BACKEND(NAME, HASH, IS_MAP)                                          \
    CONTAINER_BENCH(NAME, khash_t(NAME) *, NAME##_init, NAME##_reserve,        \
                    NAME##_insert, NAME##_get, NAME##_free, HASH)
#define KHASH_BENCH(HASH)                                                      \
    KHASH_BENCH_ONE(khash_map_##HASH, HASH, 1)                                 \
    KHASH_BENCH_ONE(khash_set_##HASH, HASH, 0)
KHASH_BENCH(xhash_fnv1a)
KHASH_BENCH(xhash_zx)
KHASH_BENCH(xhash_zxmix)
KHASH_BENCH(xhash_xxh3)
KHASH_BENCH(xhash_rapidhash)
KHASH_BENCH(khash_native)

static void bench_containers(usize n, bool mixed) {
    u64 *keys = xalloc(n * sizeof(*keys));
    u64 *misses = xalloc(n * sizeof(*misses));
    usize *order = xalloc(n * sizeof(*order));
    XRngXoshiro256ppState rng;
    xrng_xoshiro256pp_seed(&rng, 1000);
    for (usize i = 0; i < n; ++i) {
        // Odd multiplication is bijective: distinct hits and misses, with
        // varied high bits. Unlike random keys, uniqueness is guaranteed.
        keys[i] = (2 * i) * 0x9e3779b97f4a7c15ull;
        misses[i] = (2 * i + 1) * 0x9e3779b97f4a7c15ull;
        if (mixed) {
            // SplitMix's bijective finalizer preserves uniqueness/disjointness
            // while removing the regular low-bit pattern of the first dataset.
            keys[i] = xhash_zxmix(&keys[i], 8);
            misses[i] = xhash_zxmix(&misses[i], 8);
        }
        order[i] = i;
    }
    for (usize i = n; i > 1; --i) {
        usize j = xrng_xoshiro256pp_u64(&rng) % i;
        usize tmp = order[i - 1];
        order[i - 1] = order[j];
        order[j] = tmp;
    }
#define RUN(H)                                                                 \
    do {                                                                       \
        if (run_maps) {                                                        \
            BenchResult kh = khash_map_##H(keys, misses, order, n);            \
            if (run_fixed) {                                                   \
                BenchResult a = array_map_##H(keys, misses, order, n);         \
                compare(&map_summary, a, kh);                                  \
                (void)node_map_##H(keys, misses, order, n);                    \
            }                                                                  \
            if (run_dynamic) {                                                 \
                BenchResult a = dyn_array_map_##H(keys, misses, order, n);     \
                compare(&dyn_map_summary, a, kh);                              \
                (void)dyn_node_map_##H(keys, misses, order, n);                \
            }                                                                  \
        }                                                                      \
        if (run_sets) {                                                        \
            BenchResult kh = khash_set_##H(keys, misses, order, n);            \
            if (run_fixed) {                                                   \
                BenchResult a = array_set_##H(keys, misses, order, n);         \
                compare(&set_summary, a, kh);                                  \
                (void)node_set_##H(keys, misses, order, n);                    \
            }                                                                  \
            if (run_dynamic) {                                                 \
                BenchResult a = dyn_array_set_##H(keys, misses, order, n);     \
                compare(&dyn_set_summary, a, kh);                              \
                (void)dyn_node_set_##H(keys, misses, order, n);                \
            }                                                                  \
        }                                                                      \
    } while (0)
    RUN(xhash_zxmix);
    RUN(khash_native);
    if (!quick) {
        RUN(xhash_fnv1a);
        RUN(xhash_zx);
        RUN(xhash_xxh3);
        RUN(xhash_rapidhash);
    }
#undef RUN
    xfree(keys, n * sizeof(*keys));
    xfree(misses, n * sizeof(*misses));
    xfree(order, n * sizeof(*order));
}

int main(int argc, char **argv) {
    if (argc == 2 && (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h"))) {
        xprint("usage: %z [quick|hash|maps|sets|dynamic|all]\nquick (default): "
               "fixed maps, two hashes, one dataset/size\nhash: hash "
               "throughput; maps/sets: full fixed-key suites\ndynamic: "
               "dynamic-key maps/sets; all: every suite\n",
               argv[0]);
        return 0;
    }
    const char *mode = argc > 1 ? argv[1] : "quick";
    const bool all = !strcmp(mode, "all");
    quick = !strcmp(mode, "quick");
    const bool hashes = !strcmp(mode, "hash");
    run_dynamic = all || !strcmp(mode, "dynamic");
    run_maps = all || quick || run_dynamic || !strcmp(mode, "maps") ||
               !strcmp(mode, "containers");
    run_sets = all || run_dynamic || !strcmp(mode, "sets");
    run_fixed = !run_dynamic || all;
    if (argc > 2 || !(quick || hashes || run_maps || run_sets)) {
        xprint_err("usage: %z [quick|hash|maps|sets|dynamic|all]\n", argv[0]);
        return 1;
    }
    xprint("%z\n",
           "Average TSC cycles/operation; inputs generated outside timers.");
    if (hashes || all)
        bench_hashes();
    if (run_maps || run_sets) {
        xprint("%z\n",
               "8-byte keys/values. khash: libc, max load 0.77; Xhash: xalloc, "
               "max load 0.5.");
        xprint("%z\n",
               "backend/hash                     count  insert-grow "
               "insert-reserved   lookup-hit  lookup-miss update/exists");
        if (!quick) {
            xprint("%z\n", "dataset: patterned unique integers");
            bench_containers(1024, false);
            bench_containers(16384, false);
        }
        xprint("%z\n", "dataset: mixed unique integers");
        if (!quick)
            bench_containers(1024, true);
        bench_containers(16384, true);
        xprint("%z\n",
               "\nArray vs khash summary (matching hash, dataset, size, and "
               "operation):");
        summary("Fixed maps", map_summary);
        summary("Fixed sets", set_summary);
        summary("Dynamic-key maps", dyn_map_summary);
        summary("Dynamic-key sets", dyn_set_summary);
        xprint("%z\n",
               "5% is a reporting tolerance, not a confidence interval. Node "
               "timings are separate.");
        if (quick)
            xprint("%z\n",
                   "Use hash, maps, sets, dynamic, or all for broader suites.");
    }
    return 0;
}
