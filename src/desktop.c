/* ============================================================
 * SwanOS — Desktop Environment (KDE Plasma Breeze Dark Style)
 * Translucent panel, Kickoff menu, Breeze window decorations,
 * gradient wallpaper, 2x fonts, smooth alpha blending.
 * ============================================================ */

#include <stdint.h>
#include "desktop.h"
#include "vga_gfx.h"
#include "mouse.h"
#include "keyboard.h"
#include "timer.h"
#include "screen.h"
#include "string.h"
#include "fs.h"
#include "user.h"
#include "rtc.h"
#include "memory.h"
#include "llm.h"
#include "game.h"
#include "ports.h"
#include "serial.h"

/* ── Layout ───────────────────────────────────────────────── */
#define SCRW       GFX_W
#define SCRH       GFX_H
#define PANEL_H    44
#define DESK_H     (SCRH - PANEL_H)
#define TITLEBAR_H 32
#define CW         18   /* char width 2x */
#define CH         16   /* char height 2x */

/* ── KDE Breeze Dark Palette ──────────────────────────────── */
#define B_BG_DARK    0xFF1B1E20   /* Darkest background */
#define B_BG         0xFF232629   /* Normal background */
#define B_BG_ALT     0xFF2A2E32   /* Alternate background */
#define B_BG_LIGHT   0xFF31363B   /* Lighter background */
#define B_PANEL      0xDD1A1D21   /* Panel (semi-transparent) */
#define B_PANEL_HOVER 0xFF2A2E32
#define B_TITLEBAR   0xFF33373C   /* Window title bar */
#define B_TITLEBAR_F 0xFF2E3238   /* Focused title bar */
#define B_WIN_BG     0xFF232629   /* Window body */
#define B_BORDER     0xFF3E4349   /* Subtle border */
#define B_BORDER_F   0xFF3DAEE9   /* Focused window accent */
#define B_TEXT       0xFFEFF0F1   /* Primary text */
#define B_TEXT_DIM   0xFF7F8C8D   /* Dimmed text */
#define B_TEXT_INV   0xFF232629   /* Text on bright bg */
#define B_ACCENT     0xFF3DAEE9   /* KDE Blue accent */
#define B_ACCENT2    0xFF2D9AD7   /* Darker accent */
#define B_GREEN      0xFF27AE60   /* Green */
#define B_YELLOW     0xFFF39C12   /* Yellow */
#define B_RED        0xFFDA4453   /* Close button red */
#define B_ORANGE     0xFFF47750   /* Warm accent */
#define B_SEPARATOR  0xFF414548   /* Separator lines */
#define B_HOVER      0xFF3B4045   /* Hover state */
#define B_KICKOFF    0xEE1E2124   /* Kickoff menu bg */
#define B_KICKOFF_HL 0xFF2980B9   /* Kickoff highlight */
#define B_SHADOW     0x50000000   /* Drop shadow */

/* Wallpaper gradient — KDE-style blue-purple */
#define W_TOP        0xFF0D1B2A   /* Deep navy */
#define W_MID        0xFF1B2838   /* Mid blue */
#define W_BOT        0xFF1A1028   /* Purple tint */

/* ── Icons ────────────────────────────────────────────────── */
#define ICON_W     80
#define ICON_H     80
#define MAX_ICONS  5

typedef struct { int x, y; const char *label; int app_id; } desktop_icon_t;

static desktop_icon_t icons[MAX_ICONS] = {
    {50, 40,   "AI Chat",  5},
    {50, 150,  "Terminal", 0},
    {50, 260,  "Files",    1},
    {50, 370,  "Notes",    2},
    {50, 480,  "About",    3},
};
static int num_icons = 5;

/* ── Windows ──────────────────────────────────────────────── */
#define MAX_WINDOWS 6
#define WIN_TERM 0
#define WIN_FILES 1
#define WIN_NOTES 2
#define WIN_ABOUT 3
#define WIN_AI 4

typedef struct {
    int active, x, y, w, h, type;
    char title[20];
    char lines[16][40];
    int line_count, scroll;
    char input[80];
    int input_pos;
    char note_text[512];
    int note_len, note_cursor;
    char note_file[20];
    int file_sel;
} window_t;

static window_t windows[MAX_WINDOWS];
static int win_count = 0, win_focus = -1;
static int win_order[MAX_WINDOWS], win_order_count = 0;
static int dragging = 0, drag_win = -1, drag_ox, drag_oy;

/* ── Kickoff Menu ─────────────────────────────────────────── */
static int kickoff_open = 0;
#define KO_W     280
#define KO_ITEMS  7
#define KO_ITEM_H 36
#define KO_HEADER  64
#define KO_H      (KO_HEADER + KO_ITEMS * KO_ITEM_H + 8)

static const char *ko_labels[KO_ITEMS] = {
    "AI Chat", "Terminal", "Files", "Notes", "About",
    "--------", "Shut Down"
};
static int ko_ids[KO_ITEMS] = {5,0,1,2,3,-1,-2};

/* ── Wallpaper cache ──────────────────────────────────────── */
static int wp_cached = 0;
static uint32_t wp_buf[1920 * 1080];

/* ── Forward declarations ─────────────────────────────────── */
static void open_window(int type);
static void draw_desktop(void);
static int  handle_click(int mx, int my);
static void term_add_line(window_t *w, const char *text);
static void term_process_cmd(window_t *w);

/* ── Wallpaper ────────────────────────────────────────────── */
static void render_wallpaper(void) {
    for (int y = 0; y < DESK_H && y < GFX_H; y++) {
        /* 3-point gradient: top→mid→bottom */
        uint32_t c;
        int half = DESK_H / 2;
        if (y < half)
            c = ARGB(255,
                13 + (27-13)*y/half,
                27 + (40-27)*y/half,
                42 + (56-42)*y/half);
        else
            c = ARGB(255,
                27 + (26-27)*(y-half)/half,
                40 + (16-40)*(y-half)/half,
                56 + (40-56)*(y-half)/half);
        for (int x = 0; x < GFX_W; x++)
            wp_buf[y * GFX_W + x] = c;
    }
    /* Subtle radial glow at center */
    int cx = GFX_W / 2, cy = DESK_H / 2;
    for (int y = 0; y < DESK_H; y++) {
        for (int x = 0; x < GFX_W; x++) {
            int dx = x - cx, dy = y - cy;
            int dist2 = (dx*dx)/4 + dy*dy;
            int max2 = (GFX_W * GFX_H) / 8;
            if (dist2 < max2) {
                int alpha = 18 - (dist2 * 18) / max2;
                if (alpha > 0) {
                    uint32_t bg = wp_buf[y * GFX_W + x];
                    uint32_t br=(bg>>16)&0xFF, bg2=(bg>>8)&0xFF, bb=bg&0xFF;
                    uint32_t nr = br + alpha * 2; if (nr > 255) nr = 255;
                    uint32_t ng = bg2 + alpha * 3; if (ng > 255) ng = 255;
                    uint32_t nb = bb + alpha * 5; if (nb > 255) nb = 255;
                    wp_buf[y * GFX_W + x] = ARGB(255, nr, ng, nb);
                }
            }
        }
    }
    /* Stars */
    uint32_t seed = 0xBEEF;
    for (int i = 0; i < 60; i++) {
        seed = seed * 1103515245 + 12345;
        int sx = (seed >> 16) % GFX_W;
        seed = seed * 1103515245 + 12345;
        int sy = (seed >> 16) % DESK_H;
        seed = seed * 1103515245 + 12345;
        int b = 120 + (seed >> 16) % 135;
        wp_buf[sy * GFX_W + sx] = ARGB(255, b, b, b+15>255?255:b+15);
    }
    wp_cached = 1;
}

static void draw_wallpaper(void) {
    if (!wp_cached) render_wallpaper();
    uint32_t *bb = vga_backbuffer();
    for (int y = 0; y < DESK_H && y < GFX_H; y++)
        for (int x = 0; x < GFX_W; x++)
            bb[y * GFX_W + x] = wp_buf[y * GFX_W + x];
}

/* ── Desktop icons (Breeze style) ─────────────────────────── */
static void draw_icon_glyph(int x, int y, int app_id) {
    /* Icon area with rounded translucent bg on hover (simplified: always show plate) */
    vga_bb_fill_rounded_rect(x+4, y+2, 56, 46, 8, 0x18FFFFFF);
    int cx = x + 16, cy = y + 8;
    switch (app_id) {
        case 0: /* Terminal — Konsole style */
            vga_bb_fill_rounded_rect(cx-2, cy, 32, 24, 4, B_BG_DARK);
            vga_bb_draw_rect_outline(cx-2, cy, 32, 24, B_ACCENT);
            vga_bb_draw_string_2x(cx+2, cy+4, ">_", B_GREEN, 0x00000000);
            break;
        case 1: /* Files — Dolphin style */
            vga_bb_fill_rounded_rect(cx, cy, 28, 22, 4, B_ACCENT);
            vga_bb_fill_rounded_rect(cx-4, cy+4, 8, 3, 2, B_ACCENT2);
            vga_bb_fill_rect(cx+2, cy+4, 24, 16, B_BG_ALT);
            break;
        case 2: /* Notes — KWrite style */
            vga_bb_fill_rounded_rect(cx, cy, 24, 26, 3, B_TEXT);
            vga_bb_fill_rect(cx+2, cy+2, 20, 22, B_BG_LIGHT);
            for (int i = 0; i < 4; i++)
                vga_bb_draw_hline(cx+4, cy+6+i*5, 14, B_TEXT_DIM);
            break;
        case 3: /* About — info */
            vga_bb_fill_circle(cx+12, cy+12, 12, B_ACCENT);
            vga_bb_fill_circle(cx+12, cy+12, 10, B_BG);
            vga_bb_draw_string_2x(cx+6, cy+5, "i", B_ACCENT, 0x00000000);
            break;
        case 5: /* AI Chat */
            vga_bb_fill_rounded_rect(cx-2, cy, 32, 26, 6, B_ACCENT);
            vga_bb_fill_rounded_rect(cx, cy+2, 28, 22, 5, B_BG);
            vga_bb_draw_string_2x(cx+2, cy+5, "AI", B_ACCENT, 0x00000000);
            vga_bb_fill_circle(cx+28, cy+2, 4, B_YELLOW);
            break;
    }
}

static void draw_icons(void) {
    for (int i = 0; i < num_icons; i++) {
        desktop_icon_t *ic = &icons[i];
        draw_icon_glyph(ic->x, ic->y, ic->app_id);
        int lw = (int)strlen(ic->label) * CW;
        int lx = ic->x + (64 - lw) / 2;
        if (lx < 2) lx = 2;
        /* Text shadow */
        vga_bb_draw_string_2x(lx+1, ic->y+55, ic->label, 0x80000000, 0x00000000);
        vga_bb_draw_string_2x(lx, ic->y+54, ic->label, B_TEXT, 0x00000000);
    }
}

/* ── Panel (KDE Plasma bottom panel) ──────────────────────── */
static void draw_panel(void) {
    int py = DESK_H;
    /* Semi-transparent dark panel */
    vga_bb_fill_rect_alpha(0, py, SCRW, PANEL_H, B_PANEL);
    /* Top edge line */
    vga_bb_draw_hline(0, py, SCRW, B_SEPARATOR);

    /* App launcher button (KDE logo area) */
    mouse_state_t ms; mouse_get_state(&ms);
    int lhover = (ms.x >= 2 && ms.x < 50 && ms.y >= py);
    if (lhover || kickoff_open)
        vga_bb_fill_rounded_rect(2, py+4, 46, PANEL_H-8, 6, B_HOVER);
    /* Simple KDE-like icon: circle with 3 dots */
    vga_bb_fill_circle(25, py + PANEL_H/2, 12, B_ACCENT);
    vga_bb_fill_circle(25, py + PANEL_H/2, 10, B_BG_DARK);
    /* Three horizontal lines (hamburger) */
    vga_bb_draw_hline(19, py+PANEL_H/2-4, 12, B_TEXT);
    vga_bb_draw_hline(19, py+PANEL_H/2,   12, B_TEXT);
    vga_bb_draw_hline(19, py+PANEL_H/2+4, 12, B_TEXT);

    /* Task manager — window buttons */
    int bx = 56;
    /* Separator */
    vga_bb_draw_vline(bx-2, py+8, PANEL_H-16, B_SEPARATOR);

    for (int i = 0; i < win_order_count && bx < SCRW - 300; i++) {
        int wi = win_order[i];
        if (!windows[wi].active) continue;
        int focused = (wi == win_focus);
        int bw = (int)strlen(windows[wi].title) * CW + 20;
        if (bw > 200) bw = 200;
        /* Hover check */
        int thover = (ms.x >= bx && ms.x < bx+bw && ms.y >= py+4 && ms.y < py+PANEL_H-4);
        if (focused) {
            vga_bb_fill_rounded_rect(bx, py+4, bw, PANEL_H-8, 6, B_HOVER);
            /* Active indicator — accent blue line below */
            vga_bb_fill_rounded_rect(bx+8, py+PANEL_H-6, bw-16, 3, 1, B_ACCENT);
        } else if (thover) {
            vga_bb_fill_rounded_rect(bx, py+4, bw, PANEL_H-8, 6, 0xFF2A2D31);
        }
        vga_bb_draw_string_2x(bx+10, py+14, windows[wi].title, B_TEXT, 0x00000000);
        bx += bw + 4;
    }

    /* ── System tray (right side) ── */
    /* Separator before tray */
    int tx = SCRW - 250;
    vga_bb_draw_vline(tx, py+8, PANEL_H-16, B_SEPARATOR);

    /* Memory indicator */
    char mb[16]; char tmp[8];
    itoa(mem_used()/1024, tmp, 10);
    strcpy(mb, tmp); strcat(mb, "K");
    vga_bb_draw_string_2x(tx+8, py+14, mb, B_TEXT_DIM, 0x00000000);

    /* Clock */
    rtc_time_t rtc; rtc_read(&rtc);
    char clk[10]; rtc_format_time(&rtc, clk);
    int clk_x = SCRW - (int)strlen(clk) * CW - 16;
    vga_bb_draw_string_2x(clk_x, py+14, clk, B_TEXT, 0x00000000);
}

/* ── Kickoff Menu (KDE-style app launcher) ────────────────── */
static void draw_kickoff(void) {
    if (!kickoff_open) return;
    int mx_top = DESK_H - KO_H;

    /* Dark rounded menu background */
    vga_bb_fill_rounded_rect(0, mx_top, KO_W, KO_H, 10, B_KICKOFF);
    vga_bb_draw_rect_outline(0, mx_top, KO_W, KO_H, B_SEPARATOR);

    /* Header: user info */
    vga_bb_fill_rounded_rect(8, mx_top+8, KO_W-16, KO_HEADER-16, 8, B_BG_ALT);
    /* User avatar circle */
    vga_bb_fill_circle(36, mx_top+32, 16, B_ACCENT);
    vga_bb_fill_circle(36, mx_top+32, 14, B_BG_DARK);
    vga_bb_draw_string_2x(26, mx_top+26, user_current()[0] ? (char[]){user_current()[0],0} : "U",
                          B_ACCENT, 0x00000000);
    /* Username */
    vga_bb_draw_string_2x(62, mx_top+20, user_current(), B_TEXT, 0x00000000);
    vga_bb_draw_string_2x(62, mx_top+38, "SwanOS User", B_TEXT_DIM, 0x00000000);

    /* Separator */
    vga_bb_draw_hline(12, mx_top + KO_HEADER - 4, KO_W-24, B_SEPARATOR);

    /* Menu items */
    mouse_state_t ms; mouse_get_state(&ms);
    for (int i = 0; i < KO_ITEMS; i++) {
        int iy = mx_top + KO_HEADER + i * KO_ITEM_H;
        if (ko_labels[i][0] == '-') {
            vga_bb_draw_hline(16, iy + KO_ITEM_H/2, KO_W - 32, B_SEPARATOR);
            continue;
        }
        int hover = (ms.x >= 4 && ms.x < KO_W-4 &&
                     ms.y >= iy && ms.y < iy + KO_ITEM_H);
        if (hover)
            vga_bb_fill_rounded_rect(4, iy+2, KO_W-8, KO_ITEM_H-4, 6, B_KICKOFF_HL);

        /* Small icon indicator */
        uint32_t ic = (i == KO_ITEMS-1) ? B_RED : B_ACCENT;
        vga_bb_fill_circle(24, iy + KO_ITEM_H/2, 6, ic);
        vga_bb_fill_circle(24, iy + KO_ITEM_H/2, 4, B_BG_DARK);

        uint32_t fc = (i == KO_ITEMS-1) ? B_RED : B_TEXT;
        vga_bb_draw_string_2x(40, iy + 10, ko_labels[i], fc, 0x00000000);
    }
}

/* ── Breeze Window Decorations ────────────────────────────── */
static void draw_window(int wi) {
    window_t *w = &windows[wi];
    if (!w->active) return;
    int focused = (wi == win_focus);

    /* Shadow */
    vga_bb_fill_rect_alpha(w->x+4, w->y+4, w->w+2, w->h+2, B_SHADOW);

    /* Window body */
    vga_bb_fill_rounded_rect(w->x, w->y, w->w, w->h, 8, B_WIN_BG);

    /* Border — accent on focus, subtle otherwise */
    uint32_t bc = focused ? B_BORDER_F : B_BORDER;
    vga_bb_draw_rect_outline(w->x, w->y, w->w, w->h, bc);
    /* Second pixel border for focus glow */
    if (focused) {
        vga_bb_draw_hline(w->x+1, w->y, w->w-2, B_BORDER_F);
    }

    /* Title bar */
    uint32_t tb = focused ? B_TITLEBAR_F : B_TITLEBAR;
    vga_bb_fill_rect(w->x+1, w->y+1, w->w-2, TITLEBAR_H, tb);
    /* Title bar bottom separator */
    vga_bb_draw_hline(w->x+1, w->y+TITLEBAR_H, w->w-2, B_SEPARATOR);

    /* Title text (centered in title bar) */
    int tw = (int)strlen(w->title) * CW;
    int ttx = w->x + (w->w - tw) / 2;
    vga_bb_draw_string_2x(ttx, w->y+8, w->title, B_TEXT, 0x00000000);

    /* Breeze window buttons: close (red) • maximize (accent) • minimize (accent) */
    int by = w->y + (TITLEBAR_H - 12) / 2;
    /* Close — rightmost */
    int cbx = w->x + w->w - 24;
    vga_bb_fill_circle(cbx, by+6, 7, B_RED);
    vga_bb_fill_circle(cbx, by+6, 5, focused ? B_RED : B_BG_LIGHT);
    /* Maximize */
    vga_bb_fill_circle(cbx-22, by+6, 7, B_ACCENT);
    vga_bb_fill_circle(cbx-22, by+6, 5, focused ? B_ACCENT : B_BG_LIGHT);
    /* Minimize */
    vga_bb_fill_circle(cbx-44, by+6, 7, B_YELLOW);
    vga_bb_fill_circle(cbx-44, by+6, 5, focused ? B_YELLOW : B_BG_LIGHT);

    /* ── Content area ── */
    int cx = w->x + 4;
    int cy = w->y + TITLEBAR_H + 4;
    int cw = w->w - 8;
    int ch = w->h - TITLEBAR_H - 8;

    if (w->type == WIN_TERM || w->type == WIN_AI) {
        vga_bb_fill_rect(cx, cy, cw, ch, B_BG_DARK);
        int text_y = cy + 4;
        if (w->type == WIN_AI) {
            vga_bb_fill_rect(cx, cy, cw, CH+6, B_BG_ALT);
            vga_bb_draw_string_2x(cx+8, cy+3, "AI Assistant", B_ACCENT, 0x00000000);
            vga_bb_draw_hline(cx, cy+CH+6, cw, B_SEPARATOR);
            text_y = cy + CH + 10;
        }
        int max_l = (ch - CH - 12 - (w->type == WIN_AI ? CH+6 : 0)) / CH;
        int sl = 0;
        if (w->line_count > max_l) sl = w->line_count - max_l;
        int ly = text_y;
        for (int i = sl; i < w->line_count && i < sl + max_l; i++) {
            int mc = (cw - 12) / CW;
            uint32_t lc = (w->type == WIN_AI && w->lines[i][0] == '>') ? B_GREEN :
                          (w->type == WIN_AI ? B_ACCENT : B_GREEN);
            for (int j = 0; j < mc && w->lines[i][j]; j++)
                vga_bb_draw_char_2x(cx+6+j*CW, ly, w->lines[i][j], lc, 0x00000000);
            ly += CH;
        }
        /* Input */
        int iy = cy + ch - CH - 4;
        vga_bb_draw_hline(cx, iy-3, cw, B_SEPARATOR);
        vga_bb_fill_rect(cx, iy-2, cw, CH+6, B_BG_ALT);
        const char *pr = (w->type == WIN_AI) ? "?" : ">";
        uint32_t pc = (w->type == WIN_AI) ? B_YELLOW : B_ACCENT;
        vga_bb_draw_string_2x(cx+4, iy, pr, pc, 0x00000000);
        int mi = (cw - CW*3) / CW;
        int s = 0;
        if (w->input_pos > mi) s = w->input_pos - mi;
        for (int i = s; i < w->input_pos; i++)
            vga_bb_draw_char_2x(cx+CW+6+(i-s)*CW, iy, w->input[i], B_TEXT, 0x00000000);
        int cur_x = cx + CW + 6 + (w->input_pos - s) * CW;
        if (cur_x < cx+cw-4) {
            uint32_t cc = (timer_get_ticks()/30)%2 ? pc : B_BG_ALT;
            vga_bb_fill_rect(cur_x, iy, 2, CH, cc);
        }
    }
    else if (w->type == WIN_FILES) {
        vga_bb_fill_rect(cx, cy, cw, ch, B_BG_DARK);
        /* Toolbar */
        vga_bb_fill_rect(cx, cy, cw, CH+6, B_BG_ALT);
        vga_bb_draw_string_2x(cx+8, cy+3, "/ (root)", B_TEXT_DIM, 0x00000000);
        vga_bb_draw_hline(cx, cy+CH+6, cw, B_SEPARATOR);

        char listing[512]; fs_list("/", listing, sizeof(listing));
        char *p = listing;
        int fy = cy + CH + 10, fi = 0;
        while (*p && fy + CH <= cy + ch) {
            while (*p == ' ') p++;
            if (*p == '\0' || *p == '\n') { if (*p) p++; continue; }
            int is_dir = (strncmp(p, "[DIR]", 5) == 0);
            char fname[24]; int fni = 0;
            char *ns = strchr(p, ']');
            if (ns) ns += 2; else ns = p;
            while (*ns && *ns != '\n' && fni < 22) fname[fni++] = *ns++;
            fname[fni] = '\0';
            if (fi == w->file_sel)
                vga_bb_fill_rounded_rect(cx+2, fy-2, cw-4, CH+4, 4, B_KICKOFF_HL);
            uint32_t fc = is_dir ? B_YELLOW : B_ACCENT;
            const char *pf = is_dir ? "+ " : "  ";
            vga_bb_draw_string_2x(cx+8, fy, pf, B_TEXT_DIM, 0x00000000);
            vga_bb_draw_string_2x(cx+8+2*CW, fy, fname, fc, 0x00000000);
            fy += CH + 4; fi++;
            while (*p && *p != '\n') p++;
            if (*p == '\n') p++;
        }
    }
    else if (w->type == WIN_NOTES) {
        vga_bb_fill_rect(cx, cy, cw, ch, B_BG_DARK);
        vga_bb_draw_string_2x(cx+8, cy+4, "File:", B_TEXT_DIM, 0x00000000);
        vga_bb_draw_string_2x(cx+8+5*CW, cy+4,
                              w->note_file[0] ? w->note_file : "untitled", B_ACCENT, 0x00000000);
        vga_bb_draw_hline(cx, cy+CH+8, cw, B_SEPARATOR);
        int ty = cy+CH+12, tx2 = cx+8, col = 0, mx = (cw-16)/CW;
        for (int i = 0; i < w->note_len && ty+CH <= cy+ch-4; i++) {
            if (w->note_text[i] == '\n' || col >= mx) { ty += CH; col = 0; tx2 = cx+8; if (w->note_text[i]=='\n') continue; }
            vga_bb_draw_char_2x(tx2, ty, w->note_text[i], B_TEXT, 0x00000000);
            tx2 += CW; col++;
        }
        if (focused) {
            uint32_t cc = (timer_get_ticks()/30)%2 ? B_ACCENT : 0;
            vga_bb_fill_rect(tx2, ty, 2, CH, cc);
        }
        vga_bb_draw_string_2x(cx+8, cy+ch-CH-4, "Ctrl+S: save", B_TEXT_DIM, 0x00000000);
    }
    else if (w->type == WIN_ABOUT) {
        vga_bb_fill_rect(cx, cy, cw, ch, B_WIN_BG);
        int ay = cy + 12;
        vga_bb_draw_string_2x(cx+12, ay, "SwanOS", B_ACCENT, 0x00000000);
        ay += CH+6;
        vga_bb_draw_string_2x(cx+12, ay, "Version 3.0", B_TEXT, 0x00000000);
        ay += CH+6;
        vga_bb_draw_hline(cx+12, ay, cw-24, B_SEPARATOR);
        ay += 8;
        vga_bb_draw_string_2x(cx+12, ay, "AI-Powered OS", B_GREEN, 0x00000000);
        ay += CH+4;
        vga_bb_draw_string_2x(cx+12, ay, "Bare-Metal x86", B_TEXT, 0x00000000);
        ay += CH+6;
        char res[20]; itoa(GFX_W, res, 10); strcat(res, "x");
        char t2[8]; itoa(GFX_H, t2, 10); strcat(res, t2);
        vga_bb_draw_string_2x(cx+12, ay, res, B_TEXT_DIM, 0x00000000);
        ay += CH+4;
        char mb[20]; char tmp[8];
        strcpy(mb, "Mem: "); itoa(mem_total()/1024, tmp, 10); strcat(mb, tmp); strcat(mb, "K");
        vga_bb_draw_string_2x(cx+12, ay, mb, B_TEXT_DIM, 0x00000000);
        ay += CH+8;
        vga_bb_draw_string_2x(cx+12, ay, "User:", B_TEXT_DIM, 0x00000000);
        vga_bb_draw_string_2x(cx+12+6*CW, ay, user_current(), B_ACCENT, 0x00000000);
    }
}

/* ── Cursor (Breeze-like arrow) ───────────────────────────── */
static const uint8_t cur[14][10] = {
    {1,0,0,0,0,0,0,0,0,0},
    {1,1,0,0,0,0,0,0,0,0},
    {1,2,1,0,0,0,0,0,0,0},
    {1,2,2,1,0,0,0,0,0,0},
    {1,2,2,2,1,0,0,0,0,0},
    {1,2,2,2,2,1,0,0,0,0},
    {1,2,2,2,2,2,1,0,0,0},
    {1,2,2,2,2,2,2,1,0,0},
    {1,2,2,2,2,2,2,2,1,0},
    {1,2,2,2,2,1,1,1,1,1},
    {1,2,2,1,2,1,0,0,0,0},
    {1,2,1,0,1,2,1,0,0,0},
    {1,1,0,0,0,1,2,1,0,0},
    {1,0,0,0,0,0,1,1,0,0},
};

static void draw_cursor(int mx, int my) {
    for (int dy = 0; dy < 14; dy++)
        for (int dx = 0; dx < 10; dx++) {
            if (!cur[dy][dx]) continue;
            int px = mx+dx, py = my+dy;
            if (px >= SCRW || py >= SCRH) continue;
            vga_bb_putpixel(px, py, cur[dy][dx]==1 ? 0xFF101010 : B_TEXT);
        }
}

/* ── Window management ────────────────────────────────────── */
static int find_free_window(void) { for (int i=0;i<MAX_WINDOWS;i++) if (!windows[i].active) return i; return -1; }

static void bring_to_front(int wi) {
    int pos=-1;
    for (int i=0;i<win_order_count;i++) if (win_order[i]==wi) {pos=i;break;}
    if (pos>=0) { for (int i=pos;i<win_order_count-1;i++) win_order[i]=win_order[i+1]; }
    else { if (win_order_count>=MAX_WINDOWS) return; }
    win_order[win_order_count>0?win_order_count-1:0]=wi;
    if (pos<0 && win_order_count<MAX_WINDOWS) win_order_count++;
    win_focus=wi;
}

static void close_window(int wi) {
    windows[wi].active=0;
    int pos=-1;
    for (int i=0;i<win_order_count;i++) if (win_order[i]==wi) {pos=i;break;}
    if (pos>=0) { for (int i=pos;i<win_order_count-1;i++) win_order[i]=win_order[i+1]; win_order_count--; }
    win_focus=(win_order_count>0)?win_order[win_order_count-1]:-1;
}

static void open_window(int type) {
    for (int i=0;i<MAX_WINDOWS;i++) { if (windows[i].active && windows[i].type==type) { bring_to_front(i); return; } }
    int wi=find_free_window(); if (wi<0) return;
    window_t *w=&windows[wi]; memset(w,0,sizeof(window_t)); w->active=1; w->type=type;
    switch (type) {
        case WIN_TERM: strcpy(w->title,"Konsole"); w->x=280;w->y=120;w->w=820;w->h=520;
            term_add_line(w,"SwanOS Terminal"); term_add_line(w,"Type 'help'"); break;
        case WIN_FILES: strcpy(w->title,"Dolphin"); w->x=320;w->y=180;w->w=820;w->h=520; w->file_sel=0; break;
        case WIN_NOTES: strcpy(w->title,"KWrite"); w->x=360;w->y=220;w->w=820;w->h=520;
            strcpy(w->note_file,"notes.txt"); fs_read("notes.txt",w->note_text,sizeof(w->note_text)-1);
            w->note_len=strlen(w->note_text); w->note_cursor=w->note_len; break;
        case WIN_ABOUT: strcpy(w->title,"About"); w->x=420;w->y=280;w->w=620;w->h=420; break;
        case WIN_AI: strcpy(w->title,"AI Chat"); w->x=240;w->y=80;w->w=840;w->h=620;
            term_add_line(w,"SwanOS AI Assistant"); term_add_line(w,"Ask me anything!"); break;
    }
    win_order[win_order_count++]=wi; win_focus=wi;
}

/* ── Terminal helpers ─────────────────────────────────────── */
static void term_add_line(window_t *w, const char *text) {
    if (w->line_count>=16) { for (int i=0;i<15;i++) strcpy(w->lines[i],w->lines[i+1]); w->line_count=15; }
    strncpy(w->lines[w->line_count],text,39); w->lines[w->line_count][39]='\0'; w->line_count++;
}

static void term_process_cmd(window_t *w) {
    char *cmd=w->input; while (*cmd==' ') cmd++; if (!cmd[0]) return;
    char echo[44]; strcpy(echo,"> "); strcat(echo,cmd); term_add_line(w,echo);
    char cc[80]; strcpy(cc,cmd);
    char *arg=cc; while (*arg && *arg!=' ') arg++; if (*arg){*arg='\0';arg++;} while (*arg==' ') arg++;

    if (!strcmp(cc,"help")) { term_add_line(w,"Commands:"); term_add_line(w," ls cat write mkdir rm"); term_add_line(w," calc date mem whoami"); term_add_line(w," ask <question>"); term_add_line(w," clear exit"); }
    else if (!strcmp(cc,"clear")) { w->line_count=0; }
    else if (!strcmp(cc,"exit")) { for (int i=0;i<MAX_WINDOWS;i++) if (&windows[i]==w){close_window(i);break;} }
    else if (!strcmp(cc,"ls")) { char l[256]; fs_list(arg[0]?arg:"/",l,sizeof(l)); char *p=l; while (*p){char ln[40];int li=0;while(*p&&*p!='\n'&&li<38)ln[li++]=*p++;ln[li]='\0';if(li>0)term_add_line(w,ln);if(*p=='\n')p++;} }
    else if (!strcmp(cc,"cat")) { if (!arg[0]) term_add_line(w,"Usage: cat <file>"); else { char ct[256]; int r=fs_read(arg,ct,sizeof(ct)); if(r>=0) term_add_line(w,ct); else term_add_line(w,"File not found"); } }
    else if (!strcmp(cc,"write")) { char *ct=arg; while(*ct&&*ct!=' ')ct++; if(*ct){*ct='\0';ct++;} if(!arg[0]||!ct[0]) term_add_line(w,"Usage: write <f> <text>"); else{fs_write(arg,ct);term_add_line(w,"Written.");} }
    else if (!strcmp(cc,"mkdir")) { if(!arg[0]) term_add_line(w,"Usage: mkdir <name>"); else{fs_mkdir(arg);term_add_line(w,"Created.");} }
    else if (!strcmp(cc,"rm")) { if(!arg[0]) term_add_line(w,"Usage: rm <file>"); else{fs_delete(arg);term_add_line(w,"Deleted.");} }
    else if (!strcmp(cc,"date")) { rtc_time_t t;rtc_read(&t);char d[12],tb[10];rtc_format_date(&t,d);rtc_format_time(&t,tb); char m[30];strcpy(m,d);strcat(m," ");strcat(m,tb);term_add_line(w,m); }
    else if (!strcmp(cc,"mem")) { char m[40];char t[8];strcpy(m,"Used: ");itoa(mem_used()/1024,t,10);strcat(m,t);strcat(m,"K / ");itoa(mem_total()/1024,t,10);strcat(m,t);strcat(m,"K");term_add_line(w,m); }
    else if (!strcmp(cc,"whoami")) { char m[30];strcpy(m,"User: ");strcat(m,user_current());term_add_line(w,m); }
    else if (!strcmp(cc,"calc")) {
        if (!arg[0]) { term_add_line(w,"Usage: calc <expr>"); }
        else { int res=0,num=0;char op='+';int has=0;const char *e=arg;
            while(*e){if(*e>='0'&&*e<='9'){num=num*10+(*e-'0');has=1;}else if(*e=='+'||*e=='-'||*e=='*'||*e=='/'){if(has){if(op=='+')res+=num;else if(op=='-')res-=num;else if(op=='*')res*=num;else if(op=='/'&&num)res/=num;}op=*e;num=0;has=0;}e++;}
            if(has){if(op=='+')res+=num;else if(op=='-')res-=num;else if(op=='*')res*=num;else if(op=='/'&&num)res/=num;}
            char m[20];char nb[12];strcpy(m,"= ");itoa(res,nb,10);strcat(m,nb);term_add_line(w,m);}
    }
    else if (!strcmp(cc,"ask")) { if(!arg[0]) term_add_line(w,"Usage: ask <q>"); else { term_add_line(w,"Thinking..."); char rsp[512]; llm_query(arg,rsp,sizeof(rsp)); char *rp=rsp; while(*rp){char ln[40];int li=0;while(*rp&&*rp!='\n'&&li<38)ln[li++]=*rp++;ln[li]='\0';if(li>0)term_add_line(w,ln);if(*rp=='\n')rp++;} } }
    else { term_add_line(w,"[AI] ..."); char rsp[512]; llm_query(w->input_pos>0?cmd:cc,rsp,sizeof(rsp)); if(w->line_count>0)w->line_count--; char *rp=rsp; while(*rp){char ln[40];int li=0;while(*rp&&*rp!='\n'&&li<38)ln[li++]=*rp++;ln[li]='\0';if(li>0)term_add_line(w,ln);if(*rp=='\n')rp++;} }
    w->input_pos=0; w->input[0]='\0';
}

/* ── Click handling ───────────────────────────────────────── */
static int handle_click(int mx, int my) {
    if (kickoff_open) {
        int mt = DESK_H - KO_H;
        if (mx>=0 && mx<KO_W && my>=mt+KO_HEADER && my<DESK_H) {
            int idx=(my-mt-KO_HEADER)/KO_ITEM_H;
            if (idx>=0 && idx<KO_ITEMS && ko_labels[idx][0]!='-') {
                kickoff_open=0; int aid=ko_ids[idx];
                if (aid==-2) return -1;
                if (aid==5) { open_window(WIN_AI); return 0; }
                if (aid>=0&&aid<=4) open_window(aid);
                return 0;
            }
        }
        kickoff_open=0; return 0;
    }
    /* Panel launcher */
    if (my>=DESK_H && mx<50) { kickoff_open=!kickoff_open; return 0; }
    /* Panel task buttons */
    if (my>=DESK_H) {
        int bx=56;
        for (int i=0;i<win_order_count;i++) { int wi=win_order[i]; if(!windows[wi].active) continue;
            int bw=(int)strlen(windows[wi].title)*CW+20; if(bw>200) bw=200;
            if (mx>=bx&&mx<bx+bw) { bring_to_front(wi); return 0; } bx+=bw+4; }
        return 0;
    }
    /* Windows (top z first) */
    for (int zi=win_order_count-1;zi>=0;zi--) {
        int wi=win_order[zi]; window_t *w=&windows[wi]; if(!w->active) continue;
        if (mx>=w->x&&mx<w->x+w->w&&my>=w->y&&my<w->y+w->h) {
            bring_to_front(wi);
            /* Close button check */
            int cbx=w->x+w->w-24, by=w->y+(TITLEBAR_H-12)/2;
            if (mx>=cbx-7&&mx<=cbx+7&&my>=by&&my<=by+14) { close_window(wi); return 0; }
            /* Title bar drag */
            if (my>=w->y&&my<w->y+TITLEBAR_H) { dragging=1;drag_win=wi;drag_ox=mx-w->x;drag_oy=my-w->y; return 0; }
            /* Files click */
            if (w->type==WIN_FILES) { int cy=w->y+TITLEBAR_H+CH+14; int idx=(my-cy)/(CH+4); if(idx>=0) w->file_sel=idx; }
            return 0;
        }
    }
    /* Desktop icons */
    for (int i=0;i<num_icons;i++) { desktop_icon_t *ic=&icons[i];
        if (mx>=ic->x&&mx<ic->x+ICON_W&&my>=ic->y&&my<ic->y+ICON_H) {
            if (ic->app_id==5){open_window(WIN_AI);return 0;} if(ic->app_id==4) return -4;
            if (ic->app_id<=3) open_window(ic->app_id); return 0; } }
    kickoff_open=0; return 0;
}

/* ── Full desktop draw ────────────────────────────────────── */
static void draw_desktop(void) {
    draw_wallpaper(); draw_icons();
    for (int i=0;i<win_order_count;i++) draw_window(win_order[i]);
    draw_panel(); draw_kickoff();
    mouse_state_t ms; mouse_get_state(&ms); draw_cursor(ms.x,ms.y);
    vga_flip();
}

/* ── Main loop ────────────────────────────────────────────── */
void desktop_run(void) {
    serial_write("desktop_run: start\n"); screen_set_serial_mirror(0xFF000000);
    memset(windows,0,sizeof(windows)); win_count=0; win_focus=-1; win_order_count=0; kickoff_open=0; dragging=0; wp_cached=0;
    open_window(WIN_ABOUT); open_window(WIN_TERM);
    draw_desktop();
    uint32_t last_draw=0; int needs_redraw=1;
    serial_write("desktop_run: enter loop\n");

    while (1) {
        mouse_state_t ms; mouse_get_state(&ms);
        if (dragging) {
            if (mouse_left_pressed()) {
                int nx=ms.x-drag_ox, ny=ms.y-drag_oy;
                if(nx<0)nx=0; if(ny<0)ny=0;
                if(nx+windows[drag_win].w>SCRW) nx=SCRW-windows[drag_win].w;
                if(ny+windows[drag_win].h>DESK_H) ny=DESK_H-windows[drag_win].h;
                windows[drag_win].x=nx; windows[drag_win].y=ny; needs_redraw=1;
            } else dragging=0;
        }
        if (ms.clicked) {
            mouse_clear_events();
            if (!dragging) {
                int r=handle_click(ms.x,ms.y);
                if (r==-1) { screen_init(); screen_set_serial_mirror(1); screen_clear();
                    screen_set_color(VGA_DARK_GREY,VGA_BLACK); screen_print("\n\n   Shutting down...\n");
                    screen_delay(500); __asm__ volatile("cli; hlt"); while(1); }
                if (r==-4) { game_snake(); wp_cached=0; }
                needs_redraw=1;
            }
        }
        if (ms.moved) { mouse_clear_events(); needs_redraw=1; }
        if (keyboard_has_key()) {
            char c=keyboard_getchar(); needs_redraw=1;
            if (win_focus>=0 && windows[win_focus].active) {
                window_t *fw=&windows[win_focus];
                if (fw->type==WIN_TERM) {
                    if(c=='\n') term_process_cmd(fw);
                    else if(c=='\b'){if(fw->input_pos>0){fw->input_pos--;fw->input[fw->input_pos]='\0';}}
                    else if(c>=' '&&fw->input_pos<78){fw->input[fw->input_pos++]=c;fw->input[fw->input_pos]='\0';}
                } else if (fw->type==WIN_NOTES) {
                    if(c==19) fs_write(fw->note_file,fw->note_text);
                    else if(c=='\b'){if(fw->note_len>0){fw->note_len--;fw->note_text[fw->note_len]='\0';}}
                    else if(c=='\n'){if(fw->note_len<510){fw->note_text[fw->note_len++]='\n';fw->note_text[fw->note_len]='\0';}}
                    else if(c>=' '&&fw->note_len<510){fw->note_text[fw->note_len++]=c;fw->note_text[fw->note_len]='\0';}
                } else if (fw->type==WIN_FILES) {
                    if((uint8_t)c==KEY_UP&&fw->file_sel>0) fw->file_sel--;
                    if((uint8_t)c==KEY_DOWN) fw->file_sel++;
                } else if (fw->type==WIN_AI) {
                    if(c=='\n'){if(fw->input_pos>0){
                        char echo[44];strcpy(echo,"> ");strcat(echo,fw->input);term_add_line(fw,echo);term_add_line(fw,"[AI] ...");
                        char rsp[512];llm_query(fw->input,rsp,sizeof(rsp));if(fw->line_count>0)fw->line_count--;
                        char *rp=rsp;while(*rp){char ln[40];int li=0;while(*rp&&*rp!='\n'&&li<38)ln[li++]=*rp++;ln[li]='\0';if(li>0)term_add_line(fw,ln);if(*rp=='\n')rp++;}
                        fw->input_pos=0;fw->input[0]='\0';}}
                    else if(c=='\b'){if(fw->input_pos>0){fw->input_pos--;fw->input[fw->input_pos]='\0';}}
                    else if(c>=' '&&fw->input_pos<78){fw->input[fw->input_pos++]=c;fw->input[fw->input_pos]='\0';}
                }
            }
        }
        uint32_t ticks=timer_get_ticks();
        if (ticks-last_draw>10||needs_redraw) { draw_desktop(); last_draw=ticks; needs_redraw=0; }
        __asm__ volatile("hlt");
    }
}
