#include "process.h"
#include "memory.h"
#include "string.h"
#include "gdt.h"
#include "idt.h"
#include "kernel_ai.h"
#include "screen.h"
#include "fs.h"

extern page_directory_t *current_dir;
int yield_requested = 0;

static process_t processes[MAX_PROCESSES];
process_t *current_process = 0;
static uint32_t next_pid = 1;
int process_scheduling_enabled = 0;

void general_protection_fault_handler(registers_t *regs) {
    screen_set_color(4, 0); /* Red on black */
    screen_print("\nEXCEPTION: GPF in Process [PID ");
    char num[16];
    itoa(current_process ? current_process->pid : 0, num, 10);
    screen_print(num);
    screen_print("]\n");

    int restart = kernel_ai_analyze_crash(regs->eip, current_process ? current_process->pid : 0, "Privilege Violation");
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
        while(1) { __asm__ volatile("hlt"); }
    }
}

void process_init(void) {
    memset(processes, 0, sizeof(processes));
    
    process_t *init = &processes[0];
    init->pid = 0;
    init->state = 1; /* running */
    init->page_directory = current_dir;
    init->next = init;
    
    current_process = init;
    
    register_interrupt_handler(13, general_protection_fault_handler);
}

int process_create(void (*entry_point)(void), uint8_t ring) {
    int i;
    for (i = 1; i < MAX_PROCESSES; i++) {
        if (processes[i].state == 0) break;
    }
    if (i == MAX_PROCESSES) return -1;
    
    process_t *p = &processes[i];
    p->pid = next_pid++;
    p->state = 2; /* ready */
    
    uint32_t stack = (uint32_t)pmm_alloc_page();
    if (!stack) return -1;
    p->kernel_stack = stack + 4096;
    
    uint32_t *stk = (uint32_t *)p->kernel_stack;
    
    /* Hardware pushes for iret */
    if (ring == 3) {
        uint32_t user_stack = (uint32_t)pmm_alloc_page();
        *--stk = 0x23; /* User SS (0x20 | RPL 3) */
        *--stk = user_stack + 4096; /* User ESP */
        *--stk = 0x202; /* EFLAGS (IF set) */
        *--stk = 0x1B;  /* User CS (0x18 | RPL 3) */
        *--stk = (uint32_t)entry_point; /* EIP */
    } else {
        *--stk = 0x202; /* EFLAGS (IF set) */
        *--stk = 0x08;  /* Kernel CS */
        *--stk = (uint32_t)entry_point; /* EIP */
    }
    
    *--stk = 0; /* Error Code */
    *--stk = 0; /* Int No */
    
    /* pusha (8 general purpose registers) */
    for (int j = 0; j < 8; j++) *--stk = 0;
    
    /* Initial data segment */
    *--stk = (ring == 3) ? 0x23 : 0x10;
    
    p->esp = (uint32_t)stk;
    p->page_directory = current_dir;
    
    /* Insert into list */
    p->next = current_process->next;
    current_process->next = p;
    
    return p->pid;
}

void process_start_scheduling(void) {
    process_scheduling_enabled = 1;
}

uint32_t switch_context(uint32_t current_esp) {
    if (!current_process || !process_scheduling_enabled) return current_esp;
    
    registers_t *regs = (registers_t *)current_esp;
    
    /* Only switch on timer tick (IRQ0 = INT 32) or explicit yield */
    if (regs->int_no != 32 && !yield_requested) return current_esp;
    yield_requested = 0;
    
    current_process->esp = current_esp;
    
    /* Round robin scheduler */
    do {
        current_process = current_process->next;
    } while (current_process->state != 1 && current_process->state != 2);
    
    current_process->state = 1;
    
    tss_set_kernel_stack(current_process->kernel_stack);
    
    if (current_dir != current_process->page_directory) {
        paging_switch_dir(current_process->page_directory);
    }
    
    return current_process->esp;
}

int process_ipc_send(uint32_t dest_pid, void *msg, uint32_t len) {
    if (len > IPC_MAX_MSG_LEN) return -1;
    
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state != 0 && processes[i].pid == dest_pid) {
            if (processes[i].has_msg) return -2; /* Mailbox full */
            
            processes[i].msg.sender_pid = current_process->pid;
            processes[i].msg.len = len;
            memcpy(processes[i].msg.data, msg, len);
            processes[i].has_msg = 1;
            
            if (processes[i].state == 3) {
                processes[i].state = 2; /* Wake up blocked process */
            }
            return 0;
        }
    }
    return -1;
}

int process_ipc_recv(uint32_t *src_pid, void *msg, uint32_t max_len) {
    if (!current_process->has_msg) {
        return -1; /* No message */
    }
    
    uint32_t copy_len = current_process->msg.len;
    if (copy_len > max_len) copy_len = max_len;
    
    memcpy(msg, current_process->msg.data, copy_len);
    if (src_pid) *src_pid = current_process->msg.sender_pid;
    
    current_process->has_msg = 0;
    return copy_len;
}

extern page_directory_t *kernel_dir;

int process_exec(const char *filename) {
    char data[8192]; /* Load up to 8KB for this basic OS */
    int size = fs_read(filename, data, sizeof(data));
    if (size <= 0) return -1;
    
    int i;
    for (i = 1; i < MAX_PROCESSES; i++) {
        if (processes[i].state == 0) break;
    }
    if (i == MAX_PROCESSES) return -1;
    
    process_t *p = &processes[i];
    p->pid = next_pid++;
    p->state = 2; /* ready */
    
    p->page_directory = paging_clone_dir(kernel_dir);
    if (!p->page_directory) {
        p->state = 0;
        return -1;
    }
    
    uint32_t virt_start = 0x40000000; /* Map user app at 1 GB */
    
    for (int offset = 0; offset < size; offset += 4096) {
        uint32_t phys = (uint32_t)pmm_alloc_page();
        if (!phys) { p->state = 0; return -1; }
        int chunk = size - offset;
        if (chunk > 4096) chunk = 4096;
        memcpy((void*)phys, data + offset, chunk);
        paging_map_page(p->page_directory, phys, virt_start + offset, PAGE_USER | PAGE_RW);
    }
    
    /* Dedicated user stack at 0xB0000000 */
    uint32_t virt_stack = 0xB0000000;
    uint32_t phys_stk = (uint32_t)pmm_alloc_page();
    if (!phys_stk) { p->state = 0; return -1; }
    paging_map_page(p->page_directory, phys_stk, virt_stack, PAGE_USER | PAGE_RW);
    
    uint32_t kernel_stack = (uint32_t)pmm_alloc_page();
    if (!kernel_stack) { p->state = 0; return -1; }
    p->kernel_stack = kernel_stack + 4096;
    
    uint32_t *stk = (uint32_t *)p->kernel_stack;
    
    /* Hardware pushes for iret to Ring 3 */
    *--stk = 0x23;                   /* User SS */
    *--stk = virt_stack + 4096 - 4;  /* User ESP */
    *--stk = 0x202;                  /* EFLAGS (IF set) */
    *--stk = 0x1B;                   /* User CS */
    *--stk = virt_start;             /* EIP (Entry point of loaded code) */
    
    *--stk = 0; /* Error Code */
    *--stk = 0; /* Int No */
    
    for (int j = 0; j < 8; j++) *--stk = 0; /* pusha */
    
    *--stk = 0x23; /* Initial data segment */
    p->esp = (uint32_t)stk;
    
    p->next = current_process->next;
    current_process->next = p;
    
    return p->pid;
}
