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

/* Wallpaper gradient — Modern Vibrant */
#define W_TOP        0xFF150A21   /* Deep indigo */
#define W_MID        0xFF4E1A47   /* Vibrant magenta */
#define W_BOT        0xFF0D2840   /* Deep cyan/ocean  */

/* ── Icons ────────────────────────────────────────────────── */
#define MAX_WINDOWS 8
#define WIN_TERM 0
#define WIN_FILES 1
#define WIN_NOTES 2
#define WIN_ABOUT 3
#define WIN_AI 4
#define WIN_CALC 5
#define WIN_SYSMON 6

#define ICON_W     80
#define ICON_H     80
#define MAX_ICONS  5

typedef struct { int x, y; const char *label; int app_id; } desktop_icon_t;

#undef MAX_ICONS
#define MAX_ICONS 7
static desktop_icon_t icons[MAX_ICONS] = {
    {50, 40,   "AI Chat",  WIN_AI},
    {50, 150,  "Terminal", WIN_TERM},
    {50, 260,  "Files",    WIN_FILES},
    {50, 370,  "Notes",    WIN_NOTES},
    {50, 480,  "About",    WIN_ABOUT},
    {50, 590,  "Calc",     WIN_CALC},
    {50, 700,  "SysMon",   WIN_SYSMON},
};
static int num_icons = 7;

/* ── Windows ──────────────────────────────────────────────── */

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
    int calc_v1;
    int calc_op;
    int calc_new;
    int sysmon_history[60];
    int sysmon_head;
} window_t;

static window_t windows[MAX_WINDOWS];
static int win_count = 0, win_focus = -1;
static int win_order[MAX_WINDOWS], win_order_count = 0;
static int dragging = 0, drag_win = -1, drag_ox, drag_oy;

/* ── Kickoff Menu ─────────────────────────────────────────── */
static int kickoff_open = 0;
#define KO_W     280
#define KO_ITEMS  9
#define KO_ITEM_H 36
#define KO_HEADER  64
#define KO_H      (KO_HEADER + KO_ITEMS * KO_ITEM_H + 8)

static const char *ko_labels[KO_ITEMS] = {
    "AI Chat", "Terminal", "Files", "Notes", "Calc", "SysMonitor", "About",
    "--------", "Shut Down"
};
static int ko_ids[KO_ITEMS] = {WIN_AI,WIN_TERM,WIN_FILES,WIN_NOTES,WIN_CALC,WIN_SYSMON,WIN_ABOUT,-1,-2};

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
        if (y < half) {
            uint32_t tr=(W_TOP>>16)&0xFF, tg=(W_TOP>>8)&0xFF, tb=W_TOP&0xFF;
            uint32_t mr=(W_MID>>16)&0xFF, mg=(W_MID>>8)&0xFF, mb=W_MID&0xFF;
            c = ARGB(255, tr + (mr-tr)*y/half, tg + (mg-tg)*y/half, tb + (mb-tb)*y/half);
        } else {
            uint32_t mr=(W_MID>>16)&0xFF, mg=(W_MID>>8)&0xFF, mb=W_MID&0xFF;
            uint32_t br=(W_BOT>>16)&0xFF, bg=(W_BOT>>8)&0xFF, bb=W_BOT&0xFF;
            c = ARGB(255, mr + (br-mr)*(y-half)/half, mg + (bg-mg)*(y-half)/half, mb + (bb-mb)*(y-half)/half);
        }
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
    /* Richer drop shadows for icons */
    vga_bb_fill_rounded_rect(x+6, y+4, 56, 46, 12, B_SHADOW);
    /* Frosted glass plate */
    vga_bb_fill_rounded_rect_gradient(x+4, y+2, 56, 46, 10, 0x2AFFFFFF, 0x10FFFFFF);
    vga_bb_draw_rect_outline(x+4, y+2, 56, 46, 0x33FFFFFF);
    int cx = x + 16, cy = y + 8;
    switch (app_id) {
        case 0: /* Terminal — Konsole style */
            vga_bb_fill_rounded_rect(cx-2, cy, 32, 24, 4, B_BG_DARK);
            vga_bb_fill_rect(cx-2, cy, 32, 6, B_TITLEBAR);
            vga_bb_draw_rect_outline(cx-2, cy, 32, 24, B_ACCENT);
            vga_bb_draw_string_2x(cx, cy+7, ">_", B_GREEN, 0x00000000);
            vga_bb_fill_circle(cx+26, cy+3, 1, B_RED);
            break;
        case 1: /* Files — Dolphin style */
            vga_bb_fill_rounded_rect(cx, cy, 28, 22, 4, B_ACCENT2); /* Back folder flap */
            vga_bb_fill_rounded_rect(cx-4, cy+4, 10, 3, 2, B_ACCENT2);
            vga_bb_fill_rounded_rect(cx-2, cy+6, 30, 18, 4, B_ACCENT); /* Front flap */
            vga_bb_fill_rect(cx+2, cy+8, 22, 12, B_BG_ALT);
            break;
        case 2: /* Notes — Richer pages */
            vga_bb_fill_rounded_rect(cx-1, cy+1, 24, 26, 3, B_SHADOW);
            vga_bb_fill_rounded_rect(cx, cy, 24, 26, 3, B_TEXT);
            vga_bb_fill_rect(cx+2, cy+2, 20, 22, B_BG_LIGHT);
            for (int i = 0; i < 4; i++)
                vga_bb_draw_hline(cx+4, cy+6+i*5, 14, B_ACCENT);
            break;
        case 3: /* About — info */
            vga_bb_fill_circle(cx+13, cy+13, 12, B_SHADOW);
            vga_bb_fill_circle(cx+12, cy+12, 12, B_ACCENT);
            vga_bb_fill_circle(cx+12, cy+12, 10, B_BG);
            vga_bb_draw_string_2x(cx+6, cy+5, "i", B_ACCENT, 0x00000000);
            vga_bb_draw_string_2x(cx+6, cy+5, "i", 0x80000000, 0x00000000); /* Drop shadow on i */
            vga_bb_draw_string_2x(cx+6, cy+4, "i", B_ACCENT, 0x00000000);
            break;
        case WIN_AI: /* AI Chat */
            vga_bb_fill_rounded_rect(cx-1, cy+1, 32, 26, 6, B_SHADOW);
            vga_bb_fill_rounded_rect(cx-2, cy, 32, 26, 6, B_ACCENT);
            vga_bb_fill_rounded_rect(cx, cy+2, 28, 22, 5, B_BG);
            vga_bb_draw_string_2x(cx+3, cy+6, "AI", 0x80000000, 0x00000000);
            vga_bb_draw_string_2x(cx+2, cy+5, "AI", B_ACCENT, 0x00000000);
            vga_bb_fill_circle(cx+28, cy+2, 4, B_YELLOW);
            break;
        case WIN_CALC:
            vga_bb_fill_rounded_rect(cx-2, cy, 28, 34, 4, B_BG_DARK);
            vga_bb_draw_rect_outline(cx-2, cy, 28, 34, B_ACCENT2);
            vga_bb_fill_rect(cx, cy+4, 24, 8, B_BG_ALT);
            vga_bb_draw_string_2x(cx+10, cy+1, "=", B_GREEN, 0x00000000);
            for(int yy=0;yy<3;yy++) for(int xx=0;xx<3;xx++) vga_bb_fill_rect(cx+2+xx*8, cy+16+yy*6, 6, 4, B_SEPARATOR);
            break;
        case WIN_SYSMON:
            vga_bb_fill_rounded_rect(cx-2, cy, 32, 28, 4, B_WIN_BG);
            vga_bb_draw_rect_outline(cx-2, cy, 32, 28, B_ACCENT);
            vga_bb_fill_rect(cx+2, cy+16, 6, 10, B_GREEN);
            vga_bb_fill_rect(cx+10, cy+10, 6, 16, B_YELLOW);
            vga_bb_fill_rect(cx+18, cy+6, 6, 20, B_RED);
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

/* Dock Layout Globals for Clicks */
static int dock_x = 0, dock_w = 0, dock_y = 0;
static int kickoff_x = 0;

/* ── Panel (Floating Dock) ──────────────────────── */
static void draw_panel(void) {
    /* Calculate required dock width */
    int wins_w = 0;
    for (int i = 0; i < win_order_count; i++) {
        int wi = win_order[i];
        if (!windows[wi].active) continue;
        int bw = (int)strlen(windows[wi].title) * CW + 20;
        if (bw > 200) bw = 200;
        wins_w += bw + 8;
    }
    
    dock_w = 56 + 16 /* launcher + prepad */ + wins_w + 240 /* tray */;
    if (dock_w > SCRW - 40) dock_w = SCRW - 40;
    dock_x = (SCRW - dock_w) / 2;
    dock_y = SCRH - PANEL_H - 12;

    /* Dock shadow */
    vga_bb_fill_rounded_rect(dock_x-4, dock_y-2, dock_w+8, PANEL_H+10, 16, B_SHADOW);
    /* Frosted glass / translucent dock */
    vga_bb_fill_rounded_rect(dock_x, dock_y, dock_w, PANEL_H, 12, B_PANEL);
    vga_bb_draw_rect_outline(dock_x, dock_y, dock_w, PANEL_H, 0x33FFFFFF);

    /* App launcher button (KDE logo area) */
    mouse_state_t ms; mouse_get_state(&ms);
    int lhover = (ms.x >= dock_x+8 && ms.x < dock_x+54 && ms.y >= dock_y);
    kickoff_x = dock_x + 8; /* Save for kickoff menu opening */
    
    if (lhover || kickoff_open)
        vga_bb_fill_rounded_rect(dock_x+8, dock_y+4, 46, PANEL_H-8, 8, B_HOVER);
        
    /* Simple KDE-like icon: circle with 3 dots */
    vga_bb_fill_circle(dock_x+31, dock_y + PANEL_H/2, 12, B_ACCENT);
    vga_bb_fill_circle(dock_x+31, dock_y + PANEL_H/2, 10, B_BG_DARK);
    /* Three horizontal lines (hamburger) */
    vga_bb_draw_hline(dock_x+25, dock_y+PANEL_H/2-4, 12, B_TEXT);
    vga_bb_draw_hline(dock_x+25, dock_y+PANEL_H/2,   12, B_TEXT);
    vga_bb_draw_hline(dock_x+25, dock_y+PANEL_H/2+4, 12, B_TEXT);

    /* Task manager — window buttons */
    int bx = dock_x + 64;
    /* Separator */
    vga_bb_draw_vline(bx-4, dock_y+8, PANEL_H-16, B_SEPARATOR);

    for (int i = 0; i < win_order_count && bx < dock_x + dock_w - 250; i++) {
        int wi = win_order[i];
        if (!windows[wi].active) continue;
        int focused = (wi == win_focus);
        int bw = (int)strlen(windows[wi].title) * CW + 20;
        if (bw > 200) bw = 200;
        /* Hover check */
        int thover = (ms.x >= bx && ms.x < bx+bw && ms.y >= dock_y+4 && ms.y < dock_y+PANEL_H-4);
        if (focused) {
            vga_bb_fill_rounded_rect(bx, dock_y+4, bw, PANEL_H-8, 6, B_HOVER);
            /* Active indicator — accent blue pill below */
            vga_bb_fill_rounded_rect(bx+bw/2-8, dock_y+PANEL_H-6, 16, 3, 1, B_ACCENT);
        } else if (thover) {
            vga_bb_fill_rounded_rect(bx, dock_y+4, bw, PANEL_H-8, 6, 0xFF2A2D31);
        }
        vga_bb_draw_string_2x(bx+11, dock_y+15, windows[wi].title, 0x80000000, 0x00000000); /* shadow */
        vga_bb_draw_string_2x(bx+10, dock_y+14, windows[wi].title, B_TEXT, 0x00000000);
        bx += bw + 8;
    }

    /* ── System tray (right side) ── */
    /* Separator before tray */
    int tx = dock_x + dock_w - 240;
    vga_bb_draw_vline(tx, dock_y+8, PANEL_H-16, B_SEPARATOR);

    /* Memory indicator */
    char mb[16]; char tmp[8];
    itoa(mem_used()/1024, tmp, 10);
    strcpy(mb, tmp); strcat(mb, "K");
    vga_bb_draw_string_2x(tx+9, dock_y+15, mb, 0x80000000, 0x00000000); /* shadow */
    vga_bb_draw_string_2x(tx+8, dock_y+14, mb, B_TEXT_DIM, 0x00000000);

    /* Clock */
    rtc_time_t rtc; rtc_read(&rtc);
    char clk[10]; rtc_format_time(&rtc, clk);
    int clk_x = dock_x + dock_w - (int)strlen(clk) * CW - 16;
    vga_bb_draw_string_2x(clk_x+1, dock_y+15, clk, 0x80000000, 0x00000000); /* shadow */
    vga_bb_draw_string_2x(clk_x, dock_y+14, clk, B_TEXT, 0x00000000);

}

/* ── Context Menu ─────────────────────────────────────────── */
static int ctx_menu_open = 0;
static int ctx_menu_x = 0;
static int ctx_menu_y = 0;
#define CTX_W 240
#define CTX_ITEMS 3
#define CTX_ITEM_H 36
#define CTX_H (CTX_ITEMS * CTX_ITEM_H + 8)
static const char *ctx_labels[CTX_ITEMS] = { "New Note", "System Monitor", "Refresh Wallpaper" };

static void draw_context_menu(void) {
    if (!ctx_menu_open) return;
    vga_bb_fill_rounded_rect(ctx_menu_x+4, ctx_menu_y+4, CTX_W, CTX_H, 8, B_SHADOW);
    vga_bb_fill_rounded_rect(ctx_menu_x, ctx_menu_y, CTX_W, CTX_H, 6, B_KICKOFF);
    vga_bb_draw_rect_outline(ctx_menu_x, ctx_menu_y, CTX_W, CTX_H, B_SEPARATOR);
    mouse_state_t ms; mouse_get_state(&ms);
    for (int i=0; i<CTX_ITEMS; i++) {
        int iy = ctx_menu_y + 4 + i*CTX_ITEM_H;
        int hover = (ms.x >= ctx_menu_x+4 && ms.x < ctx_menu_x+CTX_W-4 && ms.y >= iy && ms.y < iy+CTX_ITEM_H);
        if (hover) vga_bb_fill_rounded_rect(ctx_menu_x+4, iy+2, CTX_W-8, CTX_ITEM_H-4, 4, B_KICKOFF_HL);
        vga_bb_draw_string_2x(ctx_menu_x+10, iy+10, ctx_labels[i], B_TEXT, 0x00000000);
    }
}

/* ── Kickoff Menu (Centered above launcher) ───────────────── */
static void draw_kickoff(void) {
    if (!kickoff_open) return;
    int mx_top = dock_y - KO_H - 12;
    int mx_left = kickoff_x;

    /* Drop shadow for menu */
    vga_bb_fill_rounded_rect(mx_left+4, mx_top+4, KO_W, KO_H, 12, B_SHADOW);

    /* Dark rounded menu background */
    vga_bb_fill_rounded_rect_gradient(mx_left, mx_top, KO_W, KO_H, 10, B_KICKOFF, 0xFF14171A);
    vga_bb_draw_rect_outline(mx_left, mx_top, KO_W, KO_H, B_SEPARATOR);

    /* Header: user info */
    vga_bb_fill_rounded_rect(mx_left+8, mx_top+8, KO_W-16, KO_HEADER-16, 8, B_BG_ALT);
    /* User avatar circle */
    vga_bb_fill_circle(mx_left+36, mx_top+32, 16, B_ACCENT);
    vga_bb_fill_circle(mx_left+36, mx_top+32, 14, B_BG_DARK);
    vga_bb_draw_string_2x(mx_left+27, mx_top+27, user_current()[0] ? (char[]){user_current()[0],0} : "U", 0x80000000, 0);
    vga_bb_draw_string_2x(mx_left+26, mx_top+26, user_current()[0] ? (char[]){user_current()[0],0} : "U", B_ACCENT, 0);
    /* Username */
    vga_bb_draw_string_2x(mx_left+63, mx_top+21, user_current(), 0x80000000, 0);
    vga_bb_draw_string_2x(mx_left+62, mx_top+20, user_current(), B_TEXT, 0);
    vga_bb_draw_string_2x(mx_left+62, mx_top+38, "SwanOS User", B_TEXT_DIM, 0);

    /* Separator */
    vga_bb_draw_hline(mx_left+12, mx_top + KO_HEADER - 4, KO_W-24, B_SEPARATOR);

    /* Menu items */
    mouse_state_t ms; mouse_get_state(&ms);
    for (int i = 0; i < KO_ITEMS; i++) {
        int iy = mx_top + KO_HEADER + i * KO_ITEM_H;
        if (ko_labels[i][0] == '-') {
            vga_bb_draw_hline(mx_left+16, iy + KO_ITEM_H/2, KO_W - 32, B_SEPARATOR);
            continue;
        }
        int hover = (ms.x >= mx_left+4 && ms.x < mx_left+KO_W-4 &&
                     ms.y >= iy && ms.y < iy + KO_ITEM_H);
        if (hover)
            vga_bb_fill_rounded_rect(mx_left+4, iy+2, KO_W-8, KO_ITEM_H-4, 6, B_KICKOFF_HL);

        /* Small icon indicator */
        uint32_t ic = (i == KO_ITEMS-1) ? B_RED : B_ACCENT;
        vga_bb_fill_circle(mx_left+24, iy + KO_ITEM_H/2, 6, ic);
        vga_bb_fill_circle(mx_left+24, iy + KO_ITEM_H/2, 4, B_BG_DARK);

        uint32_t fc = (i == KO_ITEMS-1) ? B_RED : B_TEXT;
        if (hover) vga_bb_draw_string_2x(mx_left+41, iy + 11, ko_labels[i], 0x80000000, 0);
        vga_bb_draw_string_2x(mx_left+40, iy + 10, ko_labels[i], fc, 0x00000000);
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
    vga_bb_draw_string_2x(ttx+2, w->y+10, w->title, B_SHADOW, 0x00000000);
    vga_bb_draw_string_2x(ttx+1, w->y+9, w->title, 0x80000000, 0x00000000);
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
        vga_bb_draw_string_2x(cx+12, ay, "Version 2.0", B_TEXT, 0x00000000);
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
    else if (w->type == WIN_CALC) {
        vga_bb_fill_rect(cx, cy, cw, ch, B_BG_DARK);
        vga_bb_fill_rounded_rect(cx+10, cy+10, cw-20, 60, 4, B_BG_ALT);
        int tw = (int)strlen(w->input) * CW;
        vga_bb_draw_string_2x(cx+cw-10-tw-8, cy+30, w->input, B_TEXT, 0x00000000);
        const char* btns[20] = { "C", " ", " ", "/", "7", "8", "9", "*", "4", "5", "6", "-", "1", "2", "3", "+", "0", "0", ".", "=" };
        int bw = (cw - 50) / 4; int bh = (ch - 90 - 40) / 5;
        for (int i=0; i<20; i++) {
            if (i == 17) continue; /* merged zero */
            int bx = cx + 10 + (i%4)*(bw+10); int by = cy + 80 + (i/4)*(bh+10);
            int rbw = bw; if (i == 16) rbw = bw*2 + 10;
            uint32_t bg = B_WIN_BG; if (i%4 == 3 || i == 19) bg = B_ACCENT2; else if (i < 3) bg = B_SEPARATOR;
            vga_bb_fill_rounded_rect(bx, by, rbw, bh, 6, bg);
            int sw = (int)strlen(btns[i]) * CW;
            vga_bb_draw_string_2x(bx+(rbw-sw)/2, by+(bh-CH)/2, btns[i], B_TEXT, 0x00000000);
        }
    }
    else if (w->type == WIN_SYSMON) {
        vga_bb_fill_rect(cx, cy, cw, ch, B_BG_DARK);
        vga_bb_draw_string_2x(cx+10, cy+10, "Memory Usage History", B_ACCENT, 0x00000000);
        vga_bb_draw_hline(cx+10, cy+40, cw-20, B_SEPARATOR);
        int g_x = cx+20, g_y = cy+60, g_w = cw-40, g_h = ch-120;
        vga_bb_fill_rect(g_x, g_y, g_w, g_h, B_WIN_BG);
        vga_bb_draw_rect_outline(g_x, g_y, g_w, g_h, B_BORDER);
        int max_m = mem_total() / 1024; if (max_m <= 0) max_m = 131072;
        int bar_w = g_w / 60;
        for (int i=0; i<60; i++) {
            int val = w->sysmon_history[(w->sysmon_head + i) % 60];
            if (val > 0) {
                int bh = (val * g_h) / max_m; if (bh > g_h) bh = g_h;
                uint32_t bc = B_GREEN; if (bh > g_h/2) bc = B_YELLOW; if (bh > (g_h*4)/5) bc = B_RED;
                vga_bb_fill_rect(g_x + i*bar_w + 1, g_y + g_h - bh, bar_w-2, bh, bc);
            }
        }
        char mtext[60]; char tmp[16]; strcpy(mtext, "Used: "); itoa(mem_used()/1024, tmp, 10); strcat(mtext, tmp);
        strcat(mtext, " KB / "); itoa(max_m, tmp, 10); strcat(mtext, tmp); strcat(mtext, " KB");
        vga_bb_draw_string_2x(cx+20, cy+ch-40, mtext, B_TEXT, 0x00000000);
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
        case WIN_TERM: strcpy(w->title,"Console"); w->x=280;w->y=120;w->w=820;w->h=520;
            term_add_line(w,"SwanOS Terminal"); term_add_line(w,"Type 'help'"); break;
        case WIN_FILES: strcpy(w->title,"Dolphin"); w->x=320;w->y=180;w->w=820;w->h=520; w->file_sel=0; break;
        case WIN_NOTES: strcpy(w->title,"KWrite"); w->x=360;w->y=220;w->w=820;w->h=520;
            strcpy(w->note_file,"notes.txt"); fs_read("notes.txt",w->note_text,sizeof(w->note_text)-1);
            w->note_len=strlen(w->note_text); w->note_cursor=w->note_len; break;
        case WIN_ABOUT: strcpy(w->title,"About"); w->x=420;w->y=280;w->w=620;w->h=420; break;
        case WIN_AI: strcpy(w->title,"AI Chat"); w->x=240;w->y=80;w->w=840;w->h=620;
            term_add_line(w,"SwanOS AI Assistant"); term_add_line(w,"Ask me anything!"); break;
        case WIN_CALC: strcpy(w->title,"Calculator"); w->x=500;w->y=160;w->w=320;w->h=460; w->input[0]='0'; w->input[1]='\0'; w->calc_new=1; break;
        case WIN_SYSMON: strcpy(w->title,"System Monitor"); w->x=600;w->y=300;w->w=560;w->h=380; w->sysmon_head=0; memset(w->sysmon_history,0,sizeof(w->sysmon_history)); break;
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

    if (!strcmp(cc,"help")) { term_add_line(w,"Commands:"); term_add_line(w," ls cat write mkdir rm"); term_add_line(w," calc date mem whoami"); term_add_line(w," ask <question> setkey <key> aikey"); term_add_line(w," clear exit"); }
    else if (!strcmp(cc,"aikey")) { if (llm_ready()) term_add_line(w,"API key is configured."); else term_add_line(w,"No API key set. Use 'setkey <KEY>'."); }
    else if (!strcmp(cc,"setkey")) { if(!arg[0]) term_add_line(w,"Usage: setkey <KEY>"); else { llm_set_api_key(arg); term_add_line(w,"API key saved."); } }
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
    if (ctx_menu_open) {
        if (mx >= ctx_menu_x && mx < ctx_menu_x+CTX_W && my >= ctx_menu_y && my < ctx_menu_y+CTX_H) {
            int idx = (my - ctx_menu_y - 4) / CTX_ITEM_H;
            if (idx == 0) open_window(WIN_NOTES);
            else if (idx == 1) open_window(WIN_SYSMON);
            else if (idx == 2) wp_cached = 0;
        }
        ctx_menu_open = 0; return 0;
    }
    if (kickoff_open) {
        int mx_top = dock_y - KO_H - 12;
        int mx_left = kickoff_x;
        if (mx>=mx_left && mx<mx_left+KO_W && my>=mx_top+KO_HEADER && my<mx_top+KO_H) {
            int idx=(my-mx_top-KO_HEADER)/KO_ITEM_H;
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
    if (my>=dock_y && my<dock_y+PANEL_H && mx>=dock_x && mx<dock_x+64) { kickoff_open=!kickoff_open; return 0; }
    /* Panel task buttons */
    if (my>=dock_y && my<dock_y+PANEL_H) {
        int bx=dock_x + 64;
        for (int i=0;i<win_order_count;i++) { int wi=win_order[i]; if(!windows[wi].active) continue;
            int bw=(int)strlen(windows[wi].title)*CW+20; if(bw>200) bw=200;
            if (mx>=bx&&mx<bx+bw) { bring_to_front(wi); return 0; } bx+=bw+8; }
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
            /* Calc click */
            if (w->type == WIN_CALC) {
                int cx2 = w->x+4, cy2 = w->y+TITLEBAR_H+4, cw2 = w->w-8, ch2 = w->h-TITLEBAR_H-8;
                int bw = (cw2 - 50) / 4, bh = (ch2 - 90 - 40) / 5;
                for (int i=0; i<20; i++) {
                    if (i == 17) continue;
                    int bx = cx2 + 10 + (i%4)*(bw+10), by = cy2 + 80 + (i/4)*(bh+10);
                    int rbw = bw; if (i == 16) rbw = bw*2 + 10;
                    if (mx >= bx && mx < bx+rbw && my >= by && my < by+bh) {
                        const char* btns[20] = { "C", " ", " ", "/", "7", "8", "9", "*", "4", "5", "6", "-", "1", "2", "3", "+", "0", "0", ".", "=" };
                        char btn = btns[i][0];
                        if (btn >= '0' && btn <= '9') {
                            if (w->calc_new || w->input[0]=='0') { w->input[0]=btn; w->input[1]='\0'; w->calc_new=0; }
                            else { int l = strlen(w->input); if (l < 15) { w->input[l]=btn; w->input[l+1]='\0'; } }
                        }
                        else if (i == 0) { w->input[0]='0'; w->input[1]='\0'; w->calc_new=1; w->calc_v1=0; w->calc_op=0; }
                        else if (btn == '+' || btn == '-' || btn == '*' || btn == '/') {
                            int v = 0; int neg=0; char *p=w->input; if(*p=='-'){neg=1;p++;} for(; *p; p++) if(*p>='0'&&*p<='9') v=v*10+(*p-'0'); if(neg)v=-v;
                            if (w->calc_op && !w->calc_new) {
                                if (w->calc_op==1) w->calc_v1 += v; else if (w->calc_op==2) w->calc_v1 -= v;
                                else if (w->calc_op==3) w->calc_v1 *= v; else if (w->calc_op==4 && v!=0) w->calc_v1 /= v;
                                int cv=w->calc_v1; char tbuf[20];
                                if(cv<0){ tbuf[0]='-'; itoa(-cv, tbuf+1, 10); } else itoa(cv, tbuf, 10);
                                strcpy(w->input, tbuf);
                            } else w->calc_v1 = v;
                            if(btn=='+')w->calc_op=1; else if(btn=='-')w->calc_op=2; else if(btn=='*')w->calc_op=3; else w->calc_op=4;
                            w->calc_new=1;
                        }
                        else if (btn == '=') {
                            int v = 0; int neg=0; char *p=w->input; if(*p=='-'){neg=1;p++;} for(; *p; p++) if(*p>='0'&&*p<='9') v=v*10+(*p-'0'); if(neg)v=-v;
                            if (w->calc_op) {
                                if (w->calc_op==1) w->calc_v1 += v; else if (w->calc_op==2) w->calc_v1 -= v;
                                else if (w->calc_op==3) w->calc_v1 *= v; else if (w->calc_op==4 && v!=0) w->calc_v1 /= v;
                                int cv=w->calc_v1; char tbuf[20];
                                if(cv<0){ tbuf[0]='-'; itoa(-cv, tbuf+1, 10); } else itoa(cv, tbuf, 10);
                                strcpy(w->input, tbuf); w->calc_op=0; w->calc_new=1;
                            }
                        }
                        return 0;
                    }
                }
            }
            return 0;
        }
    }
    /* Desktop icons */
    for (int i=0;i<num_icons;i++) { desktop_icon_t *ic=&icons[i];
        if (mx>=ic->x&&mx<ic->x+ICON_W&&my>=ic->y&&my<ic->y+ICON_H) {
            if (ic->app_id==WIN_AI){open_window(WIN_AI);return 0;} if(ic->app_id==4) return -4;
            if (ic->app_id<=3) open_window(ic->app_id); return 0; } }
    kickoff_open=0; return 0;
}

/* ── Full desktop draw ────────────────────────────────────── */
static void draw_desktop(void) {
    draw_wallpaper(); draw_icons();
    for (int i=0;i<win_order_count;i++) draw_window(win_order[i]);
    draw_panel(); draw_kickoff(); draw_context_menu();
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

    int last_blink = 0;
    int last_minute = -1;
    uint32_t sysmon_tick = 0;

    while (1) {
        mouse_state_t ms; mouse_get_state(&ms);
        int l_click = (ms.clicked && (ms.buttons & 1));
        int r_click = (ms.clicked && (ms.buttons & 2));
        if (ms.clicked) mouse_clear_events();
        
        if (r_click) {
            /* Right click -> Context menu */
            if (!ctx_menu_open) {
                ctx_menu_open = 1; ctx_menu_x = ms.x; ctx_menu_y = ms.y;
                if (ctx_menu_x + CTX_W > SCRW) ctx_menu_x = SCRW - CTX_W;
                if (ctx_menu_y + CTX_H > SCRH) ctx_menu_y = SCRH - CTX_H;
                needs_redraw = 1;
            }
        }
        else if (l_click && ctx_menu_open) {
             /* Left click might click context menu items or dismiss it */
            handle_click(ms.x, ms.y);
            needs_redraw = 1;
        }
        else if (l_click) {
            if (!dragging) {
                int lr=handle_click(ms.x,ms.y);
                if (lr==-1) { screen_init(); screen_set_serial_mirror(1); screen_clear();
                    screen_set_color(VGA_DARK_GREY,VGA_BLACK); screen_print("\n\n   Shutting down...\n");
                    screen_delay(500); __asm__ volatile("cli; hlt"); while(1); }
                if (lr==-4) { game_snake(); wp_cached=0; }
            }
            needs_redraw=1;
        }
        if (dragging) {
            if (ms.buttons & 1) { /* Left explicitly held */
                int nx=ms.x-drag_ox, ny=ms.y-drag_oy;
                if(nx<0)nx=0; if(ny<0)ny=0;
                if(nx+windows[drag_win].w>SCRW) nx=SCRW-windows[drag_win].w;
                if(ny+windows[drag_win].h>DESK_H) ny=DESK_H-windows[drag_win].h;
                windows[drag_win].x=nx; windows[drag_win].y=ny; needs_redraw=1;
            } else dragging=0;
        }
        if (ms.moved) { needs_redraw=1; }
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
        /* Sysmon history update (every 1 second roughly = 100 ticks at 100Hz) */
        if (ticks - sysmon_tick > 100) {
            for (int i=0; i<MAX_WINDOWS; i++) {
                if (windows[i].active && windows[i].type == WIN_SYSMON) {
                    windows[i].sysmon_history[windows[i].sysmon_head] = mem_used() / 1024;
                    windows[i].sysmon_head = (windows[i].sysmon_head + 1) % 60;
                    needs_redraw = 1;
                }
            }
            sysmon_tick = ticks;
        }

        /* Cursor blink optimization */
        int cur_blink = (ticks / 30) % 2;
        if (cur_blink != last_blink) { last_blink = cur_blink; needs_redraw = 1; }

        /* Clock update optimization */
        rtc_time_t rtc; rtc_read(&rtc);
        if (rtc.minute != last_minute) { last_minute = rtc.minute; needs_redraw = 1; }

        if (needs_redraw || dragging) { draw_desktop(); needs_redraw=0; last_draw = ticks; }
        __asm__ volatile("hlt");
    }
}
