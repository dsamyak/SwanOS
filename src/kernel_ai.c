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

/* ── Intent Parsing ───────────────────────────────────────── */
/* Map app name strings to window IDs
   These must match WIN_* defines in desktop.c:
   WIN_TERM=0, WIN_FILES=1, WIN_NOTES=2, WIN_ABOUT=3, WIN_AI=4,
   WIN_CALC=5, WIN_SYSMON=6, WIN_STORE=7, WIN_BROWSER=8,
   WIN_NETWORK=9, WIN_AUDIT=10, WIN_DRAW=11, WIN_CLOCK=12 */

static int match_app_name(const char *name) {
    /* Case-insensitive prefix matching */
    if (strstr(name, "term") || strstr(name, "shell") || strstr(name, "swan_shell") || strstr(name, "swanshell")) return 0;
    if (strstr(name, "file") || strstr(name, "feather")) return 1;
    if (strstr(name, "note") || strstr(name, "love") || strstr(name, "loveletter")) return 2;
    if (strstr(name, "about") || strstr(name, "song") || strstr(name, "swansong")) return 3;
    if (strstr(name, "ai") || strstr(name, "soul") || strstr(name, "chat") || strstr(name, "swansoul")) return 4;
    if (strstr(name, "calc") || strstr(name, "count") || strstr(name, "swancount")) return 5;
    if (strstr(name, "monitor") || strstr(name, "sysmon") || strstr(name, "heartbeat")) return 6;
    if (strstr(name, "store") || strstr(name, "nest") || strstr(name, "swannest")) return 7;
    if (strstr(name, "browser") || strstr(name, "lake") || strstr(name, "web") || strstr(name, "swanlake")) return 8;
    if (strstr(name, "network") || strstr(name, "wing") || strstr(name, "winglink")) return 9;
    if (strstr(name, "audit") || strstr(name, "watch") || strstr(name, "swanwatch")) return 10;
    if (strstr(name, "draw") || strstr(name, "paint") || strstr(name, "swandraw")) return 11;
    if (strstr(name, "clock") || strstr(name, "time") || strstr(name, "swanclock")) return 12;
    return -1;
}

ai_intent_t kernel_ai_parse_intent(const char *response) {
    ai_intent_t intent;
    memset(&intent, 0, sizeof(intent));
    intent.action = AI_ACTION_ANSWER;
    intent.app_id = -1;
    
    /* Copy answer text */
    strncpy(intent.answer, response, 127);
    intent.answer[127] = '\0';
    
    /* Scan for action tags */
    const char *p = response;
    
    /* [OPEN:appname] */
    const char *open_tag = strstr(p, "[OPEN:");
    if (open_tag) {
        const char *start = open_tag + 6;
        const char *end = strchr(start, ']');
        if (end && (end - start) < 32) {
            char app_name[32];
            int len = (int)(end - start);
            strncpy(app_name, start, len);
            app_name[len] = '\0';
            int aid = match_app_name(app_name);
            if (aid >= 0) {
                intent.action = AI_ACTION_OPEN;
                intent.app_id = aid;
                /* Extract text after the tag as the answer */
                const char *after = end + 1;
                while (*after == ' ') after++;
                if (*after) strncpy(intent.answer, after, 127);
                return intent;
            }
        }
    }
    
    /* [TIME] */
    if (strstr(p, "[TIME]")) {
        intent.action = AI_ACTION_TIME;
        return intent;
    }
    
    /* [MEM] */
    if (strstr(p, "[MEM]")) {
        intent.action = AI_ACTION_MEM;
        return intent;
    }
    
    /* [HELP] */
    if (strstr(p, "[HELP]")) {
        intent.action = AI_ACTION_HELP;
        return intent;
    }
    
    return intent;
}

/* ── Health Summary Generator ─────────────────────────────── */
void kernel_ai_get_health_summary(char *buf, int len) {
    uint32_t mt = mem_total();
    uint32_t mu = mem_used();
    uint32_t mem_pct = (mt > 0) ? (mu * 100) / mt : 0;
    int nprocs = process_count_active();
    uint32_t uptime = timer_get_seconds();
    int ai_on = ai_status.connected;
    
    char tmp[12];
    
    if (mem_pct > 90) {
        strcpy(buf, "CRITICAL: Memory at ");
        itoa(mem_pct, tmp, 10); strcat(buf, tmp);
        strcat(buf, "% - close apps now!");
    } else if (mem_pct > 70) {
        strcpy(buf, "Warning: Memory at ");
        itoa(mem_pct, tmp, 10); strcat(buf, tmp);
        strcat(buf, "%, ");
        itoa(nprocs, tmp, 10); strcat(buf, tmp);
        strcat(buf, " running");
    } else if (nprocs > 8) {
        strcpy(buf, "Busy: ");
        itoa(nprocs, tmp, 10); strcat(buf, tmp);
        strcat(buf, " procs, mem ");
        itoa(mem_pct, tmp, 10); strcat(buf, tmp);
        strcat(buf, "% - stable");
    } else if (uptime < 30) {
        strcpy(buf, "Just booted - all systems go");
        if (ai_on) strcat(buf, ", AI online");
    } else {
        strcpy(buf, "Healthy: ");
        itoa(mem_pct, tmp, 10); strcat(buf, tmp);
        strcat(buf, "% mem, ");
        itoa(nprocs, tmp, 10); strcat(buf, tmp);
        strcat(buf, " procs");
        if (ai_on) strcat(buf, ", AI on");
    }
    
    /* Ensure null-termination */
    buf[len - 1] = '\0';
}

/* ── App Descriptions for Narrator ────────────────────────── */
const char *kernel_ai_get_app_description(int app_id) {
    switch (app_id) {
        case 0:  return "SwanShell - Terminal for commands and AI queries";
        case 1:  return "Feather - File manager for browsing your files";
        case 2:  return "LoveLetter - Text editor for notes and documents";
        case 3:  return "SwanSong - About SwanOS, version and system info";
        case 4:  return "SwanSoul - AI chat assistant, ask anything";
        case 5:  return "SwanCount - Calculator for math operations";
        case 6:  return "Heartbeat - System monitor with CPU and memory";
        case 7:  return "SwanNest - App store for software downloads";
        case 8:  return "SwanLake - Web browser with Firefox and Brave";
        case 9:  return "WingLink - Network settings and connection status";
        case 10: return "SwanWatch - Security audit log viewer";
        case 11: return "SwanDraw - Pixel art drawing canvas";
        case 12: return "SwanClock - Clock with analog display and stopwatch";
        default: return "SwanOS application";
    }
}

