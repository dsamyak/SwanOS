/* ============================================================
 * SwanOS — Audit System
 * Structured event logging with ring buffer and host persistence.
 * Tracks logins, file ops, app launches, commands, etc.
 * ============================================================ */

#include "audit.h"
#include "string.h"
#include "user.h"
#include "rtc.h"
#include "llm.h"

static audit_entry_t entries[AUDIT_MAX_ENTRIES];
static int entry_head = 0;   /* next write position */
static int entry_count = 0;  /* total entries written (can exceed buffer) */

void audit_init(void) {
    memset(entries, 0, sizeof(entries));
    entry_head = 0;
    entry_count = 0;
}

void audit_log(int type, const char *detail) {
    audit_entry_t *e = &entries[entry_head];

    e->type = (uint8_t)type;
    e->used = 1;

    /* Copy current user */
    const char *u = user_current();
    strncpy(e->user, u, AUDIT_USER_LEN - 1);
    e->user[AUDIT_USER_LEN - 1] = '\0';

    /* Copy detail */
    if (detail) {
        strncpy(e->detail, detail, AUDIT_DETAIL_LEN - 1);
        e->detail[AUDIT_DETAIL_LEN - 1] = '\0';
    } else {
        e->detail[0] = '\0';
    }

    /* Timestamp from RTC */
    rtc_time_t t;
    rtc_read(&t);
    e->hour   = t.hour;
    e->minute = t.minute;
    e->second = t.second;
    e->day    = t.day;
    e->month  = t.month;

    /* Advance ring buffer */
    entry_head = (entry_head + 1) % AUDIT_MAX_ENTRIES;
    entry_count++;

    /* Persist to host bridge */
    char audit_msg[128];
    strcpy(audit_msg, "[");
    strcat(audit_msg, audit_type_name(type));
    strcat(audit_msg, "] ");
    strcat(audit_msg, e->user);
    strcat(audit_msg, ": ");
    if (detail) strcat(audit_msg, detail);
    llm_host_audit(audit_msg);
}

int audit_get_count(void) {
    return entry_count;
}

const audit_entry_t *audit_get_entry(int index) {
    /* index 0 = oldest available, higher = newer */
    int total = entry_count;
    if (total > AUDIT_MAX_ENTRIES) total = AUDIT_MAX_ENTRIES;
    if (index < 0 || index >= total) return 0;

    int start;
    if (entry_count <= AUDIT_MAX_ENTRIES)
        start = 0;
    else
        start = entry_head; /* oldest entry is at head after wrap */

    int real = (start + index) % AUDIT_MAX_ENTRIES;
    return &entries[real];
}

const char *audit_type_name(int type) {
    switch (type) {
        case AUDIT_LOGIN:       return "LOGIN";
        case AUDIT_LOGOUT:      return "LOGOUT";
        case AUDIT_FILE_CREATE: return "FILE_CREATE";
        case AUDIT_FILE_DELETE: return "FILE_DELETE";
        case AUDIT_APP_OPEN:    return "APP_OPEN";
        case AUDIT_APP_CLOSE:   return "APP_CLOSE";
        case AUDIT_COMMAND:     return "COMMAND";
        case AUDIT_FILE_WRITE:  return "FILE_WRITE";
        case AUDIT_SYSTEM:      return "SYSTEM";
        default:                return "UNKNOWN";
    }
}

int audit_format_recent(char *buf, int buf_len, int count) {
    buf[0] = '\0';
    int total = entry_count;
    if (total > AUDIT_MAX_ENTRIES) total = AUDIT_MAX_ENTRIES;
    if (count > total) count = total;
    if (count <= 0) {
        strcpy(buf, "  No audit events recorded.\n");
        return 0;
    }

    int start_idx = total - count;
    int written = 0;

    for (int i = start_idx; i < total && (int)strlen(buf) < buf_len - 100; i++) {
        const audit_entry_t *e = audit_get_entry(i);
        if (!e || !e->used) continue;

        /* Format: [HH:MM:SS] TYPE user: detail */
        char line[128];
        char tmp[8];

        strcpy(line, "  [");
        itoa(e->hour, tmp, 10);
        if (e->hour < 10) strcat(line, "0");
        strcat(line, tmp);
        strcat(line, ":");
        itoa(e->minute, tmp, 10);
        if (e->minute < 10) strcat(line, "0");
        strcat(line, tmp);
        strcat(line, ":");
        itoa(e->second, tmp, 10);
        if (e->second < 10) strcat(line, "0");
        strcat(line, tmp);
        strcat(line, "] ");

        strcat(line, audit_type_name(e->type));
        strcat(line, " ");
        strcat(line, e->user);
        if (e->detail[0]) {
            strcat(line, ": ");
            /* Truncate detail for display */
            int dlen = strlen(e->detail);
            if (dlen > 30) {
                char trunc[34];
                strncpy(trunc, e->detail, 30);
                trunc[30] = '.';
                trunc[31] = '.';
                trunc[32] = '.';
                trunc[33] = '\0';
                strcat(line, trunc);
            } else {
                strcat(line, e->detail);
            }
        }
        strcat(line, "\n");

        if ((int)(strlen(buf) + strlen(line)) < buf_len - 1) {
            strcat(buf, line);
            written++;
        }
    }

    return written;
}
