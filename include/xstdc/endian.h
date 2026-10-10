#pragma once

#include "xstdc/types.h"

// Reverse the byte order of a 128-bit integer.
static inline u128 xbswap128(u128 x);
// Reverse the byte order of a 64-bit integer.
static inline u64 xbswap64(u64 x);
// Reverse the byte order of a 32-bit integer.
static inline u32 xbswap32(u32 x);
// Reverse the byte order of a 16-bit integer.
static inline u16 xbswap16(u16 x);

// host to little endian
static inline u128 xh128_tole(u128 h);
static inline u64 xh64_tole(u64 h);
static inline u32 xh32_tole(u32 h);
static inline u16 xh16_tole(u16 h);

// host to big endian
static inline u128 xh128_tobe(u128 h);
static inline u64 xh64_tobe(u64 h);
static inline u32 xh32_tobe(u32 h);
static inline u16 xh16_tobe(u16 h);

// little endian to host
static inline u128 xle128_toh(u128 h);
static inline u64 xle64_toh(u64 h);
static inline u32 xle32_toh(u32 h);
static inline u16 xle16_toh(u16 h);

// big endian to host
static inline u128 xbe128_toh(u128 h);
static inline u64 xbe64_toh(u64 h);
static inline u32 xbe32_toh(u32 h);
static inline u16 xbe16_toh(u16 h);

// Definitions

static inline u128 xbswap128(u128 x) {
    return ((u128)xbswap64((u64)x) << 64) | xbswap64((u64)(x >> 64));
}

static inline u64 xbswap64(u64 x) { return __builtin_bswap64(x); }
static inline u32 xbswap32(u32 x) { return __builtin_bswap32(x); }
static inline u16 xbswap16(u16 x) { return __builtin_bswap16(x); }

// Also accept GCC byte-order macros when endian flags are unavailable.
#if defined(__LITTLE_ENDIAN__) ||                                              \
    (defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) &&            \
     __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)

static inline u128 xh128_tole(u128 h) { return h; }
static inline u64 xh64_tole(u64 h) { return h; }
static inline u32 xh32_tole(u32 h) { return h; }
static inline u16 xh16_tole(u16 h) { return h; }

static inline u128 xh128_tobe(u128 h) { return xbswap128(h); }
static inline u64 xh64_tobe(u64 h) { return xbswap64(h); }
static inline u32 xh32_tobe(u32 h) { return xbswap32(h); }
static inline u16 xh16_tobe(u16 h) { return xbswap16(h); }

#elif defined(__BIG_ENDIAN__) ||                                               \
    (defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) &&               \
     __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)

static inline u128 xh128_tole(u128 h) { return xbswap128(h); }
static inline u64 xh64_tole(u64 h) { return xbswap64(h); }
static inline u32 xh32_tole(u32 h) { return xbswap32(h); }
static inline u16 xh16_tole(u16 h) { return xbswap16(h); }

static inline u128 xh128_tobe(u128 h) { return h; }
static inline u64 xh64_tobe(u64 h) { return h; }
static inline u32 xh32_tobe(u32 h) { return h; }
static inline u16 xh16_tobe(u16 h) { return h; }

#else
#error "Unsupported host byte order"
#endif

static inline u128 xle128_toh(u128 h) { return xh128_tole(h); }
static inline u64 xle64_toh(u64 h) { return xh64_tole(h); }
static inline u32 xle32_toh(u32 h) { return xh32_tole(h); }
static inline u16 xle16_toh(u16 h) { return xh16_tole(h); }

static inline u128 xbe128_toh(u128 h) { return xh128_tobe(h); }
static inline u64 xbe64_toh(u64 h) { return xh64_tobe(h); }
static inline u32 xbe32_toh(u32 h) { return xh32_tobe(h); }
static inline u16 xbe16_toh(u16 h) { return xh16_tobe(h); }
