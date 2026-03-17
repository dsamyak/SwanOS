/* ============================================================
 * SwanOS — VESA High-Res Graphics Driver (Optimized)
 * Modern 32-bit ARGB Framebuffer Rendering.
 * Fast memcpy-based flip, optimized fills, gradients,
 * rounded rectangles, and 2x backbuffer font.
 * ============================================================ */

#include "vga_gfx.h"
#include "multiboot.h"
#include "string.h"

int GFX_W = 1920;
int GFX_H = 1080;
int GFX_BPP = 32;
int GFX_PITCH = 1920 * 4;

static uint32_t *VESA_FB = (uint32_t *)0xFD000000;

/* Backbuffer for double buffering high-res UI */
static uint32_t backbuf[1920 * 1080];

/* ── Fast 32-bit memory operations ────────────────────────── */

static inline void memset32(uint32_t *dest, uint32_t val, int count) {
    while (count >= 8) {
        dest[0] = val; dest[1] = val; dest[2] = val; dest[3] = val;
        dest[4] = val; dest[5] = val; dest[6] = val; dest[7] = val;
        dest += 8; count -= 8;
    }
    while (count-- > 0) *dest++ = val;
}

static inline void memcpy32(uint32_t *dest, const uint32_t *src, int count) {
    while (count >= 8) {
        dest[0] = src[0]; dest[1] = src[1]; dest[2] = src[2]; dest[3] = src[3];
        dest[4] = src[4]; dest[5] = src[5]; dest[6] = src[6]; dest[7] = src[7];
        dest += 8; src += 8; count -= 8;
    }
    while (count-- > 0) *dest++ = *src++;
}

/* ── Inline color helpers ─────────────────────────────────── */

static inline uint32_t lerp_color(uint32_t c0, uint32_t c1, int t, int max) {
    /* Linearly interpolate between c0 and c1, t in [0, max) */
    if (max <= 0) return c0;
    uint32_t r0 = (c0 >> 16) & 0xFF, g0 = (c0 >> 8) & 0xFF, b0 = c0 & 0xFF;
    uint32_t r1 = (c1 >> 16) & 0xFF, g1 = (c1 >> 8) & 0xFF, b1 = c1 & 0xFF;
    uint32_t r = r0 + (((int)r1 - (int)r0) * t) / max;
    uint32_t g = g0 + (((int)g1 - (int)g0) * t) / max;
    uint32_t b = b0 + (((int)b1 - (int)b0) * t) / max;
    return ARGB(255, r, g, b);
}

void vesa_gfx_init(multiboot_info_t *mboot) {
    if (mboot->flags & MULTIBOOT_INFO_FRAMEBUFFER_INFO) {
        VESA_FB = (uint32_t *)(uint32_t)mboot->framebuffer_addr;
        GFX_W = mboot->framebuffer_width;
        GFX_H = mboot->framebuffer_height;
        GFX_PITCH = mboot->framebuffer_pitch;
        GFX_BPP = mboot->framebuffer_bpp;
    }
    vga_clear(0);
}

void vga_gfx_exit(void) {
    /* No cleanup needed for linear framebuffer */
}

/* ── Basic Primitives ─────────────────────────────────────── */

void vga_putpixel(int x, int y, uint32_t color) {
    if (x >= 0 && x < GFX_W && y >= 0 && y < GFX_H) {
        VESA_FB[(y * GFX_PITCH / 4) + x] = color;
    }
}

void vga_putpixel_alpha(int x, int y, uint32_t color) {
    if (x < 0 || x >= GFX_W || y < 0 || y >= GFX_H) return;
    uint32_t a = (color >> 24) & 0xFF;
    if (a == 255) {
        VESA_FB[(y * GFX_PITCH / 4) + x] = color;
        return;
    }
    if (a == 0) return;

    uint32_t bg = VESA_FB[(y * GFX_PITCH / 4) + x];
    uint32_t br = (bg >> 16) & 0xFF;
    uint32_t bg_g = (bg >> 8) & 0xFF;
    uint32_t bb = bg & 0xFF;

    uint32_t fr = (color >> 16) & 0xFF;
    uint32_t fg_g = (color >> 8) & 0xFF;
    uint32_t fb = color & 0xFF;

    uint32_t inv = 255 - a;
    uint32_t out_r = (fr * a + br * inv) / 255;
    uint32_t out_g = (fg_g * a + bg_g * inv) / 255;
    uint32_t out_b = (fb * a + bb * inv) / 255;

    VESA_FB[(y * GFX_PITCH / 4) + x] = ARGB(255, out_r, out_g, out_b);
}

void vga_clear(uint32_t color) {
    int pitch4 = GFX_PITCH / 4;
    for (int y = 0; y < GFX_H; y++) {
        memset32(&VESA_FB[y * pitch4], color, GFX_W);
    }
}

void vga_fill_rect(int x, int y, int w, int h, uint32_t color) {
    /* Clamp to screen */
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w > GFX_W ? GFX_W : x + w;
    int y1 = y + h > GFX_H ? GFX_H : y + h;
    int cw = x1 - x0;
    if (cw <= 0) return;
    int pitch4 = GFX_PITCH / 4;
    for (int j = y0; j < y1; j++)
        memset32(&VESA_FB[j * pitch4 + x0], color, cw);
}

void vga_draw_hline(int x, int y, int len, uint32_t color) {
    if (y < 0 || y >= GFX_H || len <= 0) return;
    int x0 = x < 0 ? 0 : x;
    int x1 = x + len > GFX_W ? GFX_W : x + len;
    if (x0 >= x1) return;
    int pitch4 = GFX_PITCH / 4;
    memset32(&VESA_FB[y * pitch4 + x0], color, x1 - x0);
}

void vga_draw_vline(int x, int y, int len, uint32_t color) {
    if (x < 0 || x >= GFX_W || len <= 0) return;
    int y0 = y < 0 ? 0 : y;
    int y1 = y + len > GFX_H ? GFX_H : y + len;
    int pitch4 = GFX_PITCH / 4;
    for (int j = y0; j < y1; j++)
        VESA_FB[j * pitch4 + x] = color;
}

void vga_draw_circle(int cx, int cy, int r, uint32_t color) {
    int x = 0, y = r, d = 3 - 2 * r;
    while (x <= y) {
        vga_putpixel(cx+x,cy+y,color); vga_putpixel(cx-x,cy+y,color);
        vga_putpixel(cx+x,cy-y,color); vga_putpixel(cx-x,cy-y,color);
        vga_putpixel(cx+y,cy+x,color); vga_putpixel(cx-y,cy+x,color);
        vga_putpixel(cx+y,cy-x,color); vga_putpixel(cx-y,cy-x,color);
        if (d < 0) d += 4*x+6; else { d += 4*(x-y)+10; y--; }
        x++;
    }
}

void vga_fill_circle(int cx, int cy, int r, uint32_t color) {
    for (int y = -r; y <= r; y++)
        for (int x = -r; x <= r; x++)
            if (x*x + y*y <= r*r) vga_putpixel(cx+x, cy+y, color);
}

void vga_draw_ring(int cx, int cy, int r, int thickness, uint32_t color) {
    for (int y = -(r+thickness); y <= (r+thickness); y++) {
        for (int x = -(r+thickness); x <= (r+thickness); x++) {
            int dist_sq = x*x + y*y;
            int outer_sq = (r+thickness) * (r+thickness);
            int inner_sq = (r > thickness) ? (r-thickness) * (r-thickness) : 0;
            if (dist_sq <= outer_sq && dist_sq >= inner_sq)
                vga_putpixel(cx + x, cy + y, color);
        }
    }
}

/* ── Fast Double Buffering (Optimized) ────────────────────── */

void vga_flip(void) {
    int pitch4 = GFX_PITCH / 4;
    for (int y = 0; y < GFX_H; y++) {
        memcpy32(&VESA_FB[y * pitch4], &backbuf[y * GFX_W], GFX_W);
    }
}

uint32_t *vga_backbuffer(void) {
    return backbuf;
}

void vga_clear_bb(uint32_t color) {
    memset32(backbuf, color, GFX_W * GFX_H);
}

/* ── Optimized Backbuffer Drawing ─────────────────────────── */

void vga_bb_putpixel(int x, int y, uint32_t color) {
    if (x >= 0 && x < GFX_W && y >= 0 && y < GFX_H)
        backbuf[y * GFX_W + x] = color;
}

void vga_bb_fill_rect(int x, int y, int w, int h, uint32_t color) {
    /* Clamp once, then fast-fill rows */
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w > GFX_W ? GFX_W : x + w;
    int y1 = y + h > GFX_H ? GFX_H : y + h;
    int cw = x1 - x0;
    if (cw <= 0) return;
    for (int j = y0; j < y1; j++)
        memset32(&backbuf[j * GFX_W + x0], color, cw);
}

void vga_bb_draw_hline(int x, int y, int len, uint32_t color) {
    if (y < 0 || y >= GFX_H || len <= 0) return;
    int x0 = x < 0 ? 0 : x;
    int x1 = x + len > GFX_W ? GFX_W : x + len;
    if (x0 >= x1) return;
    memset32(&backbuf[y * GFX_W + x0], color, x1 - x0);
}

void vga_bb_draw_vline(int x, int y, int len, uint32_t color) {
    if (x < 0 || x >= GFX_W || len <= 0) return;
    int y0 = y < 0 ? 0 : y;
    int y1 = y + len > GFX_H ? GFX_H : y + len;
    for (int j = y0; j < y1; j++)
        backbuf[j * GFX_W + x] = color;
}

void vga_bb_draw_rect_outline(int x, int y, int w, int h, uint32_t color) {
    vga_bb_draw_hline(x, y, w, color);
    vga_bb_draw_hline(x, y + h - 1, w, color);
    vga_bb_draw_vline(x, y, h, color);
    vga_bb_draw_vline(x + w - 1, y, h, color);
}

/* Alpha blended backbuffer rect (optimized) */
void vga_bb_fill_rect_alpha(int x, int y, int w, int h, uint32_t color) {
    uint32_t a = (color >> 24) & 0xFF;
    if (a == 255) { vga_bb_fill_rect(x, y, w, h, color); return; }
    if (a == 0) return;

    /* Pre-extract foreground channels */
    uint32_t fr = (color >> 16) & 0xFF;
    uint32_t fg = (color >> 8) & 0xFF;
    uint32_t fb = color & 0xFF;
    uint32_t inv = 255 - a;

    /* Clamp */
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w > GFX_W ? GFX_W : x + w;
    int y1 = y + h > GFX_H ? GFX_H : y + h;

    for (int j = y0; j < y1; j++) {
        uint32_t *row = &backbuf[j * GFX_W];
        for (int i = x0; i < x1; i++) {
            uint32_t bg = row[i];
            uint32_t br = (bg >> 16) & 0xFF;
            uint32_t bg_g = (bg >> 8) & 0xFF;
            uint32_t bb = bg & 0xFF;
            uint32_t or2 = (fr * a + br * inv) / 255;
            uint32_t og = (fg * a + bg_g * inv) / 255;
            uint32_t ob = (fb * a + bb * inv) / 255;
            row[i] = ARGB(255, or2, og, ob);
        }
    }
}

/* ── Gradient Fill (Vertical) ─────────────────────────────── */

void vga_bb_fill_gradient_v(int x, int y, int w, int h,
                            uint32_t color_top, uint32_t color_bot) {
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w > GFX_W ? GFX_W : x + w;
    int y1 = y + h > GFX_H ? GFX_H : y + h;
    int cw = x1 - x0;
    if (cw <= 0 || h <= 0) return;

    for (int j = y0; j < y1; j++) {
        uint32_t c = lerp_color(color_top, color_bot, j - y, h);
        memset32(&backbuf[j * GFX_W + x0], c, cw);
    }
}

/* ── Rounded Rectangle ────────────────────────────────────── */

void vga_bb_fill_rounded_rect(int x, int y, int w, int h, int r, uint32_t color) {
    if (r < 0) r = 0;
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;

    /* Middle section (full width rows) */
    vga_bb_fill_rect(x, y + r, w, h - 2 * r, color);

    /* Top and bottom bands with rounded corners */
    for (int dy = 0; dy < r; dy++) {
        /* Quarter-circle offset */
        int dx = r;
        /* Bresenham-ish: find x where x^2 + (r-1-dy)^2 <= r^2 */
        int ry = r - 1 - dy;
        while (dx > 0 && dx * dx + ry * ry > r * r) dx--;
        int inset = r - dx;

        /* Top band */
        vga_bb_fill_rect(x + inset, y + dy, w - 2 * inset, 1, color);
        /* Bottom band */
        vga_bb_fill_rect(x + inset, y + h - 1 - dy, w - 2 * inset, 1, color);
    }
}

void vga_bb_fill_rounded_rect_gradient(int x, int y, int w, int h, int r,
                                       uint32_t color_top, uint32_t color_bot) {
    if (r < 0) r = 0;
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;

    for (int dy = 0; dy < h; dy++) {
        uint32_t c = lerp_color(color_top, color_bot, dy, h);
        int inset = 0;
        if (dy < r) {
            int ry = r - 1 - dy;
            int dx = r;
            while (dx > 0 && dx * dx + ry * ry > r * r) dx--;
            inset = r - dx;
        } else if (dy >= h - r) {
            int ry = r - 1 - (h - 1 - dy);
            int dx = r;
            while (dx > 0 && dx * dx + ry * ry > r * r) dx--;
            inset = r - dx;
        }
        int rx = x + inset;
        int rw = w - 2 * inset;
        if (rw > 0) {
            int rx0 = rx < 0 ? 0 : rx;
            int rx1 = rx + rw > GFX_W ? GFX_W : rx + rw;
            int yy = y + dy;
            if (yy >= 0 && yy < GFX_H && rx0 < rx1)
                memset32(&backbuf[yy * GFX_W + rx0], c, rx1 - rx0);
        }
    }
}

/* ── Backbuffer filled circle ─────────────────────────────── */
void vga_bb_fill_circle(int cx, int cy, int r, uint32_t color) {
    for (int dy = -r; dy <= r; dy++) {
        int py = cy + dy;
        if (py < 0 || py >= GFX_H) continue;
        /* Width at this scanline */
        int dx = r;
        while (dx > 0 && dx * dx + dy * dy > r * r) dx--;
        int x0 = cx - dx;
        int x1 = cx + dx;
        if (x0 < 0) x0 = 0;
        if (x1 >= GFX_W) x1 = GFX_W - 1;
        if (x0 <= x1)
            memset32(&backbuf[py * GFX_W + x0], color, x1 - x0 + 1);
    }
}

/* ── Backbuffer circle outline (Bresenham) ────────────────── */
void vga_bb_draw_circle(int cx, int cy, int r, uint32_t color) {
    int x = 0, y = r, d = 3 - 2 * r;
    while (x <= y) {
        vga_bb_putpixel(cx+x,cy+y,color); vga_bb_putpixel(cx-x,cy+y,color);
        vga_bb_putpixel(cx+x,cy-y,color); vga_bb_putpixel(cx-x,cy-y,color);
        vga_bb_putpixel(cx+y,cy+x,color); vga_bb_putpixel(cx-y,cy+x,color);
        vga_bb_putpixel(cx+y,cy-x,color); vga_bb_putpixel(cx-y,cy-x,color);
        if (d < 0) d += 4*x+6; else { d += 4*(x-y)+10; y--; }
        x++;
    }
}

/* Alpha blended backbuffer circle */
void vga_bb_fill_circle_alpha(int cx, int cy, int r, uint32_t color) {
    uint32_t a = (color >> 24) & 0xFF;
    if (a == 0) return;
    uint32_t fr = (color >> 16) & 0xFF;
    uint32_t fg = (color >> 8) & 0xFF;
    uint32_t fb = color & 0xFF;
    uint32_t inv = 255 - a;

    for (int dy = -r; dy <= r; dy++) {
        int py = cy + dy;
        if (py < 0 || py >= GFX_H) continue;
        int dx = r;
        while (dx > 0 && dx * dx + dy * dy > r * r) dx--;
        int x0 = cx - dx;
        int x1 = cx + dx;
        if (x0 < 0) x0 = 0;
        if (x1 >= GFX_W) x1 = GFX_W - 1;
        uint32_t *row = &backbuf[py * GFX_W];
        for (int i = x0; i <= x1; i++) {
            if (a == 255) { row[i] = color; continue; }
            uint32_t bg = row[i];
            uint32_t br = (bg >> 16) & 0xFF;
            uint32_t bg_g = (bg >> 8) & 0xFF;
            uint32_t bb = bg & 0xFF;
            uint32_t or2 = (fr * a + br * inv) / 255;
            uint32_t og = (fg * a + bg_g * inv) / 255;
            uint32_t ob = (fb * a + bb * inv) / 255;
            row[i] = ARGB(255, or2, og, ob);
        }
    }
}

/* ── Clean 8x8 bitmap font — Full ASCII 32..126 ───────── */
static const uint8_t font8x8_full[95][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /*   (32) */
    {0x18,0x18,0x18,0x18,0x18,0x00,0x18,0x00}, /* ! (33) */
    {0x6C,0x6C,0x24,0x00,0x00,0x00,0x00,0x00}, /* " (34) */
    {0x24,0x7E,0x24,0x24,0x7E,0x24,0x00,0x00}, /* # (35) */
    {0x18,0x3E,0x60,0x3C,0x06,0x7C,0x18,0x00}, /* $ (36) */
    {0x62,0x66,0x0C,0x18,0x30,0x66,0x46,0x00}, /* % (37) */
    {0x38,0x6C,0x38,0x76,0xDC,0xCC,0x76,0x00}, /* & (38) */
    {0x18,0x18,0x30,0x00,0x00,0x00,0x00,0x00}, /* ' (39) */
    {0x0C,0x18,0x30,0x30,0x30,0x18,0x0C,0x00}, /* ( (40) */
    {0x30,0x18,0x0C,0x0C,0x0C,0x18,0x30,0x00}, /* ) (41) */
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, /* * (42) */
    {0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00}, /* + (43) */
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30}, /* , (44) */
    {0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00}, /* - (45) */
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00}, /* . (46) */
    {0x02,0x06,0x0C,0x18,0x30,0x60,0x40,0x00}, /* / (47) */
    {0x3C,0x66,0x6E,0x76,0x66,0x66,0x3C,0x00}, /* 0 */
    {0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00}, /* 1 */
    {0x3C,0x66,0x06,0x0C,0x18,0x30,0x7E,0x00}, /* 2 */
    {0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0x00}, /* 3 */
    {0x0E,0x1E,0x36,0x66,0x7F,0x06,0x06,0x00}, /* 4 */
    {0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0x00}, /* 5 */
    {0x1C,0x30,0x60,0x7C,0x66,0x66,0x3C,0x00}, /* 6 */
    {0x7E,0x06,0x0C,0x18,0x30,0x30,0x30,0x00}, /* 7 */
    {0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0x00}, /* 8 */
    {0x3C,0x66,0x66,0x3E,0x06,0x0C,0x38,0x00}, /* 9 */
    {0x00,0x18,0x18,0x00,0x18,0x18,0x00,0x00}, /* : (58) */
    {0x00,0x18,0x18,0x00,0x18,0x18,0x30,0x00}, /* ; (59) */
    {0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00}, /* < (60) */
    {0x00,0x00,0x7E,0x00,0x7E,0x00,0x00,0x00}, /* = (61) */
    {0x60,0x30,0x18,0x0C,0x18,0x30,0x60,0x00}, /* > (62) */
    {0x3C,0x66,0x06,0x0C,0x18,0x00,0x18,0x00}, /* ? (63) */
    {0x3C,0x66,0x6E,0x6E,0x60,0x62,0x3C,0x00}, /* @ (64) */
    {0x18,0x3C,0x66,0x66,0x7E,0x66,0x66,0x00}, /* A */
    {0x7C,0x66,0x66,0x7C,0x66,0x66,0x7C,0x00}, /* B */
    {0x3C,0x66,0x60,0x60,0x60,0x66,0x3C,0x00}, /* C */
    {0x78,0x6C,0x66,0x66,0x66,0x6C,0x78,0x00}, /* D */
    {0x7E,0x60,0x60,0x78,0x60,0x60,0x7E,0x00}, /* E */
    {0x7E,0x60,0x60,0x78,0x60,0x60,0x60,0x00}, /* F */
    {0x3C,0x66,0x60,0x6E,0x66,0x66,0x3E,0x00}, /* G */
    {0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0x00}, /* H */
    {0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, /* I */
    {0x1E,0x0C,0x0C,0x0C,0x0C,0x6C,0x38,0x00}, /* J */
    {0x66,0x6C,0x78,0x70,0x78,0x6C,0x66,0x00}, /* K */
    {0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0x00}, /* L */
    {0x63,0x77,0x7F,0x6B,0x63,0x63,0x63,0x00}, /* M */
    {0x66,0x76,0x7E,0x7E,0x6E,0x66,0x66,0x00}, /* N */
    {0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}, /* O */
    {0x7C,0x66,0x66,0x7C,0x60,0x60,0x60,0x00}, /* P */
    {0x3C,0x66,0x66,0x66,0x6A,0x6C,0x36,0x00}, /* Q */
    {0x7C,0x66,0x66,0x7C,0x6C,0x66,0x66,0x00}, /* R */
    {0x3C,0x66,0x60,0x3C,0x06,0x66,0x3C,0x00}, /* S */
    {0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00}, /* T */
    {0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}, /* U */
    {0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0x00}, /* V */
    {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00}, /* W */
    {0x66,0x66,0x3C,0x18,0x3C,0x66,0x66,0x00}, /* X */
    {0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0x00}, /* Y */
    {0x7E,0x06,0x0C,0x18,0x30,0x60,0x7E,0x00}, /* Z */
    {0x3C,0x30,0x30,0x30,0x30,0x30,0x3C,0x00}, /* [ (91) */
    {0x40,0x60,0x30,0x18,0x0C,0x06,0x02,0x00}, /* \ (92) */
    {0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0x00}, /* ] (93) */
    {0x10,0x38,0x6C,0x00,0x00,0x00,0x00,0x00}, /* ^ (94) */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x7E,0x00}, /* _ (95) */
    {0x30,0x18,0x0C,0x00,0x00,0x00,0x00,0x00}, /* ` (96) */
    {0x00,0x00,0x3C,0x06,0x3E,0x66,0x3E,0x00}, /* a */
    {0x60,0x60,0x7C,0x66,0x66,0x66,0x7C,0x00}, /* b */
    {0x00,0x00,0x3C,0x60,0x60,0x60,0x3C,0x00}, /* c */
    {0x06,0x06,0x3E,0x66,0x66,0x66,0x3E,0x00}, /* d */
    {0x00,0x00,0x3C,0x66,0x7E,0x60,0x3C,0x00}, /* e */
    {0x1C,0x30,0x7C,0x30,0x30,0x30,0x30,0x00}, /* f */
    {0x00,0x00,0x3E,0x66,0x66,0x3E,0x06,0x3C}, /* g */
    {0x60,0x60,0x7C,0x66,0x66,0x66,0x66,0x00}, /* h */
    {0x18,0x00,0x38,0x18,0x18,0x18,0x3C,0x00}, /* i */
    {0x0C,0x00,0x1C,0x0C,0x0C,0x0C,0x6C,0x38}, /* j */
    {0x60,0x60,0x66,0x6C,0x78,0x6C,0x66,0x00}, /* k */
    {0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, /* l */
    {0x00,0x00,0x66,0x7F,0x7F,0x6B,0x63,0x00}, /* m */
    {0x00,0x00,0x7C,0x66,0x66,0x66,0x66,0x00}, /* n */
    {0x00,0x00,0x3C,0x66,0x66,0x66,0x3C,0x00}, /* o */
    {0x00,0x00,0x7C,0x66,0x66,0x7C,0x60,0x60}, /* p */
    {0x00,0x00,0x3E,0x66,0x66,0x3E,0x06,0x06}, /* q */
    {0x00,0x00,0x7C,0x66,0x60,0x60,0x60,0x00}, /* r */
    {0x00,0x00,0x3E,0x60,0x3C,0x06,0x7C,0x00}, /* s */
    {0x30,0x30,0x7C,0x30,0x30,0x30,0x1C,0x00}, /* t */
    {0x00,0x00,0x66,0x66,0x66,0x66,0x3E,0x00}, /* u */
    {0x00,0x00,0x66,0x66,0x66,0x3C,0x18,0x00}, /* v */
    {0x00,0x00,0x63,0x6B,0x7F,0x7F,0x36,0x00}, /* w */
    {0x00,0x00,0x66,0x3C,0x18,0x3C,0x66,0x00}, /* x */
    {0x00,0x00,0x66,0x66,0x66,0x3E,0x06,0x3C}, /* y */
    {0x00,0x00,0x7E,0x0C,0x18,0x30,0x7E,0x00}, /* z */
    {0x0E,0x18,0x18,0x70,0x18,0x18,0x0E,0x00}, /* { (123) */
    {0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x00}, /* | (124) */
    {0x70,0x18,0x18,0x0E,0x18,0x18,0x70,0x00}, /* } (125) */
    {0x76,0xDC,0x00,0x00,0x00,0x00,0x00,0x00}, /* ~ (126) */
};

static int full_char_idx(char c) {
    if (c >= 32 && c <= 126) return c - 32;
    return 0;
}

/* ── Front buffer text (unchanged for boot splash) ────────── */

void vga_draw_char(int x, int y, char c, uint32_t color) {
    int idx = full_char_idx(c);
    const uint8_t *g = font8x8_full[idx];
    for (int r = 0; r < 8; r++) {
        uint8_t bits = g[r];
        for (int col = 0; col < 8; col++)
            if (bits & (0x80 >> col))
                vga_putpixel(x + col, y + r, color);
    }
}

void vga_draw_string(int x, int y, const char *str, uint32_t color) {
    while (*str) { vga_draw_char(x, y, *str, color); x += 9; str++; }
}

void vga_draw_char_2x(int x, int y, char c, uint32_t color) {
    int idx = full_char_idx(c);
    const uint8_t *g = font8x8_full[idx];
    for (int r = 0; r < 8; r++) {
        uint8_t bits = g[r];
        for (int col = 0; col < 8; col++) {
            if (bits & (0x80 >> col)) {
                vga_putpixel(x + col*2,   y + r*2,   color);
                vga_putpixel(x + col*2+1, y + r*2,   color);
                vga_putpixel(x + col*2,   y + r*2+1, color);
                vga_putpixel(x + col*2+1, y + r*2+1, color);
            }
        }
    }
}

void vga_draw_string_2x(int x, int y, const char *str, uint32_t color) {
    while (*str) { vga_draw_char_2x(x, y, *str, color); x += 18; str++; }
}

void vga_draw_char_3x(int x, int y, char c, uint32_t color) {
    int idx = full_char_idx(c);
    const uint8_t *g = font8x8_full[idx];
    for (int r = 0; r < 8; r++) {
        uint8_t bits = g[r];
        for (int col = 0; col < 8; col++) {
            if (bits & (0x80 >> col)) {
                for (int dy = 0; dy < 3; dy++)
                    for (int dx = 0; dx < 3; dx++)
                        vga_putpixel(x + col*3 + dx, y + r*3 + dy, color);
            }
        }
    }
}

void vga_draw_string_3x(int x, int y, const char *str, uint32_t color) {
    while (*str) { vga_draw_char_3x(x, y, *str, color); x += 26; str++; }
}

/* ── Backbuffer Text Drawing (1x) ─────────────────────────── */

void vga_bb_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg) {
    int idx = full_char_idx(c);
    const uint8_t *g = font8x8_full[idx];
    for (int r = 0; r < 8; r++) {
        uint8_t bits = g[r];
        for (int col = 0; col < 8; col++) {
            if (bits & (0x80 >> col))
                vga_bb_putpixel(x + col, y + r, fg);
            else if ((bg >> 24) == 255)
                vga_bb_putpixel(x + col, y + r, bg);
        }
    }
}

void vga_bb_draw_string(int x, int y, const char *str, uint32_t fg, uint32_t bg) {
    while (*str) {
        vga_bb_draw_char(x, y, *str, fg, bg);
        x += 9;
        str++;
    }
}

/* ── Backbuffer Text Drawing (2x — 16x16 effective) ─────── */

void vga_bb_draw_char_2x(int x, int y, char c, uint32_t fg, uint32_t bg) {
    int idx = full_char_idx(c);
    const uint8_t *g = font8x8_full[idx];
    for (int r = 0; r < 8; r++) {
        uint8_t bits = g[r];
        int py0 = y + r * 2;
        int py1 = py0 + 1;
        for (int col = 0; col < 8; col++) {
            int px0 = x + col * 2;
            int px1 = px0 + 1;
            if (bits & (0x80 >> col)) {
                vga_bb_putpixel(px0, py0, fg);
                vga_bb_putpixel(px1, py0, fg);
                vga_bb_putpixel(px0, py1, fg);
                vga_bb_putpixel(px1, py1, fg);
            } else if ((bg >> 24) == 255) {
                vga_bb_putpixel(px0, py0, bg);
                vga_bb_putpixel(px1, py0, bg);
                vga_bb_putpixel(px0, py1, bg);
                vga_bb_putpixel(px1, py1, bg);
            }
        }
    }
}

void vga_bb_draw_string_2x(int x, int y, const char *str, uint32_t fg, uint32_t bg) {
    while (*str) {
        vga_bb_draw_char_2x(x, y, *str, fg, bg);
        x += 18;
        str++;
    }
}
