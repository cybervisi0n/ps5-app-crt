#ifndef PROSPERO_HEAP_H
#define PROSPERO_HEAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PS5_HEAP_HINT ((uintptr_t)0x2000000000ull)
#define PS5_HEAP_RANGE ((size_t)16 << 30)

struct ps5_heap_stats {

  uintptr_t range_base;
  size_t range_bytes;

  size_t mapped_bytes;
  size_t peak_bytes;
  unsigned segments;

  unsigned long long libc_fallbacks;

  unsigned arenas;

  size_t in_use_bytes;
};

#define PS5_HEAP_TRIM 1
void ps5_heap_trim(void);

void ps5_heap_stats(struct ps5_heap_stats *stats);

typedef void (*ps5_heap_allocated_fn)(void *pointer, size_t bytes);
typedef void (*ps5_heap_freed_fn)(void *pointer);
void ps5_heap_trace(ps5_heap_allocated_fn allocated, ps5_heap_freed_fn freed);

void *ps5_heap_malloc(size_t bytes);
void *ps5_heap_calloc(size_t count, size_t bytes);
void *ps5_heap_memalign(size_t alignment, size_t bytes);
void *ps5_heap_realloc(void *pointer, size_t bytes);
void ps5_heap_free(void *pointer);
size_t ps5_heap_usable_size(const void *pointer);

bool ps5_heap_owns(const void *pointer);

#ifdef __cplusplus
}
#endif

#endif
