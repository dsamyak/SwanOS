/* ============================================================
 * SwanOS — Built-in Snake Game
 * Classic snake game using VGA text-mode box-drawing chars.
 * Visually enhanced with arcade-style borders and modal.
 * ============================================================ */

#include "game.h"
#include "screen.h"
#include "keyboard.h"
#include "timer.h"
#include "string.h"
#include "ports.h"

/* ── Game area ────────────────────────────────────────────── */
#define GW  44   /* game width in chars */
#define GH  20   /* game height in chars */
#define GX  18   /* left offset */
#define GY  2    /* top offset */
#define MAX_SNAKE 250

static int snake_x[MAX_SNAKE], snake_y[MAX_SNAKE];
static int snake_len, snake_dir; /* 0=up 1=right 2=down 3=left */
static int food_x, food_y;
static int game_score;
static int game_over;

static int rand_range(int lo, int hi) {
    static uint32_t seed = 12345;
    seed = seed * 1103515245 + 12345 + timer_get_ticks();
    int range = hi - lo + 1;
    return lo + (int)((seed >> 16) % (unsigned)range);
}

static void place_food(void) {
    int valid = 0;
    while (!valid) {
        food_x = rand_range(1, GW - 2);
        food_y = rand_range(1, GH - 2);
        valid = 1;
        for (int i = 0; i < snake_len; i++) {
            if (snake_x[i] == food_x && snake_y[i] == food_y) {
                valid = 0;
                break;
            }
        }
    }
}

static void draw_border(void) {
    /* Drop shadow (dark grey block chars) */
    for (int y = 1; y <= GH; y++) {
        screen_put_char_at(GY + y, GX + GW, (char)219, VGA_DARK_GREY, VGA_BLACK);
        screen_put_char_at(GY + y, GX + GW + 1, (char)219, VGA_DARK_GREY, VGA_BLACK);
    }
    for (int x = 2; x <= GW + 1; x++) {
        screen_put_char_at(GY + GH, GX + x, (char)223, VGA_DARK_GREY, VGA_BLACK);
    }

    /* Double-line borders */
    screen_put_char_at(GY, GX, (char)201, VGA_CYAN, VGA_BLACK);
    for (int x = 1; x < GW - 1; x++) screen_put_char_at(GY, GX + x, (char)205, VGA_CYAN, VGA_BLACK);
    screen_put_char_at(GY, GX + GW - 1, (char)187, VGA_CYAN, VGA_BLACK);

    for (int y = 1; y < GH - 1; y++) {
        screen_put_char_at(GY + y, GX, (char)186, VGA_CYAN, VGA_BLACK);
        screen_put_char_at(GY + y, GX + GW - 1, (char)186, VGA_CYAN, VGA_BLACK);
    }

    screen_put_char_at(GY + GH - 1, GX, (char)200, VGA_CYAN, VGA_BLACK);
    for (int x = 1; x < GW - 1; x++) screen_put_char_at(GY + GH - 1, GX + x, (char)205, VGA_CYAN, VGA_BLACK);
    screen_put_char_at(GY + GH - 1, GX + GW - 1, (char)188, VGA_CYAN, VGA_BLACK);

    /* Arcade Header */
    screen_put_char_at(GY, GX + 13, (char)185, VGA_CYAN, VGA_BLACK);
    screen_put_str_at(GY, GX + 14, "   S N A K E   ", VGA_LIGHT_CYAN, VGA_BLACK);
    screen_put_char_at(GY, GX + 29, (char)204, VGA_CYAN, VGA_BLACK);
}

static void draw_score(void) {
    char buf[16];
    screen_put_str_at(GY + GH, GX + 2, " ", VGA_BLACK, VGA_BLACK);
    screen_put_char_at(GY + GH, GX + 3, (char)254, VGA_GREEN, VGA_BLACK);
    screen_put_str_at(GY + GH, GX + 5, "Score: ", VGA_WHITE, VGA_BLACK);
    itoa(game_score, buf, 10);
    screen_put_str_at(GY + GH, GX + 12, buf, VGA_YELLOW, VGA_BLACK);
    screen_put_str_at(GY + GH, GX + 12 + strlen(buf), "   ", VGA_BLACK, VGA_BLACK);
    screen_put_str_at(GY + GH, GX + 24, "[WASD] Move   [Q] Quit ", VGA_DARK_GREY, VGA_BLACK);
}

static void draw_game(void) {
    /* Clear game area */
    for (int y = 1; y < GH - 1; y++)
        for (int x = 1; x < GW - 1; x++)
            screen_put_char_at(GY + y, GX + x, ' ', VGA_BLACK, VGA_BLACK);

    /* Draw food: red diamond */
    screen_put_char_at(GY + food_y, GX + food_x, (char)4, VGA_RED, VGA_BLACK);

    /* Draw snake */
    for (int i = 0; i < snake_len; i++) {
        char ch; uint8_t color;
        if (i == 0) {
            ch = (char)219; /* head */
            color = VGA_GREEN;
        } else {
            ch = (char)178; /* segmented body */
            color = VGA_LIGHT_GREEN;
        }
        screen_put_char_at(GY + snake_y[i], GX + snake_x[i], ch, color, VGA_BLACK);
    }
}

static int key_available(void) {
    return (inb(0x64) & 1);
}

static char try_getkey(void) {
    if (!(inb(0x64) & 1)) return 0;
    uint8_t sc = inb(0x60);
    if (sc & 0x80) return 0;

    switch (sc) {
        case 0x11: return 'w';
        case 0x1E: return 'a';
        case 0x1F: return 's';
        case 0x20: return 'd';
        case 0x10: return 'q';
        case 0x48: return 'w';
        case 0x4B: return 'a';
        case 0x50: return 's';
        case 0x4D: return 'd';
        default: return 0;
    }
}

void game_snake(void) {
    screen_hide_cursor();
    screen_clear();

    snake_len = 5;
    snake_dir = 1;
    game_score = 0;
    game_over = 0;

    for (int i = 0; i < snake_len; i++) {
        snake_x[i] = GW / 2 - i;
        snake_y[i] = GH / 2;
    }

    place_food();
    draw_border();
    draw_score();
    draw_game();

    while (inb(0x64) & 1) inb(0x60);

    uint32_t last_tick = timer_get_ticks();
    int speed = 8; 

    while (!game_over) {
        char key = try_getkey();
        if (key == 'q') break;
        if (key == 'w' && snake_dir != 2) snake_dir = 0;
        if (key == 'd' && snake_dir != 3) snake_dir = 1;
        if (key == 's' && snake_dir != 0) snake_dir = 2;
        if (key == 'a' && snake_dir != 1) snake_dir = 3;

        uint32_t now = timer_get_ticks();
        if (now - last_tick < (uint32_t)speed) {
            __asm__ volatile ("hlt");
            continue;
        }
        last_tick = now;

        int nx = snake_x[0], ny = snake_y[0];
        switch (snake_dir) {
            case 0: ny--; break;
            case 1: nx++; break;
            case 2: ny++; break;
            case 3: nx--; break;
        }

        if (nx <= 0 || nx >= GW - 1 || ny <= 0 || ny >= GH - 1) {
            game_over = 1;
            break;
        }

        for (int i = 0; i < snake_len; i++) {
            if (snake_x[i] == nx && snake_y[i] == ny) {
                game_over = 1;
                break;
            }
        }
        if (game_over) break;

        int ate = (nx == food_x && ny == food_y);

        if (ate) {
            if (snake_len < MAX_SNAKE) snake_len++;
            game_score += 10;
            if (speed > 3 && game_score % 40 == 0) speed--;
        }

        for (int i = snake_len - 1; i > 0; i--) {
            snake_x[i] = snake_x[i - 1];
            snake_y[i] = snake_y[i - 1];
        }
        snake_x[0] = nx;
        snake_y[0] = ny;

        if (ate) place_food();

        draw_game();
        draw_score();
    }

    if (game_over) {
        int m_left = GX + (GW / 2) - 10;
        int m_top = GY + (GH / 2) - 3;

        /* Game Over Modal Drop Shadow */
        screen_fill_rect(m_top + 1, m_left + 1, m_top + 6, m_left + 21, ' ', VGA_BLACK, VGA_DARK_GREY);

        /* Model Box */
        screen_draw_box(m_top, m_left, m_top + 5, m_left + 20, VGA_RED, VGA_BLACK, 2);
        screen_fill_rect(m_top + 1, m_left + 1, m_top + 4, m_left + 19, ' ', VGA_WHITE, VGA_BLACK);

        screen_put_str_at(m_top + 1, m_left + 5, " GAME OVER ", VGA_LIGHT_CYAN, VGA_RED);
        
        char buf[32], nb[16];
        strcpy(buf, "Score: ");
        itoa(game_score, nb, 10);
        strcat(buf, nb);
        screen_put_str_at(m_top + 3, m_left + 10 - (int)(strlen(buf) / 2), buf, VGA_YELLOW, VGA_BLACK);

        screen_put_str_at(m_top + 4, m_left + 4, "[Any Key to Quit]", VGA_DARK_GREY, VGA_BLACK);

        while (inb(0x64) & 1) inb(0x60);
        keyboard_getchar();
    }

    while (inb(0x64) & 1) inb(0x60);

    screen_show_cursor();
    screen_clear();
}
