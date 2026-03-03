/* ============================================================
 * SwanOS — Built-in Snake Game
 * Classic snake game using VGA text-mode box-drawing chars.
 * ============================================================ */

#include "game.h"
#include "screen.h"
#include "keyboard.h"
#include "timer.h"
#include "string.h"
#include "ports.h"

/* ── Game area ────────────────────────────────────────────── */
#define GW  40   /* game width in chars */
#define GH  20   /* game height in chars */
#define GX  20   /* left offset */
#define GY  2    /* top offset */
#define MAX_SNAKE 200

static int snake_x[MAX_SNAKE], snake_y[MAX_SNAKE];
static int snake_len, snake_dir; /* 0=up 1=right 2=down 3=left */
static int food_x, food_y;
static int game_score;
static int game_over;

/* Simple pseudo-random using timer ticks */
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
    /* Top and bottom */
    for (int x = 0; x < GW; x++) {
        screen_put_char_at(GY, GX + x, (char)BOX_DH, VGA_CYAN, VGA_BLACK);
        screen_put_char_at(GY + GH - 1, GX + x, (char)BOX_DH, VGA_CYAN, VGA_BLACK);
    }
    /* Left and right */
    for (int y = 0; y < GH; y++) {
        screen_put_char_at(GY + y, GX, (char)BOX_DV, VGA_CYAN, VGA_BLACK);
        screen_put_char_at(GY + y, GX + GW - 1, (char)BOX_DV, VGA_CYAN, VGA_BLACK);
    }
    /* Corners */
    screen_put_char_at(GY, GX, (char)BOX_DTL, VGA_CYAN, VGA_BLACK);
    screen_put_char_at(GY, GX + GW - 1, (char)BOX_DTR, VGA_CYAN, VGA_BLACK);
    screen_put_char_at(GY + GH - 1, GX, (char)BOX_DBL, VGA_CYAN, VGA_BLACK);
    screen_put_char_at(GY + GH - 1, GX + GW - 1, (char)BOX_DBR, VGA_CYAN, VGA_BLACK);
}

static void draw_score(void) {
    char buf[32];
    screen_put_str_at(GY - 1, GX, "SNAKE", VGA_GREEN, VGA_BLACK);
    screen_put_str_at(GY - 1, GX + 8, "Score: ", VGA_WHITE, VGA_BLACK);
    itoa(game_score, buf, 10);
    screen_put_str_at(GY - 1, GX + 15, buf, VGA_YELLOW, VGA_BLACK);
    screen_put_str_at(GY - 1, GX + 20, "   ", VGA_BLACK, VGA_BLACK); /* clear trailing */

    /* Controls hint */
    screen_put_str_at(GY + GH, GX, "WASD=Move  Q=Quit", VGA_DARK_GREY, VGA_BLACK);
}

static void draw_game(void) {
    /* Clear game area */
    for (int y = 1; y < GH - 1; y++)
        for (int x = 1; x < GW - 1; x++)
            screen_put_char_at(GY + y, GX + x, ' ', VGA_BLACK, VGA_BLACK);

    /* Draw food */
    screen_put_char_at(GY + food_y, GX + food_x, (char)DIAMOND, VGA_RED, VGA_BLACK);

    /* Draw snake */
    for (int i = 0; i < snake_len; i++) {
        char ch;
        uint8_t color;
        if (i == 0) {
            /* Head — use arrow based on direction */
            ch = (char)BLOCK_FULL;
            color = VGA_GREEN;
        } else {
            ch = (char)BLOCK_MED;
            color = VGA_LIGHT_GREEN;
        }
        screen_put_char_at(GY + snake_y[i], GX + snake_x[i], ch, color, VGA_BLACK);
    }
}

/* Check for key without blocking */
static volatile char kb_peek_buf;
static volatile int  kb_peek_valid = 0;

static int key_available(void) {
    /* Check PS/2 status register for data */
    return (inb(0x64) & 1);
}

static char try_getkey(void) {
    /* Non-blocking: check if a keyboard scancode is available */
    if (!(inb(0x64) & 1)) return 0;

    uint8_t sc = inb(0x60);
    if (sc & 0x80) return 0; /* release event */

    /* Minimal scancode→char for game controls */
    switch (sc) {
        case 0x11: return 'w';  /* W */
        case 0x1E: return 'a';  /* A */
        case 0x1F: return 's';  /* S */
        case 0x20: return 'd';  /* D */
        case 0x10: return 'q';  /* Q */
        case 0x48: return 'w';  /* Up arrow */
        case 0x4B: return 'a';  /* Left arrow */
        case 0x50: return 's';  /* Down arrow */
        case 0x4D: return 'd';  /* Right arrow */
        default: return 0;
    }
}

void game_snake(void) {
    screen_hide_cursor();
    screen_clear();

    /* Initialize snake in center */
    snake_len = 4;
    snake_dir = 1; /* moving right */
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

    /* Flush keyboard buffer */
    while (inb(0x64) & 1) inb(0x60);

    uint32_t last_tick = timer_get_ticks();
    int speed = 8; /* ticks between moves (lower = faster) */

    while (!game_over) {
        /* Input (non-blocking) */
        char key = try_getkey();
        if (key == 'q') break;
        if (key == 'w' && snake_dir != 2) snake_dir = 0;
        if (key == 'd' && snake_dir != 3) snake_dir = 1;
        if (key == 's' && snake_dir != 0) snake_dir = 2;
        if (key == 'a' && snake_dir != 1) snake_dir = 3;

        /* Tick-based movement */
        uint32_t now = timer_get_ticks();
        if (now - last_tick < (uint32_t)speed) {
            __asm__ volatile ("hlt");
            continue;
        }
        last_tick = now;

        /* Calculate new head position */
        int nx = snake_x[0], ny = snake_y[0];
        switch (snake_dir) {
            case 0: ny--; break; /* up */
            case 1: nx++; break; /* right */
            case 2: ny++; break; /* down */
            case 3: nx--; break; /* left */
        }

        /* Wall collision */
        if (nx <= 0 || nx >= GW - 1 || ny <= 0 || ny >= GH - 1) {
            game_over = 1;
            break;
        }

        /* Self collision */
        for (int i = 0; i < snake_len; i++) {
            if (snake_x[i] == nx && snake_y[i] == ny) {
                game_over = 1;
                break;
            }
        }
        if (game_over) break;

        /* Check food */
        int ate = (nx == food_x && ny == food_y);

        /* Move snake: shift body backward */
        if (!ate) {
            /* Remove tail */
            if (snake_len > 0) {
                /* Shift from the end */
            }
        } else {
            /* Grow */
            if (snake_len < MAX_SNAKE) snake_len++;
            game_score += 10;
            /* Speed up gradually */
            if (speed > 3 && game_score % 50 == 0) speed--;
        }

        /* Shift body */
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

    /* Game over screen */
    if (game_over) {
        screen_put_str_at(GY + GH / 2 - 1, GX + GW / 2 - 5, "GAME OVER!", VGA_RED, VGA_BLACK);
        char buf[32];
        strcpy(buf, "Score: ");
        char nb[16];
        itoa(game_score, nb, 10);
        strcat(buf, nb);
        screen_put_str_at(GY + GH / 2 + 1, GX + GW / 2 - (int)(strlen(buf) / 2), buf, VGA_YELLOW, VGA_BLACK);
        screen_put_str_at(GY + GH / 2 + 3, GX + GW / 2 - 9, "Press any key...", VGA_DARK_GREY, VGA_BLACK);

        /* Flush and wait for keypress */
        while (inb(0x64) & 1) inb(0x60);
        keyboard_getchar();
    }

    /* Flush remaining input */
    while (inb(0x64) & 1) inb(0x60);

    screen_show_cursor();
    screen_clear();
}
