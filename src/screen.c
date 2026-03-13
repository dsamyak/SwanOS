/* ============================================================
 * SwanOS — VESA Text Mode CLI Driver
 * Scaled 2x text drawing over native 1920x1080 Framebuffer.
 * ============================================================ */

#include "screen.h"
#include "vga_gfx.h"
#include "string.h"
#include "serial.h"

#define CHAR_W 16
#define CHAR_H 16
#define MAX_COLS 120 // 1920 / 16
#define MAX_ROWS 67  // 1080 / 16

typedef struct {
    char c;
    uint32_t fg;
    uint32_t bg;
} tchar_t;

static tchar_t tbuf[MAX_COLS * MAX_ROWS];
static int cols = 80;
static int rows = 25;

static int cursor_row = 0;
static int cursor_col = 0;

static uint32_t current_fg = 0xFFFFFFFF;
static uint32_t current_bg = 0xFF000000;
static int serial_mirror = 1;

static uint32_t ansi_colors[16] = {
    0xFF000000, 0xFF0000AA, 0xFF00AA00, 0xFF00AAAA,
    0xFFAA0000, 0xFFAA00AA, 0xFFAA5500, 0xFFAAAAAA,
    0xFF555555, 0xFF5555FF, 0xFF55FF55, 0xFF55FFFF,
    0xFFFF5555, 0xFFFF55FF, 0xFFFFFF55, 0xFFFFFFFF
};

static void redraw_char(int r, int c) {
    if (r < 0 || r >= rows || c < 0 || c >= cols) return;
    tchar_t t = tbuf[r * cols + c];
    vga_fill_rect(c * CHAR_W, r * CHAR_H, CHAR_W, CHAR_H, t.bg);
    if (t.c) {
        vga_draw_char_2x(c * CHAR_W, r * CHAR_H, t.c, t.fg);
    }
}

static void update_cursor(void) {
    /* Draw a simple block cursor since we don't have hardware cursor */
    /* Draw over the current char */
    if (cursor_row >= rows || cursor_col >= cols) return;
    tchar_t t = tbuf[cursor_row * cols + cursor_col];
    vga_fill_rect(cursor_col * CHAR_W, cursor_row * CHAR_H, CHAR_W, CHAR_H, 0xAAAAAAAA);
    if (t.c) {
        vga_draw_char_2x(cursor_col * CHAR_W, cursor_row * CHAR_H, t.c, 0xFF000000);
    }
}

static void clear_cursor(void) {
    redraw_char(cursor_row, cursor_col);
}

static void scroll(void) {
    if (cursor_row < rows) return;
    clear_cursor();
    
    // Move buffer up
    for (int r = 0; r < rows - 1; r++) {
        for (int c = 0; c < cols; c++) {
            tbuf[r * cols + c] = tbuf[(r + 1) * cols + c];
        }
    }
    // Clear last line
    for (int c = 0; c < cols; c++) {
        tbuf[(rows - 1) * cols + c].c = ' ';
        tbuf[(rows - 1) * cols + c].fg = current_fg;
        tbuf[(rows - 1) * cols + c].bg = current_bg;
    }
    
    // Redraw entire screen
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            redraw_char(r, c);
        }
    }
    
    cursor_row = rows - 1;
}

void screen_init(void) {
    cols = GFX_W / CHAR_W;
    rows = GFX_H / CHAR_H;
    if (cols > MAX_COLS) cols = MAX_COLS;
    if (rows > MAX_ROWS) rows = MAX_ROWS;
    current_fg = ansi_colors[15];
    current_bg = ansi_colors[0];
    screen_clear();
}

void screen_clear(void) {
    for (int i = 0; i < cols * rows; i++) {
        tbuf[i].c = ' ';
        tbuf[i].fg = current_fg;
        tbuf[i].bg = current_bg;
    }
    cursor_row = 0; cursor_col = 0;
    vga_clear(current_bg);
    update_cursor();
}

void screen_set_color(uint8_t fg, uint8_t bg) {
    current_fg = ansi_colors[fg & 0x0F];
    current_bg = ansi_colors[bg & 0x0F];

    if (serial_mirror) {
        serial_putchar('\x1b');
        serial_putchar('[');
        static const uint8_t vga_to_ansi[] = {
            30, 34, 32, 36, 31, 35, 33, 37,
            90, 94, 92, 96, 91, 95, 93, 97
        };
        uint8_t a_fg = vga_to_ansi[fg & 0x0F];
        if (a_fg >= 90) {
            serial_putchar('0' + (a_fg / 10));
            serial_putchar('0' + (a_fg % 10));
        } else {
            serial_putchar('0' + (a_fg / 10));
            serial_putchar('0' + (a_fg % 10));
        }
        serial_putchar('m');
    }
}

void screen_putchar(char c) {
    clear_cursor();
    if (c == '\n') { cursor_col = 0; cursor_row++; }
    else if (c == '\r') { cursor_col = 0; }
    else if (c == '\t') { cursor_col = (cursor_col + 4) & ~3; }
    else {
        int idx = cursor_row * cols + cursor_col;
        tbuf[idx].c = c;
        tbuf[idx].fg = current_fg;
        tbuf[idx].bg = current_bg;
        redraw_char(cursor_row, cursor_col);
        cursor_col++;
    }
    if (cursor_col >= cols) { cursor_col = 0; cursor_row++; }
    scroll();
    update_cursor();

    if (serial_mirror) {
        serial_putchar(c);
    }
}

void screen_print(const char *str) {
    while (*str) screen_putchar(*str++);
}

void screen_print_color(const char *str, uint8_t fg, uint8_t bg) {
    uint32_t prev_fg = current_fg;
    uint32_t prev_bg = current_bg;
    screen_set_color(fg, bg);
    screen_print(str);
    current_fg = prev_fg;
    current_bg = prev_bg;
}

void screen_print_at(const char *str, int row, int col) {
    clear_cursor();
    int old_row = cursor_row, old_col = cursor_col;
    cursor_row = row; cursor_col = col;
    while (*str && cursor_col < cols) {
        int idx = cursor_row * cols + cursor_col;
        tbuf[idx].c = *str;
        tbuf[idx].fg = current_fg;
        tbuf[idx].bg = current_bg;
        redraw_char(cursor_row, cursor_col);
        cursor_col++; str++;
    }
    cursor_row = old_row; cursor_col = old_col;
    update_cursor();
}

void screen_newline(void) {
    clear_cursor();
    cursor_col = 0; cursor_row++;
    scroll(); update_cursor();
    if (serial_mirror) serial_putchar('\n');
}

void screen_backspace(void) {
    clear_cursor();
    if (cursor_col > 0) cursor_col--;
    else if (cursor_row > 0) { cursor_row--; cursor_col = cols - 1; }
    int idx = cursor_row * cols + cursor_col;
    tbuf[idx].c = ' ';
    tbuf[idx].fg = current_fg;
    tbuf[idx].bg = current_bg;
    redraw_char(cursor_row, cursor_col);
    update_cursor();
    if (serial_mirror) serial_putchar('\b');
}

int screen_get_row(void) { return cursor_row; }
int screen_get_col(void) { return cursor_col; }

void screen_put_char_at(int row, int col, char c, uint8_t fg, uint8_t bg) {
    if (row < 0 || row >= rows || col < 0 || col >= cols) return;
    int idx = row * cols + col;
    tbuf[idx].c = c;
    tbuf[idx].fg = ansi_colors[fg & 0x0F];
    tbuf[idx].bg = ansi_colors[bg & 0x0F];
    redraw_char(row, col);
}

void screen_put_str_at(int row, int col, const char *str, uint8_t fg, uint8_t bg) {
    while (*str && col < cols) {
        if (row >= 0 && row < rows) {
            int idx = row * cols + col;
            tbuf[idx].c = *str;
            tbuf[idx].fg = ansi_colors[fg & 0x0F];
            tbuf[idx].bg = ansi_colors[bg & 0x0F];
            redraw_char(row, col);
        }
        str++; col++;
    }
}

void screen_fill_row(int row, int col_start, int col_end, char c, uint8_t fg, uint8_t bg) {
    for (int col = col_start; col <= col_end && col < cols; col++) {
        screen_put_char_at(row, col, c, fg, bg);
    }
}

void screen_fill_rect(int r1, int c1, int r2, int c2, char c, uint8_t fg, uint8_t bg) {
    for (int r = r1; r <= r2; r++)
        screen_fill_row(r, c1, c2, c, fg, bg);
}

void screen_draw_box(int r1, int c1, int r2, int c2, uint8_t fg, uint8_t bg, int style) {
    char h, v, tl, tr, bl, br;
    if (style == 1) { 
        h = (char)BOX_H; v = (char)BOX_V;
        tl = (char)BOX_TL; tr = (char)BOX_TR;
        bl = (char)BOX_BL; br = (char)BOX_BR;
    } else { 
        h = (char)BOX_DH; v = (char)BOX_DV;
        tl = (char)BOX_DTL; tr = (char)BOX_DTR;
        bl = (char)BOX_DBL; br = (char)BOX_DBR;
    }

    screen_put_char_at(r1, c1, tl, fg, bg);
    screen_put_char_at(r1, c2, tr, fg, bg);
    screen_put_char_at(r2, c1, bl, fg, bg);
    screen_put_char_at(r2, c2, br, fg, bg);

    for (int col = c1+1; col < c2; col++) {
        screen_put_char_at(r1, col, h, fg, bg);
        screen_put_char_at(r2, col, h, fg, bg);
    }
    for (int row = r1+1; row < r2; row++) {
        screen_put_char_at(row, c1, v, fg, bg);
        screen_put_char_at(row, c2, v, fg, bg);
    }
}

void screen_set_cursor(int row, int col) {
    clear_cursor();
    cursor_row = row; cursor_col = col;
    update_cursor();
}

void screen_hide_cursor(void) {
    clear_cursor();
}

void screen_show_cursor(void) {
    update_cursor();
}

void screen_delay(int ms) {
    for (int i = 0; i < ms; i++) {
        for (volatile int j = 0; j < 4000; j++) {
            __asm__ volatile ("nop");
        }
    }
}

void screen_set_serial_mirror(int enable) {
    serial_mirror = enable;
}
