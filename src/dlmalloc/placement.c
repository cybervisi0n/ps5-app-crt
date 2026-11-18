#include "placement.h"

#include "prospero/sceKernel.h"

#define PROSPERO_AREA_STEP ((uintptr_t)0x100000000ull)
#define PROSPERO_AREA_END ((uintptr_t)0x10000000000ull)
#define PROSPERO_AREA_CANDIDATES                                                   \
  ((unsigned)((PROSPERO_AREA_END - PROSPERO_VIEW_AREA) / PROSPERO_AREA_STEP))

#define PROSPERO_NEAR_GRANULE ((uintptr_t)0x4000000ull)

#define PROSPERO_LOWEST ((uintptr_t)0x1000000ull)

int32_t sceKernelAllocateDirectMemoryForApp(int64_t search_start,
                                            int64_t search_end, size_t length,
                                            size_t alignment, int memory_type,
                                            int64_t *physical_start)
    __attribute__((weak));

int prospero_allocate_direct(size_t bytes, int64_t *start) {
  const int32_t result = sceKernelAllocateDirectMemory(
      0, sceKernelGetDirectMemorySize(), bytes, PROSPERO_DIRECT_UNIT,
      PROSPERO_SCE_KERNEL_DIRECT_TYPE_CPU, start);
  if (result == 0 || !sceKernelAllocateDirectMemoryForApp)
    return result;
  if (sceKernelAllocateDirectMemoryForApp(
          0, sceKernelGetDirectMemorySize(), bytes, PROSPERO_DIRECT_UNIT,
          PROSPERO_SCE_KERNEL_DIRECT_TYPE_CPU, start) == 0)
    return 0;
  return result;
}

static uintptr_t candidate(uintptr_t hint, unsigned attempt) {
  if (attempt == 0 && hint != 0)
    return hint;
  return PROSPERO_VIEW_AREA +
         (uintptr_t)(attempt - (hint != 0 ? 1u : 0u)) * PROSPERO_AREA_STEP;
}

int prospero_map_placed(int64_t direct_start, size_t bytes, uintptr_t hint,
                        int protection, int no_place, void **address) {
  int32_t last = no_place;
  for (unsigned attempt = 0; attempt <= PROSPERO_AREA_CANDIDATES; attempt++) {
    const uintptr_t at = candidate(hint, attempt);
    if (!prospero_outside_gpu_window(at, bytes))
      continue;
    void *placed = (void *)at;
    const int32_t result = sceKernelMapDirectMemory(
        &placed, bytes, protection, 0, direct_start, PROSPERO_DIRECT_UNIT);
    if (result != 0) {
      last = result;
      continue;
    }
    if (prospero_outside_gpu_window((uintptr_t)placed, bytes)) {
      *address = placed;
      return 0;
    }
    sceKernelMunmap(placed, bytes);
    last = no_place;
  }
  return last;
}

int prospero_reserve_placed(size_t bytes, uintptr_t hint, size_t alignment,
                            int no_place, void **address) {
  int32_t last = no_place;
  for (unsigned attempt = 0; attempt <= PROSPERO_AREA_CANDIDATES; attempt++) {
    const uintptr_t at = candidate(hint, attempt);
    if (!prospero_outside_gpu_window(at, bytes))
      continue;
    void *placed = (void *)at;
    const int32_t result =
        sceKernelReserveVirtualRange(&placed, bytes, 0, alignment);
    if (result != 0) {
      last = result;
      continue;
    }
    if (prospero_outside_gpu_window((uintptr_t)placed, bytes)) {
      *address = placed;
      return 0;
    }
    sceKernelMunmap(placed, bytes);
    last = no_place;
  }
  return last;
}

bool prospero_near_enough(uintptr_t base, size_t bytes, uintptr_t anchor) {
  if (base < PROSPERO_LOWEST || !prospero_outside_gpu_window(base, bytes))
    return false;
  const uintptr_t low = anchor > PROSPERO_NEAR_REACH ? anchor - PROSPERO_NEAR_REACH : 0;
  return base >= low && base + bytes <= anchor + PROSPERO_NEAR_REACH;
}

static bool try_reserve(uintptr_t at, size_t bytes, uintptr_t anchor,
                        void **address) {
  void *placed = (void *)at;
  if (sceKernelReserveVirtualRange(&placed, bytes, 0, PROSPERO_DIRECT_UNIT) != 0)
    return false;
  if (prospero_near_enough((uintptr_t)placed, bytes, anchor)) {
    *address = placed;
    return true;
  }
  sceKernelMunmap(placed, bytes);
  return false;
}

int prospero_reserve_near(size_t bytes, uintptr_t anchor, uintptr_t hint,
                          int no_place, void **address) {
  if (bytes >= PROSPERO_NEAR_REACH)
    return no_place;
  if (hint != 0 && prospero_near_enough(hint, bytes, anchor) &&
      try_reserve(hint, bytes, anchor, address))
    return 0;
  const uintptr_t origin = anchor / PROSPERO_NEAR_GRANULE * PROSPERO_NEAR_GRANULE;
  const unsigned steps = (unsigned)(PROSPERO_NEAR_REACH / PROSPERO_NEAR_GRANULE);

  for (unsigned step = 1; step <= steps; step++) {
    const uintptr_t above = origin + step * PROSPERO_NEAR_GRANULE;
    if (prospero_near_enough(above, bytes, anchor) &&
        try_reserve(above, bytes, anchor, address))
      return 0;
    const uintptr_t distance =
        step * PROSPERO_NEAR_GRANULE + prospero_round_up(bytes, PROSPERO_NEAR_GRANULE);
    if (origin > distance) {
      const uintptr_t below = origin - distance;
      if (prospero_near_enough(below, bytes, anchor) &&
          try_reserve(below, bytes, anchor, address))
        return 0;
    }
  }
  return no_place;
}
