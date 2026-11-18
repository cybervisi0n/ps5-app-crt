#include "heap.h"

#include "placement.h"
#include "prospero/sceKernel.h"

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

#include "names.h"
typedef void *mspace;
mspace create_mspace(size_t capacity, int locked);
void *mspace_malloc(mspace space, size_t bytes);
void *mspace_calloc(mspace space, size_t count, size_t bytes);
void *mspace_realloc(mspace space, void *pointer, size_t bytes);
void *mspace_memalign(mspace space, size_t alignment, size_t bytes);
size_t mspace_usable_size(const void *pointer);
void mspace_free(mspace space, void *pointer);
size_t prospero_mspace_in_use(mspace space);
int prospero_mspace_trim(mspace space);

#define HEAP_MAX_SEGMENTS 8192

#define HEAP_GAP HEAP_UNIT
#define HEAP_UNIT PROSPERO_DIRECT_UNIT
#define HEAP_PROTECTION                                                        \
  (PROSPERO_SCE_KERNEL_PROT_CPU_READ | PROSPERO_SCE_KERNEL_PROT_CPU_WRITE)

struct heap_segment {
  size_t offset;
  size_t bytes;
  int64_t direct_start;
};

static struct heap_segment segments[HEAP_MAX_SEGMENTS];
static unsigned segment_count;
static size_t mapped_bytes, peak_bytes;
static unsigned long long libc_fallbacks;

static uintptr_t range_base;
static size_t range_bytes;

#define HEAP_ARENAS 8

static int range_state;
static mspace arenas[HEAP_ARENAS];
static int arena_states[HEAP_ARENAS];
static unsigned arena_next;
static pthread_key_t arena_key;
static pthread_once_t arena_key_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t segment_lock = PTHREAD_MUTEX_INITIALIZER;

static bool reserve_range(void) {

  for (size_t bytes = PS5_HEAP_RANGE; bytes >= ((size_t)1 << 30); bytes /= 2) {
    void *address = NULL;
    if (prospero_reserve_placed(bytes, PS5_HEAP_HINT, HEAP_UNIT, -1, &address) ==
        0) {
      range_base = (uintptr_t)address;
      range_bytes = bytes;
      return true;
    }
  }
  return false;
}

static void *heap_map_locked(size_t bytes) {
  bytes = prospero_round_up(bytes, HEAP_UNIT);
  if (bytes == 0 || segment_count >= HEAP_MAX_SEGMENTS)
    return (void *)-1;

  unsigned slot = 0;
  size_t offset = 0;
  for (; slot < segment_count; slot++) {
    if (segments[slot].offset - offset >= bytes + HEAP_GAP)
      break;
    offset = segments[slot].offset + segments[slot].bytes + HEAP_GAP;
  }
  if (offset > range_bytes || range_bytes - offset < bytes)
    return (void *)-1;

  int64_t start = -1;
  if (prospero_allocate_direct(bytes, &start) != 0)
    return (void *)-1;
  void *const at = (void *)(range_base + offset);
  void *mapped = at;
  if (sceKernelMapDirectMemory(&mapped, bytes, HEAP_PROTECTION,
                               PROSPERO_SCE_KERNEL_MAP_FIXED, start,
                               HEAP_UNIT) != 0 ||
      mapped != at) {
    sceKernelReleaseDirectMemory(start, bytes);
    return (void *)-1;
  }

  memmove(&segments[slot + 1], &segments[slot],
          (segment_count - slot) * sizeof(segments[0]));
  segments[slot] = (struct heap_segment){
      .offset = offset, .bytes = bytes, .direct_start = start};
  segment_count++;
  mapped_bytes += bytes;
  if (mapped_bytes > peak_bytes)
    peak_bytes = mapped_bytes;
  return at;
}

void *prospero_heap_map(size_t bytes) {
  pthread_mutex_lock(&segment_lock);
  void *const at = heap_map_locked(bytes);
  pthread_mutex_unlock(&segment_lock);
  return at;
}

static int heap_unmap_locked(void *address, size_t bytes) {
  if ((uintptr_t)address < range_base)
    return -1;
  const size_t offset = (uintptr_t)address - range_base;
  unsigned first = 0;
  while (first < segment_count && segments[first].offset < offset)
    first++;
  if (first == segment_count || segments[first].offset != offset)
    return -1;

  unsigned last = first;
  size_t covered = 0;
  while (last < segment_count && covered < bytes &&
         segments[last].offset == offset + covered) {
    covered += segments[last].bytes;
    last++;
  }
  if (covered != bytes)
    return -1;

  for (unsigned i = first; i < last; i++) {
    void *at = (void *)(range_base + segments[i].offset);
    sceKernelMunmap(at, segments[i].bytes);
    sceKernelReserveVirtualRange(&at, segments[i].bytes,
                                 PROSPERO_SCE_KERNEL_MAP_FIXED, PROSPERO_PAGE);
    sceKernelReleaseDirectMemory(segments[i].direct_start, segments[i].bytes);
    mapped_bytes -= segments[i].bytes;
  }
  memmove(&segments[first], &segments[last],
          (segment_count - last) * sizeof(segments[0]));
  segment_count -= last - first;
  return 0;
}

int prospero_heap_unmap(void *address, size_t bytes) {
  pthread_mutex_lock(&segment_lock);
  const int result = heap_unmap_locked(address, bytes);
  pthread_mutex_unlock(&segment_lock);
  return result;
}

static bool once(int *state, bool (*make)(void *), void *context) {
  int seen = __atomic_load_n(state, __ATOMIC_ACQUIRE);
  if (seen == 0 &&
      __atomic_compare_exchange_n(state, &seen, 1, false, __ATOMIC_ACQ_REL,
                                  __ATOMIC_ACQUIRE))
    __atomic_store_n(state, make(context) ? 2 : 3, __ATOMIC_RELEASE);

  while ((seen = __atomic_load_n(state, __ATOMIC_ACQUIRE)) == 1) {
    const struct timespec pause = {0, 50000};
    nanosleep(&pause, NULL);
  }
  return seen == 2;
}

static bool make_range(void *unused) {
  (void)unused;
  return reserve_range();
}

static bool make_arena(void *slot) {
  mspace *const arena = slot;
  *arena = create_mspace(0, 1);
  return *arena != NULL;
}

static mspace arena(unsigned index) {
  return once(&arena_states[index], make_arena, &arenas[index]) ? arenas[index]
                                                                : NULL;
}

static void make_arena_key(void) { pthread_key_create(&arena_key, NULL); }

static mspace title_heap(void) {
  if (!once(&range_state, make_range, NULL))
    return NULL;
  pthread_once(&arena_key_once, make_arena_key);
  uintptr_t slot = (uintptr_t)pthread_getspecific(arena_key);
  if (slot == 0) {
    slot =
        __atomic_fetch_add(&arena_next, 1, __ATOMIC_RELAXED) % HEAP_ARENAS + 1;
    pthread_setspecific(arena_key, (void *)slot);
  }
  const mspace space = arena((unsigned)slot - 1);
  return space || slot == 1 ? space : arena(0);
}

static mspace any_arena(void) { return arenas[0]; }

bool ps5_heap_owns(const void *pointer) {
  if (__atomic_load_n(&range_state, __ATOMIC_ACQUIRE) != 2)
    return false;
  return (uintptr_t)pointer - range_base < range_bytes;
}

void ps5_heap_stats(struct ps5_heap_stats *stats) {
  const bool ready = __atomic_load_n(&range_state, __ATOMIC_ACQUIRE) == 2;
  stats->range_base = ready ? range_base : 0;
  stats->range_bytes = ready ? range_bytes : 0;

  stats->mapped_bytes = mapped_bytes;
  stats->peak_bytes = peak_bytes;
  stats->segments = segment_count;
  stats->arenas = 0;
  for (unsigned i = 0; i < HEAP_ARENAS; i++)
    stats->arenas += __atomic_load_n(&arena_states[i], __ATOMIC_ACQUIRE) == 2;
  stats->libc_fallbacks = __atomic_load_n(&libc_fallbacks, __ATOMIC_RELAXED);
  stats->in_use_bytes = 0;
  for (unsigned i = 0; i < HEAP_ARENAS; i++)
    if (__atomic_load_n(&arena_states[i], __ATOMIC_ACQUIRE) == 2)
      stats->in_use_bytes += prospero_mspace_in_use(arenas[i]);
}

void ps5_heap_trim(void) {
  for (unsigned i = 0; i < HEAP_ARENAS; i++)
    if (__atomic_load_n(&arena_states[i], __ATOMIC_ACQUIRE) == 2)
      prospero_mspace_trim(arenas[i]);
}

void prospero_heap_count_fallback(void) {
  __atomic_fetch_add(&libc_fallbacks, 1, __ATOMIC_RELAXED);
}

void *ps5_heap_malloc(size_t bytes) {
  const mspace space = title_heap();
  return space ? mspace_malloc(space, bytes) : NULL;
}

void *ps5_heap_calloc(size_t count, size_t bytes) {
  const mspace space = title_heap();
  return space ? mspace_calloc(space, count, bytes) : NULL;
}

void *ps5_heap_memalign(size_t alignment, size_t bytes) {
  const mspace space = title_heap();
  return space ? mspace_memalign(space, alignment, bytes) : NULL;
}

void *ps5_heap_realloc(void *pointer, size_t bytes) {
  return pointer ? mspace_realloc(any_arena(), pointer, bytes)
                 : ps5_heap_malloc(bytes);
}

void ps5_heap_free(void *pointer) {
  if (pointer)
    mspace_free(any_arena(), pointer);
}

size_t ps5_heap_usable_size(const void *pointer) {
  return pointer ? mspace_usable_size(pointer) : 0;
}
