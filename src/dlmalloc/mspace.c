#include <errno.h>
#include <stddef.h>

#include "names.h"

void *prospero_heap_map(size_t bytes);
int prospero_heap_unmap(void *address, size_t bytes);

#define ONLY_MSPACES 1
#define USE_LOCKS 1

#define USE_SPIN_LOCKS 0

#define FOOTERS 1
#define HAVE_MORECORE 0
#define HAVE_MMAP 1
#define HAVE_MREMAP 0
#define MMAP(s) prospero_heap_map(s)
#define DIRECT_MMAP(s) prospero_heap_map(s)
#define MUNMAP(a, s) prospero_heap_unmap((a), (s))

#define MMAP_CLEARS 0

#define MALLOC_ALIGNMENT ((size_t)32)

#define DEFAULT_GRANULARITY ((size_t)2 << 20)
#define DEFAULT_MMAP_THRESHOLD ((size_t)32 << 20)

#define DEFAULT_TRIM_THRESHOLD ((size_t)32 << 20)
#define MALLOC_FAILURE_ACTION errno = ENOMEM
#define NO_MALLINFO 0
#define NO_MALLOC_STATS 1

#define malloc_getpagesize ((size_t)0x10000)
#include "malloc.c"

size_t
prospero_mspace_in_use(mspace space)
{
   return (size_t)mspace_mallinfo(space).uordblks;
}

int
prospero_mspace_trim(mspace space)
{
   return mspace_trim(space, 0);
}
