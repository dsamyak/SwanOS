#ifndef VGA_GFX_H
#define VGA_GFX_H

#include <stdint.h>

#define GFX_W   320
#define GFX_H   200

/* Mode switching */
void vga_gfx_init(void);
void vga_gfx_exit(void);

/* Drawing primitives */
void vga_putpixel(int x, int y, uint8_t color);
void vga_clear(uint8_t color);
void vga_fill_rect(int x, int y, int w, int h, uint8_t color);
void vga_draw_hline(int x, int y, int len, uint8_t color);
void vga_draw_vline(int x, int y, int len, uint8_t color);
void vga_draw_circle(int cx, int cy, int r, uint8_t color);
void vga_fill_circle(int cx, int cy, int r, uint8_t color);
void vga_draw_ring(int cx, int cy, int r, int thickness, uint8_t color);

/* Palette */
void vga_set_palette(uint8_t idx, uint8_t r, uint8_t g, uint8_t b);
void vga_set_palette_range(uint8_t start, int count, const uint8_t *rgb);

/* Effects */
void vga_fade_from_black(int speed_ms);
void vga_fade_to_black(int speed_ms);
void vga_vsync(void);

/* Font rendering (1x, 2x, 3x scale) */
void vga_draw_char(int x, int y, char c, uint8_t color);
void vga_draw_string(int x, int y, const char *str, uint8_t color);
void vga_draw_char_2x(int x, int y, char c, uint8_t color);
void vga_draw_string_2x(int x, int y, const char *str, uint8_t color);
void vga_draw_char_3x(int x, int y, char c, uint8_t color);
void vga_draw_string_3x(int x, int y, const char *str, uint8_t color);

#endif
