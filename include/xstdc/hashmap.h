#pragma once

#include "xstdc/alloc.h"
#include "xstdc/assert.h"
#include "xstdc/mem.h"
#include "xstdc/types.h"
#if defined(__x86_64__)
#include <emmintrin.h>
#endif

#ifndef XHASHMAP_EXTERN_INLINE
#define XHASHMAP_EXTERN_INLINE inline
#endif

// Regular containers have fixed key/value sizes established by init. Their
// get/insert/contains calls omit key length. XDyn* containers allow variable
// key lengths, passed to each operation; values remain fixed-size in every map.
// All containers own copies, return true on duplicate insertion, and support
// repeated free/reuse. Values are aligned to 8 bytes (not over-aligned types).
// Node value pointers survive growth; array value pointers do not.

typedef u64 (*HashFn)(const void *key, usize klen);

// Preserve overlapping-update semantics while keeping common value sizes as
// loads/stores even when val_sz is only known at runtime.
static inline __attribute__((always_inline)) void
xhash_copy_value(void *dst, void *src, usize size);

// Separate chaining. Nodes own copies of the key and value. Use the same,
// deterministic hash function for every operation on a live container.
// Keys are byte strings (NULL is permitted only for a zero-length key).
// Values are aligned to 8 bytes.
// init requires an uninitialized/freed container. Do not copy live containers.
// Allocation failures are handled by the allocator; sizes must fit usize.
typedef struct XDynNodeHashEntry {
    struct XDynNodeHashEntry *next;
    usize klen;
    u8 data[] __attribute__((aligned(8)));
} XDynNodeHashEntry;

// Nodes are bump-allocated in blocks; growing a map never moves a node.
typedef struct XHashNodeBlock {
    struct XHashNodeBlock *next;
    usize capacity, used;
    u8 data[] __attribute__((aligned(8)));
} XHashNodeBlock;

typedef struct {
    usize val_sz;
    usize count;
    usize bucket_count;
    XDynNodeHashEntry **buckets;
    XHashNodeBlock *blocks;
} XDynNodeHashMap;

typedef struct {
    XDynNodeHashMap map;
} XDynNodeHashSet;

// Initialize an empty hashmap with the given element sizes.
XHASHMAP_EXTERN_INLINE void xdnhm_init(XDynNodeHashMap *nhm, usize val_sz);

// Allocate an aligned node from the block list, adding a block if needed.
static inline void *xhash_node_alloc(XHashNodeBlock **blocks,
                                     const usize size_in);

// tell the hashmap to expect N total elements
XHASHMAP_EXTERN_INLINE void xdnhm_reserve(XDynNodeHashMap *nhm, HashFn hash_fn,
                                          usize n);

// returns the element with the given key. if it does not exist, returns
// nullptr.
XHASHMAP_EXTERN_INLINE void *xdnhm_get(XDynNodeHashMap *nhm, HashFn hash_fn,
                                       const void *key, usize klen);

// inserts the element. if it already exists, update its value. returns true if
// the element already existed
XHASHMAP_EXTERN_INLINE bool xdnhm_insert(XDynNodeHashMap *nhm, HashFn hash_fn,
                                         const void *key, usize klen,
                                         void *val);

// Free the hashmap; retain element sizes for reuse or repeated free.
XHASHMAP_EXTERN_INLINE void xdnhm_free(XDynNodeHashMap *nhm);

// Initialize an empty hashset with variable-length keys.
XHASHMAP_EXTERN_INLINE void xdnhs_init(XDynNodeHashSet *nhs);
// tell the hashset to expect N total elements
XHASHMAP_EXTERN_INLINE void xdnhs_reserve(XDynNodeHashSet *nhs, HashFn hash_fn,
                                          usize n);
// inserts the element. Returns true if it already existed.
XHASHMAP_EXTERN_INLINE bool xdnhs_insert(XDynNodeHashSet *nhs, HashFn hash_fn,
                                         const void *key, usize klen);
// returns whether the hash map contains a certain element.
XHASHMAP_EXTERN_INLINE bool xdnhs_contains(XDynNodeHashSet *nhs, HashFn hash_fn,
                                           const void *key, usize klen);
// Free the hashset; retain element sizes for reuse or repeated free.
XHASHMAP_EXTERN_INLINE void xdnhs_free(XDynNodeHashSet *nhs);

// Group probing. Values live in an aligned array; copied keys live in a
// contiguous byte buffer for keys longer than 8 bytes; short keys are inline.
// Keep the same hash function for all operations.
// Growing the table invalidates value pointers. Updates do not move values.
// NULL keys are allowed for length zero; zero-sized values return a marker.
// init requires an uninitialized/freed container. Do not copy live containers.
typedef struct {
    u64 hash, key;
    usize length; // key length + 1; valid only for occupied control bytes
} XDynArrayHashEntry;

typedef struct {
    usize val_sz, stride, count, bucket_count;
    XDynArrayHashEntry *buckets;
    u8 *values, *keys, *control;
    usize keys_len, keys_capacity;
} XDynArrayHashMap;

typedef struct {
    XDynArrayHashMap map;
} XDynArrayHashSet;

// Initialize an empty hashmap with the given element sizes.
XHASHMAP_EXTERN_INLINE void xdahm_init(XDynArrayHashMap *ahm, usize val_sz);

// Pack a key of at most eight bytes into a zero-padded word.
static inline __attribute__((always_inline)) u64
xhash_short_key(const void *key, usize klen);

// Probe 16 control bytes at once. The extra 16 bytes mirror the first group, so
// unaligned loads at the end of the table can wrap without special cases.
static inline __attribute__((always_inline)) unsigned
xhash_control_matches(const u8 *control, u8 tag);
// Derive a nonzero control tag from a hash.
static inline u8 xhash_control_tag(u64 hash);
// Mark a bucket occupied and store its hash tag.
static inline void xdahm_mark(XDynArrayHashMap *ahm, usize slot, u64 hash);
// Find an empty bucket along the hash probe sequence.
static inline __attribute__((always_inline)) usize
xdahm_empty(XDynArrayHashMap *ahm, u64 hash);
// Find a matching bucket, or an empty bucket when inserting.
static inline usize xdahm_slot(XDynArrayHashMap *ahm, u64 hash, const void *key,
                               usize klen, u64 small, bool inserting);

// tell the hashmap to expect N total elements
XHASHMAP_EXTERN_INLINE void xdahm_reserve(XDynArrayHashMap *ahm, HashFn hash_fn,
                                          usize n);

// returns the element with the given key. if it does not exist, returns
// nullptr.
XHASHMAP_EXTERN_INLINE void *xdahm_get(XDynArrayHashMap *ahm, HashFn hash_fn,
                                       const void *key, usize klen);

// Keep allocation and key-arena growth outside the short-key insertion path.
static inline __attribute__((noinline)) XDynArrayHashMap
xdahm_insert_slow(XDynArrayHashMap map, HashFn hash_fn, const void *key,
                  usize klen, void *val, u64 hash, u64 small, usize slot);

// inserts the element. if it already exists, update its value. returns true if
// the element already existed
XHASHMAP_EXTERN_INLINE bool xdahm_insert(XDynArrayHashMap *ahm, HashFn hash_fn,
                                         const void *key, usize klen,
                                         void *val);

// Free the hashmap; retain element sizes for reuse or repeated free.
XHASHMAP_EXTERN_INLINE void xdahm_free(XDynArrayHashMap *ahm);

// Initialize an empty hashset with variable-length keys.
XHASHMAP_EXTERN_INLINE void xdahs_init(XDynArrayHashSet *ahs);
// tell the hashset to expect N total elements
XHASHMAP_EXTERN_INLINE void xdahs_reserve(XDynArrayHashSet *ahs, HashFn hash_fn,
                                          usize n);
// inserts the element. Returns true if it already existed.
XHASHMAP_EXTERN_INLINE bool xdahs_insert(XDynArrayHashSet *ahs, HashFn hash_fn,
                                         const void *key, usize klen);
// returns whether the hash map contains a certain element.
XHASHMAP_EXTERN_INLINE bool xdahs_contains(XDynArrayHashSet *ahs,
                                           HashFn hash_fn, const void *key,
                                           usize klen);
// Free the hashset; retain element sizes for reuse or repeated free.
XHASHMAP_EXTERN_INLINE void xdahs_free(XDynArrayHashSet *ahs);

// Fixed-size keys and values; node addresses survive growth.
typedef struct XNodeHashEntry {
    struct XNodeHashEntry *next;
    u8 data[] __attribute__((aligned(8)));
} XNodeHashEntry;

typedef struct {
    usize key_sz, val_sz;
    usize count;
    usize bucket_count;
    XNodeHashEntry **buckets;
    XHashNodeBlock *blocks;
} XNodeHashMap;

typedef struct {
    XNodeHashMap map;
} XNodeHashSet;

// Initialize an empty hashmap with the given element sizes.
XHASHMAP_EXTERN_INLINE void xnhm_init(XNodeHashMap *nhm, const usize key_sz,
                                      const usize val_sz);

// tell the hashmap to expect N total elements
XHASHMAP_EXTERN_INLINE void xnhm_reserve(XNodeHashMap *nhm,
                                         HashFn const hash_fn, usize n);

// returns the element with the given key. if it does not exist, returns
// nullptr.
XHASHMAP_EXTERN_INLINE void *xnhm_get(XNodeHashMap *nhm, HashFn const hash_fn,
                                      const void *key);

// inserts the element. if it already exists, update its value. returns true if
// the element already existed
XHASHMAP_EXTERN_INLINE bool xnhm_insert(XNodeHashMap *nhm, HashFn const hash_fn,
                                        const void *key, void *val);

// Free the hashmap; retain element sizes for reuse or repeated free.
XHASHMAP_EXTERN_INLINE void xnhm_free(XNodeHashMap *nhm);

// Initialize an empty hashset with the given element sizes.
XHASHMAP_EXTERN_INLINE void xnhs_init(XNodeHashSet *nhs, const usize key_sz);
// tell the hashset to expect N total elements
XHASHMAP_EXTERN_INLINE void xnhs_reserve(XNodeHashSet *nhs,
                                         HashFn const hash_fn, usize n);
// inserts the element. Returns true if it already existed.
XHASHMAP_EXTERN_INLINE bool xnhs_insert(XNodeHashSet *nhs, HashFn const hash_fn,
                                        const void *key);
// returns whether the hash map contains a certain element.
XHASHMAP_EXTERN_INLINE bool
xnhs_contains(XNodeHashSet *nhs, HashFn const hash_fn, const void *key);
// Free the hashset; retain element sizes for reuse or repeated free.
XHASHMAP_EXTERN_INLINE void xnhs_free(XNodeHashSet *nhs);

// Fixed-size keys and values, stored directly in parallel arrays. Values are
// aligned to 8 bytes. Growth invalidates value pointers; updates do not.
// Use the same deterministic hash function throughout the container's lifetime.
// A zero-sized key is allowed (NULL); a zero-sized value returns a marker.
typedef struct {
    usize key_sz, val_sz, stride, count, bucket_count;
    u8 *keys, *values, *control;
} XArrayHashMap;

typedef struct {
    XArrayHashMap map;
} XArrayHashSet;

// Initialize an empty hashmap with the given element sizes.
XHASHMAP_EXTERN_INLINE void xahm_init(XArrayHashMap *ahm, const usize key_sz,
                                      const usize val_sz);

// Compare key bytes, specializing common fixed widths.
static inline __attribute__((always_inline)) bool
xhash_key_equal(const void *a, const void *b, const usize size);
// Return the key storage at slot.
static inline void *xahm_key(const XArrayHashMap *ahm, const usize slot);
// Mark a bucket occupied and store its hash tag.
static inline void xahm_mark(XArrayHashMap *ahm, const usize slot,
                             const u64 hash);
// Find an empty bucket along the hash probe sequence.
static inline __attribute__((always_inline)) usize
xahm_empty(const XArrayHashMap *ahm, const u64 hash);
// Find a matching bucket, or an empty bucket when inserting.
static inline usize xahm_slot(const XArrayHashMap *ahm, const u64 hash,
                              const void *key, const bool inserting,
                              const usize key_sz);
// Release the map arrays without changing its metadata.
static inline void xahm_release_arrays(const XArrayHashMap *ahm);
// Build larger arrays from a map snapshot, rehashing existing entries.
static inline XArrayHashMap xahm_grow(const XArrayHashMap old,
                                      HashFn const hash_fn, const usize n);

// Rebuild a map snapshot with room for n elements.
static inline XArrayHashMap xahm_rebuild(XArrayHashMap map, HashFn hash_fn,
                                         const usize n);
// Copy replacement storage and count fields into the map.
static inline void xahm_publish(XArrayHashMap *ahm,
                                const XArrayHashMap *grown);
// Grow a map snapshot and insert the new key and value.
static inline __attribute__((noinline)) XArrayHashMap
xahm_insert_growing(XArrayHashMap map, HashFn hash_fn, const void *key,
                    void *val, const u64 hash);

// tell the hashmap to expect N total elements
XHASHMAP_EXTERN_INLINE void xahm_reserve(XArrayHashMap *ahm,
                                         HashFn const hash_fn, const usize n);

// returns the element with the given key. if it does not exist, returns
// nullptr.
XHASHMAP_EXTERN_INLINE void *xahm_get(XArrayHashMap *ahm, HashFn const hash_fn,
                                      const void *key);

// inserts the element. if it already exists, update its value. returns true if
// the element already existed
XHASHMAP_EXTERN_INLINE bool xahm_insert(XArrayHashMap *ahm,
                                        HashFn const hash_fn, const void *key,
                                        void *val);

// Free the hashmap; retain element sizes for reuse or repeated free.
XHASHMAP_EXTERN_INLINE void xahm_free(XArrayHashMap *ahm);
// Initialize an empty hashset with the given element sizes.
XHASHMAP_EXTERN_INLINE void xahs_init(XArrayHashSet *ahs, const usize key_sz);
// tell the hashset to expect N total elements
XHASHMAP_EXTERN_INLINE void xahs_reserve(XArrayHashSet *ahs,
                                         HashFn const hash_fn, const usize n);
// inserts the element. Returns true if it already existed.
XHASHMAP_EXTERN_INLINE bool xahs_insert(XArrayHashSet *ahs,
                                        HashFn const hash_fn, const void *key);
// returns whether the hash map contains a certain element.
XHASHMAP_EXTERN_INLINE bool
xahs_contains(XArrayHashSet *ahs, HashFn const hash_fn, const void *key);
// Free the hashset; retain element sizes for reuse or repeated free.
XHASHMAP_EXTERN_INLINE void xahs_free(XArrayHashSet *ahs);

// Read up to eight bytes as a little-endian integer, without alignment
// requirements.
static inline __attribute__((always_inline)) u64 xhash_read_le(const u8 *p,
                                                               unsigned n);
// Return the FNV-1a hash of klen key bytes.
XHASHMAP_EXTERN_INLINE u64 xhash_fnv1a(const void *key, usize klen);
// passthrough designed for integers, requires klen ∈ {1;2;4;8;}
XHASHMAP_EXTERN_INLINE u64 xhash_zx(const void *key, usize klen);
// Mix an integer key to avoid common patterns; klen must be 1, 2, 4, or 8.
XHASHMAP_EXTERN_INLINE u64 xhash_zxmix(const void *key, usize klen);

// Scalar, seed-zero XXH3-64 adapted from xxHash v0.8.3.
// https://github.com/Cyan4973/xxHash/tree/v0.8.3
/*
 * xxHash - Extremely Fast Hash algorithm
 * Header File
 * Copyright (C) 2012-2023 Yann Collet
 *
 * BSD 2-Clause License (https://www.opensource.org/licenses/bsd-license.php)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following disclaimer
 *      in the documentation and/or other materials provided with the
 *      distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * You can contact the author at:
 *   - xxHash homepage: https://www.xxhash.com
 *   - xxHash source repository: https://github.com/Cyan4973/xxHash
 */

// Multiply two words and XOR the low and high halves of the product.
static inline u64 xhash_fold(u64 a, u64 b);
// Apply the XXH64 final avalanche to a hash word.
static inline u64 xhash_xxh64_avalanche(u64 h);
// Apply the XXH3 final avalanche to a hash word.
static inline u64 xhash_xxh3_avalanche(u64 h);
// Mix sixteen key bytes with the secret into one hash word.
static inline u64 xhash_xxh3_mix16(const u8 *p, const u8 *secret);
// Accumulate one 64-byte XXH3 stripe using the secret.
static inline void xhash_xxh3_stripe(u64 *acc, const u8 *p, const u8 *secret);
// Hash a long byte string with XXH3 and the supplied secret.
static inline __attribute__((noinline)) u64 xhash_xxh3_long(const u8 *p,
                                                            usize klen,
                                                            const u8 *secret);
// Return the seed-zero XXH3 hash of klen key bytes.
XHASHMAP_EXTERN_INLINE u64 xhash_xxh3(const void *key, usize klen);

// Compact scalar rapidhash v1.0, using its default seed and secrets.
// https://github.com/Nicoshev/rapidhash/tree/rapidhash_v1.0
/*
 * rapidhash - Very fast, high quality, platform-independent hashing algorithm.
 * Copyright (C) 2024 Nicolas De Carli
 *
 * Based on 'wyhash', by Wang Yi <godspeed_china@yeah.net>
 *
 * BSD 2-Clause License (https://www.opensource.org/licenses/bsd-license.php)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following disclaimer
 *      in the documentation and/or other materials provided with the
 *      distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * You can contact the author at:
 *   - rapidhash source repository: https://github.com/Nicoshev/rapidhash
 */

// strings
static inline __attribute__((noinline)) u64 xhash_rapidhash_long(const u8 *p,
                                                                 usize klen,
                                                                 u64 seed);
// Return the fixed-seed rapidhash of klen key bytes.
XHASHMAP_EXTERN_INLINE u64 xhash_rapidhash(const void *key, usize klen);

// Definitions

static inline __attribute__((always_inline)) void
xhash_copy_value(void *dst, void *src, usize size) {
    if (size == 8) {
        u64 v;
        xmemcp(&v, src, 8);
        xmemcp(dst, &v, 8);
    } else if (size == 16) {
        u128 v;
        xmemcp(&v, src, 16);
        xmemcp(dst, &v, 16);
    } else if (size == 4) {
        u32 v;
        xmemcp(&v, src, 4);
        xmemcp(dst, &v, 4);
    } else if (size)
        xmemmv(dst, src, size);
}

XHASHMAP_EXTERN_INLINE void xdnhm_init(XDynNodeHashMap *nhm, usize val_sz) {
    *nhm = (XDynNodeHashMap){.val_sz = val_sz};
}

static inline void *xhash_node_alloc(XHashNodeBlock **blocks,
                                     const usize size_in) {
    const usize align = 8;
    usize size = size_in;
    assert(size <= (usize)-1 - (align - 1));
    size = (size + align - 1) & ~(align - 1);
    XHashNodeBlock *block = *blocks;
    if (!block || size > block->capacity - block->used) {
        usize capacity = block ? 32768 : 4096;
        if (capacity < size)
            capacity = size;
        assert(capacity <= (usize)-1 - sizeof(XHashNodeBlock));
        block = xalloc(sizeof(*block) + capacity);
        *block = (XHashNodeBlock){.next = *blocks, .capacity = capacity};
        *blocks = block;
    }
    void *result = block->data + block->used;
    block->used += size;
    return result;
}

XHASHMAP_EXTERN_INLINE void xdnhm_reserve(XDynNodeHashMap *nhm, HashFn hash_fn,
                                          usize n) {
    if (n <= nhm->bucket_count / 2)
        return;
    usize capacity = nhm->bucket_count ? nhm->bucket_count : 8;
    while (capacity / 2 < n) {
        assert(capacity <= (usize)-1 / 2);
        capacity *= 2;
    }
    assert(capacity <= (usize)-1 / sizeof(XDynNodeHashEntry *));
    XDynNodeHashEntry **buckets = xallocz(capacity * sizeof(*buckets));
    for (usize i = 0; i < nhm->bucket_count; ++i) {
        XDynNodeHashEntry *node = nhm->buckets[i];
        while (node) {
            XDynNodeHashEntry *next = node->next;
            usize slot =
                hash_fn(node->data + nhm->val_sz, node->klen) & (capacity - 1);
            node->next = buckets[slot];
            buckets[slot] = node;
            node = next;
        }
    }
    if (nhm->buckets)
        xfree(nhm->buckets, nhm->bucket_count * sizeof(*buckets));
    nhm->buckets = buckets;
    nhm->bucket_count = capacity;
}

XHASHMAP_EXTERN_INLINE void *xdnhm_get(XDynNodeHashMap *nhm, HashFn hash_fn,
                                       const void *key, usize klen) {
    if (!nhm->bucket_count)
        return 0;
    u64 hash = hash_fn(key, klen);
    usize slot = hash & (nhm->bucket_count - 1);
    for (XDynNodeHashEntry *node = nhm->buckets[slot]; node; node = node->next)
        if (node->klen == klen &&
            (!klen || xmemeq(node->data + nhm->val_sz, key, klen)))
            return node->data;
    return 0;
}

XHASHMAP_EXTERN_INLINE bool xdnhm_insert(XDynNodeHashMap *nhm, HashFn hash_fn,
                                         const void *key, usize klen,
                                         void *val) {
    u64 hash = hash_fn(key, klen);
    void *existing = 0;
    if (nhm->bucket_count) {
        usize slot = hash & (nhm->bucket_count - 1);
        for (XDynNodeHashEntry *node = nhm->buckets[slot]; node;
             node = node->next) {
            if (node->klen == klen &&
                (!klen || xmemeq(node->data + nhm->val_sz, key, klen))) {
                existing = node->data;
                break;
            }
        }
    }
    if (existing) {
        if (nhm->val_sz)
            xhash_copy_value(existing, val, nhm->val_sz);
        return true;
    }
    assert(klen <= (usize)-1 - sizeof(XDynNodeHashEntry));
    assert(nhm->val_sz <= (usize)-1 - sizeof(XDynNodeHashEntry) - klen);
    assert(nhm->count != (usize)-1);
    xdnhm_reserve(nhm, hash_fn, nhm->count + 1);
    usize size = sizeof(XDynNodeHashEntry) + nhm->val_sz + klen;
    XDynNodeHashEntry *node = xhash_node_alloc(&nhm->blocks, size);
    node->klen = klen;
    if (nhm->val_sz)
        xhash_copy_value(node->data, val, nhm->val_sz);
    if (klen)
        xmemcp(node->data + nhm->val_sz, (void *)key, klen);
    usize slot = hash & (nhm->bucket_count - 1);
    node->next = nhm->buckets[slot];
    nhm->buckets[slot] = node;
    ++nhm->count;
    return false;
}

XHASHMAP_EXTERN_INLINE void xdnhm_free(XDynNodeHashMap *nhm) {
    XHashNodeBlock *block = nhm->blocks;
    while (block) {
        XHashNodeBlock *next = block->next;
        xfree(block, sizeof(*block) + block->capacity);
        block = next;
    }
    if (nhm->buckets)
        xfree(nhm->buckets, nhm->bucket_count * sizeof(*nhm->buckets));
    xdnhm_init(nhm, nhm->val_sz);
}

XHASHMAP_EXTERN_INLINE void xdnhs_init(XDynNodeHashSet *nhs) {
    xdnhm_init(&nhs->map, 0);
}

XHASHMAP_EXTERN_INLINE void xdnhs_reserve(XDynNodeHashSet *nhs, HashFn hash_fn,
                                          usize n) {
    xdnhm_reserve(&nhs->map, hash_fn, n);
}

XHASHMAP_EXTERN_INLINE bool xdnhs_insert(XDynNodeHashSet *nhs, HashFn hash_fn,
                                         const void *key, usize klen) {
    return xdnhm_insert(&nhs->map, hash_fn, key, klen, 0);
}

XHASHMAP_EXTERN_INLINE bool xdnhs_contains(XDynNodeHashSet *nhs, HashFn hash_fn,
                                           const void *key, usize klen) {
    return xdnhm_get(&nhs->map, hash_fn, key, klen) != 0;
}

XHASHMAP_EXTERN_INLINE void xdnhs_free(XDynNodeHashSet *nhs) {
    xdnhm_free(&nhs->map);
}

XHASHMAP_EXTERN_INLINE void xdahm_init(XDynArrayHashMap *ahm, usize val_sz) {
    usize align = 8;
    assert(val_sz <= (usize)-1 - (align - 1));
    usize stride = val_sz ? (val_sz + align - 1) & ~(align - 1) : align;
    *ahm = (XDynArrayHashMap){.val_sz = val_sz, .stride = stride};
}

static inline __attribute__((always_inline)) u64
xhash_short_key(const void *key, usize klen) {
    u64 value = 0;
    if (klen)
        xmemcp(&value, (void *)key, klen);
    return value;
}

static inline __attribute__((always_inline)) unsigned
xhash_control_matches(const u8 *control, u8 tag) {
#if defined(__x86_64__)
    __m128i bytes = _mm_loadu_si128((const __m128i *)control);
    return (unsigned)_mm_movemask_epi8(
        _mm_cmpeq_epi8(bytes, _mm_set1_epi8((char)tag)));
#else
    unsigned mask = 0;
    for (unsigned i = 0; i < 16; ++i)
        mask |= (unsigned)(control[i] == tag) << i;
    return mask;
#endif
}

static inline u8 xhash_control_tag(u64 hash) {
    return (u8)((hash >> 24) ^ (hash >> 48)) | 0x80;
}

static inline void xdahm_mark(XDynArrayHashMap *ahm, usize slot, u64 hash) {
    ahm->control[slot] = xhash_control_tag(hash);
    if (slot < 16)
        ahm->control[ahm->bucket_count + slot] = ahm->control[slot];
}

static inline __attribute__((always_inline)) usize
xdahm_empty(XDynArrayHashMap *ahm, u64 hash) {
    usize slot = hash & (ahm->bucket_count - 1);
    for (;;) {
        unsigned empty = xhash_control_matches(ahm->control + slot, 0);
        if (empty)
            return (slot + __builtin_ctz(empty)) & (ahm->bucket_count - 1);
        slot = (slot + 16) & (ahm->bucket_count - 1);
    }
}

static inline usize xdahm_slot(XDynArrayHashMap *ahm, u64 hash, const void *key,
                               usize klen, u64 small, bool inserting) {
    usize group = hash & (ahm->bucket_count - 1);
    // Identity hashes can skip fingerprint setup and compare the integer key.
    if (klen <= 8 &&
        (hash == small || (inserting && ahm->bucket_count <= 2048))) {
        while (ahm->control[group]) {
            XDynArrayHashEntry *entry = &ahm->buckets[group];
            if ((entry->length == klen + 1) & (entry->key == small))
                break;
            group = (group + 1) & (ahm->bucket_count - 1);
        }
        return group;
    }
    u8 tag = xhash_control_tag(hash);
    if (klen <= 8 && ahm->bucket_count <= 2048 && ahm->control[group] == tag) {
        XDynArrayHashEntry *entry = &ahm->buckets[group];
        if ((entry->length == klen + 1) & (entry->key == small))
            return group;
    }
    for (;;) {
        unsigned matches = xhash_control_matches(ahm->control + group, tag);
        while (matches) {
            usize slot =
                (group + __builtin_ctz(matches)) & (ahm->bucket_count - 1);
            XDynArrayHashEntry *entry = &ahm->buckets[slot];
            if (entry->length == klen + 1 &&
                (klen <= 8 ? entry->key == small
                           : entry->hash == hash &&
                                 xmemeq(ahm->keys + entry->key, key, klen)))
                return slot;
            matches &= matches - 1;
        }
        unsigned empty = xhash_control_matches(ahm->control + group, 0);
        if (empty)
            return (group + __builtin_ctz(empty)) & (ahm->bucket_count - 1);
        group = (group + 16) & (ahm->bucket_count - 1);
    }
}

XHASHMAP_EXTERN_INLINE void xdahm_reserve(XDynArrayHashMap *ahm, HashFn hash_fn,
                                          usize n) {
    (void)hash_fn; // hashes are cached in occupied slots
    if (n <= ahm->bucket_count / 2)
        return;
    usize capacity = ahm->bucket_count ? ahm->bucket_count : 16;
    while (n > capacity / 2) {
        assert(capacity <= (usize)-1 / 2);
        capacity *= 2;
    }
    assert(capacity <= (usize)-1 / sizeof(XDynArrayHashEntry));
    assert(capacity <= (usize)-1 / ahm->stride);
    XDynArrayHashMap grown = *ahm;
    grown.bucket_count = capacity;
    grown.buckets = xalloc(capacity * sizeof(*grown.buckets));
    grown.values = ahm->val_sz ? xalloc(capacity * ahm->stride) : 0;
    grown.control = xallocz(capacity + 16);
    for (usize i = 0; i < ahm->bucket_count; ++i) {
        XDynArrayHashEntry *entry = &ahm->buckets[i];
        if (!ahm->control[i])
            continue;
        usize slot = xdahm_empty(&grown, entry->hash);
        grown.buckets[slot] = *entry;
        xdahm_mark(&grown, slot, entry->hash);
        if (ahm->val_sz)
            xhash_copy_value(grown.values + slot * ahm->stride,
                             ahm->values + i * ahm->stride, ahm->val_sz);
    }
    if (ahm->buckets) {
        xfree(ahm->buckets, ahm->bucket_count * sizeof(*ahm->buckets));
        if (ahm->values)
            xfree(ahm->values, ahm->bucket_count * ahm->stride);
        xfree(ahm->control, ahm->bucket_count + 16);
    }
    *ahm = grown;
}

XHASHMAP_EXTERN_INLINE void *xdahm_get(XDynArrayHashMap *ahm, HashFn hash_fn,
                                       const void *key, usize klen) {
    if (!ahm->bucket_count)
        return 0;
    usize slot = xdahm_slot(ahm, hash_fn(key, klen), key, klen,
                            klen <= 8 ? xhash_short_key(key, klen) : 0, false);
    return ahm->control[slot]
               ? (ahm->val_sz ? (void *)(ahm->values + slot * ahm->stride)
                              : (void *)&ahm->buckets[slot])
               : 0;
}

static inline __attribute__((noinline)) XDynArrayHashMap
xdahm_insert_slow(XDynArrayHashMap map, HashFn hash_fn, const void *key,
                  usize klen, void *val, u64 hash, u64 small, usize slot) {
    XDynArrayHashMap *ahm = &map;
    assert(ahm->count != (usize)-1);
    assert(klen != (usize)-1);
    // Preserve a value from this map if insertion has to relocate the array.
    void *saved = 0;
    bool grow = ahm->count + 1 > ahm->bucket_count / 2;
    if (grow && ahm->val_sz && ahm->values &&
        (usize)val - (usize)ahm->values < ahm->bucket_count * ahm->stride) {
        saved = xalloc(ahm->val_sz);
        xmemcp(saved, val, ahm->val_sz);
        val = saved;
    }
    u64 stored_key = small;
    if (klen > 8) {
        assert(klen <= (usize)-1 - ahm->keys_len);
        usize required = ahm->keys_len + klen;
        if (required > ahm->keys_capacity) {
            usize capacity = ahm->keys_capacity ? ahm->keys_capacity : 64;
            while (capacity < required) {
                if (capacity > (usize)-1 / 2) {
                    capacity = required;
                    break;
                }
                capacity *= 2;
            }
            u8 *keys = xalloc(capacity);
            if (ahm->keys_len)
                xmemcp(keys, ahm->keys, ahm->keys_len);
            xmemcp(keys + ahm->keys_len, (void *)key, klen);
            if (ahm->keys)
                xfree(ahm->keys, ahm->keys_capacity);
            ahm->keys = keys;
            ahm->keys_capacity = capacity;
        } else
            xmemmv(ahm->keys + ahm->keys_len, (void *)key, klen);
        stored_key = ahm->keys_len;
        ahm->keys_len = required;
    }
    if (grow) {
        xdahm_reserve(ahm, hash_fn, ahm->count + 1);
        slot = xdahm_empty(ahm, hash);
    }
    ahm->buckets[slot] = (XDynArrayHashEntry){hash, stored_key, klen + 1};
    xdahm_mark(ahm, slot, hash);
    if (ahm->val_sz)
        xhash_copy_value(ahm->values + slot * ahm->stride, val, ahm->val_sz);
    ++ahm->count;
    if (saved)
        xfree(saved, ahm->val_sz);
    return map;
}

XHASHMAP_EXTERN_INLINE bool xdahm_insert(XDynArrayHashMap *ahm, HashFn hash_fn,
                                         const void *key, usize klen,
                                         void *val) {
    u64 hash = hash_fn(key, klen);
    u64 small = klen <= 8 ? xhash_short_key(key, klen) : 0;
    usize slot =
        ahm->bucket_count ? xdahm_slot(ahm, hash, key, klen, small, true) : 0;
    if (ahm->bucket_count && ahm->control[slot]) {
        if (ahm->val_sz)
            xhash_copy_value(ahm->values + slot * ahm->stride, val,
                             ahm->val_sz);
        return true;
    }
    if (klen > 8 || ahm->count + 1 > ahm->bucket_count / 2) {
        const XDynArrayHashMap grown =
            xdahm_insert_slow(*ahm, hash_fn, key, klen, val, hash, small, slot);
        ahm->buckets = grown.buckets;
        ahm->values = grown.values;
        ahm->keys = grown.keys;
        ahm->control = grown.control;
        ahm->keys_len = grown.keys_len;
        ahm->keys_capacity = grown.keys_capacity;
        ahm->count = grown.count;
        ahm->bucket_count = grown.bucket_count;
        return false;
    }
    ahm->buckets[slot] = (XDynArrayHashEntry){hash, small, klen + 1};
    xdahm_mark(ahm, slot, hash);
    if (ahm->val_sz)
        xhash_copy_value(ahm->values + slot * ahm->stride, val, ahm->val_sz);
    ++ahm->count;
    return false;
}

XHASHMAP_EXTERN_INLINE void xdahm_free(XDynArrayHashMap *ahm) {
    if (ahm->buckets) {
        xfree(ahm->buckets, ahm->bucket_count * sizeof(*ahm->buckets));
        if (ahm->values)
            xfree(ahm->values, ahm->bucket_count * ahm->stride);
        xfree(ahm->control, ahm->bucket_count + 16);
    }
    if (ahm->keys)
        xfree(ahm->keys, ahm->keys_capacity);
    xdahm_init(ahm, ahm->val_sz);
}

XHASHMAP_EXTERN_INLINE void xdahs_init(XDynArrayHashSet *ahs) {
    xdahm_init(&ahs->map, 0);
}

XHASHMAP_EXTERN_INLINE void xdahs_reserve(XDynArrayHashSet *ahs, HashFn hash_fn,
                                          usize n) {
    xdahm_reserve(&ahs->map, hash_fn, n);
}

XHASHMAP_EXTERN_INLINE bool xdahs_insert(XDynArrayHashSet *ahs, HashFn hash_fn,
                                         const void *key, usize klen) {
    return xdahm_insert(&ahs->map, hash_fn, key, klen, 0);
}

XHASHMAP_EXTERN_INLINE bool xdahs_contains(XDynArrayHashSet *ahs,
                                           HashFn hash_fn, const void *key,
                                           usize klen) {
    return xdahm_get(&ahs->map, hash_fn, key, klen) != 0;
}

XHASHMAP_EXTERN_INLINE void xdahs_free(XDynArrayHashSet *ahs) {
    xdahm_free(&ahs->map);
}

XHASHMAP_EXTERN_INLINE void xnhm_init(XNodeHashMap *nhm, const usize key_sz,
                                      const usize val_sz) {
    *nhm = (XNodeHashMap){.key_sz = key_sz, .val_sz = val_sz};
}

XHASHMAP_EXTERN_INLINE void xnhm_reserve(XNodeHashMap *nhm,
                                         HashFn const hash_fn, usize n) {
    if (n <= nhm->bucket_count / 2)
        return;
    usize capacity = nhm->bucket_count ? nhm->bucket_count : 8;
    while (capacity / 2 < n) {
        assert(capacity <= (usize)-1 / 2);
        capacity *= 2;
    }
    assert(capacity <= (usize)-1 / sizeof(XNodeHashEntry *));
    XNodeHashEntry **buckets = xallocz(capacity * sizeof(*buckets));
    for (usize i = 0; i < nhm->bucket_count; ++i) {
        XNodeHashEntry *node = nhm->buckets[i];
        while (node) {
            XNodeHashEntry *next = node->next;
            const usize slot =
                hash_fn(node->data + nhm->val_sz, nhm->key_sz) & (capacity - 1);
            node->next = buckets[slot];
            buckets[slot] = node;
            node = next;
        }
    }
    if (nhm->buckets)
        xfree(nhm->buckets, nhm->bucket_count * sizeof(*buckets));
    nhm->buckets = buckets;
    nhm->bucket_count = capacity;
}

XHASHMAP_EXTERN_INLINE void *xnhm_get(XNodeHashMap *nhm, HashFn const hash_fn,
                                      const void *key) {
    const usize klen = nhm->key_sz;
    if (!nhm->bucket_count)
        return 0;
    const u64 hash = hash_fn(key, klen);
    const usize slot = hash & (nhm->bucket_count - 1);
    for (XNodeHashEntry *node = nhm->buckets[slot]; node; node = node->next)
        if (!klen || xmemeq(node->data + nhm->val_sz, key, klen))
            return node->data;
    return 0;
}

XHASHMAP_EXTERN_INLINE bool xnhm_insert(XNodeHashMap *nhm, HashFn const hash_fn,
                                        const void *key, void *val) {
    const usize klen = nhm->key_sz;
    const u64 hash = hash_fn(key, klen);
    void *existing = 0;
    if (nhm->bucket_count) {
        const usize slot = hash & (nhm->bucket_count - 1);
        for (XNodeHashEntry *node = nhm->buckets[slot]; node;
             node = node->next) {
            if (!klen || xmemeq(node->data + nhm->val_sz, key, klen)) {
                existing = node->data;
                break;
            }
        }
    }
    if (existing) {
        if (nhm->val_sz)
            xhash_copy_value(existing, val, nhm->val_sz);
        return true;
    }
    assert(klen <= (usize)-1 - sizeof(XNodeHashEntry));
    assert(nhm->val_sz <= (usize)-1 - sizeof(XNodeHashEntry) - klen);
    assert(nhm->count != (usize)-1);
    xnhm_reserve(nhm, hash_fn, nhm->count + 1);
    const usize size = sizeof(XNodeHashEntry) + nhm->val_sz + klen;
    XNodeHashEntry *node = xhash_node_alloc(&nhm->blocks, size);
    if (nhm->val_sz)
        xhash_copy_value(node->data, val, nhm->val_sz);
    if (klen)
        xmemcp(node->data + nhm->val_sz, (void *)key, klen);
    const usize slot = hash & (nhm->bucket_count - 1);
    node->next = nhm->buckets[slot];
    nhm->buckets[slot] = node;
    ++nhm->count;
    return false;
}

XHASHMAP_EXTERN_INLINE void xnhm_free(XNodeHashMap *nhm) {
    XHashNodeBlock *block = nhm->blocks;
    while (block) {
        XHashNodeBlock *next = block->next;
        xfree(block, sizeof(*block) + block->capacity);
        block = next;
    }
    if (nhm->buckets)
        xfree(nhm->buckets, nhm->bucket_count * sizeof(*nhm->buckets));
    xnhm_init(nhm, nhm->key_sz, nhm->val_sz);
}

XHASHMAP_EXTERN_INLINE void xnhs_init(XNodeHashSet *nhs, const usize key_sz) {
    xnhm_init(&nhs->map, key_sz, 0);
}

XHASHMAP_EXTERN_INLINE void xnhs_reserve(XNodeHashSet *nhs,
                                         HashFn const hash_fn, usize n) {
    xnhm_reserve(&nhs->map, hash_fn, n);
}

XHASHMAP_EXTERN_INLINE bool xnhs_insert(XNodeHashSet *nhs, HashFn const hash_fn,
                                        const void *key) {
    return xnhm_insert(&nhs->map, hash_fn, key, 0);
}

XHASHMAP_EXTERN_INLINE bool
xnhs_contains(XNodeHashSet *nhs, HashFn const hash_fn, const void *key) {
    return xnhm_get(&nhs->map, hash_fn, key) != 0;
}

XHASHMAP_EXTERN_INLINE void xnhs_free(XNodeHashSet *nhs) {
    xnhm_free(&nhs->map);
}

XHASHMAP_EXTERN_INLINE void xahm_init(XArrayHashMap *ahm, const usize key_sz,
                                      const usize val_sz) {
    assert(val_sz <= (usize)-1 - 7);
    *ahm = (XArrayHashMap){
        .key_sz = key_sz, .val_sz = val_sz, .stride = (val_sz + 7) & ~(usize)7};
}

static inline __attribute__((always_inline)) bool
xhash_key_equal(const void *a, const void *b, const usize size) {
    if (size == 8) {
        u64 x, y;
        xmemcp(&x, (void *)a, 8);
        xmemcp(&y, (void *)b, 8);
        return x == y;
    }
    if (size == 4) {
        u32 x, y;
        xmemcp(&x, (void *)a, 4);
        xmemcp(&y, (void *)b, 4);
        return x == y;
    }
    return !size || xmemeq(a, b, size);
}

static inline void *xahm_key(const XArrayHashMap *ahm, const usize slot) {
    return ahm->key_sz ? ahm->keys + slot * ahm->key_sz : 0;
}

static inline void xahm_mark(XArrayHashMap *ahm, const usize slot,
                             const u64 hash) {
    ahm->control[slot] = xhash_control_tag(hash);
    if (slot < 16)
        ahm->control[ahm->bucket_count + slot] = ahm->control[slot];
}

static inline __attribute__((always_inline)) usize
xahm_empty(const XArrayHashMap *ahm, const u64 hash) {
    const usize mask = ahm->bucket_count - 1;
    usize group = hash & mask;
    for (;;) {
        const unsigned empty = xhash_control_matches(ahm->control + group, 0);
        if (empty)
            return (group + __builtin_ctz(empty)) & mask;
        group = (group + 16) & mask;
    }
}

static inline usize xahm_slot(const XArrayHashMap *ahm, const u64 hash,
                              const void *key, const bool inserting,
                              const usize key_sz) {
    const usize mask = ahm->bucket_count - 1;
    usize group = hash & mask;
    if (key_sz <= 8 && (hash == xhash_short_key(key, key_sz) ||
                        (inserting && ahm->bucket_count <= 2048))) {
        while (ahm->control[group]) {
            if (xhash_key_equal((key_sz ? ahm->keys + group * key_sz : 0), key,
                                key_sz))
                break;
            group = (group + 1) & mask;
        }
        return group;
    }
    const u8 tag = xhash_control_tag(hash);
    for (;;) {
        unsigned matches = xhash_control_matches(ahm->control + group, tag);
        while (matches) {
            const usize slot = (group + __builtin_ctz(matches)) & mask;
            if (xhash_key_equal((key_sz ? ahm->keys + slot * key_sz : 0), key,
                                key_sz))
                return slot;
            matches &= matches - 1;
        }
        const unsigned empty = xhash_control_matches(ahm->control + group, 0);
        if (empty)
            return (group + __builtin_ctz(empty)) & mask;
        group = (group + 16) & mask;
    }
}

static inline void xahm_release_arrays(const XArrayHashMap *ahm) {
    if (!ahm->bucket_count)
        return;
    if (ahm->key_sz)
        xfree(ahm->keys, ahm->bucket_count * ahm->key_sz);
    if (ahm->val_sz)
        xfree(ahm->values, ahm->bucket_count * ahm->stride);
    xfree(ahm->control, ahm->bucket_count + 16);
}

static inline XArrayHashMap xahm_grow(const XArrayHashMap old,
                                      HashFn const hash_fn, const usize n) {
    const usize key_sz = old.key_sz, val_sz = old.val_sz, stride = old.stride;
    usize capacity = old.bucket_count ? old.bucket_count : 16;
    while (capacity / 2 < n) {
        assert(capacity <= (usize)-1 / 2);
        capacity *= 2;
    }
    assert(!key_sz || capacity <= (usize)-1 / key_sz);
    assert(!stride || capacity <= (usize)-1 / stride);
    XArrayHashMap grown = old;
    grown.keys = key_sz ? xalloc(capacity * key_sz) : 0;
    grown.values = val_sz ? xalloc(capacity * stride) : 0;
    grown.control = xallocz(capacity + 16);
    grown.bucket_count = capacity;
    for (usize i = 0; i < old.bucket_count; ++i) {
        if (!old.control[i])
            continue;
        void *key = xahm_key(&old, i);
        const u64 hash = hash_fn(key, key_sz);
        const usize slot = xahm_empty(&grown, hash);
        if (key_sz)
            xhash_copy_value(grown.keys + slot * key_sz, key, key_sz);
        if (val_sz)
            xhash_copy_value(grown.values + slot * stride,
                             old.values + i * stride, val_sz);
        xahm_mark(&grown, slot, hash);
    }
    return grown;
}

static inline XArrayHashMap xahm_rebuild(XArrayHashMap map, HashFn hash_fn,
                                         const usize n) {
    const XArrayHashMap grown = xahm_grow(map, hash_fn, n);
    xahm_release_arrays(&map);
    return grown;
}

static inline void xahm_publish(XArrayHashMap *ahm,
                                const XArrayHashMap *grown) {
    ahm->keys = grown->keys;
    ahm->values = grown->values;
    ahm->control = grown->control;
    ahm->bucket_count = grown->bucket_count;
    ahm->count = grown->count;
}

static inline __attribute__((noinline)) XArrayHashMap
xahm_insert_growing(XArrayHashMap map, HashFn hash_fn, const void *key,
                    void *val, const u64 hash) {
    const XArrayHashMap old = map;
    map = xahm_grow(old, hash_fn, old.count + 1);
    const usize slot = xahm_empty(&map, hash);
    if (map.key_sz)
        xhash_copy_value(map.keys + slot * map.key_sz, (void *)key, map.key_sz);
    if (map.val_sz)
        xhash_copy_value(map.values + slot * map.stride, val, map.val_sz);
    xahm_mark(&map, slot, hash);
    ++map.count;
    xahm_release_arrays(&old);
    return map;
}

XHASHMAP_EXTERN_INLINE void xahm_reserve(XArrayHashMap *ahm,
                                         HashFn const hash_fn, const usize n) {
    if (n <= ahm->bucket_count / 2)
        return;
    const XArrayHashMap grown = xahm_rebuild(*ahm, hash_fn, n);
    xahm_publish(ahm, &grown);
}

XHASHMAP_EXTERN_INLINE void *xahm_get(XArrayHashMap *ahm, HashFn const hash_fn,
                                      const void *key) {
    if (!ahm->bucket_count)
        return 0;
    const usize slot =
        xahm_slot(ahm, hash_fn(key, ahm->key_sz), key, false, ahm->key_sz);
    if (!ahm->control[slot])
        return 0;
    return ahm->val_sz ? ahm->values + slot * ahm->stride : ahm->control + slot;
}

XHASHMAP_EXTERN_INLINE bool xahm_insert(XArrayHashMap *ahm,
                                        HashFn const hash_fn, const void *key,
                                        void *val) {
    const usize key_sz = ahm->key_sz, val_sz = ahm->val_sz,
                stride = ahm->stride;
    const u64 hash = hash_fn(key, key_sz);
    usize slot =
        ahm->bucket_count ? xahm_slot(ahm, hash, key, true, key_sz) : 0;
    if (ahm->bucket_count && ahm->control[slot]) {
        if (val_sz)
            xhash_copy_value(ahm->values + slot * stride, val, val_sz);
        return true;
    }
    assert(ahm->count != (usize)-1);
    if (ahm->count + 1 > ahm->bucket_count / 2) {
        const XArrayHashMap grown =
            xahm_insert_growing(*ahm, hash_fn, key, val, hash);
        xahm_publish(ahm, &grown);
        return false;
    }
    if (key_sz)
        xmemcp(ahm->keys + slot * key_sz, (void *)key, key_sz);
    if (val_sz)
        xhash_copy_value(ahm->values + slot * stride, val, val_sz);
    xahm_mark(ahm, slot, hash);
    ++ahm->count;
    return false;
}

XHASHMAP_EXTERN_INLINE void xahm_free(XArrayHashMap *ahm) {
    xahm_release_arrays(ahm);
    xahm_init(ahm, ahm->key_sz, ahm->val_sz);
}

XHASHMAP_EXTERN_INLINE void xahs_init(XArrayHashSet *ahs, const usize key_sz) {
    xahm_init(&ahs->map, key_sz, 0);
}

XHASHMAP_EXTERN_INLINE void xahs_reserve(XArrayHashSet *ahs,
                                         HashFn const hash_fn, const usize n) {
    xahm_reserve(&ahs->map, hash_fn, n);
}

XHASHMAP_EXTERN_INLINE bool xahs_insert(XArrayHashSet *ahs,
                                        HashFn const hash_fn, const void *key) {
    return xahm_insert(&ahs->map, hash_fn, key, 0);
}

XHASHMAP_EXTERN_INLINE bool
xahs_contains(XArrayHashSet *ahs, HashFn const hash_fn, const void *key) {
    return xahm_get(&ahs->map, hash_fn, key) != 0;
}

XHASHMAP_EXTERN_INLINE void xahs_free(XArrayHashSet *ahs) {
    xahm_free(&ahs->map);
}

static inline __attribute__((always_inline)) u64 xhash_read_le(const u8 *p,
                                                               unsigned n) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    if (n == 8) {
        u64 v;
        xmemcp(&v, (void *)p, 8);
        return v;
    }
    if (n == 4) {
        u32 v;
        xmemcp(&v, (void *)p, 4);
        return v;
    }
    if (n == 2) {
        u16 v;
        xmemcp(&v, (void *)p, 2);
        return v;
    }
#endif
    u64 value = 0;
    for (unsigned i = 0; i < n; ++i)
        value |= (u64)p[i] << (8 * i);
    return value;
}

XHASHMAP_EXTERN_INLINE u64 xhash_fnv1a(const void *key, usize klen) {
    const u8 *p = key;
    u64 hash = 0xcbf29ce484222325ull;
    if (klen == 8) {
        for (unsigned i = 0; i < 8; ++i) {
            hash ^= p[i];
            hash *= 0x100000001b3ull;
        }
        return hash;
    }
    for (usize i = 0; i < klen; ++i) {
        hash ^= p[i];
        hash *= 0x100000001b3ull;
    }
    return hash;
}

XHASHMAP_EXTERN_INLINE u64 xhash_zx(const void *key, usize klen) {
    return xhash_read_le(key, (unsigned)klen);
}

XHASHMAP_EXTERN_INLINE u64 xhash_zxmix(const void *key, usize klen) {
    u64 x = xhash_zx(key, klen);
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
    return x ^ (x >> 31);
}

static inline u64 xhash_fold(u64 a, u64 b) {
    u128 product = (u128)a * b;
    return (u64)product ^ (u64)(product >> 64);
}

static inline u64 xhash_xxh64_avalanche(u64 h) {
    h ^= h >> 33;
    h *= 0xc2b2ae3d27d4eb4full;
    h ^= h >> 29;
    h *= 0x165667b19e3779f9ull;
    return h ^ (h >> 32);
}

static inline u64 xhash_xxh3_avalanche(u64 h) {
    h ^= h >> 37;
    h *= 0x165667919e3779f9ull;
    return h ^ (h >> 32);
}

static inline u64 xhash_xxh3_mix16(const u8 *p, const u8 *secret) {
    return xhash_fold(xhash_read_le(p, 8) ^ xhash_read_le(secret, 8),
                      xhash_read_le(p + 8, 8) ^ xhash_read_le(secret + 8, 8));
}

static inline void xhash_xxh3_stripe(u64 *acc, const u8 *p, const u8 *secret) {
    for (unsigned i = 0; i < 8; ++i) {
        u64 data = xhash_read_le(p + 8 * i, 8);
        u64 keyed = data ^ xhash_read_le(secret + 8 * i, 8);
        acc[i ^ 1] += data;
        acc[i] += (u64)(u32)keyed * (keyed >> 32);
    }
}

static inline __attribute__((noinline)) u64 xhash_xxh3_long(const u8 *p,
                                                            usize klen,
                                                            const u8 *secret) {
    u64 h = klen * 0x9e3779b185ebca87ull;
    if (klen <= 128) {
        for (usize i = 0; i <= (klen - 1) / 32; ++i) {
            h += xhash_xxh3_mix16(p + 16 * i, secret + 32 * i);
            h +=
                xhash_xxh3_mix16(p + klen - 16 * (i + 1), secret + 32 * i + 16);
        }
        return xhash_xxh3_avalanche(h);
    }
    if (klen <= 240) {
        for (unsigned i = 0; i < 8; ++i)
            h += xhash_xxh3_mix16(p + 16 * i, secret + 16 * i);
        h = xhash_xxh3_avalanche(h);
        for (usize i = 8; i < klen / 16; ++i)
            h += xhash_xxh3_mix16(p + 16 * i, secret + 16 * (i - 8) + 3);
        h += xhash_xxh3_mix16(p + klen - 16, secret + 119);
        return xhash_xxh3_avalanche(h);
    }
    u64 acc[8] = {0xc2b2ae3d,
                  0x9e3779b185ebca87ull,
                  0xc2b2ae3d27d4eb4full,
                  0x165667b19e3779f9ull,
                  0x85ebca77c2b2ae63ull,
                  0x85ebca77,
                  0x27d4eb2f165667c5ull,
                  0x9e3779b1};
    usize blocks = (klen - 1) / 1024;
    for (usize block = 0; block < blocks; ++block) {
        for (unsigned stripe = 0; stripe < 16; ++stripe)
            xhash_xxh3_stripe(acc, p + block * 1024 + stripe * 64,
                              secret + stripe * 8);
        for (unsigned i = 0; i < 8; ++i) {
            acc[i] ^= acc[i] >> 47;
            acc[i] ^= xhash_read_le(secret + 128 + 8 * i, 8);
            acc[i] *= 0x9e3779b1;
        }
    }
    for (usize stripe = 0; stripe < ((klen - 1) % 1024) / 64; ++stripe)
        xhash_xxh3_stripe(acc, p + blocks * 1024 + stripe * 64,
                          secret + stripe * 8);
    xhash_xxh3_stripe(acc, p + klen - 64, secret + 121);
    for (unsigned i = 0; i < 4; ++i)
        h +=
            xhash_fold(acc[2 * i] ^ xhash_read_le(secret + 11 + 16 * i, 8),
                       acc[2 * i + 1] ^ xhash_read_le(secret + 19 + 16 * i, 8));
    return xhash_xxh3_avalanche(h);
}

XHASHMAP_EXTERN_INLINE u64 xhash_xxh3(const void *key, usize klen) {
    static const u8 secret[192] = {
        0xb8, 0xfe, 0x6c, 0x39, 0x23, 0xa4, 0x4b, 0xbe, 0x7c, 0x01, 0x81, 0x2c,
        0xf7, 0x21, 0xad, 0x1c, 0xde, 0xd4, 0x6d, 0xe9, 0x83, 0x90, 0x97, 0xdb,
        0x72, 0x40, 0xa4, 0xa4, 0xb7, 0xb3, 0x67, 0x1f, 0xcb, 0x79, 0xe6, 0x4e,
        0xcc, 0xc0, 0xe5, 0x78, 0x82, 0x5a, 0xd0, 0x7d, 0xcc, 0xff, 0x72, 0x21,
        0xb8, 0x08, 0x46, 0x74, 0xf7, 0x43, 0x24, 0x8e, 0xe0, 0x35, 0x90, 0xe6,
        0x81, 0x3a, 0x26, 0x4c, 0x3c, 0x28, 0x52, 0xbb, 0x91, 0xc3, 0x00, 0xcb,
        0x88, 0xd0, 0x65, 0x8b, 0x1b, 0x53, 0x2e, 0xa3, 0x71, 0x64, 0x48, 0x97,
        0xa2, 0x0d, 0xf9, 0x4e, 0x38, 0x19, 0xef, 0x46, 0xa9, 0xde, 0xac, 0xd8,
        0xa8, 0xfa, 0x76, 0x3f, 0xe3, 0x9c, 0x34, 0x3f, 0xf9, 0xdc, 0xbb, 0xc7,
        0xc7, 0x0b, 0x4f, 0x1d, 0x8a, 0x51, 0xe0, 0x4b, 0xcd, 0xb4, 0x59, 0x31,
        0xc8, 0x9f, 0x7e, 0xc9, 0xd9, 0x78, 0x73, 0x64, 0xea, 0xc5, 0xac, 0x83,
        0x34, 0xd3, 0xeb, 0xc3, 0xc5, 0x81, 0xa0, 0xff, 0xfa, 0x13, 0x63, 0xeb,
        0x17, 0x0d, 0xdd, 0x51, 0xb7, 0xf0, 0xda, 0x49, 0xd3, 0x16, 0x55, 0x26,
        0x29, 0xd4, 0x68, 0x9e, 0x2b, 0x16, 0xbe, 0x58, 0x7d, 0x47, 0xa1, 0xfc,
        0x8f, 0xf8, 0xb8, 0xd1, 0x7a, 0xd0, 0x31, 0xce, 0x45, 0xcb, 0x3a, 0x8f,
        0x95, 0x16, 0x04, 0x28, 0xaf, 0xd7, 0xfb, 0xca, 0xbb, 0x4b, 0x40, 0x7e,
    };
    const u8 *p = key;
    if (klen <= 16) {
        if (klen > 8) {
            u64 a = xhash_read_le(p, 8) ^ (xhash_read_le(secret + 24, 8) ^
                                           xhash_read_le(secret + 32, 8));
            u64 b =
                xhash_read_le(p + klen - 8, 8) ^
                (xhash_read_le(secret + 40, 8) ^ xhash_read_le(secret + 48, 8));
            return xhash_xxh3_avalanche(klen + __builtin_bswap64(a) + b +
                                        xhash_fold(a, b));
        }
        if (klen >= 4) {
            u64 h =
                ((xhash_read_le(p, 4) << 32) | xhash_read_le(p + klen - 4, 4)) ^
                (xhash_read_le(secret + 8, 8) ^ xhash_read_le(secret + 16, 8));
            h ^= ((h << 49) | (h >> 15)) ^ ((h << 24) | (h >> 40));
            h *= 0x9fb21c651e98df25ull;
            h ^= (h >> 35) + klen;
            h *= 0x9fb21c651e98df25ull;
            return h ^ (h >> 28);
        }
        if (klen) {
            u32 combined = (u32)p[0] << 16 | (u32)p[klen >> 1] << 24 |
                           p[klen - 1] | (u32)klen << 8;
            return xhash_xxh64_avalanche(
                combined ^
                (xhash_read_le(secret, 4) ^ xhash_read_le(secret + 4, 4)));
        }
        return xhash_xxh64_avalanche(xhash_read_le(secret + 56, 8) ^
                                     xhash_read_le(secret + 64, 8));
    }
    return xhash_xxh3_long(p, klen, secret);
}

static inline __attribute__((noinline)) u64 xhash_rapidhash_long(const u8 *p,
                                                                 usize klen,
                                                                 u64 seed) {
    const u64 s0 = 0x2d358dccaa6c78a5ull, s1 = 0x8bb84b93962eacc9ull,
              s2 = 0x4b33a62ed433d4a3ull;
    u64 a, b;
    usize remaining = klen;
    if (remaining > 48) {
        u64 see1 = seed, see2 = seed;
        do {
            seed = xhash_fold(xhash_read_le(p, 8) ^ s0,
                              xhash_read_le(p + 8, 8) ^ seed);
            see1 = xhash_fold(xhash_read_le(p + 16, 8) ^ s1,
                              xhash_read_le(p + 24, 8) ^ see1);
            see2 = xhash_fold(xhash_read_le(p + 32, 8) ^ s2,
                              xhash_read_le(p + 40, 8) ^ see2);
            p += 48;
            remaining -= 48;
        } while (remaining >= 48);
        seed ^= see1 ^ see2;
    }
    if (remaining > 16) {
        seed = xhash_fold(xhash_read_le(p, 8) ^ s2,
                          xhash_read_le(p + 8, 8) ^ seed ^ s1);
        if (remaining > 32)
            seed = xhash_fold(xhash_read_le(p + 16, 8) ^ s2,
                              xhash_read_le(p + 24, 8) ^ seed);
    }
    // remaining may be zero: keep pointer subtraction signed in pointer
    // space, rather than wrapping an unsigned (remaining - 16).
    a = xhash_read_le(p + remaining - 16, 8);
    b = xhash_read_le(p + remaining - 8, 8);
    u128 product = (u128)(a ^ s1) * (b ^ seed);
    return xhash_fold((u64)product ^ s0 ^ klen, (u64)(product >> 64) ^ s1);
}

XHASHMAP_EXTERN_INLINE u64 xhash_rapidhash(const void *key, usize klen) {
    const u8 *p = key;
    const u64 s0 = 0x2d358dccaa6c78a5ull, s1 = 0x8bb84b93962eacc9ull;
    u64 seed = 0xbdd89aa982704029ull;
    seed ^= xhash_fold(seed ^ s0, s1) ^ klen;
    u64 a = 0, b = 0;
    if (klen <= 16) {
        if (klen >= 4) {
            const u8 *last = p + klen - 4;
            usize delta = (klen & 24) >> (klen >> 3);
            a = xhash_read_le(p, 4) << 32 | xhash_read_le(last, 4);
            b = xhash_read_le(p + delta, 4) << 32 |
                xhash_read_le(last - delta, 4);
        } else if (klen) {
            a = (u64)p[0] << 56 | (u64)p[klen >> 1] << 32 | p[klen - 1];
        }
    } else
        return xhash_rapidhash_long(p, klen, seed);
    u128 product = (u128)(a ^ s1) * (b ^ seed);
    return xhash_fold((u64)product ^ s0 ^ klen, (u64)(product >> 64) ^ s1);
}
