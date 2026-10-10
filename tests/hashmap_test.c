#include "xstdc/hashmap.h"
#include "xstdc/mem.h"
#include "xstdc/print.h"

// Generated with upstream xxHash v0.8.3 and rapidhash v1.0, default seeds.
// Input byte i is (i * 37 + 11) modulo 256.
static const struct {
    usize len;
    u64 xxh3, rapid;
} vectors[] = {
    {0, 0x2d06800538d394c2ull, 0x5a6ef77074ebc84bull},
    {1, 0x4a4139caf4136257ull, 0xe26c60fb0d6336efull},
    {2, 0x571a2ebb238829bbull, 0xd47f9c985ada4c6dull},
    {3, 0x505118c313121c0eull, 0x1498edf9f1c2f956ull},
    {4, 0x3f6889ef166b4ad1ull, 0xac0365cd048beb24ull},
    {7, 0x21d5b4b6d43277e2ull, 0x4805d1635569e520ull},
    {8, 0x93f1200f9be82671ull, 0x2cd540c96c008f1bull},
    {9, 0x05b2fd4d97edcdaeull, 0xd28b7f98e3832bfcull},
    {15, 0xc01ae71cb4542755ull, 0x776d3010af6bb8f2ull},
    {16, 0x2e4359e2a04a5e32ull, 0xd2eff36bdab492bbull},
    {17, 0xf60c53684bbc489dull, 0x2f6d95f7920a277full},
    {31, 0xa2ee08c196bd0790ull, 0xd99a2bd33859c851ull},
    {32, 0x664864df0876efc6ull, 0x02a84e6adcf04c4bull},
    {33, 0xaa998d6131d956aaull, 0x19ee1079c34158b3ull},
    {47, 0xb34d16dac5b428b5ull, 0xd74b0e4d3ecc1effull},
    {48, 0x4522f442246caadfull, 0x42cdc4723245f096ull},
    {49, 0xf025da1d0a5716c5ull, 0x65efcf36a24084a7ull},
    {63, 0x450f06b1f364ef74ull, 0x30d59f5acc343fc2ull},
    {64, 0x8e7464ffdf82c775ull, 0x2e1bb0aaa44fcd8full},
    {65, 0x8e3ac0373892cc0aull, 0x5d0b949dc5bab093ull},
    {95, 0x46e0a7c10954c139ull, 0x209ebb68fa7c3325ull},
    {96, 0xdd8813a25295a5d5ull, 0x9acbab101a6655d3ull},
    {97, 0xca141081637fc064ull, 0x156c01292c20d169ull},
    {127, 0x8b855ef51727776bull, 0x5ace095cbe2a46d2ull},
    {128, 0x5c6ab7b5b2360539ull, 0x597cd48ecc6a31b5ull},
    {129, 0x45fabbd183de7649ull, 0x768de3fefcb52045ull},
    {239, 0xa1e83bcb725e5fbdull, 0x5ed1adc605a79c62ull},
    {240, 0xc0796428068cd12eull, 0xb451d5aa34b3c5e8ull},
    {241, 0x859dc8ab6dd85c7cull, 0x59f5431e448fbd0eull},
    {255, 0xa7654c5988d34927ull, 0x73ea3ff2d8ea4c16ull},
    {256, 0x2998c04a18028d12ull, 0x787a37165bc6d676ull},
    {511, 0x1597d5f6d366c99bull, 0xede88baaca9787adull},
    {512, 0xd02220e4b3bface7ull, 0xca0f908d2e555750ull},
    {1023, 0xf35d4c7d308f7a40ull, 0xcc2a91c914ebdae7ull},
    {1024, 0x7df7f049c0c1ad73ull, 0x05b77a9c3bcf34dbull},
    {1025, 0x29adcfcac5492652ull, 0x8ed493c9bbca9f5eull},
    {2049, 0x215115dee6e09597ull, 0xf02510ad12dcea0cull},
};

static void test_hashes(void) {
    u8 data[2049 + 8];
    for (unsigned offset = 0; offset < 8; ++offset) {
        u8 *p = data + offset;
        for (usize i = 0; i < 2049; ++i)
            p[i] = (u8)(i * 37 + 11);
        for (usize i = 0; i < sizeof(vectors) / sizeof(*vectors); ++i) {
            assert(xhash_xxh3(p, vectors[i].len) == vectors[i].xxh3);
            assert(xhash_rapidhash(p, vectors[i].len) == vectors[i].rapid);
        }
        for (unsigned n = 1; n <= 8; n *= 2) {
            u64 expected = 0;
            for (unsigned i = 0; i < n; ++i)
                expected |= (u64)p[i] << (8 * i);
            assert(xhash_zx(p, n) == expected);
        }
    }
    assert(xhash_fnv1a(0, 0) == 0xcbf29ce484222325ull);
    assert(xhash_fnv1a("a", 1) == 0xaf63dc4c8601ec8cull);
    assert(xhash_fnv1a("hello", 5) == 0xa430d84680aabd0bull);
    assert(xhash_xxh3(0, 0) == vectors[0].xxh3);
    assert(xhash_rapidhash(0, 0) == vectors[0].rapid);
    u8 one[] = {1, 0, 0, 0, 0, 0, 0, 0};
    assert(xhash_zxmix(one, 8) == 0x5692161d100b05e5ull);
}

static u64 collision(const void *key, usize n) {
    (void)key;
    (void)n;
    return 7;
}

static void test_containers(HashFn hash) {
    XDynNodeHashMap map;
    XDynNodeHashSet set;
    xdnhm_init(&map, sizeof(u64));
    xdnhs_init(&set);
    u64 key = 0;
    assert(!xdnhm_get(&map, hash, &key, sizeof key));
    assert(!xdnhs_contains(&set, hash, &key, sizeof key));
    xdnhm_reserve(&map, hash, 0);
    xdnhs_reserve(&set, hash, 0);
    xdnhm_free(&map);
    xdnhs_free(&set);
    u64 *first = 0;
    for (u64 i = 0; i < 513; ++i) {
        key = i;
        u64 value = i * 17;
        assert(!xdnhm_insert(&map, hash, &key, sizeof key, &value));
        assert(!xdnhs_insert(&set, hash, &key, sizeof key));
        if (i == 0)
            first = xdnhm_get(&map, hash, &key, sizeof key);
        key = (u64)-1;
        value = (u64)-1; // Container owns copies.
    }
    xdnhm_reserve(&map, hash, 4096);
    xdnhs_reserve(&set, hash, 4096);
    xdnhm_reserve(&map, hash, 1);
    for (u64 i = 0; i < 513; ++i) {
        u64 *p = xdnhm_get(&map, hash, &i, sizeof i);
        assert(p && *p == i * 17);
        assert(xdnhs_contains(&set, hash, &i, sizeof i));
        assert(xdnhs_insert(&set, hash, &i, sizeof i));
        assert(xdnhm_insert(&map, hash, &i, sizeof i, p));
        u64 value = i + 10;
        assert(xdnhm_insert(&map, hash, &i, sizeof i, &value));
        assert(*p == value);
    }
    key = 0;
    assert(first == xdnhm_get(&map, hash, &key, sizeof key));
    assert(*first == 10);
    assert(map.count == 513 && set.map.count == 513);
    key = 9999;
    assert(!xdnhm_get(&map, hash, &key, sizeof key));
    assert(!xdnhs_contains(&set, hash, &key, sizeof key));
    xdnhm_free(&map);
    xdnhm_free(&map);
    xdnhs_free(&set);
    xdnhs_free(&set);
    assert(map.count == 0 && !map.buckets && map.val_sz == sizeof(u64));
    assert(!xdnhm_insert(&map, hash, &key, sizeof key, &key));
    assert(!xdnhs_insert(&set, hash, &key, sizeof key));
    xdnhm_free(&map);
    xdnhs_free(&set);
}

static void test_binary_keys(void) {
    XDynNodeHashMap map;
    XDynNodeHashSet set;
    xdnhm_init(&map, sizeof(f64));
    xdnhs_init(&set);
    u8 key[] = {0, 1, 0, 2};
    for (usize len = 0; len <= sizeof key; ++len) {
        f64 value = len + 0.25L;
        assert(!xdnhm_insert(&map, collision, len ? key : 0, len, &value));
        assert(!xdnhs_insert(&set, collision, len ? key : 0, len));
    }
    for (usize len = 0; len <= sizeof key; ++len) {
        f64 *p = xdnhm_get(&map, collision, key, len);
        assert(p && (usize)p % 8 == 0);
        assert(*p == len + 0.25L);
        assert(xdnhs_contains(&set, collision, key, len));
    }
    xdnhm_free(&map);
    xdnhs_free(&set);
    xdnhm_init(&map, 0);
    assert(!xdnhm_insert(&map, xhash_xxh3, 0, 0, 0));
    assert(xdnhm_get(&map, xhash_xxh3, 0, 0));
    assert(xdnhm_insert(&map, xhash_xxh3, 0, 0, 0));
    xdnhm_free(&map);
    // Large values and keys exercise allocation size and full copying.
    u8 bytes[4096];
    for (usize i = 0; i < sizeof bytes; ++i)
        bytes[i] = (u8)i;
    xdnhm_init(&map, sizeof bytes);
    assert(!xdnhm_insert(&map, xhash_rapidhash, bytes, sizeof bytes, bytes));
    u8 *value = xdnhm_get(&map, xhash_rapidhash, bytes, sizeof bytes);
    assert(value && xmemeq(value, bytes, sizeof bytes));
    xdnhm_free(&map);
}

static u64 array_collision(const void *key, usize n) {
    (void)key;
    (void)n;
    return (u64)-1; // Every probe chain wraps around the end of the table.
}

static void test_array_containers(HashFn hash) {
    XDynArrayHashMap map;
    XDynArrayHashSet set;
    xdahm_init(&map, sizeof(u64));
    xdahs_init(&set);
    u64 key = 0;
    assert(!xdahm_get(&map, hash, &key, sizeof key));
    assert(!xdahs_contains(&set, hash, &key, sizeof key));
    xdahm_reserve(&map, hash, 0);
    xdahs_reserve(&set, hash, 0);
    xdahm_free(&map);
    xdahs_free(&set);
    for (u64 i = 0; i < 513; ++i) {
        key = i;
        u64 value = i * 17;
        assert(!xdahm_insert(&map, hash, &key, sizeof key, &value));
        assert(!xdahs_insert(&set, hash, &key, sizeof key));
        key = (u64)-1;
        value = (u64)-1;
    }
    xdahm_reserve(&map, hash, 4096);
    xdahs_reserve(&set, hash, 4096);
    usize capacity = map.bucket_count;
    xdahm_reserve(&map, hash, 1);
    assert(map.bucket_count == capacity);
    for (u64 i = 0; i < 513; ++i) {
        u64 *p = xdahm_get(&map, hash, &i, sizeof i);
        assert(p && *p == i * 17);
        assert(xdahs_contains(&set, hash, &i, sizeof i));
        assert(xdahs_insert(&set, hash, &i, sizeof i));
        assert(xdahm_insert(&map, hash, &i, sizeof i, p));
        u64 value = i + 10;
        assert(xdahm_insert(&map, hash, &i, sizeof i, &value));
        assert(p == xdahm_get(&map, hash, &i, sizeof i) && *p == value);
    }
    assert(map.count == 513 && set.map.count == 513);
    key = 9999;
    assert(!xdahm_get(&map, hash, &key, sizeof key));
    assert(!xdahs_contains(&set, hash, &key, sizeof key));
    xdahm_free(&map);
    xdahm_free(&map);
    xdahs_free(&set);
    xdahs_free(&set);
    assert(!map.count && !map.buckets && !map.keys && !map.values);
    assert(map.val_sz == sizeof(u64));
    assert(!xdahm_insert(&map, hash, &key, sizeof key, &key));
    assert(!xdahs_insert(&set, hash, &key, sizeof key));
    xdahm_free(&map);
    xdahs_free(&set);
}

static void test_array_aliases(void) {
    XDynArrayHashMap map;
    xdahm_init(&map, sizeof(u64));
    for (u64 i = 0; i < 8; ++i) {
        u64 value = i + 100;
        assert(!xdahm_insert(&map, xhash_zx, &i, 8, &value));
    }
    usize capacity = map.bucket_count;
    u64 zero = 0;
    u64 *value = xdahm_get(&map, xhash_zx, &zero, 8);
    // Both arguments refer to a value invalidated by this insertion's growth.
    assert(!xdahm_insert(&map, xhash_zx, value, 8, value));
    assert(map.bucket_count > capacity);
    u64 key = 100;
    value = xdahm_get(&map, xhash_zx, &key, 8);
    assert(value && *value == 100);
    xdahm_free(&map);

    // Odd value sizes still get aligned slots. Reserve prevents table growth.
    xdahm_init(&map, 3);
    xdahm_reserve(&map, xhash_fnv1a, 100);
    capacity = map.bucket_count;
    u8 bytes[3] = {7, 0, 9};
    for (u64 i = 0; i < 100; ++i) {
        assert(!xdahm_insert(&map, xhash_fnv1a, &i, 8, bytes));
    }
    assert(map.bucket_count == capacity);
    for (u64 i = 0; i < 100; ++i) {
        void *p = xdahm_get(&map, xhash_fnv1a, &i, 8);
        assert(p && (usize)p % 8 == 0);
        assert(xmemeq(p, bytes, 3));
    }
    xdahm_free(&map);
}

static void test_array_binary_keys(void) {
    XDynArrayHashMap map;
    XDynArrayHashSet set;
    xdahm_init(&map, sizeof(f64));
    xdahs_init(&set);
    u8 key[] = {0, 1, 0, 2};
    for (usize len = 0; len <= sizeof key; ++len) {
        f64 value = len + 0.25L;
        assert(
            !xdahm_insert(&map, array_collision, len ? key : 0, len, &value));
        assert(!xdahs_insert(&set, array_collision, len ? key : 0, len));
    }
    for (usize len = 0; len <= sizeof key; ++len) {
        f64 *p = xdahm_get(&map, array_collision, key, len);
        assert(p && (usize)p % 8 == 0);
        assert(*p == len + 0.25L);
        assert(xdahs_contains(&set, array_collision, key, len));
    }
    xdahm_free(&map);
    xdahs_free(&set);
    xdahm_init(&map, 0);
    assert(!xdahm_insert(&map, xhash_xxh3, 0, 0, 0));
    assert(xdahm_get(&map, xhash_xxh3, 0, 0));
    assert(xdahm_insert(&map, xhash_xxh3, 0, 0, 0));
    xdahm_free(&map);
    // Large values and keys exercise allocation size and full copying.
    u8 bytes[4096];
    for (usize i = 0; i < sizeof bytes; ++i)
        bytes[i] = (u8)i;
    xdahm_init(&map, sizeof bytes);
    assert(!xdahm_insert(&map, xhash_rapidhash, bytes, sizeof bytes, bytes));
    u8 *value = xdahm_get(&map, xhash_rapidhash, bytes, sizeof bytes);
    assert(value && xmemeq(value, bytes, sizeof bytes));
    xdahm_free(&map);
}

// Mix inline and buffered keys, including embedded zeros and the 8/9-byte
// boundary, through multiple growths. Stored values encode their key index.
static void test_mixed_keys(HashFn hash) {
    XDynArrayHashMap array;
    XDynNodeHashMap node;
    xdahm_init(&array, sizeof(u64));
    xdnhm_init(&node, sizeof(u64));
    u8 key[129];
    for (u64 i = 0; i < 1200; ++i) {
        usize len = 8 + i % 122;
        xmemzero(key, sizeof key);
        xmemcp(key, &i, 8);
        u64 value = i * 17;
        assert(!xdahm_insert(&array, hash, key, len, &value));
        assert(!xdnhm_insert(&node, hash, key, len, &value));
        xmemzero(key, sizeof key);
    }
    xdahm_reserve(&array, hash, 3000);
    xdnhm_reserve(&node, hash, 3000);
    for (u64 i = 0; i < 1200; ++i) {
        usize len = 8 + i % 122;
        xmemzero(key, sizeof key);
        xmemcp(key, &i, 8);
        u64 *a = xdahm_get(&array, hash, key, len);
        u64 *n = xdnhm_get(&node, hash, key, len);
        assert(a && n && *a == i * 17 && *n == *a);
        u64 value = i + 1234;
        assert(xdahm_insert(&array, hash, key, len, &value));
        assert(xdnhm_insert(&node, hash, key, len, &value));
        assert(*a == value && *n == value);
        key[len - 1] ^= 0x80;
        assert(!xdahm_get(&array, hash, key, len));
        assert(!xdnhm_get(&node, hash, key, len));
    }
    assert(array.count == 1200 && node.count == 1200);
    xdahm_free(&array);
    xdnhm_free(&node);
}

// A nonempty key and hash zero must not be confused with an empty slot.
static u64 zero_hash(const void *key, usize len) {
    (void)key;
    (void)len;
    return 0;
}

static void test_large_alias(void) {
    XDynArrayHashMap map;
    xdahm_init(&map, 40000);
    u8 *value = xalloc(40000);
    for (usize j = 0; j < 40000; ++j)
        value[j] = (u8)j;
    for (u64 i = 0; i < 8; ++i) {
        assert(!xdahm_insert(&map, xhash_fnv1a, &i, 8, value));
    }
    u64 key = 0;
    void *stored = xdahm_get(&map, xhash_fnv1a, &key, 8);
    // A long key also points into the value array being relocated.
    assert(!xdahm_insert(&map, xhash_fnv1a, stored, 129, stored));
    void *found = xdahm_get(&map, xhash_fnv1a, value, 129);
    assert(found && xmemeq(found, value, 40000));
    xdahm_free(&map);
    XDynNodeHashMap node;
    xdnhm_init(&node, 40000);
    assert(!xdnhm_insert(&node, xhash_fnv1a, value, 129, value));
    found = xdnhm_get(&node, xhash_fnv1a, value, 129);
    assert(found && xmemeq(found, value, 40000));
    xdnhm_free(&node);
    xfree(value, 40000);
}

// The same behavioral checks cover both fixed layouts; only node maps promise
// stable value addresses across growth.
#define FIXED_TEST(NAME, MAP, SET, M, S, STABLE)                               \
    static void NAME(HashFn hash, const usize key_sz, const usize val_sz) {    \
        MAP map;                                                               \
        SET set;                                                               \
        M##_init(&map, key_sz, val_sz);                                        \
        S##_init(&set, key_sz);                                                \
        u8 key[129], value[129];                                               \
        xmemzero(key, sizeof key);                                             \
        xmemzero(value, sizeof value);                                         \
        const usize n = key_sz ? 193 : 1;                                      \
        assert(!M##_get(&map, hash, key_sz ? key : 0));                        \
        assert(!S##_contains(&set, hash, key_sz ? key : 0));                   \
        M##_reserve(&map, hash, 0);                                            \
        S##_reserve(&set, hash, 0);                                            \
        void *first = 0;                                                       \
        for (usize i = 0; i < n; ++i) {                                        \
            for (usize j = 0; j < key_sz; ++j)                                 \
                key[j] = (u8)(i + 7 * j);                                      \
            for (usize j = 0; j < val_sz; ++j)                                 \
                value[j] = (u8)(13 * i + j);                                   \
            assert(!M##_insert(&map, hash, key_sz ? key : 0,                   \
                               val_sz ? value : 0));                           \
            assert(!S##_insert(&set, hash, key_sz ? key : 0));                 \
            if (STABLE && i == 0)                                              \
                first = M##_get(&map, hash, key_sz ? key : 0);                 \
            xmemzero(key, sizeof key);                                         \
            xmemzero(value, sizeof value);                                     \
        }                                                                      \
        M##_reserve(&map, hash, 1000);                                         \
        S##_reserve(&set, hash, 1000);                                         \
        const usize capacity = map.bucket_count;                               \
        M##_reserve(&map, hash, 1);                                            \
        assert(map.bucket_count == capacity);                                  \
        for (usize i = 0; i < n; ++i) {                                        \
            for (usize j = 0; j < key_sz; ++j)                                 \
                key[j] = (u8)(i + 7 * j);                                      \
            for (usize j = 0; j < val_sz; ++j)                                 \
                value[j] = (u8)(13 * i + j);                                   \
            void *p = M##_get(&map, hash, key_sz ? key : 0);                   \
            assert(p);                                                         \
            if (val_sz) {                                                      \
                assert((usize)p % 8 == 0 && xmemeq(p, value, val_sz));         \
            }                                                                  \
            if (STABLE && i == 0) {                                            \
                assert(first == p);                                            \
            }                                                                  \
            assert(S##_contains(&set, hash, key_sz ? key : 0));                \
            assert(S##_insert(&set, hash, key_sz ? key : 0));                  \
            assert(M##_insert(&map, hash, key_sz ? key : 0, val_sz ? p : 0));  \
            if (val_sz)                                                        \
                value[0] ^= 0xff;                                              \
            assert(                                                            \
                M##_insert(&map, hash, key_sz ? key : 0, val_sz ? value : 0)); \
            assert(M##_get(&map, hash, key_sz ? key : 0) == p);                \
            if (val_sz) {                                                      \
                assert(xmemeq(p, value, val_sz));                              \
            }                                                                  \
        }                                                                      \
        assert(map.count == n && set.map.count == n);                          \
        if (key_sz) {                                                          \
            key[0] = 254;                                                      \
            assert(!M##_get(&map, hash, key));                                 \
            assert(!S##_contains(&set, hash, key));                            \
        }                                                                      \
        M##_free(&map);                                                        \
        M##_free(&map);                                                        \
        S##_free(&set);                                                        \
        S##_free(&set);                                                        \
        assert(map.count == 0 && map.bucket_count == 0);                       \
        assert(map.key_sz == key_sz && map.val_sz == val_sz);                  \
        assert(!M##_insert(&map, hash, key_sz ? key : 0, val_sz ? value : 0)); \
        assert(!S##_insert(&set, hash, key_sz ? key : 0));                     \
        M##_free(&map);                                                        \
        S##_free(&set);                                                        \
    }
FIXED_TEST(test_fixed_node, XNodeHashMap, XNodeHashSet, xnhm, xnhs, true)
FIXED_TEST(test_fixed_array, XArrayHashMap, XArrayHashSet, xahm, xahs, false)
#undef FIXED_TEST

static void test_fixed_containers(void) {
    const usize sizes[] = {0, 1, 2, 3, 4, 8, 9, 16, 32, 129};
    HashFn hashes[] = {xhash_fnv1a, xhash_xxh3, xhash_rapidhash, zero_hash,
                       array_collision};
    for (usize k = 0; k < sizeof sizes / sizeof *sizes; ++k)
        for (usize v = 0; v < sizeof sizes / sizeof *sizes; ++v)
            for (usize h = 0; h < sizeof hashes / sizeof *hashes; ++h) {
                test_fixed_node(hashes[h], sizes[k], sizes[v]);
                test_fixed_array(hashes[h], sizes[k], sizes[v]);
            }
    for (usize k = 1; k <= 8; k *= 2) {
        test_fixed_node(xhash_zx, k, 8);
        test_fixed_array(xhash_zx, k, 8);
        test_fixed_node(xhash_zxmix, k, 8);
        test_fixed_array(xhash_zxmix, k, 8);
    }
}

static void test_fixed_aliases(void) {
    XArrayHashMap map;
    xahm_init(&map, 16, 16);
    for (u64 i = 0; i < 8; ++i) {
        u64 key[2] = {i, i}, value[2] = {100 + i, 100 + i};
        assert(!xahm_insert(&map, xhash_fnv1a, key, value));
    }
    const usize capacity = map.bucket_count;
    u64 key[2] = {0, 0};
    void *p = xahm_get(&map, xhash_fnv1a, key);
    assert(!xahm_insert(&map, xhash_fnv1a, p, p));
    assert(map.bucket_count > capacity);
    u64 expected[2] = {100, 100};
    p = xahm_get(&map, xhash_fnv1a, expected);
    assert(p && xmemeq(p, expected, sizeof expected));
    xahm_free(&map);
    // Odd-sized keys remain tightly packed; values stay 8-byte aligned.
    xahm_init(&map, 3, 8);
    for (u8 i = 0; i < 8; ++i) {
        u8 k[3] = {i, 0, 0};
        u64 value = i;
        assert(!xahm_insert(&map, xhash_fnv1a, k, &value));
    }
    u64 value = 999;
    u8 candidate[3] = {0, 0, 255};
    assert(!xahm_insert(&map, xhash_fnv1a, candidate, &value));
    u64 *found = xahm_get(&map, xhash_fnv1a, candidate);
    assert(found && *found == value && (usize)found % 8 == 0);
    xahm_free(&map);
}

int main(void) {
    test_hashes();
    test_fixed_containers();
    test_fixed_aliases();
    test_containers(xhash_fnv1a);
    test_containers(xhash_zx);
    test_containers(xhash_zxmix);
    test_containers(xhash_xxh3);
    test_containers(xhash_rapidhash);
    test_containers(collision);
    test_binary_keys();
    test_array_containers(xhash_fnv1a);
    test_array_containers(xhash_zx);
    test_array_containers(xhash_zxmix);
    test_array_containers(xhash_xxh3);
    test_array_containers(xhash_rapidhash);
    test_array_containers(array_collision);
    test_array_binary_keys();
    test_array_aliases();
    test_array_containers(zero_hash);
    test_mixed_keys(xhash_fnv1a);
    test_mixed_keys(xhash_xxh3);
    test_mixed_keys(xhash_rapidhash);
    test_mixed_keys(array_collision);
    test_large_alias();
    xprint("%z\n", "hashmap tests passed");
}
