/* ============================================================
 * SwanOS — C++ UI Theme Engine ("Neon Aurora")
 * Advanced rendering: multi-stop gradients, soft shadows,
 * glassmorphism, frosted panels, neon borders, pill segments,
 * drag handles, refined widgets.
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

/* Simple pseudo-random hash for noise/grain */
inline uint32_t xorshift(uint32_t seed) {
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return seed;
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
        uint32_t alpha = 14 + (layers - i) * 8;
        if (alpha > 70) alpha = 70;
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
 *  Glassmorphism Panel (Original)
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
 *  Frosted Panel — Enhanced glass with noise grain texture
 * ═══════════════════════════════════════════════════════════════ */

extern "C"
void ui_frosted_panel(int x, int y, int w, int h, int radius,
                      uint32_t bg_color, uint32_t border_color) {
    /* Deep outer shadow glow — neon tint */
    vga_bb_fill_rounded_rect(x - 2, y - 2, w + 4, h + 4, radius + 2,
                             S_GLOW_PULSE);
    /* Outer neon glow halo */
    vga_bb_fill_rounded_rect(x - 1, y - 1, w + 2, h + 2, radius + 1,
                             S_ACCENT_GLOW);
    /* Main body — deep transparency */
    vga_bb_fill_rounded_rect(x, y, w, h, radius, bg_color);

    /* Noise grain overlay for frosted effect (sparse pixel dithering) */
    uint32_t seed = 0xDEAD1234;
    int step = 3;  /* every 3rd pixel to keep fast */
    for (int dy = 2; dy < h - 2; dy += step) {
        for (int dx = 2; dx < w - 2; dx += step) {
            seed = xorshift(seed);
            /* Very subtle white/dark grain particles */
            if ((seed & 0x1F) < 3) {
                uint32_t grain = (seed & 1) ? 0x08FFFFFF : 0x06000000;
                vga_bb_putpixel(x + dx, y + dy, grain);
            }
        }
    }

    /* Top frosted rim highlight */
    vga_bb_draw_hline(x + radius, y, w - 2 * radius, 0x20FFFFFF);
    /* Inner top highlight for depth */
    vga_bb_draw_hline(x + radius, y + 1, w - 2 * radius, 0x12FFFFFF);
    /* Bottom inner shadow */
    vga_bb_draw_hline(x + radius, y + h - 1, w - 2 * radius, 0x10000000);
    /* Border */
    vga_bb_draw_rect_outline(x, y, w, h, border_color);
}


/* ═══════════════════════════════════════════════════════════════
 *  Pill Segment — Individual rounded zone for dock layout
 * ═══════════════════════════════════════════════════════════════ */

extern "C"
void ui_pill_segment(int x, int y, int w, int h, int radius,
                     uint32_t bg_color, uint32_t border_color,
                     int accent_underline) {
    /* Subtle shadow beneath segment */
    vga_bb_fill_rounded_rect(x + 1, y + 1, w, h, radius, 0x20000008);
    /* Segment body */
    vga_bb_fill_rounded_rect(x, y, w, h, radius, bg_color);
    /* Top inner shine */
    vga_bb_draw_hline(x + radius, y + 1, w - 2 * radius, 0x10FFFFFF);
    /* Border */
    vga_bb_draw_rect_outline(x, y, w, h, border_color);

    /* Optional neon accent underline */
    if (accent_underline) {
        /* Gradient accent bar at bottom */
        vga_bb_fill_rounded_rect(x + 4, y + h - 3, w - 8, 2, 1, S_NEON_CYAN);
        /* Glow beneath the underline */
        vga_bb_fill_rounded_rect(x + 8, y + h - 1, w - 16, 2, 1, S_GLOW_PULSE);
    }
}


/* ═══════════════════════════════════════════════════════════════
 *  Neon Border with Outer Glow
 * ═══════════════════════════════════════════════════════════════ */

extern "C"
void ui_neon_border(int x, int y, int w, int h, int radius,
                    uint32_t neon_color) {
    RGBA c(neon_color);
    /* Outer glow halo (3 layers, decreasing alpha) */
    for (int layer = 3; layer >= 1; layer--) {
        uint32_t alpha = 8 + (3 - layer) * 6;
        uint32_t glow = (alpha << 24) | (c.r << 16) | (c.g << 8) | c.b;
        vga_bb_fill_rounded_rect(x - layer, y - layer,
                                 w + layer * 2, h + layer * 2,
                                 radius + layer, glow);
    }
    /* Main neon border — 1px outline */
    uint32_t border = (0x80 << 24) | (c.r << 16) | (c.g << 8) | c.b;
    vga_bb_draw_rect_outline(x, y, w, h, border);
    /* Inner top edge shine */
    uint32_t shine = (0x30 << 24) | (c.r << 16) | (c.g << 8) | c.b;
    vga_bb_draw_hline(x + radius, y + 1, w - 2 * radius, shine);
}


/* ═══════════════════════════════════════════════════════════════
 *  Drag Handle (6-dot grip for rearrange mode)
 * ═══════════════════════════════════════════════════════════════ */

extern "C"
void ui_drag_handle(int x, int y, int w, int h, uint32_t color) {
    /* 2 columns × 3 rows of small dots */
    int dot_r = 2;
    int cx1 = x + w / 2 - 4;
    int cx2 = x + w / 2 + 4;
    int spacing = h / 4;
    if (spacing < 4) spacing = 4;

    for (int row = 0; row < 3; row++) {
        int cy = y + spacing + row * spacing;
        if (cy + dot_r >= y + h) break;
        vga_bb_fill_circle(cx1, cy, dot_r, color);
        vga_bb_fill_circle(cx2, cy, dot_r, color);
    }
}


/* ═══════════════════════════════════════════════════════════════
 *  Desktop Icon Card — Frosted glass with hover neon glow
 * ═══════════════════════════════════════════════════════════════ */

extern "C"
void ui_icon_card(int x, int y, int w, int h, int hovered, uint32_t accent) {
    if (hovered) {
        /* Neon glow halo when hovered */
        RGBA c(accent);
        for (int layer = 3; layer >= 1; layer--) {
            uint32_t alpha = 10 + (3 - layer) * 8;
            uint32_t glow = (alpha << 24) | (c.r << 16) | (c.g << 8) | c.b;
            vga_bb_fill_rounded_rect(x - layer, y - layer,
                                     w + layer * 2, h + layer * 2,
                                     12 + layer, glow);
        }
        /* Brighter frosted glass */
        vga_bb_fill_rounded_rect(x, y, w, h, 12, 0x35FFFFFF);
        /* Neon border */
        uint32_t border = (0x60 << 24) | (c.r << 16) | (c.g << 8) | c.b;
        vga_bb_draw_rect_outline(x, y, w, h, border);
    } else {
        /* Subtle shadow */
        vga_bb_fill_rounded_rect(x + 3, y + 3, w, h, 12, S_SHADOW);
        /* Frosted glass */
        vga_bb_fill_rounded_rect(x, y, w, h, 12, 0x20FFFFFF);
        vga_bb_draw_rect_outline(x, y, w, h, 0x18FFFFFF);
    }
    /* Top inner shine */
    vga_bb_draw_hline(x + 12, y + 1, w - 24, 0x12FFFFFF);
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
 *  Aurora Wallpaper — Enhanced Neon Aurora
 * ═══════════════════════════════════════════════════════════════ */

extern "C"
void ui_render_aurora_wallpaper(uint32_t *buf, int w, int h) {
    /* 3-stop vertical gradient base: deep space → teal nebula → purple nebula */
    int half = h / 2;
    if (half <= 0) half = 1;

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

    /* Primary aurora — intense cyan glow at upper-center */
    int gcx = w / 2;
    int gcy = h / 3;
    int max_r2 = (w * w) / 5 + (h * h) / 7;
    for (int y = 0; y < h; y++) {
        uint32_t *row = &buf[y * w];
        int dy = y - gcy;
        int dy2 = dy * dy;
        for (int x = 0; x < w; x++) {
            int dx = x - gcx;
            int dist2 = (dx * dx) / 3 + dy2;
            if (dist2 < max_r2) {
                int intensity = 28 - (dist2 * 28) / max_r2;
                if (intensity > 0) {
                    RGBA bg(row[x]);
                    /* Cyan aurora tint — more vivid */
                    bg.r = clamp((int)bg.r + intensity / 3, 0, 255);
                    bg.g = clamp((int)bg.g + intensity * 5 / 2, 0, 255);
                    bg.b = clamp((int)bg.b + intensity * 2, 0, 255);
                    row[x] = bg.pack();
                }
            }
        }
    }

    /* Secondary aurora — magenta/purple glow at upper-right */
    int g2x = w * 3 / 4;
    int g2y = h / 4;
    int max2_r2 = (w * w) / 9 + (h * h) / 11;
    for (int y = 0; y < h; y++) {
        uint32_t *row = &buf[y * w];
        int dy = y - g2y;
        int dy2 = dy * dy;
        for (int x = 0; x < w; x++) {
            int dx = x - g2x;
            int dist2 = (dx * dx) / 3 + dy2;
            if (dist2 < max2_r2) {
                int intensity = 18 - (dist2 * 18) / max2_r2;
                if (intensity > 0) {
                    RGBA bg(row[x]);
                    /* Magenta/purple aurora */
                    bg.r = clamp((int)bg.r + intensity * 3, 0, 255);
                    bg.g = clamp((int)bg.g + intensity / 4, 0, 255);
                    bg.b = clamp((int)bg.b + intensity * 2, 0, 255);
                    row[x] = bg.pack();
                }
            }
        }
    }

    /* Tertiary aurora — subtle green glow at lower-left */
    int g3x = w / 4;
    int g3y = h * 2 / 3;
    int max3_r2 = (w * w) / 12 + (h * h) / 14;
    for (int y = 0; y < h; y++) {
        uint32_t *row = &buf[y * w];
        int dy = y - g3y;
        int dy2 = dy * dy;
        for (int x = 0; x < w; x++) {
            int dx = x - g3x;
            int dist2 = (dx * dx) / 2 + dy2;
            if (dist2 < max3_r2) {
                int intensity = 12 - (dist2 * 12) / max3_r2;
                if (intensity > 0) {
                    RGBA bg(row[x]);
                    /* Green-teal tint */
                    bg.r = clamp((int)bg.r + intensity / 3, 0, 255);
                    bg.g = clamp((int)bg.g + intensity * 2, 0, 255);
                    bg.b = clamp((int)bg.b + intensity, 0, 255);
                    row[x] = bg.pack();
                }
            }
        }
    }

    /* Stars — more numerous, color-tinted */
    uint32_t seed = 0xC0FFEE42;
    for (int i = 0; i < 120; i++) {
        seed = seed * 1103515245 + 12345;
        int sx = (seed >> 16) % w;
        seed = seed * 1103515245 + 12345;
        int sy = (seed >> 16) % h;
        seed = seed * 1103515245 + 12345;
        int brightness = 170 + (seed >> 16) % 85;
        /* Cool-tinted stars */
        int sr = clamp(brightness - 20 + (int)((seed >> 8) & 0x1F), 0, 255);
        int sg = clamp(brightness - 10, 0, 255);
        int sb = clamp(brightness + 10, 0, 255);
        uint32_t star = (0xFF000000) | (sr << 16) | (sg << 8) | sb;
        buf[sy * w + sx] = star;
        /* Some larger stars with glow */
        if (i < 25 && sx + 1 < w) {
            uint32_t dim = (0xFF000000) |
                           ((sr / 2) << 16) |
                           ((sg / 2) << 8) |
                           (sb / 2);
            buf[sy * w + sx + 1] = dim;
            if (sy + 1 < h) buf[(sy + 1) * w + sx] = dim;
            if (sx > 0) buf[sy * w + sx - 1] = dim;
            if (sy > 0) buf[(sy - 1) * w + sx] = dim;
        }
    }
}


/* ═══════════════════════════════════════════════════════════════
 *  Animated Aurora Wallpaper
 * ═══════════════════════════════════════════════════════════════ */

extern "C"
void ui_render_aurora_wallpaper_animated(uint32_t *buf, int w, int h, uint32_t phase) {
    /* 3-stop vertical gradient base */
    int half = h / 2;
    if (half <= 0) half = 1;

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

    /* Compute animated offsets based on phase (triangle waves) */
    int t1 = (phase >> 1) % 512; if (t1 > 255) t1 = 511 - t1;
    int t2 = (phase >> 2) % 512; if (t2 > 255) t2 = 511 - t2;
    int t3 = (phase + 128) % 512; if (t3 > 255) t3 = 511 - t3;
    
    int ox1 = (t1 - 128) * w / 2048; /* drift X */
    int oy1 = (t2 - 128) * h / 2048;
    int ox2 = (t3 - 128) * w / 2048;
    int oy2 = (t1 - 128) * h / 2048;

    /* Primary aurora */
    int gcx = w / 2 + ox1;
    int gcy = h / 3 + oy1;
    int max_r2 = (w * w) / 5 + (h * h) / 7;
    for (int y = 0; y < h; y++) {
        uint32_t *row = &buf[y * w];
        int dy = y - gcy;
        int dy2 = dy * dy;
        for (int x = 0; x < w; x++) {
            int dx = x - gcx;
            int dist2 = (dx * dx) / 3 + dy2;
            if (dist2 < max_r2) {
                int intensity = 28 - (dist2 * 28) / max_r2;
                if (intensity > 0) {
                    RGBA bg(row[x]);
                    bg.r = clamp((int)bg.r + intensity / 3 + t1/30, 0, 255);
                    bg.g = clamp((int)bg.g + intensity * 5 / 2, 0, 255);
                    bg.b = clamp((int)bg.b + intensity * 2, 0, 255);
                    row[x] = bg.pack();
                }
            }
        }
    }

    /* Secondary aurora */
    int g2x = w * 3 / 4 - ox2;
    int g2y = h / 4 + oy2;
    int max2_r2 = (w * w) / 9 + (h * h) / 11;
    for (int y = 0; y < h; y++) {
        uint32_t *row = &buf[y * w];
        int dy = y - g2y;
        int dy2 = dy * dy;
        for (int x = 0; x < w; x++) {
            int dx = x - g2x;
            int dist2 = (dx * dx) / 3 + dy2;
            if (dist2 < max2_r2) {
                int intensity = 18 - (dist2 * 18) / max2_r2;
                if (intensity > 0) {
                    RGBA bg(row[x]);
                    bg.r = clamp((int)bg.r + intensity * 3, 0, 255);
                    bg.g = clamp((int)bg.g + intensity / 4 + t2/40, 0, 255);
                    bg.b = clamp((int)bg.b + intensity * 2, 0, 255);
                    row[x] = bg.pack();
                }
            }
        }
    }

    /* Tertiary aurora */
    int g3x = w / 4 + ox2;
    int g3y = h * 2 / 3 - oy1;
    int max3_r2 = (w * w) / 12 + (h * h) / 14;
    for (int y = 0; y < h; y++) {
        uint32_t *row = &buf[y * w];
        int dy = y - g3y;
        int dy2 = dy * dy;
        for (int x = 0; x < w; x++) {
            int dx = x - g3x;
            int dist2 = (dx * dx) / 2 + dy2;
            if (dist2 < max3_r2) {
                int intensity = 12 - (dist2 * 12) / max3_r2;
                if (intensity > 0) {
                    RGBA bg(row[x]);
                    bg.r = clamp((int)bg.r + intensity / 3, 0, 255);
                    bg.g = clamp((int)bg.g + intensity * 2, 0, 255);
                    bg.b = clamp((int)bg.b + intensity + t3/40, 0, 255);
                    row[x] = bg.pack();
                }
            }
        }
    }

    /* Stars */
    uint32_t seed = 0xC0FFEE42;
    for (int i = 0; i < 120; i++) {
        seed = seed * 1103515245 + 12345; int sx = (seed >> 16) % w;
        seed = seed * 1103515245 + 12345; int sy = (seed >> 16) % h;
        seed = seed * 1103515245 + 12345; int brightness = 170 + (seed >> 16) % 85;
        
        int twinkle = (sx * 7 + sy * 13 + phase) % 256;
        if (twinkle > 128) twinkle = 256 - twinkle;
        brightness = clamp(brightness - 40 + twinkle/3, 0, 255);

        int sr = clamp(brightness - 20 + (int)((seed >> 8) & 0x1F), 0, 255);
        int sg = clamp(brightness - 10, 0, 255);
        int sb = clamp(brightness + 10, 0, 255);
        uint32_t star = (0xFF000000) | (sr << 16) | (sg << 8) | sb;
        buf[sy * w + sx] = star;
        if (i < 25 && sx + 1 < w) {
            uint32_t dim = (0xFF000000) | ((sr / 2) << 16) | ((sg / 2) << 8) | (sb / 2);
            buf[sy * w + sx + 1] = dim;
            if (sy + 1 < h) buf[(sy + 1) * w + sx] = dim;
            if (sx > 0) buf[sy * w + sx - 1] = dim;
            if (sy > 0) buf[(sy - 1) * w + sx] = dim;
        }
    }
}


/* ═══════════════════════════════════════════════════════════════
 *  Tray Icon Background with Hover Glow
 * ═══════════════════════════════════════════════════════════════ */

extern "C"
void ui_tray_icon_bg(int x, int y, int w, int h, int hovered) {
    if (hovered) {
        /* Outer neon glow halo */
        vga_bb_fill_rounded_rect(x - 1, y - 1, w + 2, h + 2,
                                 (h + 2) / 2, S_ACCENT_GLOW);
        /* Bright fill */
        vga_bb_fill_rounded_rect(x, y, w, h, h / 2, S_BG_HOVER);
        /* Top shine */
        vga_bb_draw_hline(x + h/2, y + 1, w - h > 0 ? w - h : 1, 0x15FFFFFF);
    } else {
        /* Subtle idle background */
        vga_bb_fill_rounded_rect(x, y, w, h, h / 2, 0x14FFFFFF);
    }
}


/* ═══════════════════════════════════════════════════════════════
 *  Mini Sparkline Bar Chart
 * ═══════════════════════════════════════════════════════════════ */

extern "C"
void ui_mini_graph(int x, int y, int w, int h,
                   const int *values, int count,
                   uint32_t bar_lo, uint32_t bar_hi) {
    if (count <= 0) return;
    int max_count = count > 20 ? 20 : count;

    /* Dark background */
    vga_bb_fill_rounded_rect(x, y, w, h, 3, S_BG_DEEP);
    /* Subtle border */
    vga_bb_draw_rect_outline(x, y, w, h, 0x20FFFFFF);

    int bar_w = (w - 4) / max_count;
    if (bar_w < 1) bar_w = 1;
    int gap = 1;

    for (int i = 0; i < max_count; i++) {
        int v = values[i];
        if (v < 0) v = 0;
        if (v > 100) v = 100;
        int bh = (v * (h - 4)) / 100;
        if (bh < 1 && v > 0) bh = 1;

        /* Interpolate color low → high based on value */
        uint32_t c = lerp(bar_lo, bar_hi, v, 100);
        int bx = x + 2 + i * bar_w;
        int by = y + h - 2 - bh;
        vga_bb_fill_rect(bx, by, bar_w - gap, bh, c);
    }
}


/* ═══════════════════════════════════════════════════════════════
 *  Pill-Shaped Badge
 * ═══════════════════════════════════════════════════════════════ */

extern "C"
void ui_badge(int x, int y, const char *label,
              uint32_t bg_color, uint32_t text_color) {
    int tw = slen(label) * 9;  /* 1x font width = 9px */
    int pw = tw + 10;
    int ph = 14;
    /* Pill background */
    vga_bb_fill_rounded_rect(x, y, pw, ph, ph / 2, bg_color);
    /* Top shine */
    vga_bb_draw_hline(x + ph/2, y + 1, pw - ph > 0 ? pw - ph : 1, 0x18FFFFFF);
    /* Label centered */
    vga_bb_draw_string(x + 5, y + 3, label, text_color, 0x00000000);
}


/* ═══════════════════════════════════════════════════════════════
 *  Vertical Divider with Alpha Fade
 * ═══════════════════════════════════════════════════════════════ */

extern "C"
void ui_divider_v(int x, int y, int h, uint32_t color) {
    RGBA c(color);
    for (int dy = 0; dy < h; dy++) {
        /* Fade at top and bottom 25% */
        int fade_zone = h / 4;
        if (fade_zone < 1) fade_zone = 1;
        uint32_t alpha = c.a;
        if (dy < fade_zone)
            alpha = (c.a * dy) / fade_zone;
        else if (dy > h - fade_zone)
            alpha = (c.a * (h - dy)) / fade_zone;
        uint32_t px = (alpha << 24) | (c.r << 16) | (c.g << 8) | c.b;
        vga_bb_putpixel(x, y + dy, px);
        /* Double width for visibility */
        vga_bb_putpixel(x + 1, y + dy, px);
    }
}


/* ═══════════════════════════════════════════════════════════════
 *  Glass Tooltip (renders above anchor point)
 * ═══════════════════════════════════════════════════════════════ */

extern "C"
void ui_tooltip(int anchor_x, int anchor_y,
                const char *text, uint32_t bg, uint32_t fg) {
    int tw = slen(text) * 18;  /* 2x font */
    int pw = tw + 20;
    int ph = 26;
    int tx = anchor_x - pw / 2;
    int ty = anchor_y - ph - 8;

    /* Clamp to screen */
    if (tx < 4) tx = 4;
    if (tx + pw > GFX_W - 4) tx = GFX_W - 4 - pw;
    if (ty < 2) ty = 2;

    /* Shadow */
    vga_bb_fill_rounded_rect(tx + 2, ty + 2, pw, ph, 8, S_SHADOW);
    /* Glass body */
    vga_bb_fill_rounded_rect(tx, ty, pw, ph, 8, bg);
    vga_bb_draw_rect_outline(tx, ty, pw, ph, S_GLASS_BORDER);
    /* Top shine */
    vga_bb_draw_hline(tx + 8, ty + 1, pw - 16, 0x20FFFFFF);
    /* Small pointer triangle */
    int px = anchor_x;
    for (int i = 0; i < 5; i++) {
        vga_bb_draw_hline(px - i, ty + ph + i, 2 * i + 1, bg);
    }
    /* Text */
    vga_bb_draw_string_2x(tx + 10, ty + 5, text, fg, 0x00000000);
}
