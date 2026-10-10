---
name: xstdc
description: Write, review, or modify C code that uses xstdc, applying its API contracts, ownership rules, and project conventions. Use for xstdc consumers and work on the library itself.
---

# xstdc

Use this guide when writing code with xstdc. The public headers under
[`include/xstdc`](include/xstdc) are the source of truth for signatures and contracts.
This guide covers the application-facing APIs; implementation helpers exposed in
headers are not intended as entry points. Families below specify the supported
prefixes and suffixes rather than repeating identical descriptions for every type.

Read the sections relevant to the task, using the contents below or searching for
an API name. Check the current headers before relying on a signature or behavior;
the library may have changed since this reference was written. Repository paths
are relative to the xstdc checkout. Apply these conventions within the user's
requested scope.

## Find an API

- [Recommended practices](#recommended-practices), [types](#types--typesh)
- [Allocation](#allocation--alloch), [memory](#raw-memory--memh),
  [strings](#strings--stringh), [printing](#formatting-and-printing--printh)
- [Vectors](#vectors--vech), [queues](#fifo-queues--queueh),
  [linked lists](#linked-lists--linked_listh),
  [hash maps and sets](#hash-maps-sets-and-hashing--hashmaph)
- [Sorting](#sorting--sorth), [streams](#readers-and-writers--streamh),
  [files](#files--fileh)
- [Random generation](#random-generation--rngh),
  [math](#mathematics--mathh), [byte order](#byte-order--endianh)
- [Threads](#threads-and-synchronization--threadh),
  [time and timezones](#time-dates-and-timezones--timeh-timezonesh)
- [Assertions, backtraces, and exit](#assertions-backtraces-and-process-exit),
  [building](#building-and-working-in-this-repository)

## Recommended practices

- Include the specific `xstdc/<feature>.h` headers you use.
- Use `usize` for byte lengths, element counts, and indices. Check size arithmetic
  when sizes come from external input; allocation cannot fix an overflow that
  happened while computing its argument.
- Prefer `xalloc`, `xrealloc`, and `xfree`, retaining the exact allocation size.
  Avoid the managed-size `m` variants when the size is already known: sized
  freeing is preferred and enables size validation in debug builds.
- Prefer `XStringVw` for borrowed text and `XString` for owned text. Use explicit
  lengths, including for slices and embedded NUL bytes. Keep NUL-terminated
  strings at interfaces that require them; do not add terminators to xstdc
  strings merely for printing, comparison, or file paths.
- Use `xprint` instead of `printf`. Pass pointers to the specified types, usually
  `&value`; do not create a compound literal just to take an existing value's
  address. Format strings themselves remain NUL terminated.
- Initialize containers before use. Their element sizes are fixed at initialization.
  They copy bytes, not resources behind pointers: release nested allocations
  yourself before removing elements or freeing the container.
- Do not copy live owning structs as independent owners. Borrow pointers/views
  instead, and observe each container's pointer invalidation rules.
- Reserve capacity when a total count is known. Prefer fixed-key hash containers
  when keys have equal lengths; use dynamic-key variants only when needed.
- Use assertions for programmer preconditions, not recoverable input or I/O
  failures. Check boolean/error returns where failure is part of the API.
- Use the library's memory, stream, time, and threading abstractions where they
  fit. Keep platform-specific calls at backend boundaries.

## Types — `types.h`

| Types | Meaning |
| --- | --- |
| `u8`, `u16`, `u32`, `u64`, `u128` | Unsigned integers of the named bit width. |
| `i8`, `i16`, `i32`, `i64`, `i128` | Signed integers of the named bit width. |
| `usize`, `ssize` | Unsigned/signed size types; currently aliases of `u64`/`i64`. |
| `f32`, `f64` | `float` and `double`. |
| `bool`, `true`, `false` | Boolean type and constants. |

The current code targets a 64-bit environment and uses compiler extensions such
as 128-bit integers. Do not assume it is a portable replacement for every C ABI.

## Allocation — `alloc.h`

Sizes and alignments are **bytes**, not element counts. Ordinary allocations exit
on failure; `xtry_*` functions return NULL instead. Alignment arguments must be
valid allocator alignments (nonzero powers of two).

| Function | Use and requirements |
| --- | --- |
| `xalloc(n)` | Allocate uninitialized bytes; the usual `malloc` replacement. |
| `xallocz(n)` | Allocate zeroed bytes; like `calloc`, but takes one byte count. |
| `xalloca(n, align)`, `xallocza(n, align)` | Aligned versions. These are heap allocations, **not** stack `alloca`. |
| `xrealloc(p, old, new)` | Resize, preserving the common prefix; supply the exact old byte size. May move. |
| `xrealloca(p, old, new, align)` | Resize with the requested alignment. |
| `xfree(p, n)` | Free using the exact current allocation size; NULL is allowed. |
| `xfreea(p, n, align)` | Free an aligned allocation using its size and alignment. |
| `xallocm(n)`, `xalloczm(n)` | Managed-size uninitialized/zeroed allocation. Prefer the sized family. |
| `xallocma(n, align)`, `xalloczma(n, align)` | Managed-size aligned allocations. |
| `xreallocm(p, new)`, `xreallocma(p, new, align)` | Resize without supplying the old size. |
| `xfreem(p)`, `xfreema(p, align)` | Free without supplying the size; NULL is allowed. |

Every allocation/resize variant has a matching try variant with identical
arguments: `xtry_alloc`, `xtry_allocz`, `xtry_alloca`, `xtry_allocza`,
`xtry_allocm`, `xtry_alloczm`, `xtry_allocma`, `xtry_alloczma`, `xtry_realloc`,
`xtry_realloca`, `xtry_reallocm`, and `xtry_reallocma`.
A failed try-resize leaves the original allocation intact: save its result in a
separate pointer before replacing the owner.

```c
#include "xstdc/alloc.h"

void allocation_example(void) {
    usize count = 16;
    i32 *items = xalloc(count * sizeof(*items));
    items[0] = 42;
    items = xrealloc(items, count * sizeof(*items), 32 * sizeof(*items));
    count = 32;
    xfree(items, count * sizeof(*items));
}
```

Keep zero-size allocations in the ownership lifecycle too: they may return a
non-NULL allocation requiring free. Resizing to zero is not a substitute for
freeing.

### Allocation diagnostics

- `xalloc_has_debug()` reports whether tracking is active in the linked library.
- `xalloc_debug_unfreed()` prints remaining live allocations and their last valid
  operation's backtrace. Call after cleanup to check for leaks; it is silent
  without tracking or when none remain.
- Tracking detects invalid frees/resizes, repeated frees, and incorrect supplied
  sizes/alignment. It records addresses on allocation operations and resolves
  symbols only when reporting. It does not replace a memory-access sanitizer for
  detecting out-of-bounds reads/writes.
- Tracking is enabled in unoptimized allocator builds or with
  `XSTDC_ALWAYS_DEBUG_ALLOC`/`XSTDC_DEBUG_ALLOC` when compiling the library.

## Raw memory — `mem.h`

| Function | Meaning / libc counterpart |
| --- | --- |
| `xmemcp(dst, src, n)` | Copy `n` bytes; regions must not overlap. `memcpy`. |
| `xmemmv(dst, src, n)` | Copy `n` bytes; overlap is allowed. `memmove`. |
| `xmemeq(a, b, n)` | Return whether `n` bytes match. Equality only, **not** the ordering result of `memcmp`. |
| `xmemzero(dst, n)` | Zero `n` bytes. `memset(dst, 0, n)`. |

Use these for byte buffers. They neither allocate nor manage ownership.
`xmemcp` currently declares `src` as `void *`; a cast from a const source may be
needed, although the source is not modified.

## Strings — `string.h`

`XString { char *p; usize len; }` owns its bytes.
`XStringVw { const char *p; usize len; }` borrows them. Neither requires a trailing
NUL; `len` counts bytes, not Unicode characters. A null string has `p == NULL`;
check that field when nullness matters. Equality uses only length and byte content:
a null empty view and a non-NULL empty view compare equal.

| Function or macro | Use |
| --- | --- |
| `strl("literal")` | Borrow a literal with its compile-time length. Use with literals/appropriate arrays, **not** `char *` variables. |
| `xstrv_froms(str)`, `stov(str)` | Borrow a view of an owned string, without copying. |
| `xstrv_fromz(z)` | Borrow a NUL-terminated external string, scanning for its length. NULL produces a null view. |
| `xstr_fromz(z)` | Allocate a copy of a NUL-terminated external string, without copying its terminator. NULL produces a null string. |
| `xstrv_eq(a, b)` | Compare lengths and bytes, including embedded NULs. |
| `xstrv_starts_with(text, prefix)` | Test a byte prefix; an empty prefix always matches. |
| `xstrv_end_with(text, suffix)` | Test a byte suffix; an empty suffix always matches. |
| `xstrv_concat(a, b)` | Allocate an independent concatenation. Inputs remain borrowed. |

Free an owned string using `xfree(s.p, s.len)`, including a non-NULL empty string.
Never free a borrowed view. For a slice, construct `(XStringVw){pointer, length}`
and keep the underlying storage alive. There is no separate string-free function.

```c
#include "xstdc/string.h"
#include "xstdc/print.h"

void string_example(void) {
    XString message = xstrv_concat(strl("hello, "), strl("world"));
    XStringVw first = {message.p, 5};
    xprint("%s: %s\n", &first, &message);
    xfree(message.p, message.len);
}
```

### Numeric prefix parsing

`u8parse`, `u16parse`, `u32parse`, `u64parse`, `u128parse`,
`i8parse`, `i16parse`, `i32parse`, `i64parse`, `i128parse`,
`usizeparse`, `ssizeparse`, `f32parse`, and `f64parse` take
`(XStringVw text, <type> *out)` and return the number of bytes consumed as `usize`. They parse a decimal prefix
within the view's length. No whitespace is skipped; '+' is accepted, while '-'
is accepted only for signed integers and floats. At least one digit is required.

Floats accept a decimal point and a complete e/E exponent; an incomplete exponent
is ignored (`12e+` parses as 12). `123a` parses as 123; `123.1......` as 123.1;
`123..1` as 123. Integer parsing stops before a decimal point or exponent.
Hex, infinity, and NaN syntax are not supported. Float parsing uses a dot regardless
of locale, follows libc rounding, and permits subnormals/underflow to signed zero.

Return zero for no numeric prefix or overflow, leaving `*out` unchanged.
The count includes the sign, decimal point, and any complete exponent.
For example: `f64 value; usize consumed = f64parse(strl("123.1tail"), &value);`.
Only the prefix is checked: success does not mean the entire view is numeric.

`xboolparse(XStringVw text, bool *out)` accepts the case-sensitive prefixes
`true`, `false`, `1`, and `0`, returning 4, 5, 1, and 1 bytes consumed respectively.
It does not skip whitespace or accept signs. Failure returns zero and leaves
`*out` unchanged; trailing bytes are permitted, as with numeric parsing.

## Formatting and printing — `print.h`

Unlike `printf`, formatted values are passed **by pointer**. `%z` alone takes a
character pointer directly. Use a matching type; the formatter cannot validate
whether a pointer actually points to that type.

| Specifier | Argument |
| --- | --- |
| `%u8`, `%u16`, `%u32`, `%u64`, `%u128` | Pointer to the matching unsigned type. |
| `%i8`, `%i16`, `%i32`, `%i64`, `%i128` | Pointer to the matching signed type. |
| `%usz`, `%isz` | Pointer to `usize` / `ssize`. |
| `%f32`, `%f64` | Pointer to the matching floating type. |
| `%s` | Pointer to `XString` or `XStringVw`. |
| `%c` | Pointer to one character. |
| `%b` | Pointer to one byte: zero prints `false`, nonzero `true`. |
| `%z` | NUL-terminated character pointer; intended for compatibility. |
| `%%` | Literal percent; no argument. |

```c
#include "xstdc/print.h"

void print_example(void) {
    i64 number = -42;
    f64 ratio = 1.25;
    bool ready = true;
    XStringVw label = strl("result");
    xprint("%s: %i64, ratio=%f64(r=2), ready=%b\n",
           &label, &number, &ratio, &ready);
}
```

| Function | Destination and result |
| --- | --- |
| `xprint(format, ...)` | stdout; returns bytes written. |
| `xprint_err(format, ...)` | stderr; returns bytes written. |
| `xprint_writer(writer, format, ...)` | Any `XWriter`; returns bytes accepted. Does not flush/close. |
| `xprint_sz(format, ...)` | No output; returns the full required byte count. |
| `xprint_buf(buf, capacity, format, ...)` | Writes only what fits; returns a borrowed view of bytes written. No trailing NUL. |
| `xprint_str(format, ...)` | Returns a new owned string; free with `xfree(result.p, result.len)`. |

The size-returning functions use `(usize)-1` for errors; string/view functions use
`{NULL, 0}`. Failures may leave partial output. A truncated `xprint_buf` result is
not itself an error; use `xprint_sz` if the full size matters. Format and arguments
must not overlap the output buffer. Null argument pointers are errors, but a
valid string/view object containing `{NULL, 0}` formats as empty.

Attributes go after the specifier, in parentheses:

| Attribute | Meaning |
| --- | --- |
| `x` | Lowercase hex without a prefix; integers or two hex digits per string byte. Not for floats/bools. |
| `r=N` | Float digits after the decimal point, e.g. `%f64(r=3)`. Default floats use `%g`-style formatting. |
| `e=N` | Extend to at least `N` bytes. Default padding is spaces on the left. |
| `lp=C`, `rp=C` | Padding byte on the left/right, e.g. `lp='0'`. Both together center the field. |
| `t=N` | Keep at most the first `N` bytes of the final padded field. |

Example: `%u64(x,lp='0',e=16)` prints an integer in a field of at least 16 hex
characters. Widths/truncation are byte-based, including for UTF-8. Float formatting
honors the current rounding mode and locale decimal separator.

## Vectors — `vec.h`

`XVec` is an owned contiguous array of fixed-size elements. `p` is its storage,
`sz` its live element count, and `reserved` its capacity in elements. Use it when
you need indexing, iteration over contiguous storage, or a growable array.

| Function | Use / requirements |
| --- | --- |
| `xvec_init(v, elem_sz)` | Initialize empty; `elem_sz > 0`. |
| `xvec_reserve(v, n)` | Ensure capacity for `n` total elements; does not change `sz`. |
| `xvec_append(v, value)`, `xvec_appendn(v, values, n)` | Append copies of one/many elements. |
| `xvec_insert(v, idx, value)`, `xvec_insertn(v, idx, values, n)` | Insert before `idx`; `idx <= sz`, so insertion at the end is valid. |
| `xvec_at(v, idx)` | Return an element pointer; `idx < sz`. |
| `xvat(v, T, idx)` | Typed lvalue shorthand for `*(T *)xvec_at(v, idx)`. |
| `xvec_pop(v)` | Remove the last element; requires nonempty vector; returns no value. |
| `xvec_remove(v, idx)` | Remove one element, shifting the suffix. |
| `xvec_removen(v, begin, end)` | Remove the half-open range `[begin, end)`. |
| `xvec_shrink(v)` | Reduce capacity to a growth step fitting `sz`; free storage if empty. |
| `xvec_free(v)` | Free storage, set length/capacity to zero, retain element size for reuse. |

Growth/shrink can invalidate all pointers. Insert/remove can change which element
an address refers to. Append/insert support input drawn from the vector's live
elements. Popping/removing does not automatically shrink or free nested resources.
Capacity doubles up to `XVEC_GROWTH_BYTES` (default 64 MiB), then grows in chunks
of that byte size, rounded to whole elements.

```c
#include "xstdc/vec.h"
#include "xstdc/print.h"

void vector_example(void) {
    XVec values;
    xvec_init(&values, sizeof(i32));
    i32 initial[] = {3, 1, 2};
    xvec_appendn(&values, initial, 3);
    i32 *first = xvec_at(&values, 0);
    xprint("first=%i32, count=%usz\n", first, &values.sz);
    xvec_free(&values);
}
```

## FIFO queues — `queue.h`

Choose `XQueue` for a ring buffer; choose `XLinkedQueue` when live element pointers
must survive pushes. Both copy fixed-size elements and expose `sz`.

| Ring function | Linked function | Use |
| --- | --- | --- |
| `xqueue_init(q, elem_sz)` | `xlqueue_init(q, elem_sz)` | Initialize empty; nonzero element size. |
| `xqueue_push(q, elem)` | `xlqueue_push(q, elem)` | Append a copy at the back. Ring input must not point into its own allocation. |
| `xqueue_peek(q)` | `xlqueue_peek(q)` | Borrow the oldest element, or NULL if empty. |
| `xqueue_pop(q)` | `xlqueue_pop(q)` | Remove and return the oldest element pointer, or NULL if empty. |
| `xqueue_free(q)` | `xlqueue_free(q)` | Release all storage, including retired linked nodes; retain element size. |

`xqueue_reserve(q, n)` reserves room for `n` total elements.
`xqueue_shrink(q)` halves capacity when live elements fit, or frees it if empty;
it returns the backing buffer and can invalidate all pointers. Ring growth
uses the same double-then-chunk policy as vectors, controlled by
`XQUEUE_GROWTH_BYTES` (default 64 MiB).

**Pops do not invalidate returned pointers, even across subsequent pops.** A push
can overwrite/reuse popped storage; ring growth can also move live elements.
Linked queues keep live element addresses stable, but a push may reuse a popped
node. Copy a popped value out before a later push if you need to retain it.

## Linked lists — `linked_list.h`

`XLinkedList` is singly linked; `XDoublyLinkedList` supports movement/removal in
both directions. They own copies of fixed-size elements, have `sz`, and preserve
live element pointers until that node is removed or the list is freed. Elements
have 8-byte alignment; structural mutations invalidate iterators.

A `node` argument below means the **element pointer** returned by an iterator,
not the metadata node pointer. It must be non-NULL and belong to the list.

| Singly linked | Doubly linked | Use |
| --- | --- | --- |
| `xllist_init(list, elem_sz)` | `xdllist_init(list, elem_sz)` | Initialize with a nonzero element size. |
| `xllist_append(list, elem)` | `xdllist_append(list, elem)` | Copy an element to the end. |
| `xllist_prepend(list, elem)` | `xdllist_prepend(list, elem)` | Copy an element to the front. |
| `xllist_insertafter(list, node, elem)` | `xdllist_insertafter(list, node, elem)` | Insert after a live node. |
| `xllist_pop_front(list)` | `xdllist_pop_front(list)` | Remove/free the first node; list must be nonempty. No value is returned. |
| `xllist_free(list)` | `xdllist_free(list)` | Free all nodes, retaining element size for reuse. |

Additional removal/insertion operations:

- `xllist_removeafter(list, node)`: remove/free the successor; both node and its
  successor must exist. NULL does not mean “remove the head.”
- `xdllist_remove(list, node)`: remove/free this node.
- `xdllist_insertb4(list, node, elem)`: insert before this node.

Iteration:

- `xllit_init(iter, list)` initializes `XLinkedListIter` before the head;
  `xllit_forward(iter)` returns successive element pointers, then NULL.
- `xdllit_init_head(iter, list)` / `xdllit_init_tail(iter, list)` initialize
  `XDoublyLinkedListIter` before the head / after the tail.
- `xdllit_forward(iter)` / `xdllit_backward(iter)` return the next/previous
  element or NULL at the boundary. This is a cursor between elements: reversing
  direction returns the last visited element again.

## Hash maps, sets, and hashing — `hashmap.h`

Use maps for key-to-value lookup and sets for membership. All keys compare by
byte content. All containers copy their keys and values; maps have a fixed value
size even in the dynamic-key variants. There is currently no public erase or
iterator API.

| Container | Prefix | Key sizing / pointer lifetime |
| --- | --- | --- |
| `XArrayHashMap` | `xahm` | Fixed keys; growth invalidates value pointers. |
| `XArrayHashSet` | `xahs` | Fixed keys; array storage. |
| `XNodeHashMap` | `xnhm` | Fixed keys; value pointers survive growth. |
| `XNodeHashSet` | `xnhs` | Fixed keys; node storage. |
| `XDynArrayHashMap` | `xdahm` | Variable-length keys; growth invalidates value pointers. |
| `XDynArrayHashSet` | `xdahs` | Variable-length keys; array storage. |
| `XDynNodeHashMap` | `xdnhm` | Variable-length keys; value pointers survive growth. |
| `XDynNodeHashSet` | `xdnhs` | Variable-length keys; node storage. |

Each prefix has `_init`, `_reserve`, `_insert`, and `_free`. Maps also have `_get`;
sets have `_contains`. In the signatures below, `p` is any prefix of the indicated
kind, `c` is its container pointer, and `hash` is a `HashFn`:

| Operation | Fixed keys | Dynamic keys |
| --- | --- | --- |
| Map initialization | `p_init(c, key_size, value_size)` | `p_init(c, value_size)` |
| Set initialization | `p_init(c, key_size)` | `p_init(c)` |
| Reserve | `p_reserve(c, hash, total_count)` | Same. |
| Map insertion/update | `p_insert(c, hash, key, value)` | `p_insert(c, hash, key, key_len, value)` |
| Map lookup | `p_get(c, hash, key)` | `p_get(c, hash, key, key_len)` |
| Set insertion | `p_insert(c, hash, key)` | `p_insert(c, hash, key, key_len)` |
| Membership | `p_contains(c, hash, key)` | `p_contains(c, hash, key, key_len)` |
| Cleanup | `p_free(c)` | Same. |

Insertion returns **true if the key already existed**, false for a new key.
Map insertion replaces an existing value. Lookup returns a borrowed value pointer
or NULL if absent; membership returns bool. Free releases storage and retains
sizes for reuse; repeated free is supported. Map `count` is the number of entries;
sets expose it through `set.map.count`.

Values are aligned to 8 bytes; do not use types needing greater alignment directly.
Use the same deterministic hash function on every operation, including reserve.
A NULL key is allowed only for a zero-length key. A zero-sized map value has only
a presence marker, not dereferenceable value bytes. For struct keys, initialize
padding or serialize the key: byte equality is not field-by-field equality.

`HashFn` is `u64 (*)(const void *key, usize key_len)`.

| Hash function | Useful for / requirements |
| --- | --- |
| `xhash_zx` | Integer passthrough; requires 1, 2, 4, or 8 bytes. Does not mix patterned keys. |
| `xhash_zxmix` | Mixed integer keys; requires 1, 2, 4, or 8 bytes. |
| `xhash_fnv1a` | General byte-string FNV-1a hash. **slow** |
| `xhash_xxh3` | General byte-string XXH3-64 hash with seed zero. **fast** |
| `xhash_rapidhash` | General byte-string rapidhash with a fixed seed. **very fast** |

These are noncryptographic hashes. Hash string contents using `view.p, view.len`,
not the bytes of the `XStringVw` struct (which would hash the pointer).

```c
#include "xstdc/hashmap.h"
#include "xstdc/print.h"

void hashmap_example(void) {
    XArrayHashMap map;
    xahm_init(&map, sizeof(u64), sizeof(i32));
    xahm_reserve(&map, xhash_zxmix, 100);
    u64 key = 7;
    i32 value = 42;
    bool existed = xahm_insert(&map, xhash_zxmix, &key, &value);
    i32 *found = xahm_get(&map, xhash_zxmix, &key);
    if (found)
        xprint("value=%i32, replaced=%b\n", found, &existed);
    xahm_free(&map);
}
```

For string keys, for example:
`xdahm_init(&map, sizeof(i32))`, then
`xdahm_insert(&map, xhash_xxh3, key.p, key.len, &value)`.

## Sorting — `sort.h`

All sorts write the result back into the supplied array. Sizes count **elements**;
`elemsz` is bytes per element. Empty arrays may be NULL.

`CompareFn` is `bool (*)(void *a, void *b)`. Return **true when `a` belongs after
`b`**: `a > b` gives ascending order. Return false for equal elements and define
a consistent strict ordering. This is not `qsort`'s negative/zero/positive
comparator; do not pass a `qsort` comparator directly.

| Function | Use |
| --- | --- |
| `xsort_cmp(arr, elemsz, count, cmp)` | Stable full comparison sort; preserves equal-element order. |
| `xsort_cmp_u(arr, elemsz, count, cmp)` | Unstable full comparison sort; equal elements may reorder. Closest role to `qsort`. |
| `xpsort_cmp(arr, elemsz, count, partsz, cmp)` | Stably select/sort the first `partsz` elements under the ordering; preserve the remaining suffix's relative order. |
| `xpsort_cmp_u(arr, elemsz, count, partsz, cmp)` | Unstable partial sort; suffix need not be sorted. |
| `xsort_radix_i32(arr, count)`, `xsort_radix_i64(arr, count)` | Stable ascending radix sort of signed integers. |
| `xsort_radix_u32(arr, count)`, `xsort_radix_u64(arr, count)` | Stable ascending radix sort of unsigned integers. |

Partial sorts require `partsz <= count`. Stable and radix sorts can allocate
storage proportional to the array size. Unstable sorts use one element of scratch
and logarithmic stack space. Choose stability based on equal-key ordering needs,
not an assumption that unstable must always be faster.

```c
#include "xstdc/sort.h"

static bool i32_after(void *a, void *b) {
    return *(const i32 *)a > *(const i32 *)b;
}

void sorting_example(void) {
    i32 values[] = {4, -2, 4, 1};
    xsort_cmp(values, sizeof(values[0]), 4, i32_after);
    // For plain integers, this is also available:
    xsort_radix_i32(values, 4);
}
```

## Readers and writers — `stream.h`

`XReader` abstracts exact reads and peeks; `XWriter` abstracts exact writes.
Concrete adapters expose `.base`, which you pass to these operations:

| Function | Contract |
| --- | --- |
| `xreader_read(reader, dst, n)` | Transfer exactly `n` bytes and advance; false on failure. |
| `xreader_peek(reader, dst, n)` | Transfer `n` bytes without advancing; false on failure. |
| `xwriter_write(writer, src, n)` | Transfer `n` bytes; false on failure. |
| `xbuf_reader(p, len)` | Construct an `XBufReader` borrowing fixed input storage, with `pos = 0`. |
| `xbuf_writer(p, len)` | Construct an `XBufWriter` borrowing fixed writable storage, with `pos = 0`. |

Buffer adapters never allocate/grow. If insufficient bytes remain, they return
false without modifying position or the destination buffer. Overlap is allowed;
zero-byte operations allow NULL buffers. Other backends may make partial progress
on failure: do not assume every reader/writer has the buffer adapter's rollback
behavior.

`xbuf_reader_read`, `xbuf_reader_peek`, and `xbuf_writer_write` are the buffer
callbacks with the corresponding base-interface signatures. Usually use the
`xreader_*`/`xwriter_*` dispatch functions instead. Custom adapters implement
`XReaderVTable` / `XWriterVTable` and keep the adapter alive during calls.

```c
#include "xstdc/stream.h"
#include "xstdc/print.h"

void buffer_writer_example(void) {
    char bytes[64];
    XBufWriter writer = xbuf_writer(bytes, sizeof(bytes));
    u32 number = 123;
    usize written = xprint_writer(&writer.base, "number=%u32", &number);
    if (written != (usize)-1) {
        XStringVw text = {bytes, writer.pos};
        xprint("%s\n", &text);
    }
}
```

## Files — `file.h`

`XFile` owns an open descriptor and its buffers. Paths are `XStringVw`; no
terminator is needed. Use a valid path without embedded NULs. Operations return
true on success and false on failure unless otherwise indicated.

| Function | Use / requirements |
| --- | --- |
| `xfile_open(out, path, mode)` | Open; leave `out` unchanged on failure. Needs READ and/or WRITE. |
| `xfile_free(file)` | Flush, close, and release buffers. Invalidates the file even on error; does not free the `XFile` struct. |
| `xfile_read(file, dst, n)` | Read exactly `n` bytes with read-ahead caching; false on error or early EOF. |
| `xfile_write(file, src, n)` | Buffer `n` bytes; success means accepted, not persisted. |
| `xfile_read_unbuffered(file, dst, n)` | Exact read through OS calls after synchronizing buffered state. |
| `xfile_write_unbuffered(file, src, n)` | Exact write through OS calls after synchronizing buffered state. |
| `xfile_flush(file)` | Send pending writes; retain unwritten bytes after failure for retry. Does not promise disk durability. |
| `xfile_peek(file, dst, n)` | Read without changing position, including on failure; requires a seekable file. |
| `xfile_size(file, out_u64)` | Query regular-file size without moving position; output unchanged on failure. |
| `xfile_read_all(file, dst)` | Read a regular file from its start into caller-provided storage large enough for its current size. |
| `xfile_set_pos(file, pos)` | Seek to an absolute byte position; successful seeks discard cached input. |
| `xfile_get_pos(file, out_u64)` | Query the logical byte position; output unchanged on failure. |

Mode bits: `XFILE_READ`, `XFILE_WRITE`, `XFILE_CREATE`, `XFILE_TRUNCATE`.
CREATE preserves an existing file; add TRUNCATE to replace its contents. TRUNCATE
requires WRITE. Buffered I/O uses `XFILE_BUFSIZE` (4096 bytes); non-write operations
flush pending writes. Reads/writes may make partial progress before returning
false, and the API does not report the partial byte count. Synchronize concurrent
operations that share the file position.

`xfile_writer(file)` returns `XFileWriter`; `xfile_reader(file)` returns
`XFileReader`. Both borrow the file, which must remain open. Their callbacks are
`xfile_writer_write`, `xfile_reader_read`, and `xfile_reader_peek`; use `.base` with
stream functions or `xprint_writer`.

```c
#include "xstdc/file.h"
#include "xstdc/alloc.h"
#include "xstdc/print.h"

bool read_file_example(void) {
    XFile file;
    if (!xfile_open(&file, strl("input.txt"), XFILE_READ))
        return false;
    u64 size;
    if (!xfile_size(&file, &size)) {
        xfile_free(&file);
        return false;
    }
    // The file must not grow between sizing the buffer and read_all.
    XString contents = {xalloc(size), size};
    bool ok = xfile_read_all(&file, contents.p);
    bool closed = xfile_free(&file);
    if (ok)
        xprint("%s", &contents);
    xfree(contents.p, contents.len);
    return ok && closed;
}
```

This is the `fopen`/`fread`/`fwrite`/`fclose` role, with byte-counted exact I/O and
boolean status rather than libc's item counts and `FILE *`.

## Random generation — `rng.h`

Generators own mutable state in caller-provided structs. Seed before generating;
use separate states per thread or synchronize access. Equal seeds reproduce
streams. Buffer outputs must not overlap state.

| Name in `xrng_<name>_*` | State type | Intended use |
| --- | --- | --- |
| `xoshiro256pp` | `XRngXoshiro256ppState` | General-purpose pseudorandom generation. |
| `xoshiro256pp16` | `XRngXoshiro256pp16State` | Larger 16-lane state for high-throughput generation. |
| `xoroshiro128pp` | `XRngXoroshiro128ppState` | General-purpose generation with smaller state. |
| `wyrand` | `XRngWyrandState` | Small state and low latency. |
| `splitmix64` | `XRngSplitMix64State` | Seed expansion and simple word generation. |
| `chacha20` | `XRngChaCha20State` | ChaCha20 stream; security requires an unpredictable 32-byte seed. |

Every generator above has all four operations:

- `xrng_<name>_seed(state, seed)`: initialize; `seed` is `u64` except ChaCha20,
  where it points to 32 bytes.
- `xrng_<name>_u64(state)`: generate one word and advance.
- `xrng_<name>_buf(state, dst, n)`: fill bytes; `n == 0` leaves state unchanged.
- `xrng_<name>_u64range(state, min, max)`: unbiased result in `[min, max)`;
  requires `min < max`. Prefer this to `% range` when bias matters.

Hardware equivalents need RDRAND support and have no state/seed argument:
`xrng_hw_u64()`, `xrng_hw_buf(dst, n)`, `xrng_hw_u64range(min, max)`.
`xrng_f32range(bits, min, max)` and `xrng_f64range(bits, min, max)` map a supplied
`u64` to `[min, max)`; require finite `min < max`.

```c
#include "xstdc/rng.h"
#include "xstdc/print.h"

void rng_example(void) {
    XRngXoshiro256ppState rng;
    xrng_xoshiro256pp_seed(&rng, 1234);
    u64 die = xrng_xoshiro256pp_u64range(&rng, 1, 7);
    xprint("die=%u64\n", &die);
}
```

The ordinary generators are for sampling, simulation, tests, and randomized
algorithms, not secrets. Seeding ChaCha20 with predictable bytes does not make
secure randomness. Scalar buffer fills discard unused word bytes; ChaCha20 and
the 16-lane generator preserve buffered bytes across mixed word/buffer calls.

## Mathematics — `math.h`

Names use a type prefix with no underscore, for example `u64sqrt`, `i32cbrt`,
`f64sin`. In this section `U` means any of `u8/u16/u32/u64/u128`, `I` means any of
`i8/i16/i32/i64/i128`, and `F` means `f32/f64`.

| Function family | Semantics / requirements |
| --- | --- |
| `Umin(a,b)`, `Imin(a,b)`, `Fmin(a,b)` | Smaller operand. Floating variants use the compiler’s `fmin[f]` builtin. |
| `Umax(a,b)`, `Imax(a,b)`, `Fmax(a,b)` | Larger operand. Floating variants use the compiler’s `fmax[f]` builtin. |
| `Iabs(x)`, `Fabs(x)` | Absolute value; integer versions return the matching **unsigned** type. Avoid inputs whose signed negation overflows. |
| `Usqrt(x)` | Floor square root. `u128sqrt` returns `u64`; other versions return their input type. |
| `Fsqrt(x)` | Floating square root; `sqrtf`/`sqrt` role. |
| `Ulog2(x)`, `Ulog10(x)` | Floor logarithm, returned as `u8`; require `x > 0`. |
| `Ulogn(x,n)` | Floor base-`n` logarithm, returned as `u8`; requires `x > 0`, `n >= 2`. |
| `Upow(x,e)`, `Ipow(x,e)` | Integer power; exponent is `u8`, result has base's type. |
| `Ucbrt(x)`, `Icbrt(x)` | Floor real cube root, including signed negative inputs. |
| `Unthroot(x,n)`, `Inthroot(x,n)` | Floor nth root; degree has the same type as `x`. Requires `n > 0`; negative `x` requires odd `n`. |
| `Fsin(x)`, `Fcos(x)`, `Ftan(x)` | Trigonometric functions in radians. |
| `Fsinh(x)`, `Fcosh(x)`, `Ftanh(x)` | Hyperbolic sine/cosine/tangent, using the corresponding libm functions. |
| `Fasinh(x)`, `Facosh(x)`, `Fatanh(x)` | Inverse hyperbolic functions; real domains are all real x, x >= 1, and -1 < x < 1, respectively. |
| `Fhypot(x,y)` | Euclidean length, using libm to avoid intermediate overflow/underflow. |
| `Uhypot(x,y)`, `Ihypot(x,y)` | Exact floor Euclidean length; returns the matching **unsigned** type. All signed inputs are supported; unsigned inputs require the floor result to fit. |
| `Fln(x)`, `Flog2(x)`, `Flog10(x)` | Natural/base-2/base-10 logs; `log[f]`, `log2[f]`, `log10[f]` role. |
| `Flogn(x,n)` | Base-`n` log; ordinary real domain is `x > 0`, `n > 0`, `n != 1`. |
| `Fexp(x)` | Exponential, `e^x`. |
| `Fgamma(x)` | Gamma function; `tgamma[f]` role, not log-gamma. |
| `Fpow(x,e)` | Floating power; `pow[f]` role. |
| `Fcbrt(x)` | Real cube root, including negatives; `cbrt[f]` role. |
| `Fnthroot(x,n)` | Nth root; finite nonzero degree, negative input requires an odd integer degree. |
| `Fmod(x,y)` | Floating remainder with the sign of `x`, including signed zero; `fmod[f]` role, not nonnegative modulo. |

Integer results are floored, not truncated toward zero: `i32cbrt(-9)` is `-3`.
Integer domains are unchecked preconditions; callers must keep results in range
when they need defined mathematical answers. There are no signed integer square
root/log families and no unsigned absolute-value family.

Floating transcendental accuracy follows platform libm. Composite `logn`/`nthroot`
are not guaranteed correctly rounded; do not promise a universal few-ULP bound.
Use [`tools/math_accuracy.c`](tools/math_accuracy.c) for MPFR comparisons
(`env fast=1 xmm build/math_accuracy.out`; requires MPFR and GMP).
`XMATH_MIN`, `XMATH_MAX`, and `XMATH_ABS` macros also exist, but prefer typed
functions: the macros can evaluate arguments more than once.

## Byte order — `endian.h`

For each width `N = 16, 32, 64, 128`:

| Function | Meaning |
| --- | --- |
| `xbswapN(x)` | Reverse all bytes of a `uN` integer. |
| `xhN_tole(x)`, `xhN_tobe(x)` | Host order to little/big endian representation. |
| `xleN_toh(x)`, `xbeN_toh(x)` | Little/big endian representation to host order. |

Examples: `xh32_tobe(value)` for a big-endian protocol field and
`xle64_toh(encoded)` for a little-endian integer read from a file.
These convert integer values; they do not load/store buffers. Copy an unaligned
field into an integer with `xmemcp` before converting. The big-endian 16/32-bit
operations fill the `htons`/`htonl`/`ntohs`/`ntohl` role. No 8-bit swap is needed.

## Threads and synchronization — `thread.h`

| Type / function | Use / requirements |
| --- | --- |
| `XLock lock = {0}` | Nonrecursive ticket spinlock for short critical sections. Zero-initialize. |
| `xlock_lock(lock)` | Acquire, spinning with a CPU pause hint. |
| `xlock_lock_fast(lock)` | Acquire using unpaused spinning; useful for extremely short critical sections. |
| `xlock_try_lock(lock)` | Return true only if acquired. |
| `xlock_unlock(lock)` | Release; only the owner may call it. |
| `XMutex`, `xmtx_init(mtx)` | Initialize a nonrecursive blocking mutex for longer critical sections. |
| `xmtx_lock(mtx)`, `xmtx_try_lock(mtx)`, `xmtx_unlock(mtx)` | Acquire/try/release; try returns true on acquisition. |
| `xmtx_free(mtx)` | Release mutex resources after all users/waiters are done. |
| `XThread`, `xthread_start(t, fn, arg)` | Run `void fn(void *)`; false on start failure, clearing `t`. Argument storage must outlive its use. |
| `xthread_join(t)` | Wait and release resources; exactly once per successful start. |

Spinlock acquisition/release provides acquire/release ordering. No lock is
recursive. Do not copy/move initialized mutexes or locks in use. Do not copy a
live thread, self-join, or join it concurrently. Protect shared mutable containers
and RNG states when used by multiple threads. There is no public condition
variable, detached-thread, or general atomic wrapper API.

## Time, dates, and timezones — `time.h`, `timezones.h`

Keep the three notions distinct:

| Type / clock | Meaning |
| --- | --- |
| `XTimestamp {seconds, nanos}` | Continuous TAI seconds since 1970-01-01 00:00:00 **TAI**. Not a Unix timestamp. |
| `XPosixTimestamp {seconds, nanos}` | Unix/POSIX seconds since the UTC epoch, excluding leap seconds. Compatibility input. |
| `XDuration {seconds, nanos}` | Signed elapsed duration with normalized nanos. `{-1, 500000000}` means minus half a second. |
| `XDateTime` | `year, month, day, hours, minutes, seconds, nanos, tz, index`; calendar representation. |
| `xtime_nanos()` | Monotonic elapsed-time counter with unspecified origin; excludes suspend. |
| `xtime_cycles()` | Raw TSC ticks for timing; not necessarily current core clock cycles. |

For all timestamp/duration types, `nanos` must be in `0..999999999`.
Calendar months are 1..12 and days start at 1. `index` selects the occurrence of
an ambiguous local time (0 for the first, 1 for the second); it does not make a
nonexistent local time valid.

| Function | Use / requirements |
| --- | --- |
| `xtimestamp()` | Current TAI timestamp; Linux clock must have the correct TAI–UTC offset configured. Clock failure asserts. |
| `xdate_from_posix(ts)` | POSIX timestamp to UTC date (`XTZ_UTC`, index 0). Cannot produce a leap second. |
| `xtimestamp_from_date(dt, out)` | Existing calendar date to TAI; false with unchanged output if invalid/nonexistent. Supports leap seconds. |
| `xdate_from_ts(ts, timezone)` | TAI timestamp to calendar date in the selected timezone. |
| `xtimestamp_sub_dur(ts, dur)` | Subtract elapsed duration in place; invalid normalization/overflow asserts. |
| `xdate_sub_days(dt, days)` | Calendar-day subtraction preserving clock time, zone, and occurrence index. Result may be nonexistent around timezone transitions. |
| `xdate_sub_dur(dt, dur)` | Elapsed-duration subtraction through TAI; both input/result must be convertible. |
| `xnanosleep(nanos)` | Sleep at least the requested interval, retrying interruptions. |

Use `xtime_nanos()` differences for elapsed durations, not wall-clock dates.
Calendar-day arithmetic can differ from 24 hours across DST changes. UTC
conversion supports 1960 onward; 1960–1971 uses rounded historical rate changes,
and conversions are exact from 1972 within the recorded leap-second model.
Future dates assume the last recorded TAI–UTC offset until data is updated.
Calendar years must fit `i32`.

### Timezone lookup

`XTimezoneId` is the generated enum in `timezones.h`, with named IDs including
`XTZ_UTC` and `XTZ_EUROPE_ZURICH`; `XTZ_COUNT` is the count, not a valid zone.
Use names/enum constants, not hardcoded numeric IDs. The header lists all zone
names and aliases.

- `xtimezone_from_name(view, out_id)` performs case-sensitive lookup of an
  `XStringVw`, including aliases; false leaves the output unchanged.
- `xtimezone(id)` returns an immutable, library-owned `XTimezone` descriptor.
- `xtimezone_at_unix(id, unix_seconds)` returns `XTimezoneOffset` at **POSIX**
  seconds: `utc_off_s` (seconds east of UTC), `is_dst`, and an abbreviation.
- `xtimezone_version` identifies the bundled timezone data version.

Descriptors expose transition/type tables and future rules through
`XTimezoneTransition`, `XTimezoneRules`, and `XTimezoneRule`. Prefer lookup
functions to interpreting these tables. Do not free/modify descriptors or their
pointed-to data. Abbreviations are display labels, not unique zone identifiers.

```c
#include "xstdc/time.h"

bool time_example(XTimestamp *out) {
    XTimezoneId zone;
    if (!xtimezone_from_name(strl("Europe/Zurich"), &zone))
        return false;
    XDateTime local = {.year = 2026, .month = 6, .day = 1,
                      .hours = 12, .tz = zone};
    return xtimestamp_from_date(local, out);
}
```

## Assertions, backtraces, and process exit

### `assert.h`

- `assert(condition)` reports and terminates on a violated precondition in debug
  builds. In optimized builds without `XSTDC_ALWAYS_ASSERT`, a false condition
  reaches a compiler unreachable assumption: it is undefined behavior.
- `unreachable()` marks a path that must never execute; debug builds report it.
- `xstdc_assert_fail(line, file, func)` and
  `xstdc_unreachable_reached(line, file, func)` are the reporting functions used
  by those macros; normally call the macros for automatic source locations.

Keep state-changing work outside assertion expressions. To preserve checks in
optimized tests, build with `XSTDC_ALWAYS_ASSERT`.

### `debug.h`

`XBacktrace` holds up to two caller frames; `XFullBacktrace` holds an arbitrary-depth
trace. Each `XBacktraceFrame` has an address and an optional owned symbol string.
Capture stores addresses only.

| Short trace | Full trace | Purpose |
| --- | --- | --- |
| `xbacktrace()` | `xbacktrace_full()` | Capture caller-outward addresses. |
| `xbacktrace_symbolize(trace)` | `xbacktrace_full_symbolize(trace)` | Resolve missing names; repeated calls retain existing names. |
| `xbacktrace_print(trace)` | `xbacktrace_full_print(trace)` | Print to stderr; unresolved frames print as addresses. Does not consume. |
| `xbacktrace_free(trace)` | `xbacktrace_full_free(trace)` | Release owned storage and clear; zero-initialized traces are valid. |

Capture/symbolization can allocate and are not signal-handler-safe. Optimized
inlining/tail calls may omit frames. Link with `-rdynamic` for executable symbols
on ELF. Symbolize only when names are needed, especially in frequently executed
diagnostic bookkeeping.

```c
#include "xstdc/debug.h"

void backtrace_example(void) {
    XFullBacktrace trace = xbacktrace_full();
    xbacktrace_full_symbolize(&trace);
    xbacktrace_full_print(&trace);
    xbacktrace_full_free(&trace);
}
```

### `process.h`

- `xexit(code)`: terminate, running exit handlers and flushing libc streams;
  equivalent role to `exit`.
- `xexit_hard(code)`: terminate immediately without those actions;
  equivalent role to `_Exit`/`_exit`.

Neither replaces explicit cleanup of xstdc owners. Flush/close `XFile` buffers
before exiting when their output must be delivered.

## Building and working in the xstdc repository

- Use the existing Clang/GNU11 build: `xmm all` for libraries,
  `env fast=1 xmm all` for optimized libraries.
- Tests are discovered from `tests/*.c`; for example,
  `env fast=1 xmm build/sort_test.out`, then `./build/sort_test.out`.
  `xmm tests` builds the test executables; run them separately.
- Tests link the debug library and keep assertions/allocation tracking enabled,
  including in fast mode. Benchmarks live in `benchmarks/`.
- The build produces `libxstdc` and `libxstdc_dbg`, static and shared, and links
  mimalloc and libm. Ensure validation uses the workspace library when an older
  copy is installed in `/usr/local/lib`.
- Keep structs and declarations before the header's `// Definitions` section;
  public declarations have concise comments. Preserve existing linkage patterns
  rather than redefining inline macros in application code.
- Consult existing tests/benchmarks for local style. Print diagnostics with
  `xprint`/`xprint_err`, and exercise ownership, edge cases, and return semantics.
