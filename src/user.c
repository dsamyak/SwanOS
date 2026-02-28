/* ============================================================
 * SwanOS — User Login Manager
 * Simple multi-user system (no passwords for bare-metal demo).
 * Users get created on first login.
 * ============================================================ */

#include "user.h"
#include "screen.h"
#include "keyboard.h"
#include "string.h"

static char users[MAX_USERS][MAX_USERNAME];
static int  user_count = 0;
static int  current_user = -1;

void user_init(void) {
    memset(users, 0, sizeof(users));
    user_count = 0;
    current_user = -1;
}

int user_register(const char *username) {
    if (user_count >= MAX_USERS) return -1;
    if (strlen(username) < 2 || strlen(username) >= MAX_USERNAME) return -1;

    /* Check if exists */
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i], username) == 0) return i;
    }

    strcpy(users[user_count], username);
    return user_count++;
}

const char *user_current(void) {
    if (current_user < 0) return "guest";
    return users[current_user];
}

int user_login(void) {
    char input[MAX_USERNAME];

    screen_set_color(VGA_CYAN, VGA_BLACK);
    screen_print("\n  Enter username: ");
    screen_set_color(VGA_WHITE, VGA_BLACK);

    keyboard_read_line(input, MAX_USERNAME);

    char *name = trim(input);
    if (strlen(name) < 2) {
        screen_set_color(VGA_RED, VGA_BLACK);
        screen_print("  Username must be at least 2 characters.\n");
        screen_set_color(VGA_WHITE, VGA_BLACK);
        return 0;
    }

    /* Find or create user */
    int idx = -1;
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i], name) == 0) { idx = i; break; }
    }

    if (idx < 0) {
        /* New user */
        idx = user_register(name);
        if (idx < 0) {
            screen_set_color(VGA_RED, VGA_BLACK);
            screen_print("  Cannot create user (max reached).\n");
            screen_set_color(VGA_WHITE, VGA_BLACK);
            return 0;
        }
        screen_set_color(VGA_GREEN, VGA_BLACK);
        screen_print("  Account created! ");
        screen_set_color(VGA_WHITE, VGA_BLACK);
    }

    current_user = idx;

    screen_set_color(VGA_GREEN, VGA_BLACK);
    screen_print("  Welcome, ");
    screen_print(name);
    screen_print("!\n");
    screen_set_color(VGA_WHITE, VGA_BLACK);

    return 1;
}
