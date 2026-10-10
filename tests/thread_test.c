#include "xstdc/alloc.h"
#include "xstdc/assert.h"
#include "xstdc/print.h"
#include "xstdc/thread.h"
#include <errno.h>
#include <limits.h>
#include <sched.h>
#include <unistd.h>

enum { WORKERS = 8, ITERATIONS = 12000 };
static _Thread_local unsigned local_value;

typedef struct {
    XLock spin;
    XMutex mutex;
    unsigned ready;
    bool go;
    unsigned spin_count, mutex_count;
} Shared;
typedef struct {
    Shared *shared;
    unsigned id, result;
} Worker;

static void work(void *arg) {
    Worker *w = arg;
    Shared *s = w->shared;
    assert(local_value == 0);
    local_value = w->id + 1;
    errno = (int)w->id + 100;
    unsigned *allocation = xalloc(sizeof(*allocation));
    *allocation = local_value;
    __atomic_fetch_add(&s->ready, 1, __ATOMIC_SEQ_CST);
    while (!__atomic_load_n(&s->go, __ATOMIC_ACQUIRE))
        sched_yield();
    for (unsigned i = 0; i < ITERATIONS; ++i) {
        if (w->id & 1)
            xlock_lock_fast(&s->spin);
        else
            xlock_lock(&s->spin);
        ++s->spin_count;
        xlock_unlock(&s->spin);
        xmtx_lock(&s->mutex);
        ++s->mutex_count;
        xmtx_unlock(&s->mutex);
    }
    assert(local_value == w->id + 1 && *allocation == local_value);
    // Test errno isolation before freeing, which may itself change errno.
    assert(errno == (int)w->id + 100);
    w->result = local_value;
    xfree(allocation, sizeof(*allocation));
}
static void test_contention(void) {
    Shared s = {0};
    XThread threads[WORKERS];
    Worker workers[WORKERS];
    xmtx_init(&s.mutex);
    local_value = 123;
    for (unsigned i = 0; i < WORKERS; ++i) {
        workers[i] = (Worker){.shared = &s, .id = i};
        assert(xthread_start(&threads[i], work, &workers[i]));
    }
    while (__atomic_load_n(&s.ready, __ATOMIC_SEQ_CST) != WORKERS)
        sched_yield();
    __atomic_store_n(&s.go, true, __ATOMIC_RELEASE);
    for (unsigned i = 0; i < WORKERS; ++i) {
        xthread_join(&threads[i]);
        assert(!threads[i].opaque && workers[i].result == i + 1);
    }
    assert(s.spin_count == WORKERS * ITERATIONS);
    assert(s.mutex_count == WORKERS * ITERATIONS);
    assert(local_value == 123);
    xmtx_free(&s.mutex);
    assert(!s.mutex.opaque);
}

typedef struct {
    XLock spin;
    XMutex mutex;
    bool tried;
    unsigned value;
} TryState;
static void try_worker(void *arg) {
    TryState *s = arg;
    assert(!xlock_try_lock(&s->spin));
    assert(!xmtx_try_lock(&s->mutex));
    __atomic_store_n(&s->tried, true, __ATOMIC_SEQ_CST);
    xmtx_lock(&s->mutex);
    assert(s->value == 42);
    s->value = 43;
    xmtx_unlock(&s->mutex);
}
static void test_try(void) {
    TryState s = {0};
    xmtx_init(&s.mutex);
    assert(xlock_try_lock(&s.spin));
    assert(xmtx_try_lock(&s.mutex));
    XThread t;
    assert(xthread_start(&t, try_worker, &s));
    while (!__atomic_load_n(&s.tried, __ATOMIC_SEQ_CST))
        sched_yield();
    s.value = 42;
    xlock_unlock(&s.spin);
    xmtx_unlock(&s.mutex);
    xthread_join(&t);
    assert(s.value == 43);
    assert(xlock_try_lock(&s.spin));
    xlock_unlock(&s.spin);
    assert(xmtx_try_lock(&s.mutex));
    xmtx_unlock(&s.mutex);
    xmtx_free(&s.mutex);
    xmtx_init(&s.mutex);
    xmtx_lock(&s.mutex);
    xmtx_unlock(&s.mutex);
    xmtx_free(&s.mutex);
}

typedef struct {
    XLock *lock;
    unsigned *position;
    unsigned expected;
} Ticket;
static void ticket_worker(void *arg) {
    Ticket *t = arg;
    if (t->expected & 1)
        xlock_lock_fast(t->lock);
    else
        xlock_lock(t->lock);
    assert((*t->position)++ == t->expected);
    xlock_unlock(t->lock);
}
static void test_ticket_wrap(void) {
    XLock lock = {.next = UINT_MAX - 2, .serving = UINT_MAX - 2};
    XThread threads[WORKERS];
    Ticket tickets[WORKERS];
    unsigned position = 0;
    xlock_lock_fast(&lock);
    for (unsigned i = 0; i < WORKERS; ++i) {
        tickets[i] = (Ticket){&lock, &position, i};
        assert(xthread_start(&threads[i], ticket_worker, &tickets[i]));
        while (__atomic_load_n(&lock.next, __ATOMIC_SEQ_CST) !=
               (u32)(UINT_MAX - 1 + i + 1))
            sched_yield();
        assert(!xlock_try_lock(&lock));
    }
    xlock_unlock(&lock);
    for (unsigned i = 0; i < WORKERS; ++i)
        xthread_join(&threads[i]);
    assert(position == WORKERS);
    assert(xlock_try_lock(&lock));
    xlock_unlock(&lock);
    // Try-lock itself must also handle counter wraparound.
    __atomic_store_n(&lock.next, UINT_MAX, __ATOMIC_SEQ_CST);
    __atomic_store_n(&lock.serving, UINT_MAX, __ATOMIC_SEQ_CST);
    assert(xlock_try_lock(&lock));
    xlock_unlock(&lock);
    assert(__atomic_load_n(&lock.next, __ATOMIC_SEQ_CST) == 0 &&
           __atomic_load_n(&lock.serving, __ATOMIC_SEQ_CST) == 0);
}
static void null_worker(void *arg) {
    assert(!arg);
    assert(local_value == 0);
    local_value = 1;
}
static void test_reuse(void) {
    XThread t;
    for (unsigned i = 0; i < 64; ++i) {
        assert(xthread_start(&t, null_worker, 0));
        xthread_join(&t);
        assert(!t.opaque);
    }
}
int main(void) {
    // A synchronization regression should fail the test instead of hanging it.
    alarm(30);
    test_try();
    test_ticket_wrap();
    test_contention();
    test_reuse();
    alarm(0);
    xprint("thread tests passed\n");
}
