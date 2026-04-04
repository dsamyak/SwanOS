/* ============================================================
 * SwanOS — Dynamic Process Manager with Priority Scheduling
 * Priority-based round-robin, per-process CPU accounting,
 * and AI-assisted scheduling hint support.
 * ============================================================ */

#include "process.h"
#include "memory.h"
#include "string.h"
#include "gdt.h"
#include "idt.h"
#include "kernel_ai.h"
#include "screen.h"
#include "fs.h"
#include "timer.h"

extern page_directory_t *current_dir;
int yield_requested = 0;

static process_t processes[MAX_PROCESSES];
process_t *current_process = 0;
static uint32_t next_pid = 1;
int process_scheduling_enabled = 0;
static uint32_t total_context_switches = 0;

/* Priority → time slice mapping */
static uint8_t priority_to_slice(uint8_t priority) {
    switch (priority) {
        case PRIORITY_HIGH:   return 4;
        case PRIORITY_NORMAL: return 2;
        case PRIORITY_LOW:    return 1;
        case PRIORITY_IDLE:   return 1;
        default:              return 2;
    }
}

void general_protection_fault_handler(registers_t *regs) {
    screen_set_color(4, 0); /* Red on black */
    screen_print("\nEXCEPTION: GPF in Process [PID ");
    char num[16];
    itoa(current_process ? current_process->pid : 0, num, 10);
    screen_print(num);
    if (current_process && current_process->name[0]) {
        screen_print(" '");
        screen_print(current_process->name);
        screen_print("'");
    }
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
        current_process->state = PROC_STATE_UNUSED;
        yield_requested = 1;
    } else {
        while(1) { __asm__ volatile("hlt"); }
    }
}

void process_init(void) {
    memset(processes, 0, sizeof(processes));
    total_context_switches = 0;
    
    process_t *init = &processes[0];
    init->pid = 0;
    init->state = PROC_STATE_RUNNING;
    init->page_directory = current_dir;
    init->next = init;
    init->priority = PRIORITY_NORMAL;
    init->base_slice = priority_to_slice(PRIORITY_NORMAL);
    init->time_slice = init->base_slice;
    init->cpu_ticks = 0;
    init->cpu_ticks_window = 0;
    init->last_window_ticks = 0;
    init->create_tick = 0;
    strcpy(init->name, "kernel");
    
    current_process = init;
    
    register_interrupt_handler(13, general_protection_fault_handler);
}

int process_create(void (*entry_point)(void), uint8_t ring) {
    return process_create_named(entry_point, ring, "process", PRIORITY_NORMAL);
}

int process_create_named(void (*entry_point)(void), uint8_t ring, const char *name, uint8_t priority) {
    int i;
    for (i = 1; i < MAX_PROCESSES; i++) {
        if (processes[i].state == PROC_STATE_UNUSED) break;
    }
    if (i == MAX_PROCESSES) return -1;
    
    process_t *p = &processes[i];
    p->pid = next_pid++;
    p->state = PROC_STATE_READY;
    
    /* Priority and scheduling */
    p->priority = priority;
    p->base_slice = priority_to_slice(priority);
    p->time_slice = p->base_slice;
    p->cpu_ticks = 0;
    p->cpu_ticks_window = 0;
    p->last_window_ticks = 0;
    p->create_tick = timer_get_ticks();
    
    /* Name */
    if (name) {
        strncpy(p->name, name, 15);
        p->name[15] = '\0';
    } else {
        strcpy(p->name, "unnamed");
    }
    
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
    
    /* Track CPU usage for current process */
    current_process->cpu_ticks++;
    current_process->cpu_ticks_window++;
    
    /* Priority-based scheduling: check if time slice is exhausted */
    if (!yield_requested) {
        if (current_process->time_slice > 0) {
            current_process->time_slice--;
            if (current_process->time_slice > 0) {
                /* Still has time — don't switch */
                return current_esp;
            }
        }
    }
    
    yield_requested = 0;
    
    current_process->esp = current_esp;
    
    /* Reset time slice for outgoing process */
    current_process->time_slice = current_process->base_slice;
    
    /* Priority-aware round-robin: find next runnable process */
    process_t *start = current_process;
    do {
        current_process = current_process->next;
    } while (current_process->state != PROC_STATE_RUNNING && 
             current_process->state != PROC_STATE_READY &&
             current_process != start);
    
    current_process->state = PROC_STATE_RUNNING;
    total_context_switches++;
    
    tss_set_kernel_stack(current_process->kernel_stack);
    
    if (current_dir != current_process->page_directory) {
        paging_switch_dir(current_process->page_directory);
    }
    
    return current_process->esp;
}

void process_set_priority(uint32_t pid, uint8_t priority) {
    if (priority > PRIORITY_HIGH) priority = PRIORITY_HIGH;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state != PROC_STATE_UNUSED && processes[i].pid == pid) {
            processes[i].priority = priority;
            processes[i].base_slice = priority_to_slice(priority);
            return;
        }
    }
}

int process_count_active(void) {
    int count = 0;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state != PROC_STATE_UNUSED) count++;
    }
    return count;
}

void process_cpu_window_reset(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state != PROC_STATE_UNUSED) {
            processes[i].last_window_ticks = processes[i].cpu_ticks_window;
            processes[i].cpu_ticks_window = 0;
        }
    }
}

void process_get_overview(process_overview_t *out) {
    if (!out) return;
    memset(out, 0, sizeof(process_overview_t));
    
    uint32_t total_window = 0;
    int count = 0;
    
    /* First pass: count total window ticks */
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state != PROC_STATE_UNUSED) {
            total_window += processes[i].last_window_ticks;
        }
    }
    if (total_window == 0) total_window = 1; /* Avoid div by zero */
    
    /* Second pass: fill stats */
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state != PROC_STATE_UNUSED) {
            process_stats_t *s = &out->procs[count];
            s->pid = processes[i].pid;
            strncpy(s->name, processes[i].name, 15);
            s->name[15] = '\0';
            s->state = processes[i].state;
            s->priority = processes[i].priority;
            s->cpu_ticks = processes[i].cpu_ticks;
            s->cpu_percent = (processes[i].last_window_ticks * 100) / total_window;
            
            uint32_t alive_ticks = timer_get_ticks() - processes[i].create_tick;
            s->uptime_secs = alive_ticks / timer_get_frequency();
            
            out->total_cpu_ticks += processes[i].cpu_ticks;
            count++;
        }
    }
    out->count = count;
    out->total_count = count;
    out->context_switches = total_context_switches;
}

int process_ipc_send(uint32_t dest_pid, void *msg, uint32_t len) {
    if (len > IPC_MAX_MSG_LEN) return -1;
    
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state != PROC_STATE_UNUSED && processes[i].pid == dest_pid) {
            if (processes[i].has_msg) return -2; /* Mailbox full */
            
            processes[i].msg.sender_pid = current_process->pid;
            processes[i].msg.len = len;
            memcpy(processes[i].msg.data, msg, len);
            processes[i].has_msg = 1;
            
            if (processes[i].state == PROC_STATE_BLOCKED) {
                processes[i].state = PROC_STATE_READY;
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
        if (processes[i].state == PROC_STATE_UNUSED) break;
    }
    if (i == MAX_PROCESSES) return -1;
    
    process_t *p = &processes[i];
    p->pid = next_pid++;
    p->state = PROC_STATE_READY;
    p->priority = PRIORITY_NORMAL;
    p->base_slice = priority_to_slice(PRIORITY_NORMAL);
    p->time_slice = p->base_slice;
    p->cpu_ticks = 0;
    p->cpu_ticks_window = 0;
    p->last_window_ticks = 0;
    p->create_tick = timer_get_ticks();
    strncpy(p->name, filename, 15);
    p->name[15] = '\0';
    
    p->page_directory = paging_clone_dir(kernel_dir);
    if (!p->page_directory) {
        p->state = PROC_STATE_UNUSED;
        return -1;
    }
    
    uint32_t virt_start = 0x40000000; /* Map user app at 1 GB */
    
    for (int offset = 0; offset < size; offset += 4096) {
        uint32_t phys = (uint32_t)pmm_alloc_page();
        if (!phys) { p->state = PROC_STATE_UNUSED; return -1; }
        int chunk = size - offset;
        if (chunk > 4096) chunk = 4096;
        memcpy((void*)phys, data + offset, chunk);
        paging_map_page(p->page_directory, phys, virt_start + offset, PAGE_USER | PAGE_RW);
    }
    
    /* Dedicated user stack at 0xB0000000 */
    uint32_t virt_stack = 0xB0000000;
    uint32_t phys_stk = (uint32_t)pmm_alloc_page();
    if (!phys_stk) { p->state = PROC_STATE_UNUSED; return -1; }
    paging_map_page(p->page_directory, phys_stk, virt_stack, PAGE_USER | PAGE_RW);
    
    uint32_t kernel_stack = (uint32_t)pmm_alloc_page();
    if (!kernel_stack) { p->state = PROC_STATE_UNUSED; return -1; }
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
