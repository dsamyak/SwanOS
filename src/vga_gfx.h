#ifndef VGA_GFX_H
#define VGA_GFX_H

#include <stdint.h>
#include "multiboot.h"

extern int GFX_W;
extern int GFX_H;
extern int GFX_BPP;
extern int GFX_PITCH;

#define ARGB(a, r, g, b) (((a) << 24) | ((r) << 16) | ((g) << 8) | (b))
#define RGB(r, g, b) ARGB(255, r, g, b)

/* ── Initialization ───────────────────────────────────────── */
void vesa_gfx_init(multiboot_info_t *mboot);
void vga_gfx_exit(void);

/* ── Front-buffer primitives ──────────────────────────────── */
void vga_putpixel(int x, int y, uint32_t color);
void vga_putpixel_alpha(int x, int y, uint32_t color);
void vga_clear(uint32_t color);
void vga_fill_rect(int x, int y, int w, int h, uint32_t color);
void vga_draw_hline(int x, int y, int len, uint32_t color);
void vga_draw_vline(int x, int y, int len, uint32_t color);
void vga_draw_circle(int cx, int cy, int r, uint32_t color);
void vga_fill_circle(int cx, int cy, int r, uint32_t color);
void vga_draw_ring(int cx, int cy, int r, int thickness, uint32_t color);

/* ── Double buffering ─────────────────────────────────────── */
void vga_flip(void);
uint32_t *vga_backbuffer(void);
void vga_clear_bb(uint32_t color);

/* ── Front-buffer text ────────────────────────────────────── */
void vga_draw_char(int x, int y, char c, uint32_t color);
void vga_draw_string(int x, int y, const char *str, uint32_t color);
void vga_draw_char_2x(int x, int y, char c, uint32_t color);
void vga_draw_string_2x(int x, int y, const char *str, uint32_t color);
void vga_draw_char_3x(int x, int y, char c, uint32_t color);
void vga_draw_string_3x(int x, int y, const char *str, uint32_t color);

/* ── Backbuffer primitives (optimized) ────────────────────── */
void vga_bb_putpixel(int x, int y, uint32_t color);
void vga_bb_fill_rect(int x, int y, int w, int h, uint32_t color);
void vga_bb_fill_rect_alpha(int x, int y, int w, int h, uint32_t color);
void vga_bb_draw_hline(int x, int y, int len, uint32_t color);
void vga_bb_draw_vline(int x, int y, int len, uint32_t color);
void vga_bb_draw_rect_outline(int x, int y, int w, int h, uint32_t color);

/* ── Backbuffer gradients & shapes ────────────────────────── */
void vga_bb_fill_gradient_v(int x, int y, int w, int h,
                            uint32_t color_top, uint32_t color_bot);
void vga_bb_fill_rounded_rect(int x, int y, int w, int h, int r, uint32_t color);
void vga_bb_fill_rounded_rect_gradient(int x, int y, int w, int h, int r,
                                       uint32_t color_top, uint32_t color_bot);
void vga_bb_fill_circle(int cx, int cy, int r, uint32_t color);
void vga_bb_fill_circle_alpha(int cx, int cy, int r, uint32_t color);

/* ── Backbuffer text (1x — 8x8) ──────────────────────────── */
void vga_bb_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg);
void vga_bb_draw_string(int x, int y, const char *str, uint32_t fg, uint32_t bg);

/* ── Backbuffer text (2x — 16x16) ─────────────────────────── */
void vga_bb_draw_char_2x(int x, int y, char c, uint32_t fg, uint32_t bg);
void vga_bb_draw_string_2x(int x, int y, const char *str, uint32_t fg, uint32_t bg);

#endif
