#include "xstdc/alloc.h"
#include "xstdc/process.h"
#include "xstdc/types.h"
#include <mimalloc-3.5/mimalloc.h>
#include <sys/cdefs.h>

#if (!defined(__OPTIMIZE__) || defined(XSTDC_ALWAYS_DEBUG_ALLOC)) &&           \
    !defined(XSTDC_DEBUG_ALLOC)
#define XSTDC_DEBUG_ALLOC
#endif

// mimalloc may include libc's assert.h; use the project's assertions below.
#ifdef assert
#undef assert
#endif
#ifdef bool
#undef bool
#endif
#include "xstdc/debug.h"
#include "xstdc/print.h"

#ifdef XSTDC_DEBUG_ALLOC
#include "xstdc/hashmap.h"
#include "xstdc/thread.h"

typedef struct {
    usize len, alignment;
    bool live;
    const char *operation;
    XFullBacktrace trace;
} AllocationRecord;

static XNodeHashMap allochm;
static bool allochm_initialized;
static XLock allocation_lock;
static _Thread_local bool debug_block;

static bool debug_begin(void) {
    if (debug_block)
        return false;
    // Set this before taking the lock: map/trace/diagnostic allocations must
    // bypass both tracking and the lock on this thread.
    debug_block = true;
    xlock_lock(&allocation_lock);
    if (!allochm_initialized) {
        xnhm_init(&allochm, sizeof(void *), sizeof(AllocationRecord));
        allochm_initialized = true;
    }
    return true;
}
static void debug_end(void) {
    xlock_unlock(&allocation_lock);
    debug_block = false;
}
static void debug_history(AllocationRecord *record) {
    if (record) {
        xprint_err("Last valid operation: %z\n", record->operation);
        xprint_err("Backtrace:\n");
        xbacktrace_full_symbolize(&record->trace);
        xbacktrace_full_print(&record->trace);
    } else {
        xprint_err(
            "No allocation history for this pointer. Current backtrace:\n");
        XFullBacktrace trace = xbacktrace_full();
        xbacktrace_full_symbolize(&trace);
        xbacktrace_full_print(&trace);
        xbacktrace_full_free(&trace);
    }
}
static _Noreturn void debug_fail(void *pointer, const char *reason,
                                 AllocationRecord *record) {
    const usize address = (usize)pointer;
    xprint_err("%z 0x%usz(x)\n", reason, &address);
    debug_history(record);
    // Leave tracking disabled during exit handlers on this failing thread.
    xlock_unlock(&allocation_lock);
    xexit(1);
}
static AllocationRecord *debug_check(void *pointer, usize length, bool sized,
                                     usize alignment, bool aligned) {
    AllocationRecord *record = xnhm_get(&allochm, xhash_zxmix, &pointer);
    if (!record)
        debug_fail(pointer, "Unknown allocation", 0);
    if (!record->live)
        debug_fail(pointer, "Allocation has already been freed:", record);
    if (sized && record->len != length) {
        xprint_err("Expected length %usz, received %usz\n", &record->len,
                   &length);
        debug_fail(pointer, "Corrupted length of allocation", record);
    }
    if (aligned && record->alignment != alignment) {
        xprint_err("Expected alignment %usz, received %usz\n",
                   &record->alignment, &alignment);
        debug_fail(pointer, "Corrupted alignment of allocation", record);
    }
    return record;
}
static void debug_update(AllocationRecord *record, usize length,
                         usize alignment, bool live, const char *operation) {
    xbacktrace_full_free(&record->trace);
    record->len = length;
    record->alignment = alignment;
    record->live = live;
    record->operation = operation;
    record->trace = xbacktrace_full();
}
static void debug_insert(void *pointer, usize length, usize alignment,
                         const char *operation) {
    AllocationRecord *record = xnhm_get(&allochm, xhash_zxmix, &pointer);
    if (!record) {
        AllocationRecord empty = {0};
        xnhm_insert(&allochm, xhash_zxmix, &pointer, &empty);
        record = xnhm_get(&allochm, xhash_zxmix, &pointer);
    } else if (record->live) {
        debug_fail(pointer, "Allocator returned an already live allocation",
                   record);
    }
    debug_update(record, length, alignment, true, operation);
}
#endif

static void *__attribute__((always_inline)) allocate(usize n, usize alignment,
                                                     bool aligned, bool zero) {
#ifdef XSTDC_DEBUG_ALLOC
    const bool tracking = debug_begin();
#endif
    void *p = aligned ? (zero ? mi_zalloc_aligned(n, alignment)
                              : mi_malloc_aligned(n, alignment))
                      : (zero ? mi_zalloc(n) : mi_malloc(n));
#ifdef XSTDC_DEBUG_ALLOC
    if (tracking) {
        if (p)
            debug_insert(p, n, aligned ? alignment : 0, "Allocate");
        debug_end();
    }
#endif
    return p;
}
static void *__attribute__((always_inline)) resize(void *mem, usize old,
                                                   usize n, usize alignment,
                                                   bool sized, bool aligned) {
#ifdef XSTDC_DEBUG_ALLOC
    const bool tracking = debug_begin();
    AllocationRecord *record =
        tracking && mem ? debug_check(mem, old, sized, 0, false) : 0;
#else
    (void)old;
    (void)sized;
#endif
    void *p =
        aligned ? mi_realloc_aligned(mem, n, alignment) : mi_realloc(mem, n);
#ifdef XSTDC_DEBUG_ALLOC
    if (tracking) {
        // mimalloc realloc-to-zero returns a live zero-size allocation. A NULL
        // result indicates failure, leaving the original allocation/history
        // intact.
        if (p) {
            if (record && p == mem) {
                debug_update(record, n, aligned ? alignment : 0, true,
                             "Reallocate");
            } else {
                if (record)
                    debug_update(record, record->len, record->alignment, false,
                                 "Reallocate (old address released)");
                debug_insert(p, n, aligned ? alignment : 0, "Reallocate");
            }
        }
        debug_end();
    }
#endif
    return p;
}
static void __attribute__((always_inline))
release(void *mem, usize n, usize alignment, bool sized, bool aligned) {
#ifdef XSTDC_DEBUG_ALLOC
    if (!mem)
        return;
    const bool tracking = debug_begin();
    if (tracking) {
        AllocationRecord *record =
            debug_check(mem, n, sized, alignment, aligned);
        debug_update(record, record->len, record->alignment, false, "Free");
    }
#endif
    if (aligned) {
        if (sized)
            mi_free_size_aligned(mem, n, alignment);
        else
            mi_free_aligned(mem, alignment);
    } else {
        if (sized)
            mi_free_size(mem, n);
        else
            mi_free(mem);
    }
#ifdef XSTDC_DEBUG_ALLOC
    if (tracking)
        debug_end();
#endif
}

_Noreturn static void __attribute__((noinline)) alloc_fail(usize usz) {
    static _Thread_local bool reporting;
    if (reporting) {
        xprint_err("Backtrace unavailable: allocation failed while reporting "
                   "failure.\n");
        xexit(1);
    }
    reporting = true;
#ifdef XSTDC_DEBUG_ALLOC
    // Failure can occur in the tracker itself while it owns allocation_lock.
    debug_block = true;
#endif
    xprint_err("Allocation of size %usz failed.\n", &usz);
    xprint_err("Backtrace:\n");
    XFullBacktrace bt = xbacktrace_full();

    xbacktrace_full_symbolize(&bt);
    xbacktrace_full_print(&bt);

    xexit(1);
}

static inline void __attribute__((always_inline)) checkp(void *p, usize sz) {
    if (__builtin_expect(!p, 0)) {
        alloc_fail(sz);
    }
}

void *xalloc(usize n) {
    void *p = allocate(n, 0, false, false);
    checkp(p, n);
    return p;
}
void *xalloca(usize n, usize align) {
    void *p = allocate(n, align, true, false);
    checkp(p, n);
    return p;
}
void *xallocm(usize n) {
    void *p = allocate(n, 0, false, false);
    checkp(p, n);
    return p;
}
void *xallocma(usize n, usize align) {
    void *p = allocate(n, align, true, false);
    checkp(p, n);
    return p;
}
void *xallocz(usize n) {
    void *p = allocate(n, 0, false, true);
    checkp(p, n);
    return p;
}
void *xallocza(usize n, usize align) {
    void *p = allocate(n, align, true, true);
    checkp(p, n);
    return p;
}
void *xalloczm(usize n) {
    void *p = allocate(n, 0, false, true);
    checkp(p, n);
    return p;
}
void *xalloczma(usize n, usize align) {
    void *p = allocate(n, align, true, true);
    checkp(p, n);
    return p;
}
void *xrealloc(void *mem, usize old, usize n) {
    void *p = resize(mem, old, n, 0, true, false);
    checkp(p, n);
    return p;
}
void *xrealloca(void *mem, usize old, usize n, usize align) {
    void *p = resize(mem, old, n, align, true, true);
    checkp(p, n);
    return p;
}
void *xreallocm(void *mem, usize n) {
    void *p = resize(mem, 0, n, 0, false, false);
    checkp(p, n);
    return p;
}
void *xreallocma(void *mem, usize n, usize align) {
    void *p = resize(mem, 0, n, align, false, true);
    checkp(p, n);
    return p;
}

void *xtry_alloc(usize n) { return allocate(n, 0, false, false); }
void *xtry_alloca(usize n, usize align) {
    return allocate(n, align, true, false);
}
void *xtry_allocm(usize n) { return allocate(n, 0, false, false); }
void *xtry_allocma(usize n, usize align) {
    return allocate(n, align, true, false);
}
void *xtry_allocz(usize n) { return allocate(n, 0, false, true); }
void *xtry_allocza(usize n, usize align) {
    return allocate(n, align, true, true);
}
void *xtry_alloczm(usize n) { return allocate(n, 0, false, true); }
void *xtry_alloczma(usize n, usize align) {
    return allocate(n, align, true, true);
}
void *xtry_realloc(void *mem, usize old, usize n) {
    return resize(mem, old, n, 0, true, false);
}
void *xtry_realloca(void *mem, usize old, usize n, usize align) {
    return resize(mem, old, n, align, true, true);
}
void *xtry_reallocm(void *mem, usize n) {
    return resize(mem, 0, n, 0, false, false);
}
void *xtry_reallocma(void *mem, usize n, usize align) {
    return resize(mem, 0, n, align, false, true);
}

void xfree(void *mem, usize n) { release(mem, n, 0, true, false); }
void xfreea(void *mem, usize n, usize align) {
    release(mem, n, align, true, true);
}
void xfreem(void *mem) { release(mem, 0, 0, false, false); }
void xfreema(void *mem, usize align) { release(mem, 0, align, false, true); }

void xalloc_debug_unfreed(void) {
#ifdef XSTDC_DEBUG_ALLOC
    if (!debug_begin())
        return;
    for (usize i = 0; i < allochm.bucket_count; ++i) {
        for (XNodeHashEntry *entry = allochm.buckets[i]; entry;
             entry = entry->next) {
            AllocationRecord *record = (AllocationRecord *)entry->data;
            if (!record->live)
                continue;
            void *pointer;
            xmemcp(&pointer, entry->data + allochm.val_sz, sizeof(pointer));
            const usize address = (usize)pointer;
            xprint_err("Unfreed allocation 0x%usz(x), length %usz\n", &address,
                       &record->len);
            debug_history(record);
        }
    }
    debug_end();
#endif
}

bool xalloc_has_debug() {
#ifdef XSTDC_DEBUG_ALLOC
    return true;
#else
    return false;
#endif
}
