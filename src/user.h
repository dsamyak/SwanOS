#ifndef USER_H
#define USER_H

#include <stdint.h>

#define MAX_USERS    8
#define MAX_USERNAME 16

/* ── User Profile (persisted via host bridge) ────────────── */
typedef struct {
    int  login_count;
    uint8_t last_hour;
    uint8_t last_minute;
    uint8_t last_day;
    uint8_t last_month;
    uint32_t session_start_ticks;  /* ticks when current session started */
} user_profile_t;

void user_init(void);
int  user_login(void);                     /* Interactive login, returns 1 on success */
const char *user_current(void);            /* Get current username */
int  user_register(const char *username);  /* Register a new user */
user_profile_t *user_get_profile(void);    /* Get current user's profile */
void user_save_profile(void);              /* Persist profile to host */
uint32_t user_session_seconds(void);       /* Seconds since current session started */

#endif
