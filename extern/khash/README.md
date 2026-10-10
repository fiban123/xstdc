# khash

Unmodified `khash.h` from [attractivechaos/klib](https://github.com/attractivechaos/klib),
revision `97a0fcb790b43b9e5da8994f4671021fec036f19` (header version 0.2.8).
The MIT license and copyright notices are included in the header.

Source: https://raw.githubusercontent.com/attractivechaos/klib/97a0fcb790b43b9e5da8994f4671021fec036f19/khash.h

Used only by `benchmarks/hashmap_bench.c`. Map and set adapters reuse the node
container workloads with the same hashes and inputs; native integer hash rows
are also provided. khash keeps its default libc allocator, 32-bit hash result,
and 0.77 maximum load factor. Its keys and values are inline `u64`s; node maps
support arbitrary byte keys/values and allocate owned nodes with xalloc.
Reservation converts the requested element count to sufficient khash buckets.
