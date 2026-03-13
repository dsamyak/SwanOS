#include "syscall.h"
#include "idt.h"
#include "process.h"
#include "memory.h"
#include "string.h"
#include "screen.h"

extern int yield_requested;

void syscall_handler(registers_t *regs) {
    switch (regs->eax) {
        case 0: /* YIELD */
            yield_requested = 1;
            break;
        case 1: /* MALLOC */
            regs->eax = (uint32_t)kmalloc(regs->ebx);
            break;
        case 2: /* IPC SEND */
            regs->eax = (uint32_t)process_ipc_send(regs->ebx, (void*)regs->ecx, regs->edx);
            break;
        case 3: /* IPC RECV */
            regs->eax = (uint32_t)process_ipc_recv((uint32_t*)regs->ebx, (void*)regs->ecx, regs->edx);
            break;
        default:
            screen_print("Unknown syscall!\n");
            break;
    }
}

void syscall_init(void) {
    register_interrupt_handler(128, syscall_handler);
}

static inline uint32_t syscall0(uint32_t num) {
    uint32_t a;
    __asm__ volatile("int $0x80" : "=a" (a) : "0" (num));
    return a;
}

static inline uint32_t syscall1(uint32_t num, uint32_t p1) {
    uint32_t a;
    __asm__ volatile("int $0x80" : "=a" (a) : "0" (num), "b" (p1));
    return a;
}

static inline uint32_t syscall3(uint32_t num, uint32_t p1, uint32_t p2, uint32_t p3) {
    uint32_t a;
    __asm__ volatile("int $0x80" : "=a" (a) : "0" (num), "b" (p1), "c" (p2), "d" (p3));
    return a;
}

void sys_yield(void) {
    syscall0(0);
}

void *sys_malloc(uint32_t size) {
    return (void *)syscall1(1, size);
}

int sys_ipc_send(uint32_t dest_pid, void *msg, uint32_t len) {
    return (int)syscall3(2, dest_pid, (uint32_t)msg, len);
}

int sys_ipc_recv(uint32_t *src_pid, void *msg, uint32_t max_len) {
    return (int)syscall3(3, (uint32_t)src_pid, (uint32_t)msg, max_len);
}
