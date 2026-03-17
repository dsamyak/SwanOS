#ifndef UI_THEME_H
#define UI_THEME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════
 *  SwanOS "Serenity" Theme — Soothing Ocean-Teal Palette
 *  Calming dark blues, soft teals, muted pastels.
 * ═══════════════════════════════════════════════════════════════ */

/* ── Background Tones (Deep Ocean) ──────────────────────────── */
#define S_BG_DEEP      0xFF0B1420   /* Deepest midnight */
#define S_BG_DARK      0xFF0F1923   /* Deep navy */
#define S_BG           0xFF162032   /* Normal background */
#define S_BG_ALT       0xFF1C2A3D   /* Alternate surface */
#define S_BG_LIGHT     0xFF243447   /* Lighter surface */
#define S_BG_HOVER     0xFF2A3C52   /* Hover state */

/* ── Panel & Window Chrome ──────────────────────────────────── */
#define S_PANEL        0xCC0D1B2A   /* Translucent panel */
#define S_PANEL_HOVER  0xFF1A2F45   /* Panel item hover */
#define S_TITLEBAR     0xFF142638   /* Window titlebar */
#define S_TITLEBAR_F   0xFF193350   /* Focused titlebar */
#define S_WIN_BG       0xFF131E2E   /* Window body */
#define S_BORDER       0xFF2D3748   /* Subtle border */
#define S_BORDER_F     0xFF4FD1C5   /* Focused border (teal) */
#define S_SEPARATOR    0xFF233345   /* Divider lines */

/* ── Text ───────────────────────────────────────────────────── */
#define S_TEXT         0xFFE2E8F0   /* Primary (warm white) */
#define S_TEXT_DIM     0xFF718096   /* Muted (slate gray) */
#define S_TEXT_INV     0xFF0F1923   /* On bright backgrounds */

/* ── Accents (Soft pastels) ─────────────────────────────────── */
#define S_ACCENT       0xFF4FD1C5   /* Soft teal */
#define S_ACCENT2      0xFF38B2AC   /* Deep teal */
#define S_ACCENT_GLOW  0x304FD1C5   /* Teal glow (semi-transparent) */
#define S_GREEN        0xFF68D391   /* Soft mint */
#define S_YELLOW       0xFFF6AD55   /* Warm amber */
#define S_RED          0xFFFC8181   /* Soft rose */
#define S_ORANGE       0xFFED8936   /* Warm orange */
#define S_PURPLE       0xFFB794F4   /* Soft lavender */
#define S_PINK         0xFFF687B3   /* Soft pink */
#define S_BLUE         0xFF63B3ED   /* Calm sky blue */

/* ── Effects ────────────────────────────────────────────────── */
#define S_SHADOW       0x40000000   /* Soft drop shadow */
#define S_SHADOW_DEEP  0x60000810   /* Deeper shadow with blue tint */
#define S_GLASS        0x30FFFFFF   /* Glass overlay */
#define S_GLASS_BORDER 0x20FFFFFF   /* Glass border shimmer */
#define S_HOVER        0xFF1E3350   /* General hover */
#define S_KICKOFF_BG   0xE80D1620   /* Menu background */
#define S_KICKOFF_HL   0xFF1A5276   /* Menu highlight */

/* ── Wallpaper Aurora ───────────────────────────────────────── */
#define W_TOP          0xFF0B0E1A   /* Deep space navy */
#define W_MID          0xFF0F3443   /* Ocean teal */
#define W_BOT          0xFF1A1040   /* Deep purple */

/* ═══════════════════════════════════════════════════════════════
 *  C++ Theme Rendering API
 * ═══════════════════════════════════════════════════════════════ */

/* Multi-stop gradient fill (3-color: top → mid → bottom) */
void ui_gradient_3stop(int x, int y, int w, int h,
                       uint32_t c_top, uint32_t c_mid, uint32_t c_bot);

/* Soft multi-layer shadow (more realistic depth) */
void ui_soft_shadow(int x, int y, int w, int h, int radius, int layers);

/* Glassmorphism panel with frosted edges */
void ui_glass_panel(int x, int y, int w, int h, int radius,
                    uint32_t bg_color, uint32_t border_color);

/* Card widget — rounded rect with subtle inner top highlight */
void ui_card(int x, int y, int w, int h, int radius, uint32_t bg_color);

/* Pill-shaped button with hover state */
void ui_button(int x, int y, int w, int h,
               const char *label, uint32_t bg, uint32_t text_color,
               int hovered);

/* Status dot with soft glow halo */
void ui_status_dot(int cx, int cy, int r, uint32_t color);

/* Label pair: "Key: Value" in dim/bright colors */
void ui_label_pair(int x, int y, const char *key, const char *value,
                   uint32_t key_color, uint32_t val_color);

/* Render the aurora wallpaper into a buffer */
void ui_render_aurora_wallpaper(uint32_t *buf, int w, int h);

/* Window soft shadow (multiple offset blurred layers) */
void ui_window_shadow(int x, int y, int w, int h);

/* Progress/loading bar with gradient fill and glow */
void ui_progress_bar(int x, int y, int w, int h,
                     int value, int max_val,
                     uint32_t fill_color, uint32_t track_color);

/* Refined section header with accent underline */
void ui_section_header(int x, int y, int w,
                       const char *title, uint32_t color);

#ifdef __cplusplus
}
#endif

#endif /* UI_THEME_H */
