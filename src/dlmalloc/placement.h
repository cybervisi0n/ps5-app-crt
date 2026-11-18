#ifndef PS5APP_PLACEMENT_H
#define PS5APP_PLACEMENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROSPERO_GPU_WINDOW_LOW ((uintptr_t)0x200000000ull)
#define PROSPERO_GPU_WINDOW_HIGH ((uintptr_t)0x300000000ull)

#define PROSPERO_VIEW_AREA ((uintptr_t)0x400000000ull)

#define PROSPERO_NEAR_REACH ((uintptr_t)0x7c000000ull)
#define PROSPERO_DIRECT_UNIT ((size_t)0x10000)
#define PROSPERO_PAGE ((size_t)0x4000)

static inline size_t prospero_round_up(size_t value, size_t unit) {
  return (value + unit - 1) / unit * unit;
}

static inline bool prospero_outside_gpu_window(uintptr_t base, size_t bytes) {
  return base >= PROSPERO_GPU_WINDOW_HIGH || base + bytes <= PROSPERO_GPU_WINDOW_LOW;
}

int prospero_map_placed(int64_t direct_start, size_t bytes, uintptr_t hint,
                        int protection, int no_place, void **address);

int prospero_reserve_placed(size_t bytes, uintptr_t hint, size_t alignment,
                            int no_place, void **address);

int prospero_allocate_direct(size_t bytes, int64_t *start);

bool prospero_near_enough(uintptr_t base, size_t bytes, uintptr_t anchor);

int prospero_reserve_near(size_t bytes, uintptr_t anchor, uintptr_t hint,
                          int no_place, void **address);

#endif
