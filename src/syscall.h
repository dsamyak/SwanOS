#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

void syscall_init(void);

// User mode helper functions
void sys_yield(void);
void *sys_malloc(uint32_t size);
int sys_ipc_send(uint32_t dest_pid, void *msg, uint32_t len);
int sys_ipc_recv(uint32_t *src_pid, void *msg, uint32_t max_len);

#endif
