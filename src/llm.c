/* ============================================================
 * SwanOS — LLM Client (Groq API via Serial Bridge)
 * Async queries, streaming responses, telemetry,
 * heartbeat, and latency tracking for realtime AI OS.
 *
 * Protocol:
 *   OS → Bridge:  \x01K + api_key + \x04        (set API key)
 *   OS → Bridge:  \x01Q + query text + \x04      (LLM query)
 *   OS → Bridge:  \x01T + telemetry + \x04       (telemetry data)
 *   OS → Bridge:  \x01H\x04                      (heartbeat ping)
 *   Bridge → OS:  response text + \x04            (LLM response)
 *   Bridge → OS:  \x01P + status + \x04           (heartbeat pong)
 * ============================================================ */

#include "llm.h"
#include "serial.h"
#include "screen.h"
#include "string.h"
#include "fs.h"
#include "timer.h"

#define API_KEY_FILE ".apikey"
#define MAX_KEY_LEN  128

static char api_key[MAX_KEY_LEN];
static int  key_loaded = 0;

/* ── Async Query State ────────────────────────────────────── */
static int      async_pending = 0;         /* Query in flight? */
static uint32_t async_query_tick = 0;      /* Tick when query was sent */
static char     async_stream_buf[1024];    /* Accumulating response */
static int      async_stream_len = 0;      /* Bytes accumulated so far */
static int      async_stream_complete = 0; /* Got EOT (\x04) */

/* ── Latency & Stats ─────────────────────────────────────── */
static uint32_t last_latency_ms = 0;       /* Last query RTT in ms */
static uint32_t query_count = 0;           /* Total queries this session */

/* ── Heartbeat ────────────────────────────────────────────── */
static uint32_t last_heartbeat_sent = 0;
static uint32_t last_heartbeat_recv = 0;
static int      bridge_alive = 0;

void llm_init(void) {
    api_key[0] = '\0';
    key_loaded = 0;
    async_pending = 0;
    async_stream_len = 0;
    async_stream_complete = 0;
    last_latency_ms = 0;
    query_count = 0;
    bridge_alive = 0;

    /* Try to load API key from filesystem */
    char buf[MAX_KEY_LEN];
    int r = fs_read(API_KEY_FILE, buf, sizeof(buf) - 1);
    if (r > 0 && buf[0] != '\0') {
        int len = strlen(buf);
        while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == ' ' || buf[len-1] == '\r'))
            buf[--len] = '\0';
        if (len > 0) {
            strcpy(api_key, buf);
            key_loaded = 1;
        }
    }

    if (key_loaded) {
        llm_send_key();
    }
}

void llm_set_api_key(const char *key) {
    if (!key || key[0] == '\0') return;
    strncpy(api_key, key, MAX_KEY_LEN - 1);
    api_key[MAX_KEY_LEN - 1] = '\0';
    key_loaded = 1;
    fs_write(API_KEY_FILE, api_key);
    llm_send_key();
}

const char *llm_get_api_key(void) {
    return api_key;
}

int llm_ready(void) {
    return key_loaded && api_key[0] != '\0';
}

void llm_send_key(void) {
    if (!key_loaded) return;
    serial_write_char('\x01');
    serial_write_char('K');
    const char *p = api_key;
    while (*p) serial_write_char(*p++);
    serial_write_char('\x04');
}

void llm_set_system_prompt(const char *prompt) {
    serial_write_char('\x01');
    serial_write_char('S');
    const char *p = prompt;
    while (*p) serial_write_char(*p++);
    serial_write_char('\x04');
}

/* ── Synchronous Query (blocking) ─────────────────────────── */
int llm_query(const char *question, char *response, int max_len) {
    if (!llm_ready()) {
        strcpy(response, "No API key set. Use 'setkey <KEY>' to configure.");
        return -1;
    }

    uint32_t start_tick = timer_get_ticks();
    query_count++;

    serial_write_char('\x01');
    serial_write_char('Q');
    const char *p = question;
    while (*p) serial_write_char(*p++);
    serial_write_char('\x04');

    int len = serial_read_line(response, max_len, 30);

    /* Compute latency */
    uint32_t end_tick = timer_get_ticks();
    last_latency_ms = ((end_tick - start_tick) * 1000) / timer_get_frequency();
    bridge_alive = 1;
    last_heartbeat_recv = end_tick;

    if (len == 0) {
        strcpy(response, "No response from AI bridge. Is llm_bridge.py running?");
        bridge_alive = 0;
        return -1;
    }

    return len;
}

/* ── Async Query (non-blocking) ───────────────────────────── */
void llm_query_async(const char *question) {
    if (!llm_ready() || async_pending) return;

    async_stream_len = 0;
    async_stream_complete = 0;
    async_stream_buf[0] = '\0';
    async_query_tick = timer_get_ticks();
    query_count++;

    serial_write_char('\x01');
    serial_write_char('Q');
    const char *p = question;
    while (*p) serial_write_char(*p++);
    serial_write_char('\x04');

    async_pending = 1;
}

int llm_poll_response(char *response, int max_len) {
    if (!async_pending) return -1;

    /* Non-blocking: check if serial has data */
    while (serial_data_ready() && async_stream_len < 1023) {
        char c = serial_read_char();
        if (c == '\x04') {
            /* End of transmission */
            async_stream_complete = 1;
            async_pending = 0;

            /* Compute latency */
            uint32_t end_tick = timer_get_ticks();
            last_latency_ms = ((end_tick - async_query_tick) * 1000) / timer_get_frequency();
            bridge_alive = 1;
            last_heartbeat_recv = end_tick;
            break;
        }
        /* Skip protocol headers */
        if (c == '\x01') continue;
        if (async_stream_len == 0 && (c == 'R' || c == 'S')) continue; /* Skip response type prefix */
        
        async_stream_buf[async_stream_len++] = c;
        async_stream_buf[async_stream_len] = '\0';
    }

    /* Check timeout (30 seconds) */
    if (async_pending && (timer_get_ticks() - async_query_tick) > timer_get_frequency() * 30) {
        async_pending = 0;
        async_stream_complete = 1;
        bridge_alive = 0;
        if (async_stream_len == 0) {
            strcpy(async_stream_buf, "AI bridge timeout.");
            async_stream_len = strlen(async_stream_buf);
        }
    }

    if (async_stream_complete && response) {
        int copy = async_stream_len;
        if (copy >= max_len) copy = max_len - 1;
        memcpy(response, async_stream_buf, copy);
        response[copy] = '\0';
        return copy;
    }

    return async_stream_complete ? async_stream_len : 0;
}

int llm_async_pending(void) {
    return async_pending;
}

int llm_stream_available(void) {
    return async_stream_len;
}

int llm_stream_read(char *buf, int max_len) {
    int copy = async_stream_len;
    if (copy >= max_len) copy = max_len - 1;
    if (copy > 0) {
        memcpy(buf, async_stream_buf, copy);
        buf[copy] = '\0';
    }
    return copy;
}

/* ── Telemetry ────────────────────────────────────────────── */
void llm_send_telemetry(uint32_t mem_pct, uint32_t proc_count, uint32_t uptime_s, uint32_t ctx_switches) {
    char buf[128];
    char tmp[16];
    
    serial_write_char('\x01');
    serial_write_char('T');
    
    strcpy(buf, "mem=");
    itoa(mem_pct, tmp, 10); strcat(buf, tmp);
    strcat(buf, ",procs=");
    itoa(proc_count, tmp, 10); strcat(buf, tmp);
    strcat(buf, ",up=");
    itoa(uptime_s, tmp, 10); strcat(buf, tmp);
    strcat(buf, ",ctx=");
    itoa(ctx_switches, tmp, 10); strcat(buf, tmp);
    
    const char *p = buf;
    while (*p) serial_write_char(*p++);
    serial_write_char('\x04');
}

/* ── Heartbeat ────────────────────────────────────────────── */
void llm_send_heartbeat(void) {
    serial_write_char('\x01');
    serial_write_char('H');
    serial_write_char('\x04');
    last_heartbeat_sent = timer_get_ticks();
}

int llm_bridge_connected(void) {
    /* Consider bridge alive if heartbeat response within last 10 seconds */
    uint32_t now = timer_get_ticks();
    if (bridge_alive && (now - last_heartbeat_recv) < timer_get_frequency() * 10) {
        return 1;
    }
    
    /* Check for heartbeat response in serial buffer */
    if (serial_data_ready() && !async_pending) {
        char c = serial_read_char();
        if (c == '\x01') {
            char cmd = serial_read_char();
            if (cmd == 'P') {
                /* Heartbeat pong — skip to EOT */
                while (serial_data_ready()) {
                    char x = serial_read_char();
                    if (x == '\x04') break;
                }
                bridge_alive = 1;
                last_heartbeat_recv = now;
                return 1;
            }
        }
    }
    return bridge_alive;
}

/* ── Latency & Stats ──────────────────────────────────────── */
uint32_t llm_get_last_latency(void) {
    return last_latency_ms;
}

uint32_t llm_get_query_count(void) {
    return query_count;
}

/* ── Host Persistent Storage via Bridge ──────────────────── */
void llm_host_save(const char *name, const char *content) {
    serial_write_char('\x01');
    serial_write_char('V');
    const char *p = name;
    while (*p) serial_write_char(*p++);
    serial_write_char('|');
    p = content;
    while (*p) serial_write_char(*p++);
    serial_write_char('\x04');
}

int llm_host_load(const char *name, char *buf, int max_len) {
    serial_write_char('\x01');
    serial_write_char('L');
    const char *p = name;
    while (*p) serial_write_char(*p++);
    serial_write_char('\x04');

    int len = serial_read_line(buf, max_len, 5);
    if (len == 0) {
        buf[0] = '\0';
        return -1;
    }
    return len;
}

void llm_host_audit(const char *event) {
    serial_write_char('\x01');
    serial_write_char('A');
    const char *p = event;
    while (*p) serial_write_char(*p++);
    serial_write_char('\x04');
}
