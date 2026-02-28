#ifndef IDT_H
#define IDT_H

#include <stdint.h>

/* Registers pushed by ISR/IRQ stubs */
typedef struct {
    uint32_t ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; /* pusha */
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;            /* pushed by CPU */
} registers_t;

typedef void (*isr_handler_t)(registers_t *);

void idt_init(void);
void register_interrupt_handler(uint8_t n, isr_handler_t handler);

#endif
