#ifndef AUDIT_H
#define AUDIT_H

#include <stdint.h>

/* ── Audit Event Types ───────────────────────────────────── */
#define AUDIT_LOGIN        0
#define AUDIT_LOGOUT       1
#define AUDIT_FILE_CREATE  2
#define AUDIT_FILE_DELETE  3
#define AUDIT_APP_OPEN     4
#define AUDIT_APP_CLOSE    5
#define AUDIT_COMMAND      6
#define AUDIT_FILE_WRITE   7
#define AUDIT_SYSTEM       8

#define AUDIT_MAX_ENTRIES  64
#define AUDIT_DETAIL_LEN   48
#define AUDIT_USER_LEN     16

/* ── Audit Entry ─────────────────────────────────────────── */
typedef struct {
    uint8_t  type;
    char     user[AUDIT_USER_LEN];
    char     detail[AUDIT_DETAIL_LEN];
    uint8_t  hour;
    uint8_t  minute;
    uint8_t  second;
    uint8_t  day;
    uint8_t  month;
    uint8_t  used;
} audit_entry_t;

/* ── API ─────────────────────────────────────────────────── */
void audit_init(void);
void audit_log(int type, const char *detail);
int  audit_get_count(void);
const audit_entry_t *audit_get_entry(int index);
int  audit_format_recent(char *buf, int buf_len, int count);
const char *audit_type_name(int type);

#endif
