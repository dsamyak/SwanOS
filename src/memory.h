#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>
#include <stdint.h>

#define PAGE_SIZE 4096

void  memory_init(void);

// Physical Memory Manager (Pages)
void *pmm_alloc_page(void);
void  pmm_free_page(void *ptr);

// Kernel Heap Allocator (Bump)
void *kmalloc(size_t size);
void  kfree(void *ptr);

uint32_t mem_used(void);
uint32_t mem_free(void);
uint32_t mem_total(void);

#endif
