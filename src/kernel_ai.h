#ifndef KERNEL_AI_H
#define KERNEL_AI_H

#include <stdint.h>

/* ── AI Kernel Advisor ────────────────────────────────────── */
/* The AI is the kernel's realtime co-pilot, providing crash
   analysis, scheduling hints, and system health insights. */

/* Initialize AI subsystem */
void kernel_ai_init(void);

/* Called periodically from timer callback (~every 5s) to:
   - Send telemetry to bridge
   - Poll for pending AI advice
   - Update connection health */
void kernel_ai_tick(void);

/* ── Crash Analysis ──────────────────────────────────────── */
/* Returns 1 if process should be restarted, 0 if termination needed */
int kernel_ai_analyze_crash(uint32_t fault_addr, uint32_t pid, const char *reason);

/* ── Scheduler Hints ─────────────────────────────────────── */
/* Request AI to analyze current process load and suggest priorities */
void kernel_ai_scheduler_hints(void);

/* ── Status & Advice ─────────────────────────────────────── */
#define AI_MAX_ADVICE 4
#define AI_ADVICE_LEN 48

typedef struct {
    int      connected;           /* Bridge connection alive? */
    int      query_pending;       /* Async query in flight? */
    uint32_t last_latency_ms;     /* Last response RTT */
    uint32_t queries_total;       /* Total queries this session */
    uint32_t telemetry_sent;      /* Total telemetry packets sent */
    uint32_t uptime_ticks;        /* AI subsystem uptime */
    char     advice[AI_MAX_ADVICE][AI_ADVICE_LEN]; /* Rolling advice buffer */
    int      advice_count;        /* Number of advice entries */
    int      advice_head;         /* Circular buffer head */
} kernel_ai_status_t;

/* Get current AI subsystem status (for system monitor display) */
const kernel_ai_status_t *kernel_ai_get_status(void);

/* Get a specific advice string (0 = latest) */
const char *kernel_ai_get_advice(int index);

/* Push a local advice/status message (from kernel, not LLM) */
void kernel_ai_push_advice(const char *msg);

#endif
