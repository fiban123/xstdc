#pragma once

#include "types.h"

// atomic counting spin loop mutex, designed for short lock durations
// use zero initialization: XLock lock = {0};
// Successful acquisition has acquire semantics; unlock has release semantics.
typedef struct {
    u32 next;
    u32 serving;
} XLock;

// designed for longer lock durations
// Nonrecursive; initialize before use. Do not copy or move an initialized
// mutex.
typedef struct {
    void *opaque;
} XMutex;

// Nonrecursive. Do not copy or move a lock while it is in use. Ticket counters
// wrap naturally; fewer than 2^32 acquisitions may be pending.
static inline void xlock_lock(XLock *lk);
// Unpaused spinning for very short critical sections. Same ticket ordering and
// unlock operation as xlock_lock; both variants may be used on the same lock.
static inline void xlock_lock_fast(XLock *lk);
// return true if lock is acquired
static inline bool xlock_try_lock(XLock *lk);
// must only be called by the locking thread
static inline void xlock_unlock(XLock *lk);

// Initialize a nonrecursive mutex; it must not already be initialized.
void xmtx_init(XMutex *mtx);
// Wait until the mutex is acquired.
void xmtx_lock(XMutex *mtx);
// return true if lock is acquired
bool xmtx_try_lock(XMutex *mtx);
// must only be called by the locking thread
void xmtx_unlock(XMutex *mtx);
// Release an unused mutex; no thread may own, wait for, or access it. The mutex
// storage can then be reinitialized.
void xmtx_free(XMutex *mtx);

typedef struct {
    void *opaque;
} XThread;

// Start fn(arg); return false and clear t on failure. arg must outlive its use.
// t must not hold an unjoined thread; each successful start requires one join.
bool xthread_start(XThread *t, void (*fn)(void *), void *arg);

// Waits and releases resources. Exactly one join per successful start; no self
// join or concurrent joins. Do not copy a live XThread. Storage can be reused.
void xthread_join(XThread *t);

// Definitions

static inline void xlock_lock(XLock *lk) {
    const u32 ticket = __atomic_fetch_add(&lk->next, 1, __ATOMIC_RELAXED);
    while (__atomic_load_n(&lk->serving, __ATOMIC_ACQUIRE) != ticket) {
#if defined(__i386__) || defined(__x86_64__)
        __asm__ volatile("pause");
#elif defined(__aarch64__)
        __asm__ volatile("yield");
#endif
    }
}

static inline void xlock_lock_fast(XLock *lk) {
    const u32 ticket = __atomic_fetch_add(&lk->next, 1, __ATOMIC_RELAXED);
    while (__atomic_load_n(&lk->serving, __ATOMIC_ACQUIRE) != ticket) {
    }
}

static inline bool xlock_try_lock(XLock *lk) {
    u32 ticket = __atomic_load_n(&lk->serving, __ATOMIC_ACQUIRE);
    return __atomic_compare_exchange_n(&lk->next, &ticket, ticket + 1, false,
                                       __ATOMIC_RELAXED, __ATOMIC_RELAXED);
}

static inline void xlock_unlock(XLock *lk) {
    const u32 ticket = __atomic_load_n(&lk->serving, __ATOMIC_RELAXED);
    __atomic_store_n(&lk->serving, ticket + 1, __ATOMIC_RELEASE);
}
