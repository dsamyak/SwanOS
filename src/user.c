/* ============================================================
 * SwanOS — User Login Manager
 * Styled login card with box-drawing borders.
 * Users get created on first login.
 * Per-user profiles persisted via host bridge.
 * ============================================================ */

#include "user.h"
#include "screen.h"
#include "keyboard.h"
#include "string.h"
#include "llm.h"
#include "fs.h"
#include "rtc.h"
#include "timer.h"
#include "audit.h"

static char users[MAX_USERS][MAX_USERNAME];
static int  user_count = 0;
static int  current_user = -1;
static user_profile_t current_profile;

void user_init(void) {
    memset(users, 0, sizeof(users));
    memset(&current_profile, 0, sizeof(current_profile));
    user_count = 0;
    current_user = -1;

    /* Ensure /home exists */
    fs_mkdir("/home");

    char buf[512];
    int r = llm_host_load("users.txt", buf, sizeof(buf));
    if (r > 0) {
        char *p = buf;
        while (*p && user_count < MAX_USERS) {
            char *start = p;
            while (*p && *p != '\n') p++;
            if (*p == '\n') {
                *p = '\0';
                p++;
            }
            if (strlen(start) >= 2) {
                strcpy(users[user_count++], start);
                
                /* Create home dir for user */
                char hdir[64];
                strcpy(hdir, "/home/");
                strcat(hdir, start);
                fs_mkdir(hdir);
            }
        }
    }
}

int user_register(const char *username) {
    if (user_count >= MAX_USERS) return -1;
    if (strlen(username) < 2 || strlen(username) >= MAX_USERNAME) return -1;

    /* Check if exists */
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i], username) == 0) return i;
    }

    strcpy(users[user_count], username);
    user_count++;

    /* Save back to host */
    char buf[512];
    buf[0] = '\0';
    for (int i = 0; i < user_count; i++) {
        strcat(buf, users[i]);
        strcat(buf, "\n");
    }
    llm_host_save("users.txt", buf);

    /* Create home dir */
    char hdir[64];
    strcpy(hdir, "/home/");
    strcat(hdir, username);
    fs_mkdir(hdir);

    return user_count - 1;
}

const char *user_current(void) {
    if (current_user < 0) return "guest";
    return users[current_user];
}

user_profile_t *user_get_profile(void) {
    return &current_profile;
}

/* ── Profile persistence ────────────────────────────────── */
static void load_user_profile(const char *username) {
    memset(&current_profile, 0, sizeof(current_profile));

    char fname[32];
    strcpy(fname, "profile_");
    strcat(fname, username);
    strcat(fname, ".dat");

    char buf[128];
    int r = llm_host_load(fname, buf, sizeof(buf));
    if (r > 0 && buf[0] != '\0') {
        /* Parse: login_count|last_hour|last_minute|last_day|last_month */
        char *p = buf;
        current_profile.login_count = 0;
        while (*p >= '0' && *p <= '9') {
            current_profile.login_count = current_profile.login_count * 10 + (*p - '0');
            p++;
        }
        if (*p == '|') p++;
        current_profile.last_hour = 0;
        while (*p >= '0' && *p <= '9') {
            current_profile.last_hour = current_profile.last_hour * 10 + (*p - '0');
            p++;
        }
        if (*p == '|') p++;
        current_profile.last_minute = 0;
        while (*p >= '0' && *p <= '9') {
            current_profile.last_minute = current_profile.last_minute * 10 + (*p - '0');
            p++;
        }
        if (*p == '|') p++;
        current_profile.last_day = 0;
        while (*p >= '0' && *p <= '9') {
            current_profile.last_day = current_profile.last_day * 10 + (*p - '0');
            p++;
        }
        if (*p == '|') p++;
        current_profile.last_month = 0;
        while (*p >= '0' && *p <= '9') {
            current_profile.last_month = current_profile.last_month * 10 + (*p - '0');
            p++;
        }
    }
}

void user_save_profile(void) {
    if (current_user < 0) return;

    rtc_time_t t;
    rtc_read(&t);
    current_profile.last_hour = t.hour;
    current_profile.last_minute = t.minute;
    current_profile.last_day = t.day;
    current_profile.last_month = t.month;

    char fname[32];
    strcpy(fname, "profile_");
    strcat(fname, users[current_user]);
    strcat(fname, ".dat");

    char buf[128];
    char tmp[16];
    buf[0] = '\0';
    itoa(current_profile.login_count, tmp, 10);
    strcat(buf, tmp);
    strcat(buf, "|");
    itoa(current_profile.last_hour, tmp, 10);
    strcat(buf, tmp);
    strcat(buf, "|");
    itoa(current_profile.last_minute, tmp, 10);
    strcat(buf, tmp);
    strcat(buf, "|");
    itoa(current_profile.last_day, tmp, 10);
    strcat(buf, tmp);
    strcat(buf, "|");
    itoa(current_profile.last_month, tmp, 10);
    strcat(buf, tmp);

    llm_host_save(fname, buf);
}

uint32_t user_session_seconds(void) {
    if (current_profile.session_start_ticks == 0) return 0;
    uint32_t now = timer_get_ticks();
    uint32_t elapsed = now - current_profile.session_start_ticks;
    return elapsed / 100; /* timer runs at 100Hz */
}

/* ── Styled login card ──────────────────────────────────── */

static void draw_login_card(void) {
    int top = 5;
    int left = 20;
    int width = 40;
    int height = 12;

    /* Draw double-line border box */
    screen_draw_box(top, left, top + height, left + width, VGA_CYAN, VGA_BLACK, 2);

    /* Fill interior */
    screen_fill_rect(top + 1, left + 1, top + height - 1, left + width - 1,
                     ' ', VGA_WHITE, VGA_BLACK);

    /* Header bar with accent */
    for (int c = left + 1; c < left + width; c++)
        screen_put_char_at(top + 2, c, (char)196, VGA_DARK_GREY, VGA_BLACK);

    /* Swan icon + title */
    screen_put_char_at(top + 1, left + 3, (char)6, VGA_LIGHT_CYAN, VGA_BLACK);
    screen_put_str_at(top + 1, left + 5, "SWAN", VGA_WHITE, VGA_BLACK);
    screen_put_str_at(top + 1, left + 9, "OS", VGA_LIGHT_CYAN, VGA_BLACK);
    screen_put_str_at(top + 1, left + 12, "Login", VGA_DARK_GREY, VGA_BLACK);

    /* Status indicator */
    screen_put_char_at(top + 1, left + width - 4, (char)254, VGA_GREEN, VGA_BLACK);
    screen_put_str_at(top + 1, left + width - 2, " ", VGA_WHITE, VGA_BLACK);

    /* User icon and label */
    screen_put_char_at(top + 4, left + 4, (char)4, VGA_CYAN, VGA_BLACK);
    screen_put_str_at(top + 4, left + 6, "Username:", VGA_LIGHT_GREY, VGA_BLACK);

    /* Input field with bracket decorators */
    screen_put_str_at(top + 6, left + 4, "[", VGA_DARK_GREY, VGA_BLACK);
    for (int i = 0; i < 30; i++)
        screen_put_char_at(top + 6, left + 5 + i, (char)250, VGA_DARK_GREY, VGA_BLACK);
    screen_put_str_at(top + 6, left + 35, "]", VGA_DARK_GREY, VGA_BLACK);

    /* Hint */
    screen_put_str_at(top + 8, left + 4, "New account auto-created.", VGA_DARK_GREY, VGA_BLACK);
    screen_put_str_at(top + 10, left + 4, "Min 2 chars, then Enter", VGA_DARK_GREY, VGA_BLACK);

    /* Position cursor in the input field */
    screen_set_cursor(top + 6, left + 5);
}

int user_login(void) {
    char input[MAX_USERNAME];
    int top = 5;
    int left = 20;

    draw_login_card();

    /* Read input with visual feedback inside the card */
    screen_set_color(VGA_WHITE, VGA_BLACK);
    screen_set_cursor(top + 6, left + 5);

    int pos = 0;
    while (pos < MAX_USERNAME - 1) {
        char c = keyboard_getchar();
        if (c == '\n') break;
        if (c == '\b') {
            if (pos > 0) {
                pos--;
                input[pos] = '\0';
                screen_put_char_at(top + 6, left + 5 + pos, (char)250, VGA_DARK_GREY, VGA_BLACK);
                screen_set_cursor(top + 6, left + 5 + pos);
            }
            continue;
        }
        if (c >= ' ' && pos < 29) {
            input[pos] = c;
            screen_put_char_at(top + 6, left + 5 + pos, c, VGA_WHITE, VGA_BLACK);
            pos++;
            screen_set_cursor(top + 6, left + 5 + pos);
        }
    }
    input[pos] = '\0';

    char *name = trim(input);
    if (strlen(name) < 2) {
        /* Error badge */
        screen_put_str_at(top + 8, left + 4, "                          ", VGA_WHITE, VGA_BLACK);
        screen_put_char_at(top + 8, left + 4, (char)254, VGA_RED, VGA_BLACK);
        screen_put_str_at(top + 8, left + 6, "Min 2 characters!", VGA_RED, VGA_BLACK);
        screen_delay(1000);
        /* Clear card area */
        screen_fill_rect(top, left, top + 12, left + 40, ' ', VGA_WHITE, VGA_BLACK);
        return 0;
    }

    /* Find or create user */
    int idx = -1;
    int is_new = 0;
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i], name) == 0) { idx = i; break; }
    }

    if (idx < 0) {
        idx = user_register(name);
        is_new = 1;
        if (idx < 0) {
            screen_put_str_at(top + 8, left + 4, "                          ", VGA_WHITE, VGA_BLACK);
            screen_put_char_at(top + 8, left + 4, (char)254, VGA_RED, VGA_BLACK);
            screen_put_str_at(top + 8, left + 6, "Max users reached!", VGA_RED, VGA_BLACK);
            screen_delay(1000);
            screen_fill_rect(top, left, top + 12, left + 40, ' ', VGA_WHITE, VGA_BLACK);
            return 0;
        }
        /* New user badge */
        screen_put_str_at(top + 8, left + 4, "                          ", VGA_WHITE, VGA_BLACK);
        screen_put_char_at(top + 8, left + 4, (char)254, VGA_LIGHT_CYAN, VGA_BLACK);
        screen_put_str_at(top + 8, left + 6, "Account created!", VGA_LIGHT_CYAN, VGA_BLACK);
    }

    current_user = idx;

    /* Load and update profile */
    load_user_profile(name);
    current_profile.login_count++;
    current_profile.session_start_ticks = timer_get_ticks();

    /* Show welcome message with login info */
    screen_put_str_at(top + 10, left + 4, "                          ", VGA_WHITE, VGA_BLACK);
    if (is_new) {
        screen_put_char_at(top + 10, left + 4, (char)254, VGA_GREEN, VGA_BLACK);
        screen_put_str_at(top + 10, left + 6, "Welcome, ", VGA_GREEN, VGA_BLACK);
        screen_put_str_at(top + 10, left + 15, name, VGA_WHITE, VGA_BLACK);
        screen_put_str_at(top + 10, left + 15 + (int)strlen(name), "!", VGA_GREEN, VGA_BLACK);
    } else {
        screen_put_char_at(top + 10, left + 4, (char)254, VGA_GREEN, VGA_BLACK);
        screen_put_str_at(top + 10, left + 6, "Welcome back #", VGA_GREEN, VGA_BLACK);
        char cnt[8];
        itoa(current_profile.login_count, cnt, 10);
        screen_put_str_at(top + 10, left + 20, cnt, VGA_YELLOW, VGA_BLACK);
        screen_put_str_at(top + 10, left + 20 + (int)strlen(cnt), " ", VGA_WHITE, VGA_BLACK);
        screen_put_str_at(top + 10, left + 21 + (int)strlen(cnt), name, VGA_WHITE, VGA_BLACK);
    }

    /* Show last login info if returning user */
    if (!is_new && current_profile.last_day > 0) {
        char last_info[30];
        char tmp[8];
        strcpy(last_info, "Last: ");
        itoa(current_profile.last_day, tmp, 10);
        strcat(last_info, tmp);
        strcat(last_info, "/");
        itoa(current_profile.last_month, tmp, 10);
        strcat(last_info, tmp);
        strcat(last_info, " ");
        itoa(current_profile.last_hour, tmp, 10);
        if (current_profile.last_hour < 10) strcat(last_info, "0");
        strcat(last_info, tmp);
        strcat(last_info, ":");
        itoa(current_profile.last_minute, tmp, 10);
        if (current_profile.last_minute < 10) strcat(last_info, "0");
        strcat(last_info, tmp);
        screen_put_str_at(top + 8, left + 4, "                          ", VGA_WHITE, VGA_BLACK);
        screen_put_str_at(top + 8, left + 4, last_info, VGA_DARK_GREY, VGA_BLACK);
    }

    /* Save profile with updated data */
    user_save_profile();

    /* Audit log */
    audit_log(AUDIT_LOGIN, name);

    screen_delay(1200);

    /* Clear card area */
    screen_fill_rect(top, left, top + 12, left + 40, ' ', VGA_WHITE, VGA_BLACK);

    /* Reset cursor below the card area */
    screen_set_cursor(top, 0);
    screen_set_color(VGA_WHITE, VGA_BLACK);

    return 1;
}
