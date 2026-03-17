/* ============================================================
 * SwanOS — C++ UI Theme Engine ("Serenity")
 * Advanced rendering: multi-stop gradients, soft shadows,
 * glassmorphism, refined widgets.
 * ============================================================ */

#include "ui_theme.h"

extern "C" {
#include "vga_gfx.h"
#include "string.h"
}

/* ── Internal helpers ─────────────────────────────────────── */

namespace {

/* Extract color channels */
struct RGBA {
    uint32_t r, g, b, a;
    RGBA() : r(0), g(0), b(0), a(255) {}
    RGBA(uint32_t color)
        : r((color >> 16) & 0xFF)
        , g((color >>  8) & 0xFF)
        , b( color        & 0xFF)
        , a((color >> 24) & 0xFF) {}

    uint32_t pack() const {
        return (a << 24) | (r << 16) | (g << 8) | b;
    }
};

/* Linear interpolation between two colors */
inline uint32_t lerp(uint32_t c0, uint32_t c1, int t, int max) {
    if (max <= 0) return c0;
    RGBA a(c0), b(c1);
    uint32_t rr = a.r + (((int)b.r - (int)a.r) * t) / max;
    uint32_t gg = a.g + (((int)b.g - (int)a.g) * t) / max;
    uint32_t bb = a.b + (((int)b.b - (int)a.b) * t) / max;
    uint32_t aa = a.a + (((int)b.a - (int)a.a) * t) / max;
    return (aa << 24) | (rr << 16) | (gg << 8) | bb;
}

/* Clamp integer */
inline int clamp(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

/* String length (avoiding name collision) */
inline int slen(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

} /* anonymous namespace */


/* ═══════════════════════════════════════════════════════════════
 *  Multi-Stop Gradient (3 colors)
 * ═══════════════════════════════════════════════════════════════ */

extern "C"
void ui_gradient_3stop(int x, int y, int w, int h,
                       uint32_t c_top, uint32_t c_mid, uint32_t c_bot) {
    int half = h / 2;
    if (half <= 0) half = 1;
    uint32_t *bb = vga_backbuffer();

    for (int dy = 0; dy < h; dy++) {
        int py = y + dy;
        if (py < 0 || py >= GFX_H) continue;

        uint32_t c;
        if (dy < half) {
            c = lerp(c_top, c_mid, dy, half);
        } else {
            c = lerp(c_mid, c_bot, dy - half, h - half);
        }

        /* Fill row */
        int x0 = x < 0 ? 0 : x;
        int x1 = x + w > GFX_W ? GFX_W : x + w;
        uint32_t *row = &bb[py * GFX_W];
        for (int px = x0; px < x1; px++)
            row[px] = c;
    }
}


/* ═══════════════════════════════════════════════════════════════
 *  Soft Shadow (Multi-Layer)
 * ═══════════════════════════════════════════════════════════════ */

extern "C"
void ui_soft_shadow(int x, int y, int w, int h, int radius, int layers) {
    for (int i = layers; i >= 1; i--) {
        int off = i * 2;
        /* Alpha decreases with each layer */
        uint32_t alpha = 12 + (layers - i) * 6;
        if (alpha > 60) alpha = 60;
        uint32_t sc = (alpha << 24) | 0x000510;
        vga_bb_fill_rounded_rect(x + off, y + off,
                                 w + i, h + i,
                                 radius + i, sc);
    }
}

extern "C"
void ui_window_shadow(int x, int y, int w, int h) {
    /* 4-layer soft shadow for windows */
    ui_soft_shadow(x, y, w, h, 10, 4);
}


/* ═══════════════════════════════════════════════════════════════
 *  Glassmorphism Panel
 * ═══════════════════════════════════════════════════════════════ */

extern "C"
void ui_glass_panel(int x, int y, int w, int h, int radius,
                    uint32_t bg_color, uint32_t border_color) {
    /* Outer glow */
    vga_bb_fill_rounded_rect(x - 1, y - 1, w + 2, h + 2, radius + 1,
                             S_ACCENT_GLOW);
    /* Main body */
    vga_bb_fill_rounded_rect(x, y, w, h, radius, bg_color);
    /* Top highlight (frosted rim) */
    vga_bb_draw_hline(x + radius, y, w - 2 * radius, S_GLASS_BORDER);
    /* Border */
    vga_bb_draw_rect_outline(x, y, w, h, border_color);
    /* Inner top highlight line for depth */
    vga_bb_draw_hline(x + radius, y + 1, w - 2 * radius, S_GLASS);
}


/* ═══════════════════════════════════════════════════════════════
 *  Card Widget
 * ═══════════════════════════════════════════════════════════════ */

extern "C"
void ui_card(int x, int y, int w, int h, int radius, uint32_t bg_color) {
    /* Subtle shadow beneath */
    vga_bb_fill_rounded_rect(x + 2, y + 2, w, h, radius, S_SHADOW);
    /* Card body */
    vga_bb_fill_rounded_rect(x, y, w, h, radius, bg_color);
    /* Top inner highlight (subtle brightness) */
    vga_bb_draw_hline(x + radius, y + 1, w - 2 * radius, S_GLASS_BORDER);
    /* Border */
    vga_bb_draw_rect_outline(x, y, w, h, S_BORDER);
}


/* ═══════════════════════════════════════════════════════════════
 *  Button Widget
 * ═══════════════════════════════════════════════════════════════ */

extern "C"
void ui_button(int x, int y, int w, int h,
               const char *label, uint32_t bg, uint32_t text_color,
               int hovered) {
    uint32_t fill = bg;
    if (hovered) {
        /* Brighten on hover */
        RGBA c(bg);
        c.r = clamp((int)c.r + 20, 0, 255);
        c.g = clamp((int)c.g + 20, 0, 255);
        c.b = clamp((int)c.b + 20, 0, 255);
        fill = c.pack();
    }
    /* Button shadow */
    vga_bb_fill_rounded_rect(x + 1, y + 1, w, h, h / 2, S_SHADOW);
    /* Button body */
    vga_bb_fill_rounded_rect(x, y, w, h, h / 2, fill);
    /* Top highlight */
    vga_bb_draw_hline(x + h/2, y + 1, w - h, 0x18FFFFFF);
    /* Label centered */
    int tw = slen(label) * 18; /* CW=18 for 2x font */
    int tx = x + (w - tw) / 2;
    int ty = y + (h - 16) / 2;
    vga_bb_draw_string_2x(tx, ty, label, text_color, 0x00000000);
}


/* ═══════════════════════════════════════════════════════════════
 *  Status Dot with Glow
 * ═══════════════════════════════════════════════════════════════ */

extern "C"
void ui_status_dot(int cx, int cy, int r, uint32_t color) {
    /* Outer glow halo */
    RGBA c(color);
    uint32_t glow = (0x25 << 24) | (c.r << 16) | (c.g << 8) | c.b;
    vga_bb_fill_circle_alpha(cx, cy, r + 4, glow);
    /* Outer ring */
    vga_bb_fill_circle(cx, cy, r, color);
    /* Inner dark center */
    vga_bb_fill_circle(cx, cy, r - 2, S_BG_DARK);
    /* Bright core */
    vga_bb_fill_circle(cx, cy, r - 4 > 0 ? r - 4 : 1, color);
}


/* ═══════════════════════════════════════════════════════════════
 *  Label Pair ("Key: Value")
 * ═══════════════════════════════════════════════════════════════ */

extern "C"
void ui_label_pair(int x, int y, const char *key, const char *value,
                   uint32_t key_color, uint32_t val_color) {
    vga_bb_draw_string_2x(x, y, key, key_color, 0x00000000);
    int kw = slen(key) * 18;
    vga_bb_draw_string_2x(x + kw, y, value, val_color, 0x00000000);
}


/* ═══════════════════════════════════════════════════════════════
 *  Section Header with Accent Underline
 * ═══════════════════════════════════════════════════════════════ */

extern "C"
void ui_section_header(int x, int y, int w,
                       const char *title, uint32_t color) {
    vga_bb_draw_string_2x(x + 4, y, title, color, 0x00000000);
    int tw = slen(title) * 18;
    /* Accent underline with fade */
    vga_bb_draw_hline(x, y + 20, tw + 8, color);
    /* Faded extension */
    uint32_t faded = (0x40 << 24) | (color & 0x00FFFFFF);
    vga_bb_draw_hline(x + tw + 8, y + 20, w - tw - 12, faded);
}


/* ═══════════════════════════════════════════════════════════════
 *  Progress Bar with Gradient Fill
 * ═══════════════════════════════════════════════════════════════ */

extern "C"
void ui_progress_bar(int x, int y, int w, int h,
                     int value, int max_val,
                     uint32_t fill_color, uint32_t track_color) {
    /* Track */
    vga_bb_fill_rounded_rect(x, y, w, h, h / 2, track_color);
    /* Fill */
    if (max_val <= 0) max_val = 1;
    int fw = (value * w) / max_val;
    if (fw > w) fw = w;
    if (fw > 0) {
        vga_bb_fill_rounded_rect(x, y, fw, h, h / 2, fill_color);
        /* Shine on top */
        vga_bb_draw_hline(x + h/2, y + 1, fw - h > 0 ? fw - h : 1, 0x20FFFFFF);
    }
}


/* ═══════════════════════════════════════════════════════════════
 *  Aurora Wallpaper
 * ═══════════════════════════════════════════════════════════════ */

extern "C"
void ui_render_aurora_wallpaper(uint32_t *buf, int w, int h) {
    /* 3-stop vertical gradient base: deep navy → ocean teal → deep purple */
    int half = h / 2;
    if (half <= 0) half = 1;

    RGBA top(W_TOP), mid(W_MID), bot(W_BOT);

    for (int y = 0; y < h; y++) {
        uint32_t c;
        if (y < half) {
            c = lerp(W_TOP, W_MID, y, half);
        } else {
            c = lerp(W_MID, W_BOT, y - half, h - half);
        }
        uint32_t *row = &buf[y * w];
        for (int x = 0; x < w; x++)
            row[x] = c;
    }

    /* Aurora shimmer — soft teal glow at upper-center */
    int gcx = w / 2;
    int gcy = h / 3;
    int max_r2 = (w * w) / 6 + (h * h) / 8;
    for (int y = 0; y < h; y++) {
        uint32_t *row = &buf[y * w];
        int dy = y - gcy;
        int dy2 = dy * dy;
        for (int x = 0; x < w; x++) {
            int dx = x - gcx;
            int dist2 = (dx * dx) / 3 + dy2;
            if (dist2 < max_r2) {
                int intensity = 22 - (dist2 * 22) / max_r2;
                if (intensity > 0) {
                    RGBA bg(row[x]);
                    /* Teal aurora tint */
                    bg.r = clamp((int)bg.r + intensity / 2, 0, 255);
                    bg.g = clamp((int)bg.g + intensity * 2, 0, 255);
                    bg.b = clamp((int)bg.b + intensity * 3 / 2, 0, 255);
                    row[x] = bg.pack();
                }
            }
        }
    }

    /* Secondary aurora — lavender glow at upper-right */
    int g2x = w * 3 / 4;
    int g2y = h / 4;
    int max2_r2 = (w * w) / 10 + (h * h) / 12;
    for (int y = 0; y < h; y++) {
        uint32_t *row = &buf[y * w];
        int dy = y - g2y;
        int dy2 = dy * dy;
        for (int x = 0; x < w; x++) {
            int dx = x - g2x;
            int dist2 = (dx * dx) / 3 + dy2;
            if (dist2 < max2_r2) {
                int intensity = 14 - (dist2 * 14) / max2_r2;
                if (intensity > 0) {
                    RGBA bg(row[x]);
                    bg.r = clamp((int)bg.r + intensity * 2, 0, 255);
                    bg.g = clamp((int)bg.g + intensity / 2, 0, 255);
                    bg.b = clamp((int)bg.b + intensity * 2, 0, 255);
                    row[x] = bg.pack();
                }
            }
        }
    }

    /* Subtle stars */
    uint32_t seed = 0xC0FFEE42;
    for (int i = 0; i < 80; i++) {
        seed = seed * 1103515245 + 12345;
        int sx = (seed >> 16) % w;
        seed = seed * 1103515245 + 12345;
        int sy = (seed >> 16) % h;
        seed = seed * 1103515245 + 12345;
        int brightness = 160 + (seed >> 16) % 80;
        /* Slight blue-white tint */
        uint32_t star = (0xFF000000) |
                        ((brightness - 10) << 16) |
                        ((brightness - 5) << 8) |
                        brightness;
        buf[sy * w + sx] = star;
        /* Some larger stars */
        if (i < 20 && sx + 1 < w) {
            uint32_t dim = (0xFF000000) |
                           ((brightness / 2) << 16) |
                           ((brightness / 2) << 8) |
                           (brightness / 2);
            buf[sy * w + sx + 1] = dim;
            if (sy + 1 < h) buf[(sy + 1) * w + sx] = dim;
        }
    }
}
