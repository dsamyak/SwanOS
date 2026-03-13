#ifndef KERNEL_AI_H
#define KERNEL_AI_H

#include <stdint.h>

void kernel_ai_init(void);

/* Returns 1 if process should be restarted, 0 if termiantion is needed */
int kernel_ai_analyze_crash(uint32_t fault_addr, uint32_t pid, const char *reason);

void kernel_ai_scheduler_hints(void);

#endif
