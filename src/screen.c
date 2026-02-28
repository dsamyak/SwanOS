/* ============================================================
 * SwanOS — VGA Text Mode Driver
 * 80×25, 16 colors, hardware cursor, positioned drawing
 * ============================================================ */

#include "screen.h"
#include "ports.h"
#include "string.h"

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((uint16_t *)0xB8000)

static int cursor_row = 0;
static int cursor_col = 0;
static uint8_t current_color = 0x0F;

static inline uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)(unsigned char)c | ((uint16_t)color << 8);
}

static void update_cursor(void) {
    uint16_t pos = cursor_row * VGA_WIDTH + cursor_col;
    outb(0x3D4, 14);
    outb(0x3D5, (uint8_t)(pos >> 8));
    outb(0x3D4, 15);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
}

static void scroll(void) {
    if (cursor_row < VGA_HEIGHT) return;
    for (int i = 0; i < (VGA_HEIGHT - 1) * VGA_WIDTH; i++)
        VGA_MEMORY[i] = VGA_MEMORY[i + VGA_WIDTH];
    for (int i = 0; i < VGA_WIDTH; i++)
        VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + i] = vga_entry(' ', current_color);
    cursor_row = VGA_HEIGHT - 1;
}

void screen_init(void) {
    current_color = 0x0F;
    screen_clear();
}

void screen_clear(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        VGA_MEMORY[i] = vga_entry(' ', current_color);
    cursor_row = 0; cursor_col = 0;
    update_cursor();
}

void screen_set_color(uint8_t fg, uint8_t bg) {
    current_color = (bg << 4) | (fg & 0x0F);
}

void screen_putchar(char c) {
    if (c == '\n') { cursor_col = 0; cursor_row++; }
    else if (c == '\r') { cursor_col = 0; }
    else if (c == '\t') { cursor_col = (cursor_col + 4) & ~3; }
    else {
        VGA_MEMORY[cursor_row * VGA_WIDTH + cursor_col] = vga_entry(c, current_color);
        cursor_col++;
    }
    if (cursor_col >= VGA_WIDTH) { cursor_col = 0; cursor_row++; }
    scroll();
    update_cursor();
}

void screen_print(const char *str) {
    while (*str) screen_putchar(*str++);
}

void screen_print_color(const char *str, uint8_t fg, uint8_t bg) {
    uint8_t prev = current_color;
    screen_set_color(fg, bg);
    screen_print(str);
    current_color = prev;
}

void screen_print_at(const char *str, int row, int col) {
    int old_row = cursor_row, old_col = cursor_col;
    cursor_row = row; cursor_col = col;
    screen_print(str);
    cursor_row = old_row; cursor_col = old_col;
    update_cursor();
}

void screen_newline(void) {
    cursor_col = 0; cursor_row++;
    scroll(); update_cursor();
}

void screen_backspace(void) {
    if (cursor_col > 0) cursor_col--;
    else if (cursor_row > 0) { cursor_row--; cursor_col = VGA_WIDTH - 1; }
    VGA_MEMORY[cursor_row * VGA_WIDTH + cursor_col] = vga_entry(' ', current_color);
    update_cursor();
}

int screen_get_row(void) { return cursor_row; }
int screen_get_col(void) { return cursor_col; }

/* ── Positioned Drawing ─────────────────────────────────── */

void screen_put_char_at(int row, int col, char c, uint8_t fg, uint8_t bg) {
    if (row < 0 || row >= VGA_HEIGHT || col < 0 || col >= VGA_WIDTH) return;
    uint8_t color = (bg << 4) | (fg & 0x0F);
    VGA_MEMORY[row * VGA_WIDTH + col] = vga_entry(c, color);
}

void screen_put_str_at(int row, int col, const char *str, uint8_t fg, uint8_t bg) {
    uint8_t color = (bg << 4) | (fg & 0x0F);
    while (*str && col < VGA_WIDTH) {
        if (row >= 0 && row < VGA_HEIGHT)
            VGA_MEMORY[row * VGA_WIDTH + col] = vga_entry(*str, color);
        str++; col++;
    }
}

void screen_fill_row(int row, int col_start, int col_end, char c, uint8_t fg, uint8_t bg) {
    uint8_t color = (bg << 4) | (fg & 0x0F);
    for (int col = col_start; col <= col_end && col < VGA_WIDTH; col++)
        VGA_MEMORY[row * VGA_WIDTH + col] = vga_entry(c, color);
}

void screen_fill_rect(int r1, int c1, int r2, int c2, char c, uint8_t fg, uint8_t bg) {
    for (int r = r1; r <= r2; r++)
        screen_fill_row(r, c1, c2, c, fg, bg);
}

void screen_draw_box(int r1, int c1, int r2, int c2, uint8_t fg, uint8_t bg, int style) {
    char h, v, tl, tr, bl, br;
    if (style == 1) { /* single */
        h = (char)BOX_H; v = (char)BOX_V;
        tl = (char)BOX_TL; tr = (char)BOX_TR;
        bl = (char)BOX_BL; br = (char)BOX_BR;
    } else { /* double */
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
    cursor_row = row; cursor_col = col;
    update_cursor();
}

void screen_hide_cursor(void) {
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x20); /* bit 5 disables cursor */
}

void screen_show_cursor(void) {
    outb(0x3D4, 0x0A);
    outb(0x3D5, (inb(0x3D5) & 0xC0) | 13); /* cursor start scanline */
    outb(0x3D4, 0x0B);
    outb(0x3D5, (inb(0x3D5) & 0xE0) | 15); /* cursor end scanline */
}
