/* ============================================================
 * SwanOS — Desktop Environment (SwanDE Cygnus Style)
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
#include "network.h"
#include "ui_theme.h"

/* ── Layout ───────────────────────────────────────────────── */
#define SCRW       GFX_W
#define SCRH       GFX_H
#define PANEL_H    44
#define DESK_H     (SCRH - PANEL_H)
#define TITLEBAR_H 32
#define CW         18   /* char width 2x */
#define CH         16   /* char height 2x */

/* ── Serenity Palette — Alias old B_* names to new S_* theme ─ */
#define B_BG_DARK    S_BG_DARK
#define B_BG         S_BG
#define B_BG_ALT     S_BG_ALT
#define B_BG_LIGHT   S_BG_LIGHT
#define B_PANEL      S_PANEL
#define B_PANEL_HOVER S_PANEL_HOVER
#define B_TITLEBAR   S_TITLEBAR
#define B_TITLEBAR_F S_TITLEBAR_F
#define B_WIN_BG     S_WIN_BG
#define B_BORDER     S_BORDER
#define B_BORDER_F   S_BORDER_F
#define B_TEXT       S_TEXT
#define B_TEXT_DIM   S_TEXT_DIM
#define B_TEXT_INV   S_TEXT_INV
#define B_ACCENT     S_ACCENT
#define B_ACCENT2    S_ACCENT2
#define B_GREEN      S_GREEN
#define B_YELLOW     S_YELLOW
#define B_RED        S_RED
#define B_ORANGE     S_ORANGE
#define B_SEPARATOR  S_SEPARATOR
#define B_HOVER      S_HOVER
#define B_KICKOFF    S_KICKOFF_BG
#define B_KICKOFF_HL S_KICKOFF_HL
#define B_SHADOW     S_SHADOW

/* ── Icons ────────────────────────────────────────────────── */
#define MAX_WINDOWS 12
#define WIN_TERM 0
#define WIN_FILES 1
#define WIN_NOTES 2
#define WIN_ABOUT 3
#define WIN_AI 4
#define WIN_CALC 5
#define WIN_SYSMON 6
#define WIN_STORE 7
#define WIN_BROWSER 8
#define WIN_NETWORK 9

#define ICON_W     80
#define ICON_H     80
#define MAX_ICONS  10

typedef struct { int x, y; const char *label; int app_id; } desktop_icon_t;

static desktop_icon_t icons[MAX_ICONS] = {
    {50, 40,   "AI Chat",  WIN_AI},
    {50, 150,  "Terminal", WIN_TERM},
    {50, 260,  "Files",    WIN_FILES},
    {50, 370,  "Notes",    WIN_NOTES},
    {50, 480,  "About",    WIN_ABOUT},
    {50, 590,  "Calc",     WIN_CALC},
    {50, 700,  "SysMon",   WIN_SYSMON},
    {170, 40,  "Store",    WIN_STORE},
    {170, 150, "Browser",  WIN_BROWSER},
    {170, 260, "Network",  WIN_NETWORK},
};
static int num_icons = 10;

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
    int store_sel;
    int store_downloaded[6];
    int browser_tab;
} window_t;

static window_t windows[MAX_WINDOWS];
static int win_count = 0, win_focus = -1;
static int win_order[MAX_WINDOWS], win_order_count = 0;
static int dragging = 0, drag_win = -1, drag_ox, drag_oy;

/* ── Kickoff Menu ─────────────────────────────────────────── */
static int kickoff_open = 0;
#define KO_W     280
#define KO_ITEMS  12
#define KO_ITEM_H 36
#define KO_HEADER  64
#define KO_H      (KO_HEADER + KO_ITEMS * KO_ITEM_H + 8)

static const char *ko_labels[KO_ITEMS] = {
    "AI Chat", "Terminal", "Files", "Notes", "Calc", "SysMonitor", "About",
    "Store", "Browser", "Network",
    "--------", "Shut Down"
};
static int ko_ids[KO_ITEMS] = {WIN_AI,WIN_TERM,WIN_FILES,WIN_NOTES,WIN_CALC,WIN_SYSMON,WIN_ABOUT,WIN_STORE,WIN_BROWSER,WIN_NETWORK,-1,-2};

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
    const int h = (DESK_H < GFX_H) ? DESK_H : GFX_H;
    /* C++ aurora wallpaper — soothing navy/teal/purple gradient with shimmer */
    ui_render_aurora_wallpaper(wp_buf, GFX_W, h);
    wp_cached = 1;
}


static void draw_wallpaper(void) {
    if (!wp_cached) render_wallpaper();
    uint32_t *bb = vga_backbuffer();
    const int h = (DESK_H < GFX_H) ? DESK_H : GFX_H;
    /* Fast blit: copy entire rows at once */
    for (int y = 0; y < h; y++) {
        uint32_t *dst = &bb[y * GFX_W];
        const uint32_t *src = &wp_buf[y * GFX_W];
        for (int x = 0; x < GFX_W; x++)
            dst[x] = src[x];
    }
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
        case WIN_STORE: /* Shopping bag with download arrow */
            vga_bb_fill_rounded_rect(cx-1, cy+1, 28, 30, 4, B_SHADOW);
            vga_bb_fill_rounded_rect(cx-2, cy, 28, 30, 4, B_GREEN);
            vga_bb_fill_rounded_rect(cx, cy+2, 24, 26, 3, B_BG_DARK);
            /* Bag handle */
            vga_bb_draw_hline(cx+6, cy-2, 12, B_GREEN);
            vga_bb_draw_vline(cx+6, cy-2, 4, B_GREEN);
            vga_bb_draw_vline(cx+17, cy-2, 4, B_GREEN);
            /* Down arrow */
            vga_bb_fill_rect(cx+9, cy+6, 6, 10, B_GREEN);
            vga_bb_fill_rect(cx+6, cy+14, 12, 2, B_GREEN);
            vga_bb_fill_rect(cx+8, cy+16, 8, 2, B_GREEN);
            vga_bb_fill_rect(cx+10, cy+18, 4, 2, B_GREEN);
            /* Shield badge */
            vga_bb_fill_circle(cx+22, cy+24, 5, B_ACCENT);
            vga_bb_draw_string_2x(cx+18, cy+19, "S", B_BG_DARK, 0x00000000);
            break;
        case WIN_BROWSER: /* Globe icon */
            vga_bb_fill_circle(cx+13, cy+15, 14, B_SHADOW);
            vga_bb_fill_circle(cx+12, cy+14, 14, B_ACCENT);
            vga_bb_fill_circle(cx+12, cy+14, 12, B_BG_DARK);
            /* Horizontal lines (latitude) */
            vga_bb_draw_hline(cx+2, cy+9, 20, B_ACCENT);
            vga_bb_draw_hline(cx+2, cy+14, 20, B_ACCENT);
            vga_bb_draw_hline(cx+2, cy+19, 20, B_ACCENT);
            /* Vertical line (meridian) */
            vga_bb_draw_vline(cx+12, cy+2, 24, B_ACCENT);
            /* Ellipse (longitude) */
            vga_bb_draw_vline(cx+7, cy+5, 18, B_ACCENT2);
            vga_bb_draw_vline(cx+17, cy+5, 18, B_ACCENT2);
            /* Orange compass dot */
            vga_bb_fill_circle(cx+20, cy+4, 4, B_ORANGE);
            vga_bb_fill_circle(cx+20, cy+4, 2, B_BG_DARK);
            break;
        case WIN_NETWORK: /* Network / Ethernet icon */
            vga_bb_fill_rounded_rect(cx-1, cy+1, 32, 28, 6, B_SHADOW);
            vga_bb_fill_rounded_rect(cx-2, cy, 32, 28, 6, B_ACCENT);
            vga_bb_fill_rounded_rect(cx, cy+2, 28, 24, 5, B_BG_DARK);
            /* WiFi arcs */
            vga_bb_draw_circle(cx+14, cy+22, 14, B_GREEN);
            vga_bb_draw_circle(cx+14, cy+22, 10, B_GREEN);
            vga_bb_draw_circle(cx+14, cy+22, 6, B_GREEN);
            /* Center dot */
            vga_bb_fill_circle(cx+14, cy+22, 2, B_GREEN);
            /* Mask bottom half */
            vga_bb_fill_rect(cx, cy+23, 28, 4, B_BG_DARK);
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

/* ── Panel (Floating Dock) — Enhanced ──────────── */
static int quick_settings_open = 0;  /* Quick settings popup state */
static int notification_count = 3;   /* Simulated notification count */

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
    
    dock_w = 56 + 16 + wins_w + 420 /* wider tray for date */;
    if (dock_w > SCRW - 40) dock_w = SCRW - 40;
    dock_x = (SCRW - dock_w) / 2;
    dock_y = SCRH - PANEL_H - 12;

    /* Glassmorphism dock via C++ theme engine */
    ui_soft_shadow(dock_x, dock_y, dock_w, PANEL_H, 14, 3);
    ui_glass_panel(dock_x, dock_y, dock_w, PANEL_H, 12, B_PANEL, S_GLASS_BORDER);

    /* ── App Launcher Button ── */
    mouse_state_t ms; mouse_get_state(&ms);
    const int msx = ms.x, msy = ms.y;
    int lhover = (msx >= dock_x+6 && msx < dock_x+56 && msy >= dock_y);
    kickoff_x = dock_x + 8;
    
    /* Gradient launcher pill */
    if (lhover || kickoff_open) {
        vga_bb_fill_rounded_rect(dock_x+6, dock_y+4, 50, PANEL_H-8, 10, S_ACCENT2);
        vga_bb_fill_rounded_rect(dock_x+7, dock_y+5, 48, PANEL_H-10, 9, B_HOVER);
    }
    /* Swan icon — circle with wing shape */
    vga_bb_fill_circle(dock_x+31, dock_y+PANEL_H/2, 13, S_ACCENT);
    vga_bb_fill_circle(dock_x+31, dock_y+PANEL_H/2, 11, S_BG_DEEP);
    /* S letter for Swan */
    vga_bb_draw_string_2x(dock_x+24, dock_y+PANEL_H/2-7, "S", S_ACCENT, 0x00000000);

    /* ── Task Buttons ── */
    int bx = dock_x + 66;
    vga_bb_draw_vline(bx-6, dock_y+10, PANEL_H-20, B_SEPARATOR);

    for (int i = 0; i < win_order_count && bx < dock_x + dock_w - 300; i++) {
        int wi = win_order[i];
        if (!windows[wi].active) continue;
        int focused = (wi == win_focus);
        int bw = (int)strlen(windows[wi].title) * CW + 24;
        if (bw > 200) bw = 200;
        int thover = (msx >= bx && msx < bx+bw && msy >= dock_y+4 && msy < dock_y+PANEL_H-4);
        if (focused) {
            vga_bb_fill_rounded_rect(bx, dock_y+4, bw, PANEL_H-8, 8, S_BG_HOVER);
            /* Accent pill indicator — teal */
            vga_bb_fill_rounded_rect(bx+bw/2-10, dock_y+PANEL_H-5, 20, 3, 1, S_ACCENT);
            /* Colored dot for window type */
            uint32_t dot_c = S_ACCENT;
            if (windows[wi].type == WIN_AI) dot_c = S_PURPLE;
            else if (windows[wi].type == WIN_TERM) dot_c = S_GREEN;
            else if (windows[wi].type == WIN_FILES) dot_c = S_YELLOW;
            else if (windows[wi].type == WIN_NETWORK) dot_c = S_BLUE;
            else if (windows[wi].type == WIN_BROWSER) dot_c = S_ORANGE;
            vga_bb_fill_circle(bx+10, dock_y+PANEL_H/2, 4, dot_c);
        } else if (thover) {
            vga_bb_fill_rounded_rect(bx, dock_y+4, bw, PANEL_H-8, 8, S_BG_ALT);
        }
        vga_bb_draw_string_2x(bx+19, dock_y+15, windows[wi].title, 0x60000000, 0x00000000);
        vga_bb_draw_string_2x(bx+18, dock_y+14, windows[wi].title, focused ? B_TEXT : B_TEXT_DIM, 0x00000000);
        bx += bw + 6;
    }

    /* ── System Tray ── */
    int tx = dock_x + dock_w - 380;
    vga_bb_draw_vline(tx, dock_y+10, PANEL_H-20, B_SEPARATOR);

    /* Uptime indicator */
    uint32_t up_secs = timer_get_seconds();
    char ut[20]; char tmp[8];
    itoa(up_secs / 60, tmp, 10);
    strcpy(ut, tmp); strcat(ut, "m");
    vga_bb_draw_string(tx+8, dock_y+PANEL_H/2-3, ut, B_TEXT_DIM, 0x00000000);

    /* Memory */
    int mem_ix = tx + 48;
    itoa(mem_used()/1024, tmp, 10);
    char mb[16]; strcpy(mb, tmp); strcat(mb, "K");
    vga_bb_draw_string(mem_ix, dock_y+PANEL_H/2-3, mb, B_TEXT_DIM, 0x00000000);

    /* Dot separator */
    vga_bb_fill_circle(mem_ix+40, dock_y+PANEL_H/2, 2, B_SEPARATOR);

    /* ── Network icon ── */
    int net_ix = mem_ix + 52;
    int net_hover = (msx >= net_ix-6 && msx < net_ix+22 && msy >= dock_y+4 && msy < dock_y+PANEL_H-4);
    if (net_hover) vga_bb_fill_rounded_rect(net_ix-6, dock_y+6, 28, PANEL_H-12, 6, B_HOVER);
    {
        net_status_t *ns = net_get_status();
        uint32_t nc = ns->connected ? S_GREEN : S_RED;
        vga_bb_draw_circle(net_ix+8, dock_y+PANEL_H/2+6, 10, nc);
        vga_bb_draw_circle(net_ix+8, dock_y+PANEL_H/2+6, 7, nc);
        vga_bb_draw_circle(net_ix+8, dock_y+PANEL_H/2+6, 4, nc);
        vga_bb_fill_circle(net_ix+8, dock_y+PANEL_H/2+6, 2, nc);
        vga_bb_fill_rect(net_ix-2, dock_y+PANEL_H/2+7, 20, 10, B_PANEL);
        /* Tooltip on hover */
        if (net_hover) {
            const char *tip = ns->connected ? "Connected" : "Disconnected";
            int tw = (int)strlen(tip) * CW;
            vga_bb_fill_rounded_rect(net_ix-tw/2+8, dock_y-24, tw+12, 20, 6, S_BG_LIGHT);
            vga_bb_draw_string_2x(net_ix-tw/2+14, dock_y-22, tip, B_TEXT, 0x00000000);
        }
    }

    /* ── Volume icon ── */
    int vol_ix = net_ix + 34;
    int vol_hover = (msx >= vol_ix-4 && msx < vol_ix+22 && msy >= dock_y+4 && msy < dock_y+PANEL_H-4);
    if (vol_hover) vga_bb_fill_rounded_rect(vol_ix-4, dock_y+6, 26, PANEL_H-12, 6, B_HOVER);
    vga_bb_fill_rect(vol_ix, dock_y+PANEL_H/2-4, 5, 8, S_TEXT);
    vga_bb_fill_rect(vol_ix+5, dock_y+PANEL_H/2-6, 4, 12, S_TEXT);
    vga_bb_draw_circle(vol_ix+11, dock_y+PANEL_H/2, 4, S_TEXT_DIM);
    vga_bb_draw_circle(vol_ix+11, dock_y+PANEL_H/2, 7, S_TEXT_DIM);
    if (vol_hover) {
        vga_bb_fill_rounded_rect(vol_ix-10, dock_y-24, 50, 20, 6, S_BG_LIGHT);
        vga_bb_draw_string_2x(vol_ix-4, dock_y-22, "75%", B_TEXT, 0x00000000);
    }

    /* ── Battery icon ── */
    int bat_ix = vol_ix + 32;
    int bat_hover = (msx >= bat_ix-4 && msx < bat_ix+28 && msy >= dock_y+4 && msy < dock_y+PANEL_H-4);
    if (bat_hover) vga_bb_fill_rounded_rect(bat_ix-4, dock_y+6, 32, PANEL_H-12, 6, B_HOVER);
    vga_bb_draw_rect_outline(bat_ix, dock_y+PANEL_H/2-6, 20, 12, S_TEXT);
    vga_bb_fill_rect(bat_ix+20, dock_y+PANEL_H/2-3, 3, 6, S_TEXT);
    vga_bb_fill_rect(bat_ix+2, dock_y+PANEL_H/2-4, 16, 8, S_GREEN);
    /* Lightning bolt for charging */
    vga_bb_draw_vline(bat_ix+10, dock_y+PANEL_H/2-4, 4, S_BG_DEEP);
    vga_bb_draw_vline(bat_ix+9, dock_y+PANEL_H/2, 4, S_BG_DEEP);
    if (bat_hover) {
        vga_bb_fill_rounded_rect(bat_ix-16, dock_y-24, 72, 20, 6, S_BG_LIGHT);
        vga_bb_draw_string_2x(bat_ix-10, dock_y-22, "AC Power", B_TEXT, 0x00000000);
    }

    /* ── Notification bell with badge ── */
    int bell_ix = bat_ix + 38;
    int bell_hover = (msx >= bell_ix-4 && msx < bell_ix+22 && msy >= dock_y+4 && msy < dock_y+PANEL_H-4);
    if (bell_hover) vga_bb_fill_rounded_rect(bell_ix-4, dock_y+6, 26, PANEL_H-12, 6, B_HOVER);
    /* Bell shape */
    vga_bb_fill_circle(bell_ix+8, dock_y+PANEL_H/2-3, 7, S_YELLOW);
    vga_bb_fill_rect(bell_ix+1, dock_y+PANEL_H/2, 14, 6, S_YELLOW);
    vga_bb_fill_rect(bell_ix-1, dock_y+PANEL_H/2+6, 18, 2, S_YELLOW);
    vga_bb_fill_circle(bell_ix+8, dock_y+PANEL_H/2+10, 2, S_YELLOW);
    /* Notification count badge */
    if (notification_count > 0) {
        vga_bb_fill_circle(bell_ix+16, dock_y+PANEL_H/2-8, 7, S_RED);
        char nc_str[4]; itoa(notification_count, nc_str, 10);
        vga_bb_draw_string(bell_ix+13, dock_y+PANEL_H/2-12, nc_str, 0xFFFFFFFF, 0x00000000);
    }
    if (bell_hover) {
        char ntip[24]; strcpy(ntip, ""); itoa(notification_count, tmp, 10);
        strcat(ntip, tmp); strcat(ntip, " notifications");
        int tw = (int)strlen(ntip) * CW;
        vga_bb_fill_rounded_rect(bell_ix-tw/2, dock_y-24, tw+12, 20, 6, S_BG_LIGHT);
        vga_bb_draw_string_2x(bell_ix-tw/2+6, dock_y-22, ntip, B_TEXT, 0x00000000);
    }

    /* ── Clock + Date section ── */
    vga_bb_draw_vline(bell_ix+30, dock_y+10, PANEL_H-20, B_SEPARATOR);
    rtc_time_t rtc; rtc_read(&rtc);
    /* Time — larger */
    char clk[10]; rtc_format_time(&rtc, clk);
    clk[5] = '\0'; /* Show only HH:MM */
    int clk_x = dock_x + dock_w - 110;
    vga_bb_draw_string_2x(clk_x+1, dock_y+6, clk, 0x60000000, 0x00000000);
    vga_bb_draw_string_2x(clk_x, dock_y+5, clk, S_TEXT, 0x00000000);
    /* Date line — smaller */
    char wday[4]; rtc_format_weekday(&rtc, wday);
    char date[16]; rtc_format_date(&rtc, date);
    /* Show "Mon 17 Mar" style */
    char dateline[20];
    strcpy(dateline, wday); strcat(dateline, " ");
    char dm[4]; itoa(rtc.day, dm, 10); strcat(dateline, dm);
    /* Month abbreviation */
    const char *months[] = {"","Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    int mi = rtc.month; if (mi < 1) mi = 1; if (mi > 12) mi = 12;
    strcat(dateline, " "); strcat(dateline, months[mi]);
    vga_bb_draw_string(clk_x, dock_y+24, dateline, S_TEXT_DIM, 0x00000000);

    /* ── Quick Settings Popup ── */
    if (quick_settings_open) {
        int qx = dock_x + dock_w - 260, qy = dock_y - 240;
        int qw = 240, qh = 230;
        ui_soft_shadow(qx, qy, qw, qh, 10, 3);
        ui_glass_panel(qx, qy, qw, qh, 10, S_BG, S_GLASS_BORDER);
        /* Header */
        vga_bb_draw_string_2x(qx+14, qy+10, "Quick Settings", S_ACCENT, 0x00000000);
        vga_bb_draw_hline(qx+10, qy+30, qw-20, S_SEPARATOR);
        /* Volume slider */
        vga_bb_draw_string_2x(qx+14, qy+40, "Volume", S_TEXT, 0x00000000);
        ui_progress_bar(qx+14, qy+60, qw-28, 8, 75, 100, S_ACCENT, S_BG_ALT);
        vga_bb_draw_string(qx+qw-36, qy+56, "75%", S_TEXT_DIM, 0x00000000);
        /* Brightness */
        vga_bb_draw_string_2x(qx+14, qy+80, "Brightness", S_TEXT, 0x00000000);
        ui_progress_bar(qx+14, qy+100, qw-28, 8, 90, 100, S_YELLOW, S_BG_ALT);
        vga_bb_draw_string(qx+qw-36, qy+96, "90%", S_TEXT_DIM, 0x00000000);
        /* DND toggle */
        vga_bb_draw_string_2x(qx+14, qy+124, "Do Not Disturb", S_TEXT, 0x00000000);
        vga_bb_fill_rounded_rect(qx+qw-56, qy+124, 40, 18, 9, S_BG_LIGHT);
        vga_bb_fill_circle(qx+qw-56+10, qy+133, 7, S_TEXT_DIM); /* Off state */
        /* Separator */
        vga_bb_draw_hline(qx+10, qy+152, qw-20, S_SEPARATOR);
        /* Network status */
        {
            net_status_t *ns = net_get_status();
            vga_bb_fill_circle(qx+22, qy+168, 5, ns->connected ? S_GREEN : S_RED);
            vga_bb_draw_string_2x(qx+36, qy+162, ns->connected ? "Connected" : "No Connection", S_TEXT, 0x00000000);
            if (ns->connected)
                vga_bb_draw_string(qx+36, qy+180, ns->ip_addr, S_TEXT_DIM, 0x00000000);
        }
        /* Uptime */
        uint32_t ut_s = timer_get_seconds();
        char upstr[32]; strcpy(upstr, "Uptime: ");
        char ubuf[8]; itoa(ut_s / 3600, ubuf, 10); strcat(upstr, ubuf); strcat(upstr, "h ");
        itoa((ut_s % 3600) / 60, ubuf, 10); strcat(upstr, ubuf); strcat(upstr, "m");
        vga_bb_draw_string(qx+14, qy+200, upstr, S_TEXT_DIM, 0x00000000);
    }
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

    /* Multi-layer soft shadow via C++ theme */
    ui_window_shadow(w->x, w->y, w->w, w->h);

    /* Window body */
    vga_bb_fill_rounded_rect(w->x, w->y, w->w, w->h, 10, B_WIN_BG);

    /* Border — accent on focus, subtle otherwise */
    uint32_t bc = focused ? B_BORDER_F : B_BORDER;
    vga_bb_draw_rect_outline(w->x, w->y, w->w, w->h, bc);
    /* Focus glow: top + bottom accent lines */
    if (focused) {
        vga_bb_draw_hline(w->x+1, w->y, w->w-2, B_BORDER_F);
        vga_bb_draw_hline(w->x+1, w->y+1, w->w-2, S_ACCENT_GLOW);
    }
    /* Inner top highlight for depth */
    vga_bb_draw_hline(w->x+10, w->y+1, w->w-20, S_GLASS_BORDER);

    /* Title bar */
    uint32_t tb = focused ? B_TITLEBAR_F : B_TITLEBAR;
    vga_bb_fill_rect(w->x+1, w->y+1, w->w-2, TITLEBAR_H, tb);
    /* Title bar bottom separator */
    vga_bb_draw_hline(w->x+1, w->y+TITLEBAR_H, w->w-2, B_SEPARATOR);

    /* Title text (centered in title bar) */
    int tw = (int)strlen(w->title) * CW;
    int ttx = w->x + (w->w - tw) / 2;
    /* Single shadow + text (reduced from 3 draws to 2) */
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
        int mc = (cw - 12) / CW; /* Hoist out of loop — constant per window */
        int is_ai = (w->type == WIN_AI);
        for (int i = sl; i < w->line_count && i < sl + max_l; i++) {
            uint32_t lc = (is_ai && w->lines[i][0] == '>') ? B_GREEN :
                          (is_ai ? B_ACCENT : B_GREEN);
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
        static const char* const btns[20] = { "C", " ", " ", "/", "7", "8", "9", "*", "4", "5", "6", "-", "1", "2", "3", "+", "0", "0", ".", "=" };
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
    else if (w->type == WIN_STORE) {
        vga_bb_fill_rect(cx, cy, cw, ch, B_BG_DARK);
        /* ── Security header bar ── */
        vga_bb_fill_rect(cx, cy, cw, 36, B_BG_ALT);
        /* Shield icon */
        vga_bb_fill_circle(cx+22, cy+18, 10, B_GREEN);
        vga_bb_fill_circle(cx+22, cy+18, 8, B_BG_DARK);
        vga_bb_draw_string_2x(cx+16, cy+12, "S", B_GREEN, 0x00000000);
        /* HTTPS lock icon */
        vga_bb_fill_rounded_rect(cx+42, cy+10, 14, 12, 2, B_GREEN);
        vga_bb_fill_rect(cx+46, cy+5, 6, 7, 0x00000000);
        vga_bb_draw_rect_outline(cx+45, cy+4, 8, 8, B_GREEN);
        vga_bb_draw_string_2x(cx+62, cy+10, "SwanOS Store", B_ACCENT, 0x00000000);
        /* Verified badge */
        vga_bb_fill_rounded_rect(cx+cw-140, cy+8, 120, 20, 6, B_GREEN);
        vga_bb_draw_string_2x(cx+cw-132, cy+10, "Verified", B_BG_DARK, 0x00000000);
        vga_bb_draw_hline(cx, cy+36, cw, B_SEPARATOR);
        /* ── Column headers ── */
        int hy = cy + 40;
        vga_bb_fill_rect(cx, hy, cw, CH+4, B_BG_LIGHT);
        vga_bb_draw_string_2x(cx+8, hy+2, "App Name", B_TEXT_DIM, 0x00000000);
        vga_bb_draw_string_2x(cx+220, hy+2, "Type", B_TEXT_DIM, 0x00000000);
        vga_bb_draw_string_2x(cx+310, hy+2, "Platform", B_TEXT_DIM, 0x00000000);
        vga_bb_draw_string_2x(cx+cw-120, hy+2, "Status", B_TEXT_DIM, 0x00000000);
        vga_bb_draw_hline(cx, hy+CH+4, cw, B_SEPARATOR);
        /* ── App catalog ── */
        static const char *store_names[5] = { "Firefox", "Brave", "VLC Player", "LibreOffice", "7-Zip" };
        static const char *store_cats[5]  = { "Browser", "Browser", "Media", "Office", "Utility" };
        static const char *store_hash[5]  = { "a3f8..c91e", "b7d2..f40a", "c1e5..d82b", "d4a9..e73c", "e8b3..a65d" };
        static const char *store_size[5]  = { "95 MB", "112 MB", "42 MB", "310 MB", "1.5 MB" };
        static const int   store_dmg[5]   = { 1, 1, 1, 1, 0 }; /* has dmg? */
        int ey = hy + CH + 8;
        mouse_state_t sms; mouse_get_state(&sms);
        for (int si = 0; si < 5 && ey + 58 < cy + ch - 36; si++) {
            int sel = (si == w->store_sel);
            int hover = (sms.x >= cx+4 && sms.x < cx+cw-4 && sms.y >= ey && sms.y < ey+54);
            if (sel || hover)
                vga_bb_fill_rounded_rect(cx+2, ey, cw-4, 54, 6, sel ? 0xFF1A3A50 : B_BG_ALT);
            /* App name */
            vga_bb_draw_string_2x(cx+12, ey+4, store_names[si], B_TEXT, 0x00000000);
            /* SHA-256 hash */
            vga_bb_draw_string_2x(cx+12, ey+22, "SHA256:", B_TEXT_DIM, 0x00000000);
            vga_bb_draw_string_2x(cx+12+7*CW, ey+22, store_hash[si], B_YELLOW, 0x00000000);
            /* Category */
            vga_bb_draw_string_2x(cx+220, ey+4, store_cats[si], B_ACCENT2, 0x00000000);
            /* Size */
            vga_bb_draw_string_2x(cx+220, ey+22, store_size[si], B_TEXT_DIM, 0x00000000);
            /* Platform badges */
            vga_bb_fill_rounded_rect(cx+310, ey+4, 44, 16, 4, B_ACCENT2);
            vga_bb_draw_string_2x(cx+314, ey+4, ".exe", B_BG_DARK, 0x00000000);
            if (store_dmg[si]) {
                vga_bb_fill_rounded_rect(cx+360, ey+4, 44, 16, 4, B_ORANGE);
                vga_bb_draw_string_2x(cx+364, ey+4, ".dmg", B_BG_DARK, 0x00000000);
            }
            /* Download / Installed button */
            int downloaded = w->store_downloaded[si];
            if (downloaded) {
                vga_bb_fill_rounded_rect(cx+cw-130, ey+8, 110, 24, 6, B_GREEN);
                vga_bb_draw_string_2x(cx+cw-120, ey+12, "Installed", B_BG_DARK, 0x00000000);
            } else {
                int dbtn_hover = (sms.x >= cx+cw-130 && sms.x < cx+cw-20 && sms.y >= ey+8 && sms.y < ey+32);
                vga_bb_fill_rounded_rect(cx+cw-130, ey+8, 110, 24, 6, dbtn_hover ? B_ACCENT : B_ACCENT2);
                vga_bb_draw_string_2x(cx+cw-120, ey+12, "Download", B_TEXT, 0x00000000);
            }
            /* Verified check */
            vga_bb_fill_circle(cx+cw-150, ey+20, 6, B_GREEN);
            vga_bb_draw_string_2x(cx+cw-155, ey+14, "v", B_BG_DARK, 0x00000000);
            vga_bb_draw_hline(cx+8, ey+54, cw-16, B_SEPARATOR);
            ey += 58;
        }
        /* ── Bottom status bar ── */
        vga_bb_fill_rect(cx, cy+ch-32, cw, 32, B_BG_ALT);
        vga_bb_draw_hline(cx, cy+ch-32, cw, B_SEPARATOR);
        vga_bb_draw_string_2x(cx+8, cy+ch-24, "5 apps | All verified | Secure", B_TEXT_DIM, 0x00000000);
        /* Lock icon in status */
        vga_bb_fill_circle(cx+cw-30, cy+ch-18, 6, B_GREEN);
    }
    else if (w->type == WIN_BROWSER) {
        vga_bb_fill_rect(cx, cy, cw, ch, B_BG_DARK);
        /* ── Tab bar ── */
        vga_bb_fill_rect(cx, cy, cw, 32, B_BG_ALT);
        /* Firefox tab */
        int t0_hover = (w->browser_tab == 0);
        vga_bb_fill_rounded_rect(cx+4, cy+2, 120, 28, 6, t0_hover ? B_BG_DARK : B_BG_LIGHT);
        /* Firefox icon: orange circle */
        vga_bb_fill_circle(cx+20, cy+16, 8, B_ORANGE);
        vga_bb_fill_circle(cx+20, cy+16, 5, B_BG_DARK);
        vga_bb_fill_rect(cx+20, cy+11, 6, 5, B_ORANGE); /* flame tail */
        vga_bb_draw_string_2x(cx+34, cy+8, "Firefox", t0_hover ? B_TEXT : B_TEXT_DIM, 0x00000000);
        if (t0_hover) vga_bb_fill_rect(cx+4, cy+28, 120, 2, B_ORANGE);
        /* Brave tab */
        int t1_hover = (w->browser_tab == 1);
        vga_bb_fill_rounded_rect(cx+130, cy+2, 100, 28, 6, t1_hover ? B_BG_DARK : B_BG_LIGHT);
        /* Brave icon: lion shield */
        vga_bb_fill_rounded_rect(cx+142, cy+8, 12, 16, 3, B_RED);
        vga_bb_fill_rounded_rect(cx+144, cy+10, 8, 12, 2, B_ORANGE);
        vga_bb_draw_string_2x(cx+160, cy+8, "Brave", t1_hover ? B_TEXT : B_TEXT_DIM, 0x00000000);
        if (t1_hover) vga_bb_fill_rect(cx+130, cy+28, 100, 2, B_RED);
        vga_bb_draw_hline(cx, cy+30, cw, B_SEPARATOR);
        /* ── URL bar ── */
        vga_bb_fill_rect(cx, cy+32, cw, 28, B_BG_LIGHT);
        /* Nav buttons */
        vga_bb_fill_circle(cx+16, cy+46, 8, B_HOVER);
        vga_bb_draw_string_2x(cx+10, cy+40, "<", B_TEXT_DIM, 0x00000000);
        vga_bb_fill_circle(cx+36, cy+46, 8, B_HOVER);
        vga_bb_draw_string_2x(cx+30, cy+40, ">", B_TEXT_DIM, 0x00000000);
        vga_bb_fill_circle(cx+56, cy+46, 8, B_HOVER);
        vga_bb_draw_string_2x(cx+50, cy+40, "R", B_TEXT_DIM, 0x00000000);
        /* HTTPS lock + URL */
        vga_bb_fill_rounded_rect(cx+74, cy+36, cw-90, 20, 6, B_BG_DARK);
        vga_bb_fill_circle(cx+88, cy+46, 5, B_GREEN);
        vga_bb_draw_string_2x(cx+82, cy+40, "L", B_BG_DARK, 0x00000000);
        const char *url = (w->browser_tab == 0) ? "https://www.mozilla.org" : "https://brave.com";
        vga_bb_draw_string_2x(cx+100, cy+40, url, B_TEXT, 0x00000000);
        vga_bb_draw_hline(cx, cy+60, cw, B_SEPARATOR);
        /* ── Content area ── */
        int by = cy + 64;
        int bch = ch - 64;
        if (w->browser_tab == 0) {
            /* Firefox landing */
            vga_bb_fill_rect(cx, by, cw, bch, 0xFF1C1B22); /* FF dark bg */
            /* Large Firefox logo */
            int fx = cx + cw/2 - 20, fy = by + 40;
            vga_bb_fill_circle(fx, fy, 30, B_ORANGE);
            vga_bb_fill_circle(fx, fy, 22, 0xFF1C1B22);
            vga_bb_fill_rect(fx, fy-28, 14, 14, B_ORANGE);
            vga_bb_fill_circle(fx+4, fy, 18, B_ORANGE);
            vga_bb_fill_circle(fx+4, fy, 14, 0xFF1C1B22);
            vga_bb_draw_string_2x(cx+cw/2-8*CW, by+100, "Mozilla Firefox", B_ORANGE, 0x00000000);
            vga_bb_draw_string_2x(cx+cw/2-6*CW, by+120, "Version 130.0", B_TEXT_DIM, 0x00000000);
            vga_bb_draw_string_2x(cx+cw/2-10*CW, by+150, "Fast. Private. Secure.", B_TEXT, 0x00000000);
            /* Feature pills */
            vga_bb_fill_rounded_rect(cx+40, by+180, 100, 24, 8, B_ORANGE);
            vga_bb_draw_string_2x(cx+48, by+184, "Privacy", 0xFF1C1B22, 0x00000000);
            vga_bb_fill_rounded_rect(cx+160, by+180, 120, 24, 8, B_ACCENT2);
            vga_bb_draw_string_2x(cx+168, by+184, "Tracking", 0xFF1C1B22, 0x00000000);
            vga_bb_fill_rounded_rect(cx+300, by+180, 100, 24, 8, B_GREEN);
            vga_bb_draw_string_2x(cx+308, by+184, "Secure", 0xFF1C1B22, 0x00000000);
            /* Platform support */
            vga_bb_draw_string_2x(cx+40, by+220, "Platforms:", B_TEXT_DIM, 0x00000000);
            vga_bb_fill_rounded_rect(cx+40+10*CW, by+218, 44, 18, 4, B_ACCENT2);
            vga_bb_draw_string_2x(cx+44+10*CW, by+220, ".exe", B_BG_DARK, 0x00000000);
            vga_bb_fill_rounded_rect(cx+92+10*CW, by+218, 44, 18, 4, B_ORANGE);
            vga_bb_draw_string_2x(cx+96+10*CW, by+220, ".dmg", B_BG_DARK, 0x00000000);
        } else {
            /* Brave landing */
            vga_bb_fill_rect(cx, by, cw, bch, 0xFF12112A); /* Brave dark bg */
            /* Large Brave logo */
            int bx2 = cx + cw/2 - 16, by2 = by + 30;
            vga_bb_fill_rounded_rect(bx2-8, by2, 40, 50, 8, B_RED);
            vga_bb_fill_rounded_rect(bx2-4, by2+4, 32, 42, 6, B_ORANGE);
            vga_bb_fill_rounded_rect(bx2, by2+10, 24, 30, 4, 0xFF12112A);
            vga_bb_fill_rounded_rect(bx2+4, by2+14, 16, 22, 3, B_ORANGE);
            vga_bb_draw_string_2x(cx+cw/2-7*CW, by+100, "Brave Browser", B_ORANGE, 0x00000000);
            vga_bb_draw_string_2x(cx+cw/2-5*CW, by+120, "Version 1.73", B_TEXT_DIM, 0x00000000);
            vga_bb_draw_string_2x(cx+cw/2-12*CW, by+150, "Browse privately. Search privately.", B_TEXT, 0x00000000);
            /* Feature pills */
            vga_bb_fill_rounded_rect(cx+40, by+180, 100, 24, 8, B_RED);
            vga_bb_draw_string_2x(cx+48, by+184, "Ad Block", 0xFFFFFFFF, 0x00000000);
            vga_bb_fill_rounded_rect(cx+160, by+180, 100, 24, 8, B_ORANGE);
            vga_bb_draw_string_2x(cx+168, by+184, "Shields", 0xFFFFFFFF, 0x00000000);
            vga_bb_fill_rounded_rect(cx+280, by+180, 100, 24, 8, B_GREEN);
            vga_bb_draw_string_2x(cx+288, by+184, "Crypto", 0xFF1C1B22, 0x00000000);
            /* Platform support */
            vga_bb_draw_string_2x(cx+40, by+220, "Platforms:", B_TEXT_DIM, 0x00000000);
            vga_bb_fill_rounded_rect(cx+40+10*CW, by+218, 44, 18, 4, B_ACCENT2);
            vga_bb_draw_string_2x(cx+44+10*CW, by+220, ".exe", B_BG_DARK, 0x00000000);
            vga_bb_fill_rounded_rect(cx+92+10*CW, by+218, 44, 18, 4, B_ORANGE);
            vga_bb_draw_string_2x(cx+96+10*CW, by+220, ".dmg", B_BG_DARK, 0x00000000);
        }
    }
    else if (w->type == WIN_NETWORK) {
        vga_bb_fill_rect(cx, cy, cw, ch, B_BG_DARK);
        net_status_t *ns = net_get_status();
        /* ── Header bar ── */
        vga_bb_fill_rect(cx, cy, cw, 40, B_BG_ALT);
        /* WiFi icon */
        vga_bb_draw_circle(cx+24, cy+28, 12, B_ACCENT);
        vga_bb_draw_circle(cx+24, cy+28, 8, B_ACCENT);
        vga_bb_fill_circle(cx+24, cy+28, 3, B_ACCENT);
        vga_bb_fill_rect(cx+12, cy+29, 24, 12, B_BG_ALT); /* mask bottom */
        vga_bb_draw_string_2x(cx+48, cy+12, "Network Settings", B_ACCENT, 0x00000000);
        vga_bb_draw_hline(cx, cy+40, cw, B_SEPARATOR);

        int ny = cy + 52;
        /* ── Connection Status Card ── */
        vga_bb_fill_rounded_rect(cx+16, ny, cw-32, 80, 8, B_BG_ALT);
        vga_bb_draw_rect_outline(cx+16, ny, cw-32, 80, B_BORDER);
        /* Status dot */
        uint32_t nsc = ns->connected ? B_GREEN : B_RED;
        vga_bb_fill_circle(cx+40, ny+20, 8, nsc);
        vga_bb_fill_circle(cx+40, ny+20, 6, B_BG_DARK);
        vga_bb_fill_circle(cx+40, ny+20, 3, nsc);
        /* Status text */
        vga_bb_draw_string_2x(cx+60, ny+12, "Status:", B_TEXT_DIM, 0x00000000);
        vga_bb_draw_string_2x(cx+60+8*CW, ny+12, net_status_str(), ns->connected ? B_GREEN : B_RED, 0x00000000);
        /* Connection type */
        vga_bb_draw_string_2x(cx+60, ny+34, "Type:", B_TEXT_DIM, 0x00000000);
        vga_bb_draw_string_2x(cx+60+6*CW, ny+34, ns->net_type, B_TEXT, 0x00000000);
        /* Speed */
        if (ns->detected) {
            char spd[16]; char tmp2[8];
            itoa(ns->link_speed, tmp2, 10);
            strcpy(spd, tmp2); strcat(spd, " Mbps");
            vga_bb_draw_string_2x(cx+60, ny+56, "Speed:", B_TEXT_DIM, 0x00000000);
            vga_bb_draw_string_2x(cx+60+7*CW, ny+56, spd, B_TEXT, 0x00000000);
        }
        ny += 96;

        /* ── Adapter Card ── */
        vga_bb_fill_rounded_rect(cx+16, ny, cw-32, 60, 8, B_BG_ALT);
        vga_bb_draw_rect_outline(cx+16, ny, cw-32, 60, B_BORDER);
        vga_bb_draw_string_2x(cx+28, ny+8, "Adapter:", B_TEXT_DIM, 0x00000000);
        vga_bb_draw_string_2x(cx+28+9*CW, ny+8, ns->nic_name, B_ACCENT, 0x00000000);
        if (ns->detected) {
            char vid[20]; char tmp2[8];
            strcpy(vid, "PCI ");
            itoa(ns->vendor_id, tmp2, 16);
            strcat(vid, tmp2); strcat(vid, ":");
            itoa(ns->device_id, tmp2, 16);
            strcat(vid, tmp2);
            vga_bb_draw_string_2x(cx+28, ny+32, vid, B_TEXT_DIM, 0x00000000);
        }
        ny += 76;

        /* ── IP Configuration Card ── */
        vga_bb_fill_rounded_rect(cx+16, ny, cw-32, 120, 8, B_BG_ALT);
        vga_bb_draw_rect_outline(cx+16, ny, cw-32, 120, B_BORDER);
        vga_bb_draw_string_2x(cx+28, ny+8, "IP Address:", B_TEXT_DIM, 0x00000000);
        vga_bb_draw_string_2x(cx+28+12*CW, ny+8, ns->ip_addr, ns->connected ? B_GREEN : B_TEXT_DIM, 0x00000000);
        vga_bb_draw_string_2x(cx+28, ny+32, "MAC:", B_TEXT_DIM, 0x00000000);
        vga_bb_draw_string_2x(cx+28+5*CW, ny+32, ns->mac_addr, B_TEXT, 0x00000000);
        vga_bb_draw_string_2x(cx+28, ny+56, "Gateway:", B_TEXT_DIM, 0x00000000);
        vga_bb_draw_string_2x(cx+28+9*CW, ny+56, ns->gateway, B_TEXT, 0x00000000);
        vga_bb_draw_string_2x(cx+28, ny+80, "DNS:", B_TEXT_DIM, 0x00000000);
        vga_bb_draw_string_2x(cx+28+5*CW, ny+80, ns->dns, B_TEXT, 0x00000000);
        vga_bb_draw_string_2x(cx+28, ny+100, "Subnet:", B_TEXT_DIM, 0x00000000);
        vga_bb_draw_string_2x(cx+28+8*CW, ny+100, "255.255.255.0", B_TEXT, 0x00000000);
        ny += 136;

        /* ── Connect/Disconnect Button ── */
        if (ns->detected) {
            mouse_state_t nms; mouse_get_state(&nms);
            int btn_x = cx + cw/2 - 80, btn_y = ny + 8;
            int btn_hover = (nms.x >= btn_x && nms.x < btn_x+160 && nms.y >= btn_y && nms.y < btn_y+36);
            uint32_t btn_c = ns->connected ? B_RED : B_GREEN;
            if (btn_hover) btn_c = ns->connected ? B_ORANGE : B_ACCENT;
            vga_bb_fill_rounded_rect(btn_x, btn_y, 160, 36, 8, btn_c);
            const char *btn_txt = ns->connected ? "Disconnect" : "Connect";
            int btw2 = (int)strlen(btn_txt) * CW;
            vga_bb_draw_string_2x(btn_x + (160-btw2)/2, btn_y+10, btn_txt, B_BG_DARK, 0x00000000);
        } else {
            vga_bb_draw_string_2x(cx+cw/2-10*CW, ny+16, "No network adapter found", B_TEXT_DIM, 0x00000000);
        }
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
        case WIN_STORE: strcpy(w->title,"Store"); w->x=200;w->y=60;w->w=860;w->h=560; w->store_sel=0; memset(w->store_downloaded,0,sizeof(w->store_downloaded)); break;
        case WIN_BROWSER: strcpy(w->title,"Browser"); w->x=180;w->y=40;w->w=900;w->h=640; w->browser_tab=0; break;
        case WIN_NETWORK: strcpy(w->title,"Network"); w->x=300;w->y=100;w->w=700;w->h=580; break;
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
                if (aid>=0) { open_window(aid); return 0; }
                return 0;
            }
        }
        kickoff_open=0; return 0;
    }
    /* Panel launcher */
    if (my>=dock_y && my<dock_y+PANEL_H && mx>=dock_x && mx<dock_x+64) { kickoff_open=!kickoff_open; quick_settings_open=0; return 0; }
    /* Panel clicks */
    if (my>=dock_y && my<dock_y+PANEL_H) {
        /* Clock/date area → toggle quick settings */
        if (mx >= dock_x + dock_w - 120) { quick_settings_open=!quick_settings_open; return 0; }
        /* Network tray icon */
        int ntx = dock_x + dock_w - 380 + 100;
        if (mx >= ntx-6 && mx < ntx+22) { open_window(WIN_NETWORK); return 0; }
        /* Task buttons */
        int bx=dock_x + 66;
        for (int i=0;i<win_order_count;i++) { int wi=win_order[i]; if(!windows[wi].active) continue;
            int bw=(int)strlen(windows[wi].title)*CW+24; if(bw>200) bw=200;
            if (mx>=bx&&mx<bx+bw) { bring_to_front(wi); return 0; } bx+=bw+6; }
        return 0;
    }
    /* Close quick settings on click outside */
    if (quick_settings_open) { quick_settings_open=0; }
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
            /* Store click */
            if (w->type == WIN_STORE) {
                int scx = w->x+4, scy = w->y+TITLEBAR_H+4, scw = w->w-8;
                int hy = scy + 40;
                int ey = hy + CH + 8;
                for (int si = 0; si < 5; si++) {
                    if (mx >= scx+4 && mx < scx+scw-4 && my >= ey && my < ey+54) {
                        w->store_sel = si;
                        /* Check download button click */
                        if (mx >= scx+scw-130 && mx < scx+scw-20 && my >= ey+8 && my < ey+32) {
                            w->store_downloaded[si] = 1;
                        }
                        return 0;
                    }
                    ey += 58;
                }
            }
            /* Browser click — tab switching */
            if (w->type == WIN_BROWSER) {
                int bcx = w->x+4, bcy = w->y+TITLEBAR_H+4;
                if (my >= bcy && my < bcy+30) {
                    if (mx >= bcx+4 && mx < bcx+124) { w->browser_tab = 0; return 0; }
                    if (mx >= bcx+130 && mx < bcx+230) { w->browser_tab = 1; return 0; }
                }
            }
            /* Network click — connect/disconnect button */
            if (w->type == WIN_NETWORK) {
                net_status_t *ns = net_get_status();
                if (ns->detected) {
                    int ncx = w->x+4, ncw = w->w-8;
                    int btn_x = ncx + ncw/2 - 80;
                    int btn_y = w->y+TITLEBAR_H+4 + 52 + 96 + 76 + 136 + 8;
                    if (mx >= btn_x && mx < btn_x+160 && my >= btn_y && my < btn_y+36) {
                        net_toggle_connection();
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
            if (ic->app_id>=0) { open_window(ic->app_id); return 0; }
        } }
    kickoff_open=0; return 0;
}

/* ── Full desktop draw ────────────────────────────────────── */
static void draw_desktop(void) {
    draw_wallpaper();
    draw_icons();
    /* Only draw active windows to save cycles */
    for (int i = 0; i < win_order_count; i++) {
        int wi = win_order[i];
        if (windows[wi].active) draw_window(wi);
    }
    draw_panel();
    draw_kickoff();
    draw_context_menu();
    mouse_state_t ms; mouse_get_state(&ms);
    draw_cursor(ms.x, ms.y);
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
        if (ms.moved) needs_redraw = 1;
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

        /* Cursor blink — only trigger redraw if a text-input window is focused */
        int cur_blink = (ticks / 30) % 2;
        if (cur_blink != last_blink) {
            last_blink = cur_blink;
            if (win_focus >= 0 && windows[win_focus].active) {
                int ft = windows[win_focus].type;
                if (ft == WIN_TERM || ft == WIN_AI || ft == WIN_NOTES)
                    needs_redraw = 1;
            }
        }

        /* Clock update — check every second instead of every tick */
        if (ticks - last_draw > 50) {
            rtc_time_t rtc; rtc_read(&rtc);
            if (rtc.minute != last_minute) { last_minute = rtc.minute; needs_redraw = 1; }
        }

        if (needs_redraw || dragging) { draw_desktop(); needs_redraw = 0; last_draw = ticks; }
        __asm__ volatile("hlt");
    }
}
