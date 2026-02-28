#ifndef SCREEN_H
#define SCREEN_H

#include <stdint.h>

/* VGA colors */
enum vga_color {
    VGA_BLACK = 0, VGA_BLUE, VGA_GREEN, VGA_CYAN,
    VGA_RED, VGA_MAGENTA, VGA_BROWN, VGA_LIGHT_GREY,
    VGA_DARK_GREY, VGA_LIGHT_BLUE, VGA_LIGHT_GREEN, VGA_LIGHT_CYAN,
    VGA_LIGHT_RED, VGA_LIGHT_MAGENTA, VGA_YELLOW, VGA_WHITE,
};

/* CP437 box-drawing characters */
#define BOX_H       196  /* ─ */
#define BOX_V       179  /* │ */
#define BOX_TL      218  /* ┌ */
#define BOX_TR      191  /* ┐ */
#define BOX_BL      192  /* └ */
#define BOX_BR      217  /* ┘ */
#define BOX_LT      195  /* ├ */
#define BOX_RT      180  /* ┤ */
#define BOX_TT      194  /* ┬ */
#define BOX_BT      193  /* ┴ */
#define BOX_CROSS   197  /* ┼ */

#define BOX_DH      205  /* ═ */
#define BOX_DV      186  /* ║ */
#define BOX_DTL     201  /* ╔ */
#define BOX_DTR     187  /* ╗ */
#define BOX_DBL     200  /* ╚ */
#define BOX_DBR     188  /* ╝ */
#define BOX_DLT     204  /* ╠ */
#define BOX_DRT     185  /* ╣ */
#define BOX_DTT     203  /* ╦ */
#define BOX_DBT     202  /* ╩ */

#define BLOCK_FULL  219  /* █ */
#define BLOCK_DARK  178  /* ▓ */
#define BLOCK_MED   177  /* ▒ */
#define BLOCK_LIGHT 176  /* ░ */
#define ARROW_RIGHT 16   /* ► */
#define BULLET      254  /* ■ */

void screen_init(void);
void screen_clear(void);
void screen_putchar(char c);
void screen_print(const char *str);
void screen_print_color(const char *str, uint8_t fg, uint8_t bg);
void screen_print_at(const char *str, int row, int col);
void screen_set_color(uint8_t fg, uint8_t bg);
void screen_newline(void);
void screen_backspace(void);
int  screen_get_row(void);
int  screen_get_col(void);

/* Positioned drawing for GUI */
void screen_put_char_at(int row, int col, char c, uint8_t fg, uint8_t bg);
void screen_put_str_at(int row, int col, const char *str, uint8_t fg, uint8_t bg);
void screen_fill_row(int row, int col_start, int col_end, char c, uint8_t fg, uint8_t bg);
void screen_fill_rect(int r1, int c1, int r2, int c2, char c, uint8_t fg, uint8_t bg);
void screen_draw_box(int r1, int c1, int r2, int c2, uint8_t fg, uint8_t bg, int style);
void screen_set_cursor(int row, int col);
void screen_hide_cursor(void);
void screen_show_cursor(void);

#endif
