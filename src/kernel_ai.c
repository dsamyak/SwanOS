/* ============================================================
 * SwanOS — Realtime AI Kernel Advisor
 * Non-blocking telemetry loop, crash analysis via LLM,
 * scheduler hints, and rolling advice buffer for display.
 * ============================================================ */

#include "kernel_ai.h"
#include "llm.h"
#include "screen.h"
#include "string.h"
#include "memory.h"
#include "process.h"
#include "timer.h"

/* ── State ─────────────────────────────────────────────────── */
static kernel_ai_status_t ai_status;
static uint32_t init_tick = 0;
static uint32_t last_telemetry_tick = 0;
static uint32_t last_heartbeat_tick = 0;
static uint32_t last_scheduler_tick = 0;

/* Hex helper for crash analysis */
static char hex_chars[] = "0123456789ABCDEF";

static void to_hex(uint32_t val, char *buf) {
    buf[0] = '0'; buf[1] = 'x';
    for (int i = 7; i >= 0; i--) {
        buf[2 + i] = hex_chars[val & 0xF];
        val >>= 4;
    }
    buf[10] = '\0';
}

void kernel_ai_init(void) {
    memset(&ai_status, 0, sizeof(ai_status));
    init_tick = timer_get_ticks();
    last_telemetry_tick = init_tick;
    last_heartbeat_tick = init_tick;
    last_scheduler_tick = init_tick;
    
    ai_status.connected = 0;
    ai_status.advice_count = 0;
    ai_status.advice_head = 0;
    
    /* Initial advice messages */
    kernel_ai_push_advice("AI Advisor online");
    kernel_ai_push_advice("Monitoring system...");
    
    /* Send initial heartbeat */
    llm_send_heartbeat();
}

void kernel_ai_push_advice(const char *msg) {
    int slot = ai_status.advice_head;
    strncpy(ai_status.advice[slot], msg, AI_ADVICE_LEN - 1);
    ai_status.advice[slot][AI_ADVICE_LEN - 1] = '\0';
    ai_status.advice_head = (ai_status.advice_head + 1) % AI_MAX_ADVICE;
    if (ai_status.advice_count < AI_MAX_ADVICE)
        ai_status.advice_count++;
}

void kernel_ai_tick(void) {
    uint32_t now = timer_get_ticks();
    uint32_t freq = timer_get_frequency();
    
    ai_status.uptime_ticks = now - init_tick;
    ai_status.query_pending = llm_async_pending();
    ai_status.last_latency_ms = llm_get_last_latency();
    ai_status.queries_total = llm_get_query_count();
    
    /* ── Heartbeat every 8 seconds ── */
    if (now - last_heartbeat_tick > freq * 8) {
        llm_send_heartbeat();
        last_heartbeat_tick = now;
    }
    
    /* Update connection status */
    ai_status.connected = llm_bridge_connected();
    
    /* ── Telemetry every 5 seconds ── */
    if (now - last_telemetry_tick > freq * 5) {
        uint32_t mt = mem_total();
        uint32_t mem_pct = (mt > 0) ? (mem_used() * 100) / mt : 0;
        uint32_t procs = (uint32_t)process_count_active();
        uint32_t uptime = timer_get_seconds();
        
        /* Get context switch count from process overview */
        process_overview_t ov;
        process_get_overview(&ov);
        
        llm_send_telemetry(mem_pct, procs, uptime, ov.context_switches);
        ai_status.telemetry_sent++;
        last_telemetry_tick = now;
        
        /* Generate local advice based on system state */
        if (mem_pct > 80) {
            kernel_ai_push_advice("WARN: Memory >80%!");
        } else if (mem_pct > 50) {
            kernel_ai_push_advice("Memory usage moderate");
        }
        
        if (procs > 10) {
            kernel_ai_push_advice("Many processes active");
        }
    }
    
    /* ── Reset CPU measurement window every second ── */
    process_cpu_window_reset();
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
    
    /* Push advice about the crash */
    char advice[AI_ADVICE_LEN];
    strcpy(advice, "Crash: PID ");
    strcat(advice, num);
    strcat(advice, " ");
    strncpy(advice + strlen(advice), reason, AI_ADVICE_LEN - strlen(advice) - 1);
    advice[AI_ADVICE_LEN - 1] = '\0';
    kernel_ai_push_advice(advice);
    
    if (llm_query(prompt, response, sizeof(response)) > 0) {
        if (strstr(response, "RESTART")) {
            kernel_ai_push_advice("AI: RESTART process");
            return 1;
        }
    }
    
    kernel_ai_push_advice("AI: TERMINATE process");
    /* Default fallback: safety first, terminate */
    return 0;
}

void kernel_ai_scheduler_hints(void) {
    if (!llm_ready() || llm_async_pending()) return;
    
    uint32_t now = timer_get_ticks();
    uint32_t freq = timer_get_frequency();
    
    /* Only request hints every 30 seconds */
    if (now - last_scheduler_tick < freq * 30) return;
    last_scheduler_tick = now;
    
    /* Build compact process summary */
    process_overview_t ov;
    process_get_overview(&ov);
    
    if (ov.count <= 1) return; /* Only kernel process, nothing to optimize */
    
    char prompt[256];
    char tmp[16];
    strcpy(prompt, "SCHEDULER_HINT procs=");
    itoa(ov.count, tmp, 10); strcat(prompt, tmp);
    strcat(prompt, " ctx_sw=");
    itoa(ov.context_switches, tmp, 10); strcat(prompt, tmp);
    
    /* Add top 3 processes by CPU */
    int shown = 0;
    for (int i = 0; i < ov.count && shown < 3; i++) {
        if (ov.procs[i].cpu_percent > 0) {
            strcat(prompt, " [");
            strcat(prompt, ov.procs[i].name);
            strcat(prompt, ":");
            itoa(ov.procs[i].cpu_percent, tmp, 10);
            strcat(prompt, tmp);
            strcat(prompt, "%]");
            shown++;
        }
    }
    
    strcat(prompt, " -> Respond with PRIORITY suggestions");
    
    /* Use async query so we don't block the kernel */
    llm_query_async(prompt);
    kernel_ai_push_advice("Requesting sched hints");
}

const kernel_ai_status_t *kernel_ai_get_status(void) {
    return &ai_status;
}

const char *kernel_ai_get_advice(int index) {
    if (ai_status.advice_count == 0) return "";
    /* Index 0 = most recent */
    int slot = (ai_status.advice_head - 1 - index + AI_MAX_ADVICE * 2) % AI_MAX_ADVICE;
    if (index >= ai_status.advice_count) return "";
    return ai_status.advice[slot];
}
