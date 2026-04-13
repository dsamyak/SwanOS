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

/* ── AI Intent Parsing ──────────────────────────────────────── */
/* Action types returned by intent parser */
#define AI_ACTION_NONE    0   /* No action — just an answer */
#define AI_ACTION_OPEN    1   /* Open an app: app_id set */
#define AI_ACTION_TIME    2   /* Show current time */
#define AI_ACTION_MEM     3   /* Show memory info */
#define AI_ACTION_HELP    4   /* Show help */
#define AI_ACTION_ANSWER  5   /* Plain text answer */

typedef struct {
    int  action;              /* AI_ACTION_* */
    int  app_id;              /* For AI_ACTION_OPEN */
    char answer[128];         /* Text answer from AI */
} ai_intent_t;

/* Parse an LLM response for structured action tags.
   Returns parsed intent. Scans for [OPEN:name], [TIME], [MEM], [HELP] */
ai_intent_t kernel_ai_parse_intent(const char *response);

/* ── Health Summary ─────────────────────────────────────────── */
/* Generate a natural-language one-line health summary from current telemetry.
   Writes into buf (max len chars). No LLM needed — locally generated. */
void kernel_ai_get_health_summary(char *buf, int len);

/* ── Narrator Descriptions ──────────────────────────────────── */
/* Get a description string for a given app_id (for accessibility narrator) */
const char *kernel_ai_get_app_description(int app_id);

#endif
