#include "xstdc/assert.h"
#include "xstdc/rng.h"
#include <stdint.h>

bool is_rng_deterministic(void *state1, void *state2, u64 (*step_fn)(void *)) {
    bool r = true;
    for (usize i = 0; i < 1000; i++) {
        u64 a = step_fn(state1);
        u64 b = step_fn(state2);
        r &= a == b;
    }

    return r;
}

bool is_rng_uniform(void *state, u64 (*step_fn)(void *)) {
    i128 mean = UINT64_MAX / 2;
    i128 dev = 0;
    for (usize i = 0; i < 1000; i++) {
        i128 val = step_fn(state);
        dev -= val - mean;
    }

    dev /= 1000;

    if (dev < 0)
        dev = -dev;

    return dev < 1e18;
}

bool is_rng_uniform_range(void *state, u64 (*range_fn)(void *, u64, u64)) {
    u128 min = 1ull << 62ull;
    u128 max = 1ull << 63ull;
    i128 mean = (min + max) / 2;
    i128 dev = 0;
    for (usize i = 0; i < 1000; i++) {
        i128 val = range_fn(state, min, max);
        dev -= val - mean;
    }

    dev /= 1000;

    if (dev < 0)
        dev = -dev;

    return dev < 3ull * 1e17;
}

bool is_f64_uniform(f64 (*range_fn)(u64 s, f64 min, f64 max)) {
    XRngXoshiro256ppState state;
    xrng_xoshiro256pp_seed(&state, 1001);

    f64 mean = 1.0;
    f64 dev = 0.;

    for (usize i = 0; i < 1000; i++) {
        u64 r = xrng_xoshiro256pp_u64(&state);
        dev += range_fn(r, 0.0, 2.0) - mean;
    }

    dev /= 1000;

    if (dev < 0)
        dev = -dev;

    return dev <= 0.12780;
}

f64 f32range_wrapper(u64 s, f64 min, f64 max) {
    return xrng_f32range(s, min, max);
}

u64 hwrng_wrapper_u64(void *_) { return xrng_hw_u64(); }

u64 hwrng_wrapper_u64range(void *_, u64 min, u64 max) {
    return xrng_hw_u64range(min, max);
}

int main() {
    XRngXoshiro256ppState xoshiro256pp_1;
    XRngXoshiro256ppState xoshiro256pp_2;

    xrng_xoshiro256pp_seed(&xoshiro256pp_1, 1000);
    xrng_xoshiro256pp_seed(&xoshiro256pp_2, 1000);

    assert(is_rng_deterministic(&xoshiro256pp_1, &xoshiro256pp_2,
                                (void *)xrng_xoshiro256pp_u64));
    assert(is_rng_uniform(&xoshiro256pp_1, (void *)xrng_xoshiro256pp_u64));
    assert(is_rng_uniform_range(&xoshiro256pp_1,
                                (void *)xrng_xoshiro256pp_u64range));

    XRngXoshiro256pp16State xoshiro256pp16_1;
    XRngXoshiro256pp16State xoshiro256pp16_2;

    xrng_xoshiro256pp16_seed(&xoshiro256pp16_1, 1000);
    xrng_xoshiro256pp16_seed(&xoshiro256pp16_2, 1000);

    assert(is_rng_deterministic(&xoshiro256pp16_1, &xoshiro256pp16_2,
                                (void *)xrng_xoshiro256pp16_u64));
    assert(is_rng_uniform(&xoshiro256pp16_1, (void *)xrng_xoshiro256pp16_u64));
    assert(is_rng_uniform_range(&xoshiro256pp16_1,
                                (void *)xrng_xoshiro256pp16_u64range));

    XRngXoroshiro128ppState xoroshiro128pp_1;
    XRngXoroshiro128ppState xoroshiro128pp_2;

    xrng_xoroshiro128pp_seed(&xoroshiro128pp_1, 1000);
    xrng_xoroshiro128pp_seed(&xoroshiro128pp_2, 1000);

    assert(is_rng_deterministic(&xoroshiro128pp_1, &xoroshiro128pp_2,
                                (void *)xrng_xoroshiro128pp_u64));
    assert(is_rng_uniform(&xoroshiro128pp_1, (void *)xrng_xoroshiro128pp_u64));
    assert(is_rng_uniform_range(&xoroshiro128pp_1,
                                (void *)xrng_xoroshiro128pp_u64range));

    XRngWyrandState wyrand_1;
    XRngWyrandState wyrand_2;

    xrng_wyrand_seed(&wyrand_1, 1000);
    xrng_wyrand_seed(&wyrand_2, 1000);

    assert(is_rng_deterministic(&wyrand_1, &wyrand_2, (void *)xrng_wyrand_u64));
    assert(is_rng_uniform(&wyrand_1, (void *)xrng_wyrand_u64));
    assert(is_rng_uniform_range(&wyrand_1, (void *)xrng_wyrand_u64range));

    XRngChaCha20State chacha20_1;
    XRngChaCha20State chacha20_2;
    const u8 chacha20_seed[32] = {1, 2, 3, 4};

    xrng_chacha20_seed(&chacha20_1, chacha20_seed);
    xrng_chacha20_seed(&chacha20_2, chacha20_seed);

    assert(is_rng_deterministic(&chacha20_1, &chacha20_2,
                                (void *)xrng_chacha20_u64));
    assert(is_rng_uniform(&chacha20_1, (void *)xrng_chacha20_u64));
    assert(is_rng_uniform_range(&chacha20_1, (void *)xrng_chacha20_u64range));

    XRngSplitMix64State splitmix64_1;
    XRngSplitMix64State splitmix64_2;

    xrng_splitmix64_seed(&splitmix64_1, 1000);
    xrng_splitmix64_seed(&splitmix64_2, 1000);

    assert(is_rng_deterministic(&splitmix64_1, &splitmix64_2,
                                (void *)xrng_splitmix64_u64));
    assert(is_rng_uniform(&splitmix64_1, (void *)xrng_splitmix64_u64));
    assert(
        is_rng_uniform_range(&splitmix64_1, (void *)xrng_splitmix64_u64range));

    assert(!is_rng_deterministic(0, 0, hwrng_wrapper_u64));
    assert(is_rng_uniform(0, hwrng_wrapper_u64));
    assert(is_rng_uniform_range(0, hwrng_wrapper_u64range));

    assert(is_f64_uniform(xrng_f64range));
    assert(is_f64_uniform(f32range_wrapper));
}
