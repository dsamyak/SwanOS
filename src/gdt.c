#include "gdt.h"
#include "string.h"

typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed)) gdt_entry_t;

typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) gdt_ptr_t;

static gdt_entry_t gdt[6];
static gdt_ptr_t gp;
static tss_entry_t tss;

extern void gdt_flush(uint32_t);
extern void tss_flush(void);

static void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[num].base_low = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;

    gdt[num].limit_low = (limit & 0xFFFF);
    gdt[num].granularity = ((limit >> 16) & 0x0F);
    gdt[num].granularity |= (gran & 0xF0);
    gdt[num].access = access;
}

static void write_tss(int num, uint16_t ss0, uint32_t esp0) {
    uint32_t base = (uint32_t) &tss;
    uint32_t limit = sizeof(tss);

    gdt_set_gate(num, base, limit, 0xE9, 0x00);

    memset(&tss, 0, sizeof(tss));

    tss.ss0 = ss0;
    tss.esp0 = esp0;
    
    /* Default segment registers for kernel */
    tss.cs = 0x08 | 0x3;
    tss.ss = tss.ds = tss.es = tss.fs = tss.gs = 0x10 | 0x3;
    tss.iomap_base = sizeof(tss);
}

void tss_set_kernel_stack(uint32_t stack) {
    tss.esp0 = stack;
}

void gdt_init(void) {
    gp.limit = (sizeof(gdt_entry_t) * 6) - 1;
    gp.base = (uint32_t)&gdt;

    gdt_set_gate(0, 0, 0, 0, 0);                /* Null */
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); /* Kernel Code */
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF); /* Kernel Data */
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF); /* User Code */
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF); /* User Data */
    write_tss(5, 0x10, 0x0);                    /* TSS */

    gdt_flush((uint32_t)&gp);
    tss_flush();
}
