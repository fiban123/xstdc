#pragma once

#include "xstdc/mem.h"
#include "xstdc/types.h"

// xoshiro256++ and xoroshiro128++ 1.0: https://prng.di.unimi.it/
// wyrand (wyhash v4.2): https://github.com/wangyi-fudan/wyhash/blob/master/wyhash.h

// Buffer fills discard unused word bytes for scalar generators. ChaCha20 and
// the 16-lane generator retain them across mixed buffer/word calls.

// generic RNG
typedef struct {
    u64 s[4];
} XRngXoshiro256ppState;

// fat SIMD 16-lane high throughput RNG
typedef struct {
    // Structure of arrays: each row contains one word from all 16 lanes.
    u64 s[4][16];
    u8 buffer[128];
    usize used;
} XRngXoshiro256pp16State;

// generic RNG with smaller state
typedef struct {
    u64 s[2];
} XRngXoroshiro128ppState;

// tiny state and low latency RNG
typedef struct {
    u64 s;
} XRngWyrandState;

// ChaCha20 with a 256-bit key, 64-bit block counter and initially zero nonce.
// Security requires an unpredictable seed. Equal seeds give equal streams.
typedef struct {
    u32 s[16];
    u8 buffer[64];
    usize used;
} XRngChaCha20State;

// Seed extension RNG
typedef struct {
    u64 s;
} XRngSplitMix64State;

// Initialize the generator from seed.
void xrng_xoshiro256pp_seed(XRngXoshiro256ppState *state, u64 seed);
// Initialize the generator from seed.
void xrng_xoshiro256pp16_seed(XRngXoshiro256pp16State *state, u64 seed);
// Initialize the generator from seed.
void xrng_xoroshiro128pp_seed(XRngXoroshiro128ppState *state, u64 seed);
// Initialize the generator from seed.
void xrng_wyrand_seed(XRngWyrandState *state, u64 seed);
// Initialize the generator from a 32-byte seed.
void xrng_chacha20_seed(XRngChaCha20State *state, const void *seed);
// Initialize the generator from seed.
void xrng_splitmix64_seed(XRngSplitMix64State *state, u64 seed);

// Generate a random 64-bit word and advance the generator.
static inline u64 xrng_xoshiro256pp_u64(XRngXoshiro256ppState *state);
// Generate a random 64-bit word and advance the generator.
u64 xrng_xoshiro256pp16_u64(XRngXoshiro256pp16State *state);
// Generate a random 64-bit word and advance the generator.
static inline u64 xrng_xoroshiro128pp_u64(XRngXoroshiro128ppState *state);
// Generate a random 64-bit word and advance the generator.
static inline u64 xrng_wyrand_u64(XRngWyrandState *state);
// Generate a random 64-bit word and advance the generator.
u64 xrng_chacha20_u64(XRngChaCha20State *state);
// Generate a random 64-bit word and advance the generator.
u64 xrng_splitmix64_u64(XRngSplitMix64State *state);
// Generate a random 64-bit word; requires RDRAND support.
u64 xrng_hw_u64();

// Fill n bytes without overlapping state; n=0 leaves the generator unchanged.
static inline void xrng_xoshiro256pp_buf(XRngXoshiro256ppState *state,
                                         void *dst, usize n);
// Fill n bytes without overlapping state; n=0 leaves the generator unchanged.
void xrng_xoshiro256pp16_buf(XRngXoshiro256pp16State *state, void *dst,
                             usize n);
// Fill n bytes without overlapping state; n=0 leaves the generator unchanged.
static inline void xrng_xoroshiro128pp_buf(XRngXoroshiro128ppState *state,
                                           void *dst, usize n);
// Fill n bytes without overlapping state; n=0 leaves the generator unchanged.
void xrng_wyrand_buf(XRngWyrandState *state, void *dst, usize n);
// Fill n bytes without overlapping state; n=0 leaves the generator unchanged.
void xrng_chacha20_buf(XRngChaCha20State *state, void *dst, usize n);
// Fill n bytes without overlapping state; n=0 leaves the generator unchanged.
void xrng_splitmix64_buf(XRngSplitMix64State *state, void *dst, usize n);
// Fill n bytes using RDRAND; requires hardware support. n=0 does nothing.
void xrng_hw_buf(void *dst, usize n);

// Generate an unbiased integer in [min, max); requires min < max.
static inline u64 xrng_xoshiro256pp_u64range(XRngXoshiro256ppState *state,
                                             u64 min, u64 max);
// Generate an unbiased integer in [min, max); requires min < max.
u64 xrng_xoshiro256pp16_u64range(XRngXoshiro256pp16State *state, u64 min,
                                 u64 max);
// Generate an unbiased integer in [min, max); requires min < max.
static inline u64 xrng_xoroshiro128pp_u64range(XRngXoroshiro128ppState *state,
                                               u64 min, u64 max);
// Generate an unbiased integer in [min, max); requires min < max.
static inline u64 xrng_wyrand_u64range(XRngWyrandState *state, u64 min,
                                       u64 max);
// Generate an unbiased integer in [min, max); requires min < max.
u64 xrng_chacha20_u64range(XRngChaCha20State *state, u64 min, u64 max);
// Generate an unbiased integer in [min, max); requires min < max.
u64 xrng_splitmix64_u64range(XRngSplitMix64State *state, u64 min, u64 max);
// Generate an unbiased integer in [min, max); requires min < max and RDRAND.
u64 xrng_hw_u64range(u64 min, u64 max);

// Map random bits to [min, max); requires finite min < max.
static inline f32 xrng_f32range(u64 rand, f32 min, f32 max);

// Map random bits to [min, max); requires finite min < max.
static inline f64 xrng_f64range(u64 rand, f64 min, f64 max);

// Definitions

static inline u64 xrng_xoshiro256pp_u64(XRngXoshiro256ppState *state) {
    u64 *s = state->s;
    u64 sum = s[0] + s[3];
    u64 result = ((sum << 23) | (sum >> 41)) + s[0];
    u64 t = s[1] << 17;
    s[2] ^= s[0];
    s[3] ^= s[1];
    s[1] ^= s[2];
    s[0] ^= s[3];
    s[2] ^= t;
    s[3] = (s[3] << 45) | (s[3] >> 19);
    return result;
}

static inline u64 xrng_xoroshiro128pp_u64(XRngXoroshiro128ppState *state) {
    u64 a = state->s[0], b = state->s[1];
    u64 sum = a + b;
    u64 result = ((sum << 17) | (sum >> 47)) + a;
    b ^= a;
    state->s[0] = ((a << 49) | (a >> 15)) ^ b ^ (b << 21);
    state->s[1] = (b << 28) | (b >> 36);
    return result;
}

static inline u64 xrng_wyrand_u64(XRngWyrandState *state) {
    u64 s = (state->s += 0x2d358dccaa6c78a5ull);
    u128 product = (u128)s * (s ^ 0x8bb84b93962eacc9ull);
    return (u64)product ^ (u64)(product >> 64);
}

static inline void xrng_xoshiro256pp_buf(XRngXoshiro256ppState *state,
                                         void *dst, usize n) {
    if (!n)
        return;
    XRngXoshiro256ppState local = *state;
    u8 *p = dst;
    while (n >= 8) {
        u64 word = xrng_xoshiro256pp_u64(&local);
        xmemcp(p, &word, 8);
        p += 8;
        n -= 8;
    }
    if (n) {
        u64 word = xrng_xoshiro256pp_u64(&local);
        xmemcp(p, &word, n);
    }
    *state = local;
}

static inline void xrng_xoroshiro128pp_buf(XRngXoroshiro128ppState *state,
                                           void *dst, usize n) {
    if (!n)
        return;
    XRngXoroshiro128ppState local = *state;
    u8 *p = dst;
    while (n >= 8) {
        u64 word = xrng_xoroshiro128pp_u64(&local);
        xmemcp(p, &word, 8);
        p += 8;
        n -= 8;
    }
    if (n) {
        u64 word = xrng_xoroshiro128pp_u64(&local);
        xmemcp(p, &word, n);
    }
    *state = local;
}

static inline u64 xrng_xoshiro256pp_u64range(XRngXoshiro256ppState *state,
                                             u64 min, u64 max) {
    u64 width = max - min;
    u128 product = (u128)xrng_xoshiro256pp_u64(state) * width;
    if ((u64)product < width) {
        u64 threshold = -width % width;
        while ((u64)product < threshold)
            product = (u128)xrng_xoshiro256pp_u64(state) * width;
    }
    return min + (u64)(product >> 64);
}

static inline u64 xrng_xoroshiro128pp_u64range(XRngXoroshiro128ppState *state,
                                               u64 min, u64 max) {
    u64 width = max - min;
    u128 product = (u128)xrng_xoroshiro128pp_u64(state) * width;
    if ((u64)product < width) {
        u64 threshold = -width % width;
        while ((u64)product < threshold)
            product = (u128)xrng_xoroshiro128pp_u64(state) * width;
    }
    return min + (u64)(product >> 64);
}

static inline u64 xrng_wyrand_u64range(XRngWyrandState *state, u64 min,
                                       u64 max) {
    u64 width = max - min;
    u128 product = (u128)xrng_wyrand_u64(state) * width;
    if ((u64)product < width) {
        u64 threshold = -width % width;
        while ((u64)product < threshold)
            product = (u128)xrng_wyrand_u64(state) * width;
    }
    return min + (u64)(product >> 64);
}

static inline f32 xrng_f32range(u64 rand, f32 min, f32 max) {
    f32 t = (f32)(rand >> 40) * 0x1p-24f;
    // Weighted sum avoids overflow of max - min across opposite signs.
    f32 result = min <= 0 && max >= 0 ? (1.0f - t) * min + t * max
                                      : min + t * (max - min);
    if (result >= max) {
        // Predecessor of max, including max == +/-0, without a libm call.
        u32 bits;
        xmemcp(&bits, &max, 4);
        bits = max == 0 ? 0x80000001u : bits + (max > 0 ? -1u : 1u);
        xmemcp(&result, &bits, 4);
    }
    return result < min ? min : result;
}

static inline f64 xrng_f64range(u64 rand, f64 min, f64 max) {
    f64 t = (f64)(rand >> 11) * 0x1p-53;
    f64 result = min <= 0 && max >= 0 ? (1.0 - t) * min + t * max
                                      : min + t * (max - min);
    if (result >= max) {
        u64 bits;
        xmemcp(&bits, &max, 8);
        bits =
            max == 0 ? 0x8000000000000001ull : bits + (max > 0 ? -1ull : 1ull);
        xmemcp(&result, &bits, 8);
    }
    return result < min ? min : result;
}
