/* ============================================================
 * SwanOS — Simple Memory Allocator
 * Bump allocator starting at 4 MB, with 4 MB heap space.
 * No free() support (for simplicity) — memory is unlimited
 * for the OS's lifetime within the heap region.
 * ============================================================ */

#include "memory.h"
#include "string.h"

#define HEAP_START 0x400000   /* 4 MB */
#define HEAP_SIZE  0x400000   /* 4 MB heap */

static uint32_t heap_ptr = HEAP_START;

void memory_init(void) {
    heap_ptr = HEAP_START;
}

void *kmalloc(size_t size) {
    /* Align to 4 bytes */
    size = (size + 3) & ~3;

    if (heap_ptr + size > HEAP_START + HEAP_SIZE) {
        return 0; /* out of memory */
    }

    void *ptr = (void *)heap_ptr;
    heap_ptr += size;
    memset(ptr, 0, size);
    return ptr;
}

void kfree(void *ptr) {
    /* Bump allocator — no free support */
    (void)ptr;
}
