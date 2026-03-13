#include "kernel_ai.h"
#include "llm.h"
#include "screen.h"
#include "string.h"

void kernel_ai_init(void) {
    /* Ready the AI layer */
}

static char hex_chars[] = "0123456789ABCDEF";

static void to_hex(uint32_t val, char *buf) {
    buf[0] = '0'; buf[1] = 'x';
    for (int i = 7; i >= 0; i--) {
        buf[2 + i] = hex_chars[val & 0xF];
        val >>= 4;
    }
    buf[10] = '\0';
}

int kernel_ai_analyze_crash(uint32_t fault_addr, uint32_t pid, const char *reason) {
    char prompt[256];
    char response[128];
    char num[16];
    char addr_hex[16];
    
    strcpy(prompt, "PROCESS_CRASH PID:");
    itoa(pid, num, 10);
    strcat(prompt, num);
    strcat(prompt, " ADDR:");
    to_hex(fault_addr, addr_hex);
    strcat(prompt, addr_hex);
    strcat(prompt, " REASON:");
    strcat(prompt, reason);
    strcat(prompt, " -> ACTION? (Respond [RESTART] or [TERMINATE])");
    
    screen_set_color(14, 0); /* Yellow */
    screen_print("\n[AI Core] Analyzing telemetry for process crash...\n");
    
    if (llm_query(prompt, response, sizeof(response)) > 0) {
        if (strstr(response, "RESTART")) {
            return 1;
        }
    }
    
    /* Default fallback: safety first, terminate */
    return 0;
}

void kernel_ai_scheduler_hints(void) {
    /* Periodic background telemetry analysis */
}
