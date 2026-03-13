#include "paging.h"
#include "memory.h"
#include "string.h"
#include "screen.h"
#include "idt.h"
#include "kernel_ai.h"
#include "process.h"

page_directory_t *kernel_dir = 0;
page_directory_t *current_dir = 0;

extern void load_page_directory(uint32_t*);
extern void enable_paging(void);

void paging_map_page(page_directory_t *dir, uint32_t phys, uint32_t virt, uint32_t flags) {
    uint32_t pd_idx = virt >> 22;
    uint32_t pt_idx = (virt >> 12) & 0x03FF;

    if (!(dir->entries[pd_idx] & PAGE_PRESENT)) {
        uint32_t pt_phys = (uint32_t)pmm_alloc_page();
        if (!pt_phys) return; /* Out of memory */
        dir->entries[pd_idx] = pt_phys | PAGE_PRESENT | PAGE_RW | PAGE_USER;
    }

    page_table_t *pt = (page_table_t *)(dir->entries[pd_idx] & ~0xFFF);
    pt->entries[pt_idx] = phys | flags | PAGE_PRESENT;
}

page_directory_t *paging_create_dir(void) {
    page_directory_t *dir = (page_directory_t *)pmm_alloc_page();
    if (!dir) return 0;
    return dir;
}

void paging_switch_dir(page_directory_t *dir) {
    current_dir = dir;
    load_page_directory(dir->entries);
}

extern process_t *current_process;
extern int yield_requested;

void page_fault_handler(registers_t *regs) {
    uint32_t faulting_address;
    __asm__ volatile("mov %%cr2, %0" : "=r" (faulting_address));

    screen_set_color(4, 0); /* Red on black */
    screen_print("\nEXCEPTION: Page Fault in Process [PID ");
    char num[16];
    itoa(current_process ? current_process->pid : 0, num, 10);
    screen_print(num);
    screen_print("]\n");

    int restart = kernel_ai_analyze_crash(faulting_address, current_process ? current_process->pid : 0, "Memory Violation");
    if (restart) {
        screen_set_color(10, 0);
        screen_print("[AI] Action Executed: RESTART_PROCESS\n");
    } else {
        screen_set_color(12, 0);
        screen_print("[AI] Action Executed: TERMINATE_PROCESS\n");
    }
    
    if (current_process) {
        current_process->state = 0; /* terminate */
        yield_requested = 1;
    } else {
        screen_print("Fatal kernel page fault.\n");
        while(1) { __asm__ volatile("hlt"); }
    }
}

void paging_init(void) {
    kernel_dir = paging_create_dir();
    
    /* Identity map the first 128 MB */
    for (uint32_t i = 0; i < 32768; i++) {
        uint32_t addr = i * 4096;
        uint32_t flags = PAGE_RW | PAGE_USER;
        paging_map_page(kernel_dir, addr, addr, flags);
    }
    
    /* Identity Map VESA Framebuffer (Assuming it could be up to FD000000, Map top memory)
       We will map from 0xC0000000 to 0xFFFFFFFF just to be safe and cover any high VRAM */
    for (uint32_t i = 0xC0000; i < 0x100000; i++) {
        uint32_t addr = i * 4096;
        paging_map_page(kernel_dir, addr, addr, PAGE_RW | PAGE_USER);
    }
    
    register_interrupt_handler(14, page_fault_handler);

    paging_switch_dir(kernel_dir);
    enable_paging();
}
