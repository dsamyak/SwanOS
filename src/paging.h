#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>
#include <stddef.h>

#define PAGE_PRESENT  0x01
#define PAGE_RW       0x02
#define PAGE_USER     0x04

typedef struct {
    uint32_t entries[1024];
} page_table_t;

typedef struct {
    uint32_t entries[1024];
} page_directory_t;

void paging_init(void);
page_directory_t *paging_create_dir(void);
void paging_switch_dir(page_directory_t *dir);
void paging_map_page(page_directory_t *dir, uint32_t phys, uint32_t virt, uint32_t flags);
page_directory_t *paging_clone_dir(page_directory_t *src);

#endif
