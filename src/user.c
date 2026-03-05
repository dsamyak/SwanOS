/* ============================================================
 * SwanOS — User Login Manager
 * Styled login card with box-drawing borders.
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
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i], name) == 0) { idx = i; break; }
    }

    if (idx < 0) {
        idx = user_register(name);
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

    /* Success badge */
    screen_put_str_at(top + 10, left + 4, "                          ", VGA_WHITE, VGA_BLACK);
    screen_put_char_at(top + 10, left + 4, (char)254, VGA_GREEN, VGA_BLACK);
    screen_put_str_at(top + 10, left + 6, "Welcome, ", VGA_GREEN, VGA_BLACK);
    screen_put_str_at(top + 10, left + 15, name, VGA_WHITE, VGA_BLACK);
    screen_put_str_at(top + 10, left + 15 + (int)strlen(name), "!", VGA_GREEN, VGA_BLACK);

    screen_delay(800);

    /* Clear card area */
    screen_fill_rect(top, left, top + 12, left + 40, ' ', VGA_WHITE, VGA_BLACK);

    /* Reset cursor below the card area */
    screen_set_cursor(top, 0);
    screen_set_color(VGA_WHITE, VGA_BLACK);

    return 1;
}
