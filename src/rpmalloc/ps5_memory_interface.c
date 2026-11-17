#include "rpmalloc.h"

#include <stdio.h>

#include "prospero/sceKernel.h"

static rpmalloc_interface_t ps5_rpmalloc_interface;

static void * ps5_memory_map(size_t size, size_t alignment, size_t* offset, size_t* mapped_size) {
    return NULL;
}

static int ps5_memory_commit(void * address, size_t size) {
    return 0;
}

static int ps5_memory_decommit(void * address, size_t size) {
    return 0;
}

static void ps5_memory_unmap(void * address, size_t offset, size_t mapped_size) {

}

static int ps5_map_fail_callback(size_t size) {
    return 0;
}

static void ps5_error_callback(const char * message) {
    sceKernelNotifyRequest_t errorNotification = {};
    snprintf(errorNotification.message, sizeof(errorNotification.message) - 1, "%s", message);

    

}

void ps5_rpmalloc_initialize() {
    ps5_rpmalloc_interface.memory_map = ps5_memory_map;
    ps5_rpmalloc_interface.memory_commit = ps5_memory_commit;
    ps5_rpmalloc_interface.memory_decommit = ps5_memory_decommit;
    ps5_rpmalloc_interface.memory_unmap = ps5_memory_unmap;
    ps5_rpmalloc_interface.map_fail_callback = ps5_map_fail_callback;
    ps5_rpmalloc_interface.error_callback = ps5_error_callback;

    rpmalloc_initialize(&ps5_rpmalloc_interface);
}
