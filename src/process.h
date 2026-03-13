#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include "paging.h"

#define MAX_PROCESSES 64

#define IPC_MAX_MSG_LEN 256

typedef struct {
    uint32_t sender_pid;
    uint32_t len;
    uint8_t data[IPC_MAX_MSG_LEN];
} ipc_msg_t;

typedef struct process {
    uint32_t pid;
    uint32_t esp;
    uint32_t kernel_stack;
    page_directory_t *page_directory;
    uint32_t state; /* 0=unused, 1=running, 2=ready, 3=blocked */
    
    /* IPC Mailbox */
    int has_msg;
    ipc_msg_t msg;
    
    struct process *next;
} process_t;

void process_init(void);
int process_create(void (*entry_point)(void), uint8_t ring);
uint32_t switch_context(uint32_t current_esp);
void process_start_scheduling(void);

int process_ipc_send(uint32_t dest_pid, void *msg, uint32_t len);
int process_ipc_recv(uint32_t *src_pid, void *msg, uint32_t max_len);

#endif
