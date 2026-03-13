/* ============================================================
 * SwanOS — Memory Manager (Physical & Heap)
 * Includes Bitmap Page Frame Allocator + Kernel Bump Allocator
 * ============================================================ */

#include "memory.h"
#include "string.h"

#define PMM_MAX_BLOCKS 32768 /* 128 MB RAM max */
static uint32_t pmm_bitmap[PMM_MAX_BLOCKS / 32];
static uint32_t pmm_used_blocks = 0;
static uint32_t pmm_max_blocks = PMM_MAX_BLOCKS;

/* Kernel bump allocator starting at 4 MB, with 4 MB heap space */
#define HEAP_START 0x400000   /* 4 MB */
#define HEAP_SIZE  0x400000   /* 4 MB heap */

static uint32_t heap_ptr = HEAP_START;

void memory_init(void) {
    /* Initialize Bump allocator */
    heap_ptr = HEAP_START;

    /* Initialize PMM Bitmap */
    memset(pmm_bitmap, 0, sizeof(pmm_bitmap));
    pmm_used_blocks = 0;

    /* Reserve first 8 MB for kernel + heap + VGA texts */
    /* 8 MB = 2048 pages */
    uint32_t reserved_pages = (HEAP_START + HEAP_SIZE) / PAGE_SIZE;
    for (uint32_t i = 0; i < reserved_pages; i++) {
        pmm_bitmap[i / 32] |= (1 << (i % 32));
    }
    pmm_used_blocks = reserved_pages;
}

void *pmm_alloc_page(void) {
    for (uint32_t i = 0; i < pmm_max_blocks / 32; i++) {
        if (pmm_bitmap[i] != 0xFFFFFFFF) {
            for (int j = 0; j < 32; j++) {
                if (!(pmm_bitmap[i] & (1 << j))) {
                    pmm_bitmap[i] |= (1 << j);
                    pmm_used_blocks++;
                    uint32_t addr = (i * 32 + j) * PAGE_SIZE;
                    /* Zero out the physical page before returning */
                    memset((void*)addr, 0, PAGE_SIZE);
                    return (void *)addr;
                }
            }
        }
    }
    return 0; /* out of memory */
}

void pmm_free_page(void *ptr) {
    uint32_t addr = (uint32_t)ptr;
    uint32_t frame = addr / PAGE_SIZE;
    if (frame < pmm_max_blocks) {
        if (pmm_bitmap[frame / 32] & (1 << (frame % 32))) {
            pmm_bitmap[frame / 32] &= ~(1 << (frame % 32));
            pmm_used_blocks--;
        }
    }
}

void *kmalloc(size_t size) {
    /* Align to 4 bytes */
    size = (size + 3) & ~3;

    if (heap_ptr + size > HEAP_START + HEAP_SIZE) {
        return 0; /* out of kernel heap memory */
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

uint32_t mem_used(void) {
    return pmm_used_blocks * PAGE_SIZE;
}

uint32_t mem_free(void) {
    return (pmm_max_blocks - pmm_used_blocks) * PAGE_SIZE;
}

uint32_t mem_total(void) {
    return pmm_max_blocks * PAGE_SIZE;
}
