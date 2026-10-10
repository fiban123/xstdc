#include "xstdc/print.h"
#include <stdlib.h>
#include <string.h>

#include "xstdc/alloc.h"
#include "xstdc/math.h"
#include "xstdc/mem.h"
#include "xstdc/rng.h"
#include "xstdc/time.h"
#include "xstdc/types.h"

typedef struct {
    // unmanaged allocator, length needs external storage
    void *(*alloc)(usize n);
    void *(*realloc)(void *mem, usize old, usize new);
    void (*free)(void *mem, usize n);
    f32 (*get_efficiency)();
} Alloc;

void *libc_alloc(usize n) { return malloc(n); }

void *libc_realloc(void *mem, usize old, usize new) {
    return realloc(mem, new);
}

void libc_free(void *mem, usize n) { return free(mem); }

void *alloc_alloc(Alloc *alloc, usize n) { return alloc->alloc(n); }

void *alloc_realloc(Alloc *alloc, void *mem, usize old, usize new) {
    return alloc->realloc(mem, old, new);
}

void alloc_free(Alloc *alloc, void *mem, usize n) {
    return alloc->free(mem, n);
}

// alloc free cycle
void alloc_free_cycle(Alloc *alloc, usize n, usize sz) {
    void **allocs = alloc_alloc(alloc, n * sizeof(void *));

    for (usize i = 0; i < n; i++) {
        allocs[i] = alloc_alloc(alloc, sz);
    }

    for (usize i = 0; i < n; i++) {
        alloc_free(alloc, allocs[i], sz);
    }

    alloc_free(alloc, allocs, n * sizeof(void *));
}

void alloc_free_cycle_dispatch(Alloc *alloc) {
    alloc_free_cycle(alloc, 1000000, 1);
    alloc_free_cycle(alloc, 10000, 512);
    alloc_free_cycle(alloc, 100000, 19);
    alloc_free_cycle(alloc, 100000, 24);
    alloc_free_cycle(alloc, 1000, 7800);
    alloc_free_cycle(alloc, 1000000, 8);
    alloc_free_cycle(alloc, 100000, 71);
    alloc_free_cycle(alloc, 100000, 64);
    alloc_free_cycle(alloc, 10000, 512);
}

void random_alloc_free_cycle(Alloc *alloc, usize n, usize min, usize max) {
    void **allocs = alloc_alloc(alloc, n * (sizeof(void *) + sizeof(usize)));

    XRngXoshiro256ppState rng;
    xrng_xoshiro256pp_seed(&rng, 16);

    for (usize i = 0; i < n; i++) {
        usize csz = xrng_xoshiro256pp_u64range(&rng, min, max);
        allocs[i * 2] = alloc_alloc(alloc, csz);
        allocs[i * 2 + 1] = (void *)csz;
    }

    for (usize i = 0; i < n; i++) {
        alloc_free(alloc, allocs[i * 2], (usize)(allocs[i * 2 + 1]));
    }

    alloc_free(alloc, allocs, n * (sizeof(void *) + sizeof(usize)));
}

void random_alloc_free_cycle_dispatch(Alloc *alloc) {
    random_alloc_free_cycle(alloc, 1000000, 1, 7);
    random_alloc_free_cycle(alloc, 1000, 1, 8000);
    random_alloc_free_cycle(alloc, 10000, 1, 800);
    random_alloc_free_cycle(alloc, 100000, 16, 16);
    random_alloc_free_cycle(alloc, 100000, 16, 16);
    random_alloc_free_cycle(alloc, 1000000, 1, 3);
    random_alloc_free_cycle(alloc, 100, 1, 80000);
}

void random_linked_list_perm(Alloc *alloc, usize nops, usize max_size,
                             usize maxperm, usize min, usize max) {
    struct Elem {
        void *p;
        usize sz;
    };

    XRngXoshiro256ppState rng;
    xrng_xoshiro256pp_seed(&rng, 16);

    usize elems_sz = sizeof(struct Elem) * max_size;
    struct Elem *elems = alloc_alloc(alloc, elems_sz);
    xmemzero(elems, elems_sz);

    for (usize i = 0; i < nops; i++) {
        usize n = xrng_xoshiro256pp_u64range(&rng, 0, maxperm + 1);
        bool add = xrng_xoshiro256pp_u64(&rng) & 1;

        if (add) {
            for (usize i = 0; i < n; i++) {
                usize idx = xrng_xoshiro256pp_u64range(&rng, 0, max_size);

                if (elems[idx].sz == 0) {
                    usize sz = xrng_xoshiro256pp_u64range(&rng, min, max + 1);
                    elems[idx].p = alloc_alloc(alloc, sz);
                    elems[idx].sz = sz;
                }
            }
        } else {
            for (usize i = 0; i < n; i++) {
                usize idx = xrng_xoshiro256pp_u64range(&rng, 0, max_size);

                if (elems[idx].sz > 0) {
                    alloc_free(alloc, elems[idx].p, elems[idx].sz);
                    elems[idx].sz = 0;
                }
            }
        }
    }
}

void random_linked_list_perm_dispatch(Alloc *alloc) {
    random_linked_list_perm(alloc, 100000, 10, 10, 1, 20);
    random_linked_list_perm(alloc, 100000, 10, 10, 8, 8);
    random_linked_list_perm(alloc, 10000, 100, 50, 1, 20);
    random_linked_list_perm(alloc, 1000, 1000, 1000, 4, 20);
    random_linked_list_perm(alloc, 1000, 1000, 1000, 16, 16);
    random_linked_list_perm(alloc, 500, 10000, 500, 16, 16);
    random_linked_list_perm(alloc, 500, 100, 30, 1024, 1024);
}

void vector_realloc(Alloc *alloc, usize n) {
    u64 *elems = 0;
    u64 sz = 0;
    u64 cap = 0;

    XRngWyrandState rng;
    xrng_wyrand_seed(&rng, 1);

    for (usize i = 0; i < n; i++) {
        u64 elem = xrng_wyrand_u64(&rng);

        if (sz >= cap) {
            usize new = u64max(cap, 1) * 2;
            elems = alloc_realloc(alloc, elems, cap * sizeof(u64),
                                  new * sizeof(u64));
            cap = new;
        }

        elems[sz++] = elem;
    }

    for (usize i = 0; i < n; i++) {
        sz--;

        if (sz * 2 <= cap) {
            usize new = cap / 2;
            elems = alloc_realloc(alloc, elems, cap * sizeof(u64),
                                  new * sizeof(u64));
            cap = new;
        }
    }
}

void vector_realloc_dispatch(Alloc *alloc) {
    vector_realloc(alloc, 256);
    vector_realloc(alloc, 127);
    vector_realloc(alloc, 519);
    vector_realloc(alloc, 1005);
    vector_realloc(alloc, 12000);
    vector_realloc(alloc, 512);
    vector_realloc(alloc, 1000);
    vector_realloc(alloc, 1 << 16);
    vector_realloc(alloc, 10001);
}

int main(int argc, char **argv) {
    enum {
        ALGO_NONE,
        RANDOM_LINKED_LIST_PERM,
        VECTOR_REALLOC,
        ALLOC_FREE_CYC,
        RANDOM_ALLOC_FREE_CYC
    } env = 0;
    enum { ALLOC_NONE, LIBC, XALLOC } alloc = 0;

    for (usize i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--env") == 0 && ++i < argc) {
            if (strcmp(argv[i], "random_linked_list_perm") == 0) {
                env = RANDOM_LINKED_LIST_PERM;
            } else if (strcmp(argv[i], "vector_realloc") == 0) {
                env = VECTOR_REALLOC;
            } else if (strcmp(argv[i], "alloc_free_cycle") == 0) {
                env = ALLOC_FREE_CYC;
            } else if (strcmp(argv[i], "random_alloc_free_cycle") == 0) {
                env = RANDOM_ALLOC_FREE_CYC;
            }

            else
                return 1;
        }
        if (strcmp(argv[i], "--alloc") == 0 && ++i < argc) {
            if (strcmp(argv[i], "libc") == 0) {
                alloc = LIBC;
            } else if (strcmp(argv[i], "xalloc") == 0) {
                alloc = XALLOC;
            } else
                return 1;
        }
    }

    if (!env || !alloc)
        return 1;

    Alloc allocator;

    if (alloc == LIBC) {
        allocator = (Alloc){libc_alloc, libc_realloc, libc_free};
    }
    if (alloc == XALLOC) {
        allocator = (Alloc){
            xalloc,
            xrealloc,
            xfree,
        };
    }

    u64 start = xtime_cycles();

    if (env == RANDOM_LINKED_LIST_PERM) {
        random_linked_list_perm_dispatch(&allocator);
    } else if (env == VECTOR_REALLOC) {
        vector_realloc_dispatch(&allocator);
    } else if (env == ALLOC_FREE_CYC) {
        alloc_free_cycle_dispatch(&allocator);
    } else if (env == RANDOM_ALLOC_FREE_CYC) {
        random_alloc_free_cycle_dispatch(&allocator);
    }

    u64 end = xtime_cycles();

    u64 cycles = end - start;

    xprint("Benchmark result with environment %i32 and allocator %i32\n",
           &(i32){env}, &(i32){alloc});
    xprint("CPU cycles (lower is faster): %u64\n", &cycles);
}
