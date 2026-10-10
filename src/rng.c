#include "xstdc/rng.h"

#include <assert.h>
#include <string.h>

#define INLINE static inline __attribute__((always_inline))
#define ROTL(x, k, bits) (((x) << (k)) | ((x) >> ((bits) - (k))))

// Vigna's public-domain SplitMix64: https://prng.di.unimi.it/
INLINE u64 splitmix64_next(XRngSplitMix64State *state) {
    u64 z = (state->s += 0x9e3779b97f4a7c15ull);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return z ^ (z >> 31);
}

void xrng_splitmix64_seed(XRngSplitMix64State *state, u64 seed) {
    state->s = seed;
}

void xrng_wyrand_seed(XRngWyrandState *state, u64 seed) { state->s = seed; }

void xrng_xoshiro256pp_seed(XRngXoshiro256ppState *state, u64 seed) {
    XRngSplitMix64State sm = {seed};
    for (unsigned i = 0; i < 4; ++i)
        state->s[i] = splitmix64_next(&sm);
}

void xrng_xoroshiro128pp_seed(XRngXoroshiro128ppState *state, u64 seed) {
    XRngSplitMix64State sm = {seed};
    for (unsigned i = 0; i < 2; ++i)
        state->s[i] = splitmix64_next(&sm);
}

void xrng_xoshiro256pp16_seed(XRngXoshiro256pp16State *state, u64 seed) {
    XRngXoshiro256ppState lane;
    xrng_xoshiro256pp_seed(&lane, seed);
    // Separate lanes by 2^128 steps using the authors' jump polynomial.
    static const u64 jump[4] = {0x180ec6d33cfd0abaull, 0xd5a61266f0c9392cull,
                                0xa9582618e03fc9aaull, 0x39abdc4529b1661cull};
    for (unsigned l = 0; l < 16; ++l) {
        for (unsigned i = 0; i < 4; ++i)
            state->s[i][l] = lane.s[i];
        if (l == 15)
            break;
        XRngXoshiro256ppState next = {{0}};
        for (unsigned i = 0; i < 4; ++i) {
            for (unsigned bit = 0; bit < 64; ++bit) {
                if (jump[i] & (1ull << bit))
                    for (unsigned j = 0; j < 4; ++j)
                        next.s[j] ^= lane.s[j];
                xrng_xoshiro256pp_u64(&lane);
            }
        }
        lane = next;
    }
    state->used = sizeof state->buffer;
}

// Load/store state once per fill; AVX-512 keeps all 16 lanes in registers.
// Fixed-size memcpy permits unaligned destinations and avoids vector
// aliasing/alignment UB. Runtime dispatch works even with the baseline build's
// lack of -march flags.
#define DEFINE_XOSHIRO_BLOCKS(name, bytes, target_isa)                         \
    __attribute__((target(target_isa))) static void name(                      \
        u64 s[restrict 4][16], u8 *restrict dst, usize blocks) {               \
        typedef u64 vec __attribute__((vector_size(bytes)));                   \
        enum { lanes = bytes / 8, groups = 16 / lanes };                       \
        vec a[groups], b[groups], c[groups], d[groups];                        \
        memcpy(a, s[0], 128);                                                  \
        memcpy(b, s[1], 128);                                                  \
        memcpy(c, s[2], 128);                                                  \
        memcpy(d, s[3], 128);                                                  \
        while (blocks--) {                                                     \
            _Pragma("GCC unroll 8") for (unsigned g = 0; g < groups; ++g) {    \
                vec sum = a[g] + d[g];                                         \
                vec result = ROTL(sum, 23, 64) + a[g];                         \
                vec t = b[g] << 17;                                            \
                c[g] ^= a[g];                                                  \
                d[g] ^= b[g];                                                  \
                b[g] ^= c[g];                                                  \
                a[g] ^= d[g];                                                  \
                c[g] ^= t;                                                     \
                d[g] = ROTL(d[g], 45, 64);                                     \
                memcpy(dst + g * bytes, &result, bytes);                       \
            }                                                                  \
            dst += 128;                                                        \
        }                                                                      \
        memcpy(s[0], a, 128);                                                  \
        memcpy(s[1], b, 128);                                                  \
        memcpy(s[2], c, 128);                                                  \
        memcpy(s[3], d, 128);                                                  \
    }

DEFINE_XOSHIRO_BLOCKS(xoshiro16_sse2, 16, "sse2")
DEFINE_XOSHIRO_BLOCKS(xoshiro16_avx2, 32, "avx2")
DEFINE_XOSHIRO_BLOCKS(xoshiro16_avx512, 64, "avx512f")
#undef DEFINE_XOSHIRO_BLOCKS

static void xoshiro16_blocks(XRngXoshiro256pp16State *state, u8 *dst,
                             usize blocks) {
    if (__builtin_cpu_supports("avx512f"))
        xoshiro16_avx512(state->s, dst, blocks);
    else if (__builtin_cpu_supports("avx2"))
        xoshiro16_avx2(state->s, dst, blocks);
    else
        xoshiro16_sse2(state->s, dst, blocks);
}

void xrng_xoshiro256pp16_buf(XRngXoshiro256pp16State *state, void *dst,
                             usize n) {
    u8 *p = dst;
    if (!n)
        return;
    if (state->used < 128) {
        usize take = 128 - state->used;
        if (take > n)
            take = n;
        memcpy(p, state->buffer + state->used, take);
        state->used += take;
        p += take;
        n -= take;
    }
    if (n >= 128) {
        usize bytes = n & ~(usize)127;
        xoshiro16_blocks(state, p, n / 128);
        p += bytes;
        n -= bytes;
    }
    if (n) {
        xoshiro16_blocks(state, state->buffer, 1);
        memcpy(p, state->buffer, n);
        state->used = n;
    }
}

INLINE u64 xoshiro256pp16_next(XRngXoshiro256pp16State *state) {
    u64 result;
    if (state->used <= 120) {
        memcpy(&result, state->buffer + state->used, 8);
        state->used += 8;
    } else {
        xrng_xoshiro256pp16_buf(state, &result, 8);
    }
    return result;
}

// Original ChaCha20 layout: 64-bit counter, 64-bit nonce. The quarter-round
// and 20-round permutation are the same as RFC 8439 section 2.3.
// https://www.rfc-editor.org/rfc/rfc8439#section-2.3
void xrng_chacha20_seed(XRngChaCha20State *state, const void *seed) {
    // Read first so the seed may refer to the old state.
    u32 key[8];
    memcpy(key, seed, sizeof key);
    state->s[0] = 0x61707865;
    state->s[1] = 0x3320646e;
    state->s[2] = 0x79622d32;
    state->s[3] = 0x6b206574;
    memcpy(state->s + 4, key, sizeof key);
    memset(state->s + 12, 0, 4 * sizeof(u32));
    state->used = sizeof state->buffer;
}

#define QR(a, b, c, d)                                                         \
    do {                                                                       \
        a += b;                                                                \
        d ^= a;                                                                \
        d = ROTL(d, 16, 32);                                                   \
        c += d;                                                                \
        b ^= c;                                                                \
        b = ROTL(b, 12, 32);                                                   \
        a += b;                                                                \
        d ^= a;                                                                \
        d = ROTL(d, 8, 32);                                                    \
        c += d;                                                                \
        b ^= c;                                                                \
        b = ROTL(b, 7, 32);                                                    \
    } while (0)

static void chacha20_block(XRngChaCha20State *state, u8 *dst) {
    // Four quarter-rounds in parallel on baseline SSE2. Diagonal rounds
    // rotate lanes so the same vector quarter-round handles both phases.
    typedef u32 vec __attribute__((vector_size(16)));
    vec a, b, c, d;
    memcpy(&a, state->s, 16);
    memcpy(&b, state->s + 4, 16);
    memcpy(&c, state->s + 8, 16);
    memcpy(&d, state->s + 12, 16);
    vec aa = a, bb = b, cc = c, dd = d;
    for (unsigned i = 0; i < 10; ++i) {
        QR(a, b, c, d);
        b = __builtin_shufflevector(b, b, 1, 2, 3, 0);
        c = __builtin_shufflevector(c, c, 2, 3, 0, 1);
        d = __builtin_shufflevector(d, d, 3, 0, 1, 2);
        QR(a, b, c, d);
        b = __builtin_shufflevector(b, b, 3, 0, 1, 2);
        c = __builtin_shufflevector(c, c, 2, 3, 0, 1);
        d = __builtin_shufflevector(d, d, 1, 2, 3, 0);
    }
    a += aa;
    b += bb;
    c += cc;
    d += dd;
    memcpy(dst, &a, 16);
    memcpy(dst + 16, &b, 16);
    memcpy(dst + 32, &c, 16);
    memcpy(dst + 48, &d, 16);
    if (++state->s[12] == 0 && ++state->s[13] == 0) {
        // After 2^64 blocks, move to the next nonce rather than repeat output.
        if (++state->s[14] == 0 && ++state->s[15] == 0)
            __builtin_trap(); // Exhausted every counter/nonce pair for this
                              // key.
    }
}
#undef QR

void xrng_chacha20_buf(XRngChaCha20State *state, void *dst, usize n) {
    u8 *p = dst;
    if (!n)
        return;
    if (state->used < 64) {
        usize take = 64 - state->used;
        if (take > n)
            take = n;
        memcpy(p, state->buffer + state->used, take);
        state->used += take;
        p += take;
        n -= take;
    }
    while (n >= 64) {
        chacha20_block(state, p);
        p += 64;
        n -= 64;
    }
    if (n) {
        chacha20_block(state, state->buffer);
        memcpy(p, state->buffer, n);
        state->used = n;
    }
}

INLINE u64 chacha20_next(XRngChaCha20State *state) {
    u64 result;
    if (state->used <= 56) {
        memcpy(&result, state->buffer + state->used, 8);
        state->used += 8;
    } else {
        xrng_chacha20_buf(state, &result, 8);
    }
    return result;
}

INLINE u64 hw_next(void) {
    u64 result;
    __asm__ volatile("1: rdrand %0\n\tjnc 1b" : "=r"(result) : : "cc");
    return result;
}

u64 xrng_hw_u64() { return hw_next(); }

// Keep the remaining primitives inlined into their exported entry points.
#define DEFINE_U64(name, type)                                                 \
    u64 xrng_##name##_u64(type *state) { return name##_next(state); }
DEFINE_U64(xoshiro256pp16, XRngXoshiro256pp16State)
DEFINE_U64(chacha20, XRngChaCha20State)
DEFINE_U64(splitmix64, XRngSplitMix64State)
#undef DEFINE_U64

#define FILL_WORDS(next)                                                       \
    do {                                                                       \
        u8 *p = dst;                                                           \
        while (n >= 8) {                                                       \
            u64 word = (next);                                                 \
            memcpy(p, &word, 8);                                               \
            p += 8;                                                            \
            n -= 8;                                                            \
        }                                                                      \
        if (n) {                                                               \
            u64 word = (next);                                                 \
            memcpy(p, &word, n);                                               \
        }                                                                      \
    } while (0)

#define DEFINE_BUF(name, type, next)                                           \
    void xrng_##name##_buf(type *state, void *dst, usize n) {                  \
        if (!n)                                                                \
            return;                                                            \
        type local = *state;                                                   \
        FILL_WORDS(next(&local));                                              \
        *state = local;                                                        \
    }
DEFINE_BUF(wyrand, XRngWyrandState, xrng_wyrand_u64)
DEFINE_BUF(splitmix64, XRngSplitMix64State, splitmix64_next)
#undef DEFINE_BUF

void xrng_hw_buf(void *dst, usize n) { FILL_WORDS(hw_next()); }
#undef FILL_WORDS

// Lemire multiply-high rejection sampling. Division is only needed when the
// low product falls below the range width, not on the usual fast path.
#define RANGE(next)                                                            \
    do {                                                                       \
        assert(min < max);                                                     \
        u64 width = max - min;                                                 \
        u128 product = (u128)(next) * width;                                   \
        if ((u64)product < width) {                                            \
            u64 threshold = -width % width;                                    \
            while ((u64)product < threshold)                                   \
                product = (u128)(next) * width;                                \
        }                                                                      \
        return min + (u64)(product >> 64);                                     \
    } while (0)

#define DEFINE_RANGE(name, type)                                               \
    u64 xrng_##name##_u64range(type *state, u64 min, u64 max) {                \
        RANGE(name##_next(state));                                             \
    }
DEFINE_RANGE(xoshiro256pp16, XRngXoshiro256pp16State)
DEFINE_RANGE(chacha20, XRngChaCha20State)
DEFINE_RANGE(splitmix64, XRngSplitMix64State)
#undef DEFINE_RANGE

u64 xrng_hw_u64range(u64 min, u64 max) { RANGE(hw_next()); }
#undef RANGE
