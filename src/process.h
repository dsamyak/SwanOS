#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include "paging.h"

#define MAX_PROCESSES 64

#define IPC_MAX_MSG_LEN 256

/* ── Process Priority Levels ─────────────────────────────── */
#define PRIORITY_IDLE   0   /* Background — runs when nothing else needs CPU */
#define PRIORITY_LOW    1   /* Below normal — 1 tick per turn */
#define PRIORITY_NORMAL 2   /* Default — 2 ticks per turn */
#define PRIORITY_HIGH   3   /* Realtime — 4 ticks per turn */

/* ── Process States ──────────────────────────────────────── */
#define PROC_STATE_UNUSED  0
#define PROC_STATE_RUNNING 1
#define PROC_STATE_READY   2
#define PROC_STATE_BLOCKED 3

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
    
    /* ── Dynamic Scheduling Fields ───────────────────────── */
    uint8_t  priority;          /* PRIORITY_IDLE..PRIORITY_HIGH */
    uint8_t  time_slice;        /* Remaining ticks in current quantum */
    uint8_t  base_slice;        /* Ticks per quantum based on priority */
    char     name[16];          /* Human-readable process name */
    uint32_t cpu_ticks;         /* Total CPU ticks consumed */
    uint32_t cpu_ticks_window;  /* Ticks consumed in current measurement window */
    uint32_t last_window_ticks; /* Snapshot for CPU% calculation */
    uint32_t create_tick;       /* Tick when process was created */
    
    /* IPC Mailbox */
    int has_msg;
    ipc_msg_t msg;
    
    struct process *next;
} process_t;

/* ── Process Statistics (for System Monitor) ─────────────── */
typedef struct {
    uint32_t pid;
    char     name[16];
    uint32_t state;
    uint8_t  priority;
    uint32_t cpu_percent;   /* 0-100 CPU usage % */
    uint32_t cpu_ticks;
    uint32_t uptime_secs;  /* How long process has been alive */
} process_stats_t;

typedef struct {
    int      count;                        /* Number of active processes */
    int      total_count;                  /* Total slots used */
    uint32_t total_cpu_ticks;             /* Sum of all cpu_ticks */
    uint32_t context_switches;            /* Total context switches */
    process_stats_t procs[MAX_PROCESSES]; /* Per-process stats */
} process_overview_t;

void process_init(void);
int process_create(void (*entry_point)(void), uint8_t ring);
int process_create_named(void (*entry_point)(void), uint8_t ring, const char *name, uint8_t priority);
uint32_t switch_context(uint32_t current_esp);
void process_start_scheduling(void);

/* ── Dynamic Scheduling API ──────────────────────────────── */
void process_set_priority(uint32_t pid, uint8_t priority);
int  process_count_active(void);
void process_get_overview(process_overview_t *out);
void process_cpu_window_reset(void);  /* Reset CPU measurement window — call every ~1s */

int process_ipc_send(uint32_t dest_pid, void *msg, uint32_t len);
int process_ipc_recv(uint32_t *src_pid, void *msg, uint32_t max_len);
int process_exec(const char *filename);

/* Extern for crash handlers */
extern process_t *current_process;
extern int yield_requested;

#endif
