#include "xstdc/alloc.h"
#include "xstdc/assert.h"
#include "xstdc/debug.h"
#include "xstdc/mem.h"
#include "xstdc/print.h"
#include "xstdc/thread.h"
#include <errno.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

// Exported call sites make the saved operation distinguishable in diagnostics.
__attribute__((noinline)) void *alloc_test_allocate(void) {
    void *p = xalloc(17);
    __asm__ volatile("" : : "g"(p) : "memory");
    return p;
}
__attribute__((noinline)) void alloc_test_release(void *p) {
    xfree(p, 17);
    __asm__ volatile("" ::: "memory");
}
__attribute__((noinline)) void *alloc_test_resize(void *p, usize n) {
    void *next = xrealloc(p, 17, n);
    __asm__ volatile("" : : "g"(next) : "memory");
    return next;
}
static void wrong_free(void) { xfree(alloc_test_allocate(), 16); }
static void double_free(void) {
    void *p = alloc_test_allocate();
    alloc_test_release(p);
    xfreem(p);
}
static void wrong_realloc(void) {
    (void)xrealloc(alloc_test_allocate(), 16, 30);
}
static void realloc_dead(void) {
    void *p = alloc_test_allocate();
    alloc_test_release(p);
    (void)xreallocm(p, 20);
}
static void unknown_free(void) {
    int local;
    xfreem(&local);
}
static void unknown_realloc(void) {
    int local;
    (void)xreallocm(&local, 20);
}
static void realloc_history(void) {
    void *p = alloc_test_resize(alloc_test_allocate(), 1000000);
    xfree(p, 17);
}
static void realloc_old_pointer(void) {
    void *p = alloc_test_allocate();
    void *next = alloc_test_resize(p, 1000000);
    assert(next != p);
    xfreem(p);
}
static void wrong_alignment(void) { xfreea(xalloca(32, 64), 32, 32); }
static void zero_mismatch(void) { xfree(xalloc(0), 1); }
static void leak(void) {
    void *p = alloc_test_allocate();
    xalloc_debug_unfreed();
    xfree(p, 17);
    xalloc_debug_unfreed();
}
static void print_trace(void) {
    XBacktrace short_trace = xbacktrace();
    XFullBacktrace full = xbacktrace_full();
    xbacktrace_print(&short_trace);
    xbacktrace_full_print(&full);
    xbacktrace_free(&short_trace);
    xbacktrace_full_free(&full);
}
static void clean(void) {
    void *(*plain[])(usize) = {xalloc, xallocm, xallocz, xalloczm};
    void *(*aligned[])(usize, usize) = {xalloca, xallocma, xallocza, xalloczma};
    for (unsigned i = 0; i < 4; ++i) {
        u8 *p = plain[i](17);
        for (unsigned j = 0; j < 17; ++j) {
            if (i >= 2)
                assert(p[j] == 0);
            p[j] = (u8)j;
        }
        p = i & 1 ? xreallocm(p, 90) : xrealloc(p, 17, 90);
        for (unsigned j = 0; j < 17; ++j)
            assert(p[j] == j);
        if (i & 1)
            xfreem(p);
        else
            xfree(p, 90);
        p = aligned[i](17, 64);
        assert((usize)p % 64 == 0);
        for (unsigned j = 0; j < 17; ++j) {
            if (i >= 2)
                assert(p[j] == 0);
            p[j] = (u8)j;
        }
        p = i & 1 ? xreallocma(p, 90, 128) : xrealloca(p, 17, 90, 128);
        assert((usize)p % 128 == 0);
        for (unsigned j = 0; j < 17; ++j)
            assert(p[j] == j);
        if (i & 1)
            xfreema(p, 128);
        else
            xfreea(p, 90, 128);
    }
    void *p = xrealloc(0, 0, 17);
    // Same-size realloc exercises an in-place operation and updates history.
    p = xrealloc(p, 17, 17);
    p = xrealloc(p, 17, 0);
    assert(p);
    xfree(p, 0);
    xfree(xalloc(0), 0);
    xfree(0, 0);
    xfreem(0);
    xfreea(0, 0, 64);
    xfreema(0, 64);
    void *many[200];
    for (unsigned i = 0; i < 200; ++i)
        many[i] = xalloc(i);
    for (unsigned i = 0; i < 200; ++i)
        xfree(many[i], i);
    // Reused addresses must replace their freed history.
    for (unsigned i = 0; i < 300; ++i) {
        p = xalloc(17);
        xfree(p, 17);
    }
    xalloc_debug_unfreed();
}
static void worker(void *arg) {
    const usize n = *(usize *)arg;
    for (unsigned i = 0; i < 30; ++i) {
        void *p = xalloc(n);
        p = xrealloc(p, n, n + 1);
        xfree(p, n + 1);
    }
}
static void concurrent(void) {
    XThread threads[4];
    usize sizes[] = {8, 17, 33, 70};
    for (unsigned i = 0; i < 4; ++i)
        assert(xthread_start(&threads[i], worker, &sizes[i]));
    for (unsigned i = 0; i < 4; ++i)
        xthread_join(&threads[i]);
    xalloc_debug_unfreed();
}
static void check(void (*fn)(void), int status, const char *message,
                  const char *site) {
    int pipefd[2];
    assert(pipe(pipefd) == 0);
    pid_t child = fork();
    assert(child >= 0);
    if (!child) {
        close(pipefd[0]);
        assert(dup2(pipefd[1], STDERR_FILENO) == STDERR_FILENO);
        close(pipefd[1]);
        alarm(30);
        fn();
        _exit(0);
    }
    close(pipefd[1]);
    char output[16384];
    usize used = 0;
    for (;;) {
        ssize_t n = read(pipefd[0], output + used, sizeof(output) - 1 - used);
        if (n < 0 && errno == EINTR)
            continue;
        assert(n >= 0);
        if (!n)
            break;
        used += (usize)n;
        assert(used < sizeof(output) - 1);
    }
    output[used] = 0;
    close(pipefd[0]);
    int result;
    assert(waitpid(child, &result, 0) == child);
    if (!WIFEXITED(result) || WEXITSTATUS(result) != status ||
        (message ? !strstr(output, message) : used != 0) ||
        (site && !strstr(output, site))) {
        xprint_err("Unexpected sanitizer result:\n%z\n", output);
        assert(false);
    }
}
static void try_variants(void) {
    void *(*plain[])(usize) = {xtry_alloc, xtry_allocm, xtry_allocz,
                               xtry_alloczm};
    void *(*aligned[])(usize, usize) = {xtry_alloca, xtry_allocma, xtry_allocza,
                                        xtry_alloczma};
    for (unsigned i = 0; i < 4; ++i) {
        assert(!plain[i]((usize)-1));
        assert(!aligned[i]((usize)-1, 64));
        u8 *p = plain[i](17);
        assert(p);
        if (i >= 2)
            for (usize j = 0; j < 17; ++j)
                assert(p[j] == 0);
        p[0] = 42;
        assert(!(i & 1 ? xtry_reallocm(p, (usize)-1)
                       : xtry_realloc(p, 17, (usize)-1)));
        assert(p[0] == 42);
        p = i & 1 ? xtry_reallocm(p, 32) : xtry_realloc(p, 17, 32);
        assert(p && p[0] == 42);
        xfree(p, 32);
        p = aligned[i](17, 64);
        assert(p && (usize)p % 64 == 0);
        if (i >= 2)
            for (usize j = 0; j < 17; ++j)
                assert(p[j] == 0);
        p[0] = 42;
        assert(!(i & 1 ? xtry_reallocma(p, (usize)-1, 64)
                       : xtry_realloca(p, 17, (usize)-1, 64)));
        assert(p[0] == 42);
        p = i & 1 ? xtry_reallocma(p, 32, 64) : xtry_realloca(p, 17, 32, 64);
        assert(p && p[0] == 42 && (usize)p % 64 == 0);
        xfreea(p, 32, 64);
    }
    xalloc_debug_unfreed();
}
static unsigned failure_variant;
static void checked_failure(void) {
    void *p = 0;
    switch (failure_variant) {
    case 0:
        p = xalloc((usize)-1);
        break;
    case 1:
        p = xalloca((usize)-1, 64);
        break;
    case 2:
        p = xallocm((usize)-1);
        break;
    case 3:
        p = xallocma((usize)-1, 64);
        break;
    case 4:
        p = xallocz((usize)-1);
        break;
    case 5:
        p = xallocza((usize)-1, 64);
        break;
    case 6:
        p = xalloczm((usize)-1);
        break;
    case 7:
        p = xalloczma((usize)-1, 64);
        break;
    case 8:
        p = xrealloc(xalloc(17), 17, (usize)-1);
        break;
    case 9:
        p = xrealloca(xalloca(17, 64), 17, (usize)-1, 64);
        break;
    case 10:
        p = xreallocm(xalloc(17), (usize)-1);
        break;
    case 11:
        p = xreallocma(xalloca(17, 64), (usize)-1, 64);
        break;
    }
    __asm__ volatile("" : : "g"(p) : "memory");
}
int main(void) {
    check(try_variants, 0, 0, 0);
    for (failure_variant = 0; failure_variant < 12; ++failure_variant)
        check(checked_failure, 1, "Allocation of size", "Backtrace:");
    check(clean, 0, 0, 0);
    check(concurrent, 0, 0, 0);
    check(print_trace, 0, "0x", 0);
    check(wrong_free, 1, "Corrupted length", "alloc_test_allocate");
    check(double_free, 1, "already been freed", "alloc_test_release");
    check(wrong_realloc, 1, "Corrupted length", "alloc_test_allocate");
    check(realloc_dead, 1, "already been freed", "alloc_test_release");
    check(unknown_free, 1, "Unknown allocation", "No allocation history");
    check(unknown_realloc, 1, "Unknown allocation", "No allocation history");
    check(realloc_history, 1, "Last valid operation: Reallocate",
          "alloc_test_resize");
    check(realloc_old_pointer, 1, "old address released", "alloc_test_resize");
    check(wrong_alignment, 1, "Corrupted alignment", 0);
    check(zero_mismatch, 1, "Corrupted length", 0);
    check(leak, 0, "Unfreed allocation", "alloc_test_allocate");
    xprint("allocation sanitizer tests passed\n");
}
