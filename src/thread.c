#include "xstdc/thread.h"
#include "xstdc/alloc.h"
#include "xstdc/assert.h"

#include <threads.h>

// C11 backend for POSIX systems with <threads.h>. No native backend types
// appear in the public API; libc owns stack allocation and TLS setup/teardown.
typedef struct {
    mtx_t mutex;
} XMutexState;

typedef struct {
    thrd_t thread;
    void (*fn)(void *);
    void *arg;
} XThreadState;

// These void APIs cannot report OS resource errors. Fail even in fast builds.
static void thread_require(int status) {
    if (status != thrd_success)
        xstdc_assert_fail(__LINE__, __FILE__, __func__);
}

void xmtx_init(XMutex *mtx) {
    assert(mtx);
    XMutexState *state = xalloc(sizeof(*state));
    const int status = mtx_init(&state->mutex, mtx_plain);
    if (status != thrd_success) {
        xfree(state, sizeof(*state));
        thread_require(status);
        return;
    }
    mtx->opaque = state;
}

void xmtx_lock(XMutex *mtx) {
    assert(mtx && mtx->opaque);
    XMutexState *state = mtx->opaque;
    thread_require(mtx_lock(&state->mutex));
}

bool xmtx_try_lock(XMutex *mtx) {
    assert(mtx && mtx->opaque);
    XMutexState *state = mtx->opaque;
    const int status = mtx_trylock(&state->mutex);
    if (status == thrd_busy)
        return false;
    thread_require(status);
    return true;
}

void xmtx_unlock(XMutex *mtx) {
    assert(mtx && mtx->opaque);
    XMutexState *state = mtx->opaque;
    thread_require(mtx_unlock(&state->mutex));
}

void xmtx_free(XMutex *mtx) {
    assert(mtx && mtx->opaque);
    XMutexState *state = mtx->opaque;
    mtx_destroy(&state->mutex);
    xfree(state, sizeof(*state));
    mtx->opaque = 0;
}

static int thread_entry(void *arg) {
    XThreadState *state = arg;
    state->fn(state->arg);
    return 0;
}

bool xthread_start(XThread *t, void (*fn)(void *), void *arg) {
    assert(t && fn);
    XThreadState *state = xalloc(sizeof(*state));
    state->fn = fn;
    state->arg = arg;
    const int status = thrd_create(&state->thread, thread_entry, state);
    if (status != thrd_success) {
        xfree(state, sizeof(*state));
        t->opaque = 0;
        return false;
    }
    t->opaque = state;
    return true;
}

void xthread_join(XThread *t) {
    assert(t && t->opaque);
    XThreadState *state = t->opaque;
    thread_require(thrd_join(state->thread, 0));
    xfree(state, sizeof(*state));
    t->opaque = 0;
}
