#ifndef UI_THEME_H
#define UI_THEME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════
 *  SwanOS "Neon Aurora" Theme — Vibrant Floating Dock
 *  Deep space darks, neon cyan/magenta/purple accents,
 *  deeper transparency, frosted glass everywhere.
 * ═══════════════════════════════════════════════════════════════ */

/* ── Background Tones (Deep Space) ─────────────────────────── */
#define S_BG_DEEP      0xFF060A12   /* Deepest void */
#define S_BG_DARK      0xFF0A1018   /* Dark space */
#define S_BG           0xFF0E1520   /* Normal background */
#define S_BG_ALT       0xFF141E2C   /* Alternate surface */
#define S_BG_LIGHT     0xFF1C2838   /* Lighter surface */
#define S_BG_HOVER     0xFF223248   /* Hover state */

/* ── Panel & Window Chrome ─────────────────────────────────── */
#define S_PANEL        0x700A1220   /* Very translucent panel (deeper!) */
#define S_PANEL_HOVER  0xFF162A42   /* Panel item hover */
#define S_TITLEBAR     0xFF0E1E30   /* Window titlebar */
#define S_TITLEBAR_F   0xFF122A48   /* Focused titlebar */
#define S_WIN_BG       0xFF0C1622   /* Window body */
#define S_BORDER       0xFF243040   /* Subtle border */
#define S_BORDER_F     0xFF00E5FF   /* Focused border — neon cyan! */
#define S_SEPARATOR    0xFF1C2838   /* Divider lines */

/* ── Text ──────────────────────────────────────────────────── */
#define S_TEXT         0xFFE8ECF4   /* Primary (cool white) */
#define S_TEXT_DIM     0xFF5A6A80   /* Muted (cool gray) */
#define S_TEXT_INV     0xFF0A1018   /* On bright backgrounds */

/* ── Neon Accents ──────────────────────────────────────────── */
#define S_ACCENT       0xFF00E5FF   /* Neon cyan (primary) */
#define S_ACCENT2      0xFF00BCD4   /* Deep cyan */
#define S_ACCENT_GLOW  0x3000E5FF   /* Cyan glow (semi-transparent) */
#define S_NEON_CYAN    0xFF00E5FF   /* Bright neon cyan */
#define S_NEON_MAGENTA 0xFFFF006E   /* Hot magenta */
#define S_NEON_PURPLE  0xFFC050FF   /* Electric purple */
#define S_GLOW_PULSE   0x2000E5FF   /* Subtle pulse glow */

#define S_GREEN        0xFF4ADE80   /* Vivid mint */
#define S_YELLOW       0xFFFBBF24   /* Bright amber */
#define S_RED          0xFFFF6B8A   /* Neon rose */
#define S_ORANGE       0xFFFF8A40   /* Hot orange */
#define S_PURPLE       0xFFC084FC   /* Vivid lavender */
#define S_PINK         0xFFF472B6   /* Hot pink */
#define S_BLUE         0xFF60A5FA   /* Electric blue */

/* ── Effects ───────────────────────────────────────────────── */
#define S_SHADOW       0x50000008   /* Soft drop shadow w/ blue tint */
#define S_SHADOW_DEEP  0x70000818   /* Deeper shadow */
#define S_GLASS        0x25FFFFFF   /* Glass overlay */
#define S_GLASS_BORDER 0x18FFFFFF   /* Glass border shimmer */
#define S_HOVER        0xFF1A2E48   /* General hover */
#define S_KICKOFF_BG   0xE0080E18   /* Menu background */
#define S_KICKOFF_HL   0xFF0E3860   /* Menu highlight — cyan tint */

/* ── Wallpaper Aurora (more dramatic) ──────────────────────── */
#define W_TOP          0xFF060818   /* Deep space */
#define W_MID          0xFF0A2838   /* Teal nebula */
#define W_BOT          0xFF140828   /* Purple nebula */

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

/* Frosted panel — enhanced glass with noise grain texture */
void ui_frosted_panel(int x, int y, int w, int h, int radius,
                      uint32_t bg_color, uint32_t border_color);

/* Pill-shaped segment zone for segmented dock layout */
void ui_pill_segment(int x, int y, int w, int h, int radius,
                     uint32_t bg_color, uint32_t border_color,
                     int accent_underline);

/* Neon border with outer glow halo */
void ui_neon_border(int x, int y, int w, int h, int radius,
                    uint32_t neon_color);

/* Drag handle indicator (6-dot grip for rearrange mode) */
void ui_drag_handle(int x, int y, int w, int h, uint32_t color);

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

/* ── Taskbar Tray Helpers ─────────────────────────────────── */

/* Tray icon hover/background pill with optional glow */
void ui_tray_icon_bg(int x, int y, int w, int h, int hovered);

/* Tiny sparkline bar chart (up to 20 values, 0..100 range) */
void ui_mini_graph(int x, int y, int w, int h,
                   const int *values, int count,
                   uint32_t bar_lo, uint32_t bar_hi);

/* Pill-shaped labeled badge with gradient fill */
void ui_badge(int x, int y, const char *label,
              uint32_t bg_color, uint32_t text_color);

/* Vertical divider with alpha-faded endpoints */
void ui_divider_v(int x, int y, int h, uint32_t color);

/* Glass-styled tooltip with shadow (renders above, centered on anchor_x) */
void ui_tooltip(int anchor_x, int anchor_y,
                const char *text, uint32_t bg, uint32_t fg);

#ifdef __cplusplus
}
#endif

#endif /* UI_THEME_H */
