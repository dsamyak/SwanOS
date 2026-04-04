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
#include "audit.h"

/* ── Layout ───────────────────────────────────────────────── */
#define SCRW       GFX_W
#define SCRH       GFX_H
#define PANEL_H    60
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
#define MAX_WINDOWS 16
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
#define WIN_AUDIT 10
#define WIN_DRAW 11
#define WIN_CLOCK 12

#define ICON_W     80
#define ICON_H     80
#define MAX_ICONS  16

typedef struct { int x, y; const char *label; int app_id; } desktop_icon_t;

static desktop_icon_t icons[MAX_ICONS] = {
    {50, 40,   "SwanSoul",  WIN_AI},
    {50, 150,  "SwanShell", WIN_TERM},
    {50, 260,  "Feather",   WIN_FILES},
    {50, 370,  "LoveLetter",WIN_NOTES},
    {50, 480,  "SwanSong",  WIN_ABOUT},
    {50, 590,  "SwanCount", WIN_CALC},
    {50, 700,  "Heartbeat", WIN_SYSMON},
    {170, 40,  "SwanNest",  WIN_STORE},
    {170, 150, "SwanLake",  WIN_BROWSER},
    {170, 260, "WingLink",  WIN_NETWORK},
    {170, 370, "SwanWatch", WIN_AUDIT},
    {170, 480, "SwanDraw",  WIN_DRAW},
    {170, 590, "SwanClock", WIN_CLOCK},
    {0, 0, 0, 0},
};
static int num_icons = 13;

/* ── Notification Toast System ──────────────────────────── */
#define MAX_TOASTS 4
#define TOAST_DURATION 300  /* ticks (~3 seconds at 100Hz) */
typedef struct {
    char text[48];
    uint32_t color;
    uint32_t start_tick;
    int active;
} toast_t;
static toast_t toasts[MAX_TOASTS];

static void toast_show(const char *text, uint32_t color) {
    /* Find free toast slot */
    int slot = -1;
    for (int i = 0; i < MAX_TOASTS; i++) {
        if (!toasts[i].active) { slot = i; break; }
    }
    if (slot < 0) slot = 0; /* overwrite oldest */
    strncpy(toasts[slot].text, text, 47);
    toasts[slot].text[47] = '\0';
    toasts[slot].color = color;
    toasts[slot].start_tick = timer_get_ticks();
    toasts[slot].active = 1;
}

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
    int workspace;
    uint32_t open_anim_tick;  /* tick when window was opened (for animations) */
    /* SwanDraw state */
    uint8_t draw_canvas[32][32]; /* 32x32 pixel art (color index 0-7) */
    int draw_color;              /* selected palette color index */
    /* SwanClock state */
    int clock_sw_running;        /* stopwatch running? */
    uint32_t clock_sw_start;     /* stopwatch start tick */
    uint32_t clock_sw_elapsed;   /* stopwatch accumulated ticks */
} window_t;

static window_t windows[MAX_WINDOWS];
static int win_count = 0, win_focus = -1;
static int win_order[MAX_WINDOWS], win_order_count = 0;
static int dragging = 0, drag_win = -1, drag_ox, drag_oy;

/* ── Kickoff Menu ─────────────────────────────────────────── */
static int kickoff_open = 0;
#define KO_W     280
#define KO_ITEMS  15
#define KO_ITEM_H 36
#define KO_HEADER  80
#define KO_H      (KO_HEADER + KO_ITEMS * KO_ITEM_H + 8)

static const char *ko_labels[KO_ITEMS] = {
    "SwanSoul", "SwanShell", "Feather", "LoveLetter",
    "SwanCount", "Heartbeat", "SwanSong",
    "SwanNest", "SwanLake", "WingLink", "SwanWatch",
    "SwanDraw", "SwanClock",
    "--------", "Shut Down"
};
static int ko_ids[KO_ITEMS] = {WIN_AI,WIN_TERM,WIN_FILES,WIN_NOTES,WIN_CALC,WIN_SYSMON,WIN_ABOUT,WIN_STORE,WIN_BROWSER,WIN_NETWORK,WIN_AUDIT,WIN_DRAW,WIN_CLOCK,-1,-2};

/* ── Workspace ────────────────────────────────────────────── */
#define MAX_WORKSPACES 3
static int current_workspace = 0;

/* ── Wallpaper cache ──────────────────────────────────────── */
static int wp_cached = 0;
static uint32_t wp_buf[1920 * 1080];
static uint32_t wp_phase = 0;
static uint32_t wp_last_tick = 0;

/* ── Forward declarations ─────────────────────────────────── */
static void open_window(int type);
static void draw_desktop(void);
static int  handle_click(int mx, int my);
static void term_add_line(window_t *w, const char *text);
static void term_process_cmd(window_t *w);
static int  sine_approx(int deg);

/* ── Wallpaper ────────────────────────────────────────────── */
static void render_wallpaper(void) {
    /* Render full screen height so wallpaper shows through dock transparency */
    ui_render_aurora_wallpaper_animated(wp_buf, GFX_W, GFX_H, wp_phase);
    wp_cached = 1;
}

static void draw_wallpaper(void) {
    if (!wp_cached) render_wallpaper();
    uint32_t *bb = vga_backbuffer();
    /* Full screen blit — wallpaper visible through transparent dock */
    for (int y = 0; y < GFX_H; y++) {
        uint32_t *dst = &bb[y * GFX_W];
        const uint32_t *src = &wp_buf[y * GFX_W];
        for (int x = 0; x < GFX_W; x++)
            dst[x] = src[x];
    }
}

/* ── Desktop Icons (Neon Aurora Grid) ─────────────────────── */
/* Neon accent per app type for icon hover glow */
static uint32_t icon_accent(int app_id) {
    switch (app_id) {
        case WIN_AI:      return S_NEON_PURPLE;
        case WIN_TERM:    return S_GREEN;
        case WIN_FILES:   return S_YELLOW;
        case WIN_NOTES:   return S_NEON_CYAN;
        case WIN_ABOUT:   return S_BLUE;
        case WIN_CALC:    return S_PINK;
        case WIN_SYSMON:  return S_RED;
        case WIN_STORE:   return S_GREEN;
        case WIN_BROWSER: return S_ORANGE;
        case WIN_NETWORK: return S_BLUE;
        case WIN_AUDIT:   return S_NEON_MAGENTA;
        case WIN_DRAW:    return S_PINK;
        case WIN_CLOCK:   return S_YELLOW;
        default:          return S_NEON_CYAN;
    }
}

static void draw_icon_glyph(int x, int y, int app_id, int hovered) {
    /* Use themed icon card */
    ui_icon_card(x, y, 72, 56, hovered, icon_accent(app_id));
    int cx = x + 16, cy = y + 8;
    switch (app_id) {
        case 0: /* Terminal */
            vga_bb_fill_rounded_rect(cx, cy, 32, 24, 4, B_BG_DARK);
            vga_bb_fill_rect(cx, cy, 32, 6, B_TITLEBAR);
            vga_bb_draw_rect_outline(cx, cy, 32, 24, S_NEON_CYAN);
            vga_bb_draw_string_2x(cx+2, cy+7, ">_", B_GREEN, 0x00000000);
            vga_bb_fill_circle(cx+26, cy+3, 2, B_RED);
            break;
        case 1: /* Files */
            vga_bb_fill_rounded_rect(cx, cy, 28, 22, 4, B_ACCENT2);
            vga_bb_fill_rounded_rect(cx-4, cy+4, 10, 3, 2, B_ACCENT2);
            vga_bb_fill_rounded_rect(cx-2, cy+6, 30, 18, 4, S_NEON_CYAN);
            vga_bb_fill_rect(cx+2, cy+8, 22, 12, B_BG_ALT);
            break;
        case 2: /* Notes */
            vga_bb_fill_rounded_rect(cx-1, cy+1, 24, 26, 3, B_SHADOW);
            vga_bb_fill_rounded_rect(cx, cy, 24, 26, 3, B_TEXT);
            vga_bb_fill_rect(cx+2, cy+2, 20, 22, B_BG_LIGHT);
            for (int i = 0; i < 4; i++)
                vga_bb_draw_hline(cx+4, cy+6+i*5, 14, S_NEON_CYAN);
            break;
        case 3: /* About */
            vga_bb_fill_circle(cx+13, cy+13, 12, B_SHADOW);
            vga_bb_fill_circle(cx+12, cy+12, 12, S_NEON_CYAN);
            vga_bb_fill_circle(cx+12, cy+12, 10, B_BG);
            vga_bb_draw_string_2x(cx+6, cy+4, "i", S_NEON_CYAN, 0x00000000);
            break;
        case WIN_AI: /* AI Chat */
            vga_bb_fill_rounded_rect(cx-1, cy+1, 32, 26, 6, B_SHADOW);
            vga_bb_fill_rounded_rect(cx-2, cy, 32, 26, 6, S_NEON_PURPLE);
            vga_bb_fill_rounded_rect(cx, cy+2, 28, 22, 5, B_BG);
            vga_bb_draw_string_2x(cx+3, cy+6, "AI", 0x80000000, 0x00000000);
            vga_bb_draw_string_2x(cx+2, cy+5, "AI", S_NEON_PURPLE, 0x00000000);
            vga_bb_fill_circle(cx+28, cy+2, 4, S_YELLOW);
            break;
        case WIN_CALC:
            vga_bb_fill_rounded_rect(cx-2, cy, 28, 34, 4, B_BG_DARK);
            vga_bb_draw_rect_outline(cx-2, cy, 28, 34, S_PINK);
            vga_bb_fill_rect(cx, cy+4, 24, 8, B_BG_ALT);
            vga_bb_draw_string_2x(cx+10, cy+1, "=", B_GREEN, 0x00000000);
            for(int yy=0;yy<3;yy++) for(int xx=0;xx<3;xx++) vga_bb_fill_rect(cx+2+xx*8, cy+16+yy*6, 6, 4, B_SEPARATOR);
            break;
        case WIN_SYSMON:
            vga_bb_fill_rounded_rect(cx-2, cy, 32, 28, 4, B_WIN_BG);
            vga_bb_draw_rect_outline(cx-2, cy, 32, 28, S_NEON_CYAN);
            vga_bb_fill_rect(cx+2, cy+16, 6, 10, B_GREEN);
            vga_bb_fill_rect(cx+10, cy+10, 6, 16, B_YELLOW);
            vga_bb_fill_rect(cx+18, cy+6, 6, 20, B_RED);
            break;
        case WIN_STORE:
            vga_bb_fill_rounded_rect(cx-1, cy+1, 28, 30, 4, B_SHADOW);
            vga_bb_fill_rounded_rect(cx-2, cy, 28, 30, 4, B_GREEN);
            vga_bb_fill_rounded_rect(cx, cy+2, 24, 26, 3, B_BG_DARK);
            vga_bb_fill_rect(cx+9, cy+6, 6, 10, B_GREEN);
            vga_bb_fill_rect(cx+6, cy+14, 12, 2, B_GREEN);
            vga_bb_fill_rect(cx+8, cy+16, 8, 2, B_GREEN);
            vga_bb_fill_rect(cx+10, cy+18, 4, 2, B_GREEN);
            vga_bb_fill_circle(cx+22, cy+24, 5, S_NEON_CYAN);
            break;
        case WIN_BROWSER:
            vga_bb_fill_circle(cx+13, cy+15, 14, B_SHADOW);
            vga_bb_fill_circle(cx+12, cy+14, 14, S_ORANGE);
            vga_bb_fill_circle(cx+12, cy+14, 12, B_BG_DARK);
            vga_bb_draw_hline(cx+2, cy+9, 20, S_ORANGE);
            vga_bb_draw_hline(cx+2, cy+14, 20, S_ORANGE);
            vga_bb_draw_hline(cx+2, cy+19, 20, S_ORANGE);
            vga_bb_draw_vline(cx+12, cy+2, 24, S_ORANGE);
            vga_bb_fill_circle(cx+20, cy+4, 4, S_NEON_CYAN);
            vga_bb_fill_circle(cx+20, cy+4, 2, B_BG_DARK);
            break;
        case WIN_AUDIT:
            vga_bb_fill_rounded_rect(cx-1, cy+1, 28, 28, 4, B_SHADOW);
            vga_bb_fill_rounded_rect(cx-2, cy, 28, 28, 4, S_NEON_MAGENTA);
            vga_bb_fill_rounded_rect(cx, cy+2, 24, 24, 3, B_BG_DARK);
            for (int i = 0; i < 4; i++)
                vga_bb_draw_hline(cx+4, cy+6+i*5, 16, S_NEON_MAGENTA);
            vga_bb_fill_circle(cx+18, cy+6, 3, S_YELLOW);
            break;
        case WIN_NETWORK:
            vga_bb_fill_rounded_rect(cx-1, cy+1, 32, 28, 6, B_SHADOW);
            vga_bb_fill_rounded_rect(cx-2, cy, 32, 28, 6, S_BLUE);
            vga_bb_fill_rounded_rect(cx, cy+2, 28, 24, 5, B_BG_DARK);
            vga_bb_draw_circle(cx+14, cy+22, 14, B_GREEN);
            vga_bb_draw_circle(cx+14, cy+22, 10, B_GREEN);
            vga_bb_draw_circle(cx+14, cy+22, 6, B_GREEN);
            vga_bb_fill_circle(cx+14, cy+22, 2, B_GREEN);
            vga_bb_fill_rect(cx, cy+23, 28, 4, B_BG_DARK);
            break;
        case WIN_DRAW:
            vga_bb_fill_rounded_rect(cx-1, cy+1, 30, 30, 4, B_SHADOW);
            vga_bb_fill_rounded_rect(cx-2, cy, 30, 30, 4, S_PINK);
            vga_bb_fill_rounded_rect(cx, cy+2, 26, 26, 3, B_BG_DARK);
            /* Paint palette dots */
            vga_bb_fill_circle(cx+6, cy+8, 3, S_RED);
            vga_bb_fill_circle(cx+16, cy+8, 3, S_GREEN);
            vga_bb_fill_circle(cx+6, cy+18, 3, S_BLUE);
            vga_bb_fill_circle(cx+16, cy+18, 3, S_YELLOW);
            /* Brush stroke */
            vga_bb_fill_rect(cx+20, cy+4, 3, 20, S_PINK);
            break;
        case WIN_CLOCK:
            vga_bb_fill_rounded_rect(cx-1, cy+1, 30, 30, 4, B_SHADOW);
            vga_bb_fill_circle(cx+13, cy+16, 14, S_YELLOW);
            vga_bb_fill_circle(cx+13, cy+16, 12, B_BG_DARK);
            /* Clock hands */
            vga_bb_fill_rect(cx+12, cy+8, 2, 9, S_YELLOW);
            vga_bb_fill_rect(cx+13, cy+15, 7, 2, S_NEON_CYAN);
            /* Center dot */
            vga_bb_fill_circle(cx+13, cy+16, 2, S_YELLOW);
            break;
    }
}

static void draw_icons(void) {
    mouse_state_t ms; mouse_get_state(&ms);
    uint32_t anim_t = timer_get_ticks();
    /* Proper 2-column grid: 100px wide cells, 90px tall, 30px left margin */
    for (int i = 0; i < num_icons; i++) {
        desktop_icon_t *ic = &icons[i];
        /* Compute grid position: 2 columns */
        int col = i / 7;
        int row = i % 7;
        int gx = 30 + col * 110;
        int gy = 30 + row * 100;
        /* Update icon position for click handling */
        ic->x = gx;
        ic->y = gy;
        int hovered = (ms.x >= gx && ms.x < gx+80 && ms.y >= gy && ms.y < gy+80);

        /* Dynamic hover effect: neon glow ring + slight lift */
        if (hovered) {
            uint32_t accent = icon_accent(ic->app_id);
            /* Pulsing outer glow */
            int glow_a = 0x18 + (sine_approx((anim_t * 6 + i * 40) % 360) * 0x10) / 100;
            if (glow_a < 0x08) glow_a = 0x08;
            uint32_t glow_c = ((uint32_t)glow_a << 24) | (accent & 0x00FFFFFF);
            vga_bb_fill_rounded_rect(gx-6, gy-6, 84, 84, 14, glow_c);
            vga_bb_fill_rounded_rect(gx-3, gy-3, 78, 78, 12, ((uint32_t)(glow_a/2) << 24) | (accent & 0x00FFFFFF));
            /* Draw icon slightly raised (lift by 2px) */
            draw_icon_glyph(gx, gy - 2, ic->app_id, hovered);
        } else {
            draw_icon_glyph(gx, gy, ic->app_id, hovered);
        }

        /* Centered label below icon */
        int lw = (int)strlen(ic->label) * CW;
        int lx = gx + (72 - lw) / 2;
        if (lx < 2) lx = 2;
        int ly_off = hovered ? -2 : 0;
        vga_bb_draw_string_2x(lx+1, gy+62+ly_off, ic->label, 0x80000000, 0x00000000);
        vga_bb_draw_string_2x(lx, gy+61+ly_off, ic->label, hovered ? 0xFFFFFFFF : B_TEXT, 0x00000000);
    }
}

/* Dock Layout Globals for Clicks */
static int dock_x = 0, dock_w = 0, dock_y = 0;
static int kickoff_x = 0;

/* ── Panel (Floating Neon Aurora Dock) ──────────── */
static int quick_settings_open = 0;  /* Quick settings popup state */
static int notification_count = 3;   /* Simulated notification count */

/* Tray memory sparkline (20 samples, 0-100% range) */
static int tray_mem_history[20];
static int tray_mem_head = 0;
static uint32_t tray_mem_tick = 0;

/* ── Rearrange Mode ──────────────────────────────── */
static int rearrange_mode = 0;       /* 1 = tray items can be dragged */
static int tray_drag_active = 0;     /* currently dragging a tray slot */
static int tray_drag_slot = -1;      /* which slot is being dragged */
static int tray_slot_order[5] = {0,1,2,3,4}; /* MEM, NET, VOL, BAT, BELL */

/* Dock Animation State */
static uint32_t dock_anim_tick = 0;

/* Simple sine approximation (0-360 input, returns -100..100) */
static int sine_approx(int deg) {
    deg = deg % 360;
    if (deg < 0) deg += 360;
    if (deg <= 90) return (deg * 100) / 90;
    if (deg <= 180) return ((180 - deg) * 100) / 90;
    if (deg <= 270) return -((deg - 180) * 100) / 90;
    return -((360 - deg) * 100) / 90;
}

/* Animated rainbow color from phase offset */
static uint32_t rainbow_color(int phase) {
    phase = phase % 768;
    if (phase < 0) phase += 768;
    uint8_t r, g, b;
    if (phase < 256)      { r = 255 - phase; g = phase; b = 0; }
    else if (phase < 512) { r = 0; g = 511 - phase; b = phase - 256; }
    else                  { r = phase - 512; g = 0; b = 767 - phase; }
    return 0xFF000000 | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

static void draw_panel(void) {
    /* Calculate required dock width */
    int wins_w = 0;
    for (int i = 0; i < win_order_count; i++) {
        int wi = win_order[i];
        if (!windows[wi].active) continue;
        int bw = (int)strlen(windows[wi].title) * CW + 28;
        if (bw > 200) bw = 200;
        wins_w += bw + 8;
    }

    dock_w = 64 + 16 + wins_w + 520;
    if (dock_w > SCRW - 40) dock_w = SCRW - 40;
    dock_x = (SCRW - dock_w) / 2;

    /* Animated floating bob — subtle 3px oscillation */
    dock_anim_tick = timer_get_ticks();
    int bob_offset = (sine_approx((dock_anim_tick * 3) % 360) * 3) / 100;
    dock_y = SCRH - PANEL_H - 24 + bob_offset;  /* Larger floating gap */

    /* ── Dock Background: ultra-transparent frosted panel ── */
    /* Multi-layer soft shadow for floating depth */
    ui_soft_shadow(dock_x, dock_y, dock_w, PANEL_H, 22, 7);

    /* Glass panel with deeper transparency */
    vga_bb_fill_rounded_rect(dock_x, dock_y, dock_w, PANEL_H, 18, 0x40081020);
    /* Secondary frosted glass layer */
    vga_bb_fill_rounded_rect(dock_x+1, dock_y+1, dock_w-2, PANEL_H-2, 17, 0x18FFFFFF);
    /* Inner highlight reflection strip (top edge) */
    vga_bb_fill_rounded_rect(dock_x+4, dock_y+2, dock_w-8, 2, 1, 0x15FFFFFF);

    /* Animated rainbow neon border */
    int phase = (dock_anim_tick * 4) % 768;
    uint32_t neon1 = rainbow_color(phase);
    uint32_t neon2 = rainbow_color(phase + 256);
    /* Top edge — gradient from left to right */
    for (int px = 0; px < dock_w; px++) {
        int t = (px * 256) / dock_w;
        uint8_t r1 = (neon1 >> 16) & 0xFF, g1 = (neon1 >> 8) & 0xFF, b1 = neon1 & 0xFF;
        uint8_t r2 = (neon2 >> 16) & 0xFF, g2 = (neon2 >> 8) & 0xFF, b2 = neon2 & 0xFF;
        uint8_t r = r1 + ((r2 - r1) * t) / 256;
        uint8_t g = g1 + ((g2 - g1) * t) / 256;
        uint8_t b = b1 + ((b2 - b1) * t) / 256;
        uint32_t c = 0xC0000000 | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        int x = dock_x + px;
        if (x >= 0 && x < SCRW) {
            vga_bb_putpixel(x, dock_y, c);
            vga_bb_putpixel(x, dock_y + PANEL_H - 1, c);
        }
    }
    /* Side edges */
    for (int py = 0; py < PANEL_H; py++) {
        int t = (py * 256) / PANEL_H;
        uint8_t r1 = (neon1 >> 16) & 0xFF, g1 = (neon1 >> 8) & 0xFF, b1 = neon1 & 0xFF;
        uint8_t r2 = (neon2 >> 16) & 0xFF, g2 = (neon2 >> 8) & 0xFF, b2 = neon2 & 0xFF;
        uint8_t r = r1 + ((r2 - r1) * t) / 256;
        uint8_t g = g1 + ((g2 - g1) * t) / 256;
        uint8_t b = b1 + ((b2 - b1) * t) / 256;
        uint32_t c = 0xC0000000 | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        int y = dock_y + py;
        if (y >= 0 && y < SCRH) {
            vga_bb_putpixel(dock_x, y, c);
            vga_bb_putpixel(dock_x + dock_w - 1, y, c);
        }
    }
    /* Outer glow halo (2 layers) */
    uint32_t glow1 = (0x18 << 24) | (neon1 & 0x00FFFFFF);
    uint32_t glow2 = (0x08 << 24) | (neon2 & 0x00FFFFFF);
    vga_bb_draw_rect_outline(dock_x-1, dock_y-1, dock_w+2, PANEL_H+2, glow1);
    vga_bb_draw_rect_outline(dock_x-2, dock_y-2, dock_w+4, PANEL_H+4, glow2);

    mouse_state_t ms; mouse_get_state(&ms);
    const int msx = ms.x, msy = ms.y;

    /* ════════════════════════════════════════════════════════
     *  Launcher Pill Segment
     * ════════════════════════════════════════════════════════ */
    kickoff_x = dock_x + 6;
    int launcher_w = 56;
    int lhover = (msx >= dock_x+4 && msx < dock_x+4+launcher_w && msy >= dock_y+4 && msy < dock_y+PANEL_H-4);
    ui_pill_segment(dock_x+4, dock_y+4, launcher_w, PANEL_H-8, 14,
                    lhover || kickoff_open ? 0x30FFFFFF : 0x14FFFFFF,
                    0x20FFFFFF, lhover || kickoff_open);

    /* Swan monogram with neon glow */
    int swan_cx = dock_x+4+launcher_w/2, swan_cy = dock_y+PANEL_H/2;
    if (lhover || kickoff_open) {
        vga_bb_fill_circle_alpha(swan_cx, swan_cy, 18, S_GLOW_PULSE);
        vga_bb_fill_circle_alpha(swan_cx, swan_cy, 14, S_ACCENT_GLOW);
    }
    vga_bb_fill_circle(swan_cx, swan_cy, 13, S_NEON_CYAN);
    vga_bb_fill_circle(swan_cx, swan_cy, 11, S_BG_DEEP);
    vga_bb_draw_string_2x(swan_cx-7+1, swan_cy-7+1, "S", 0x60000000, 0x00000000);
    vga_bb_draw_string_2x(swan_cx-7, swan_cy-7, "S", S_NEON_CYAN, 0x00000000);
    vga_bb_fill_circle(swan_cx+8, swan_cy-8, 2, 0x60FFFFFF);

    /* ════════════════════════════════════════════════════════
     *  Task Buttons Pill Segment
     * ════════════════════════════════════════════════════════ */
    int tasks_x = dock_x + 4 + launcher_w + 6;
    /* Use actual tray position instead of hardcoded offset */
    int tray_right_ref = dock_x + dock_w - 140;
    int tray_start_ref = tray_right_ref - 360;
    int tasks_end = tray_start_ref - 12;  /* 12px gap before tray */
    int tasks_w = tasks_end - tasks_x;
    if (tasks_w < 60) tasks_w = 60;
    if (win_order_count > 0)
        ui_pill_segment(tasks_x, dock_y+4, tasks_w, PANEL_H-8, 12,
                        0x10FFFFFF, 0x15FFFFFF, 0);

    int bx = tasks_x + 8;
    for (int i = 0; i < win_order_count && bx < tasks_x + tasks_w - 20; i++) {
        int wi = win_order[i];
        if (!windows[wi].active) continue;
        int focused = (wi == win_focus);
        int bw = (int)strlen(windows[wi].title) * CW + 32;
        if (bw > 200) bw = 200;
        int thover = (msx >= bx && msx < bx+bw && msy >= dock_y+4 && msy < dock_y+PANEL_H-4);

        /* Colored mini-icon dot with neon glow */
        uint32_t dot_c = S_ACCENT;
        if (windows[wi].type == WIN_AI) dot_c = S_NEON_PURPLE;
        else if (windows[wi].type == WIN_TERM) dot_c = S_GREEN;
        else if (windows[wi].type == WIN_FILES) dot_c = S_YELLOW;
        else if (windows[wi].type == WIN_NETWORK) dot_c = S_BLUE;
        else if (windows[wi].type == WIN_BROWSER) dot_c = S_ORANGE;
        else if (windows[wi].type == WIN_CALC) dot_c = S_PINK;
        else if (windows[wi].type == WIN_SYSMON) dot_c = S_RED;
        else if (windows[wi].type == WIN_STORE) dot_c = S_GREEN;
        else if (windows[wi].type == WIN_AUDIT) dot_c = S_NEON_MAGENTA;

        if (focused) {
            /* Pulsing neon glow behind focused button */
            int pulse_alpha = 0x20 + (sine_approx((dock_anim_tick * 5) % 360) * 0x10) / 100;
            if (pulse_alpha < 0x10) pulse_alpha = 0x10;
            uint32_t pulse_color = ((uint32_t)pulse_alpha << 24) | (dot_c & 0x00FFFFFF);
            vga_bb_fill_rounded_rect(bx-2, dock_y+5, bw+4, PANEL_H-10, 12, pulse_color);
            vga_bb_fill_rounded_rect(bx, dock_y+6, bw, PANEL_H-12, 10, S_BG_HOVER);
            /* Animated gradient underline */
            int uline_phase = (dock_anim_tick * 6 + bx) % 768;
            uint32_t uc1 = rainbow_color(uline_phase);
            uint32_t uc2 = rainbow_color(uline_phase + 384);
            vga_bb_fill_rounded_rect_gradient(bx+4, dock_y+PANEL_H-8, bw-8, 3, 1, uc1, uc2);
            vga_bb_draw_hline(bx+10, dock_y+7, bw-20, 0x15FFFFFF);
        } else if (thover) {
            vga_bb_fill_rounded_rect(bx, dock_y+6, bw, PANEL_H-12, 10, 0x18FFFFFF);
        }

        if (focused)
            vga_bb_fill_circle_alpha(bx+12, dock_y+PANEL_H/2, 8, (0x30 << 24) | (dot_c & 0x00FFFFFF));
        vga_bb_fill_circle(bx+12, dock_y+PANEL_H/2, 5, dot_c);
        vga_bb_fill_circle(bx+12, dock_y+PANEL_H/2, 3, S_BG_DEEP);
        vga_bb_fill_circle(bx+12, dock_y+PANEL_H/2, 2, dot_c);

        /* Active app indicator dot below button */
        vga_bb_fill_circle(bx + bw/2, dock_y+PANEL_H-5, 2, dot_c);
        if (focused) {
            vga_bb_fill_circle_alpha(bx + bw/2, dock_y+PANEL_H-5, 4, (0x30 << 24) | (dot_c & 0x00FFFFFF));
        }

        vga_bb_draw_string_2x(bx+23, dock_y+PANEL_H/2-8, windows[wi].title, 0x40000000, 0x00000000);
        vga_bb_draw_string_2x(bx+22, dock_y+PANEL_H/2-9, windows[wi].title, focused ? B_TEXT : B_TEXT_DIM, 0x00000000);
        bx += bw + 6;
    }

    /* ════════════════════════════════════════════════════════
     *  System Tray Pill Segment
     * ════════════════════════════════════════════════════════ */
    int tray_right = dock_x + dock_w - 140;
    int tray_start = tray_right - 360;
    int tray_w = tray_right - tray_start;
    ui_pill_segment(tray_start, dock_y+4, tray_w, PANEL_H-8, 12,
                    0x10FFFFFF, 0x15FFFFFF, 0);

    /* Rearrange mode indicator */
    if (rearrange_mode) {
        vga_bb_fill_rounded_rect(tray_start+2, dock_y+4, tray_w-4, 2, 1, S_NEON_MAGENTA);
        vga_bb_fill_rounded_rect(tray_start+2, dock_y+PANEL_H-6, tray_w-4, 2, 1, S_NEON_MAGENTA);
    }

    int tx = tray_start + 8;

    /* ── 1. Memory sparkline + text ── */
    {
        int mg_w = 48, mg_h = 22;
        int mem_grp_w = mg_w + 46;
        int mem_hover = (msx >= tx-4 && msx < tx+mem_grp_w+4 && msy >= dock_y+4 && msy < dock_y+PANEL_H-4);
        ui_tray_icon_bg(tx-4, dock_y+8, mem_grp_w+8, PANEL_H-16, mem_hover);
        if (rearrange_mode)
            ui_drag_handle(tx-4, dock_y+8, 8, PANEL_H-16, S_NEON_MAGENTA);

        ui_mini_graph(tx+4, dock_y+10, mg_w, mg_h, tray_mem_history, 20, S_GREEN, S_RED);

        int mem_pct = 0;
        int mt = mem_total();
        if (mt > 0) mem_pct = (mem_used() * 100) / mt;
        char mp[8]; itoa(mem_pct, mp, 10); strcat(mp, "%");
        vga_bb_draw_string_2x(tx+mg_w+8, dock_y+12, mp, mem_pct > 80 ? S_RED : (mem_pct > 50 ? S_YELLOW : S_GREEN), 0x00000000);
        vga_bb_draw_string(tx+mg_w+8, dock_y+32, "MEM", S_TEXT_DIM, 0x00000000);

        if (mem_hover && !rearrange_mode) {
            char mtip[32]; char tmp2[8];
            strcpy(mtip, ""); itoa(mem_used()/1024, tmp2, 10); strcat(mtip, tmp2);
            strcat(mtip, "K / "); itoa(mt/1024, tmp2, 10); strcat(mtip, tmp2); strcat(mtip, "K");
            ui_tooltip(tx + mem_grp_w/2, dock_y, mtip, S_BG_LIGHT, S_TEXT);
        }
        tx += mem_grp_w + 10;
    }

    ui_divider_v(tx-3, dock_y+12, PANEL_H-24, 0x30FFFFFF);
    tx += 5;

    /* ── 2. Network icon with signal bars ── */
    {
        int net_w = 36;
        int net_hover = (msx >= tx-4 && msx < tx+net_w+4 && msy >= dock_y+4 && msy < dock_y+PANEL_H-4);
        ui_tray_icon_bg(tx-4, dock_y+8, net_w+8, PANEL_H-16, net_hover);
        if (rearrange_mode)
            ui_drag_handle(tx-4, dock_y+8, 8, PANEL_H-16, S_NEON_MAGENTA);

        net_status_t *ns = net_get_status();
        uint32_t nc = ns->connected ? S_GREEN : S_RED;

        int bar_x = tx + 2;
        int bar_base = dock_y + PANEL_H/2 + 10;
        int bar_heights[] = {8, 14, 20};
        int bar_widths = 6;
        int n_bars = ns->connected ? 3 : 1;
        for (int b = 0; b < 3; b++) {
            uint32_t bc = (b < n_bars) ? nc : S_BG_LIGHT;
            vga_bb_fill_rounded_rect(bar_x + b*(bar_widths+3), bar_base - bar_heights[b],
                                     bar_widths, bar_heights[b], 2, bc);
        }

        const char *ntype = ns->connected ? "LAN" : "OFF";
        ui_badge(tx, dock_y+PANEL_H/2 + 12, ntype, ns->connected ? S_ACCENT2 : S_BG_LIGHT, ns->connected ? S_BG_DEEP : S_TEXT_DIM);

        if (net_hover && !rearrange_mode) {
            const char *tip = ns->connected ? "Connected" : "Disconnected";
            ui_tooltip(tx + net_w/2, dock_y, tip, S_BG_LIGHT, S_TEXT);
        }
        tx += net_w + 10;
    }

    ui_divider_v(tx-3, dock_y+12, PANEL_H-24, 0x30FFFFFF);
    tx += 5;

    /* ── 3. Volume icon ── */
    {
        int vol_w = 30;
        int vol_hover = (msx >= tx-4 && msx < tx+vol_w+4 && msy >= dock_y+4 && msy < dock_y+PANEL_H-4);
        ui_tray_icon_bg(tx-4, dock_y+8, vol_w+8, PANEL_H-16, vol_hover);
        if (rearrange_mode)
            ui_drag_handle(tx-4, dock_y+8, 8, PANEL_H-16, S_NEON_MAGENTA);

        int vy = dock_y + PANEL_H/2;
        vga_bb_fill_rect(tx+2, vy-5, 6, 10, S_TEXT);
        vga_bb_fill_rect(tx+8, vy-8, 5, 16, S_TEXT);
        vga_bb_draw_circle(tx+16, vy, 5, S_NEON_CYAN);
        vga_bb_draw_circle(tx+16, vy, 9, S_TEXT_DIM);
        vga_bb_draw_circle(tx+16, vy, 13, 0x40FFFFFF);
        vga_bb_fill_rect(tx, vy-14, 16, 28, 0x00000000);
        vga_bb_fill_rect(tx+2, vy-5, 6, 10, S_TEXT);
        vga_bb_fill_rect(tx+8, vy-8, 5, 16, S_TEXT);

        ui_badge(tx+2, dock_y+PANEL_H/2 + 12, "75%", S_ACCENT2, S_BG_DEEP);

        if (vol_hover && !rearrange_mode)
            ui_tooltip(tx + vol_w/2, dock_y, "Volume 75%", S_BG_LIGHT, S_TEXT);
        tx += vol_w + 10;
    }

    /* ── 4. Battery ── */
    {
        int bat_w = 36;
        int bat_hover = (msx >= tx-4 && msx < tx+bat_w+4 && msy >= dock_y+4 && msy < dock_y+PANEL_H-4);
        ui_tray_icon_bg(tx-4, dock_y+8, bat_w+8, PANEL_H-16, bat_hover);
        if (rearrange_mode)
            ui_drag_handle(tx-4, dock_y+8, 8, PANEL_H-16, S_NEON_MAGENTA);

        int by = dock_y + PANEL_H/2 - 2;
        vga_bb_fill_rounded_rect(tx+1, by-8, 26, 16, 3, S_BG_DEEP);
        vga_bb_draw_rect_outline(tx+1, by-8, 26, 16, S_TEXT);
        vga_bb_fill_rect(tx+27, by-4, 4, 8, S_TEXT);
        vga_bb_fill_rounded_rect(tx+27, by-3, 4, 6, 2, S_TEXT);
        vga_bb_fill_rounded_rect_gradient(tx+3, by-6, 22, 12, 2, S_GREEN, S_YELLOW);
        vga_bb_fill_rect(tx+12, by-5, 3, 4, S_BG_DEEP);
        vga_bb_fill_rect(tx+10, by-1, 3, 4, S_BG_DEEP);
        vga_bb_fill_rect(tx+13, by-3, 2, 2, S_BG_DEEP);
        ui_badge(tx+4, dock_y+PANEL_H/2 + 12, "AC", S_GREEN, S_BG_DEEP);

        if (bat_hover && !rearrange_mode)
            ui_tooltip(tx + bat_w/2, dock_y, "AC Power 100%", S_BG_LIGHT, S_TEXT);
        tx += bat_w + 10;
    }

    ui_divider_v(tx-3, dock_y+12, PANEL_H-24, 0x30FFFFFF);
    tx += 5;

    /* ── 4b. Security Lock ── */
    {
        int lk_w = 22;
        int lk_hover = (msx >= tx-4 && msx < tx+lk_w+4 && msy >= dock_y+4 && msy < dock_y+PANEL_H-4);
        ui_tray_icon_bg(tx-4, dock_y+8, lk_w+8, PANEL_H-16, lk_hover);
        int ly = dock_y + PANEL_H/2;
        /* Lock body */
        vga_bb_fill_rounded_rect(tx+2, ly-2, 14, 12, 2, S_GREEN);
        /* Lock shackle (arc) */
        vga_bb_draw_rect_outline(tx+4, ly-9, 10, 8, S_GREEN);
        vga_bb_fill_rect(tx+6, ly-7, 6, 5, 0x00000000);
        /* Keyhole */
        vga_bb_fill_circle(tx+9, ly+3, 2, S_BG_DEEP);
        if (lk_hover && !rearrange_mode)
            ui_tooltip(tx + lk_w/2, dock_y, "Secure", S_BG_LIGHT, S_TEXT);
        tx += lk_w + 6;
    }

    /* ── 5. Notification bell ── */
    {
        int bell_w = 28;
        int bell_hover = (msx >= tx-4 && msx < tx+bell_w+4 && msy >= dock_y+4 && msy < dock_y+PANEL_H-4);
        ui_tray_icon_bg(tx-4, dock_y+8, bell_w+8, PANEL_H-16, bell_hover);

        int bcx = tx + bell_w/2, bcy = dock_y + PANEL_H/2 - 2;
        if (notification_count > 0)
            vga_bb_fill_circle_alpha(bcx, bcy, 14, 0x18FBB724);
        /* Animated bell swing */
        int bell_swing = sine_approx((dock_anim_tick * 8) % 360);
        int bsx = bcx + (bell_swing * 2) / 100;
        vga_bb_fill_circle(bsx, bcy-2, 8, S_YELLOW);
        vga_bb_fill_rect(bsx-8, bcy+2, 16, 5, S_YELLOW);
        vga_bb_fill_rounded_rect(bsx-10, bcy+7, 20, 3, 1, S_YELLOW);
        vga_bb_fill_circle(bsx, bcy+12, 2, S_YELLOW);

        if (notification_count > 0) {
            vga_bb_fill_circle_alpha(bsx+10, bcy-8, 10, 0x25FF6B8A);
            vga_bb_fill_circle(bsx+10, bcy-8, 8, S_RED);
            vga_bb_fill_circle(bsx+10, bcy-8, 6, 0xFFFF6B6B);
            char nc_str[4]; itoa(notification_count, nc_str, 10);
            vga_bb_draw_string(bsx+7, bcy-12, nc_str, 0xFFFFFFFF, 0x00000000);
        }

        if (bell_hover && !rearrange_mode) {
            char ntip[24]; char tmp2[4];
            itoa(notification_count, tmp2, 10);
            strcpy(ntip, tmp2); strcat(ntip, " notifications");
            ui_tooltip(tx + bell_w/2, dock_y, ntip, S_BG_LIGHT, S_TEXT);
        }
        tx += bell_w + 10;
    }

    /* ════════════════════════════════════════════════════════
     *  Clock Pill Segment
     * ════════════════════════════════════════════════════════ */
    {
        int clk_seg_x = dock_x + dock_w - 136;
        int clk_seg_w = 130;
        ui_pill_segment(clk_seg_x, dock_y+4, clk_seg_w, PANEL_H-8, 12,
                        0x14FFFFFF, 0x18FFFFFF, 0);

        rtc_time_t rtc; rtc_read(&rtc);
        char clk[10]; rtc_format_time(&rtc, clk);
        clk[5] = '\0';
        int clk_x = clk_seg_x + (clk_seg_w - 5*CW) / 2;
        vga_bb_draw_string_2x(clk_x+2, dock_y+8, clk, 0x40000000, 0x00000000);
        vga_bb_draw_string_2x(clk_x+1, dock_y+7, clk, 0x25000000, 0x00000000);
        vga_bb_draw_string_2x(clk_x, dock_y+6, clk, S_TEXT, 0x00000000);

        /* Animated neon colon pulse */
        int colon_alpha = 0x80 + (sine_approx((dock_anim_tick * 10) % 360) * 0x60) / 100;
        if (colon_alpha < 0x20) colon_alpha = 0x20;
        uint32_t colon_c = ((uint32_t)colon_alpha << 24) | (S_NEON_CYAN & 0x00FFFFFF);
        vga_bb_fill_circle(clk_x + 2*CW + CW/2, dock_y+12, 2, colon_c);
        vga_bb_fill_circle(clk_x + 2*CW + CW/2, dock_y+20, 2, colon_c);

        char wday[4]; rtc_format_weekday(&rtc, wday);
        char dateline[24];
        strcpy(dateline, wday); strcat(dateline, " ");
        char dm[4]; itoa(rtc.day, dm, 10); strcat(dateline, dm);
        const char *months[] = {"","Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
        int mi2 = rtc.month; if (mi2 < 1) mi2 = 1; if (mi2 > 12) mi2 = 12;
        strcat(dateline, " "); strcat(dateline, months[mi2]);
        int dl_w = (int)strlen(dateline) * 9;
        int dl_x = clk_seg_x + (clk_seg_w - dl_w) / 2;
        vga_bb_draw_string(dl_x, dock_y+28, dateline, S_TEXT_DIM, 0x00000000);

        uint32_t up_secs = timer_get_seconds();
        char ut[16]; char tmp2[8];
        strcpy(ut, "Up ");
        itoa(up_secs / 3600, tmp2, 10); strcat(ut, tmp2); strcat(ut, "h");
        itoa((up_secs % 3600) / 60, tmp2, 10); strcat(ut, tmp2); strcat(ut, "m");
        int ut_w = (int)strlen(ut) * 9;
        int ut_x = clk_seg_x + (clk_seg_w - ut_w) / 2;
        vga_bb_draw_string(ut_x, dock_y+40, ut, 0xFF4A5568, 0x00000000);
    }

    /* ════════════════════════════════════════════════════════
     *  Quick Settings Popup — Frosted Neon Style
     * ════════════════════════════════════════════════════════ */
    if (quick_settings_open) {
        int qx = dock_x + dock_w - 290, qy = dock_y - 320;
        int qw = 270, qh = 310;
        ui_soft_shadow(qx, qy, qw, qh, 14, 5);
        ui_frosted_panel(qx, qy, qw, qh, 14, S_BG, S_GLASS_BORDER);
        ui_neon_border(qx, qy, qw, qh, 14, S_NEON_CYAN);

        ui_section_header(qx+10, qy+12, qw-20, "Quick Settings", S_NEON_CYAN);

        int sy = qy + 42;
        vga_bb_fill_circle(qx+22, sy+8, 6, S_NEON_CYAN);
        vga_bb_fill_circle(qx+22, sy+8, 4, S_BG_DEEP);
        vga_bb_fill_rect(qx+19, sy+5, 3, 6, S_NEON_CYAN);
        vga_bb_draw_string_2x(qx+36, sy, "Volume", S_TEXT, 0x00000000);
        ui_progress_bar(qx+14, sy+22, qw-50, 8, 75, 100, S_NEON_CYAN, S_BG_ALT);
        vga_bb_draw_string(qx+qw-32, sy+18, "75%", S_TEXT_DIM, 0x00000000);

        sy += 40;
        vga_bb_fill_circle(qx+22, sy+8, 6, S_YELLOW);
        vga_bb_draw_circle(qx+22, sy+8, 9, S_YELLOW);
        vga_bb_draw_string_2x(qx+36, sy, "Brightness", S_TEXT, 0x00000000);
        ui_progress_bar(qx+14, sy+22, qw-50, 8, 90, 100, S_YELLOW, S_BG_ALT);
        vga_bb_draw_string(qx+qw-32, sy+18, "90%", S_TEXT_DIM, 0x00000000);

        vga_bb_draw_hline(qx+14, sy+38, qw-28, S_SEPARATOR);

        sy += 48;
        {
            net_status_t *ns = net_get_status();
            ui_card(qx+14, sy, 70, 54, 8, ns->connected ? S_ACCENT2 : S_BG_ALT);
            vga_bb_draw_circle(qx+49, sy+26, 10, ns->connected ? S_BG_DEEP : S_TEXT_DIM);
            vga_bb_draw_circle(qx+49, sy+26, 7, ns->connected ? S_BG_DEEP : S_TEXT_DIM);
            vga_bb_fill_circle(qx+49, sy+26, 3, ns->connected ? S_BG_DEEP : S_TEXT_DIM);
            vga_bb_fill_rect(qx+39, sy+27, 20, 12, ns->connected ? S_ACCENT2 : S_BG_ALT);
            vga_bb_draw_string(qx+24, sy+40, "Wi-Fi", ns->connected ? S_BG_DEEP : S_TEXT_DIM, 0x00000000);
        }
        ui_card(qx+94, sy, 70, 54, 8, S_BG_ALT);
        vga_bb_draw_string_2x(qx+118, sy+10, "B", S_BLUE, 0x00000000);
        vga_bb_draw_string(qx+104, sy+40, "BT", S_TEXT_DIM, 0x00000000);
        ui_card(qx+174, sy, 70, 54, 8, S_BG_ALT);
        vga_bb_fill_circle(qx+209, sy+20, 10, S_YELLOW);
        vga_bb_fill_circle(qx+214, sy+16, 10, S_BG_ALT);
        vga_bb_draw_string(qx+184, sy+40, "Night", S_TEXT_DIM, 0x00000000);

        vga_bb_draw_hline(qx+14, sy+62, qw-28, S_SEPARATOR);

        sy += 72;
        vga_bb_draw_string_2x(qx+14, sy, "Do Not Disturb", S_TEXT, 0x00000000);
        vga_bb_fill_rounded_rect(qx+qw-56, sy, 40, 20, 10, S_BG_LIGHT);
        vga_bb_fill_circle(qx+qw-56+12, sy+10, 8, S_TEXT_DIM);

        sy += 32;
        vga_bb_draw_hline(qx+14, sy-4, qw-28, S_SEPARATOR);
        {
            net_status_t *ns = net_get_status();
            ui_status_dot(qx+24, sy+10, 5, ns->connected ? S_GREEN : S_RED);
            vga_bb_draw_string_2x(qx+38, sy+2, ns->connected ? "Connected" : "No Connection", S_TEXT, 0x00000000);
            if (ns->connected)
                vga_bb_draw_string(qx+38, sy+20, ns->ip_addr, S_TEXT_DIM, 0x00000000);
        }

        sy += 34;
        {
            uint32_t ut_s = timer_get_seconds();
            char upstr[32]; char ubuf[8];
            strcpy(upstr, "Uptime: ");
            itoa(ut_s / 3600, ubuf, 10); strcat(upstr, ubuf); strcat(upstr, "h ");
            itoa((ut_s % 3600) / 60, ubuf, 10); strcat(upstr, ubuf); strcat(upstr, "m");
            vga_bb_draw_string(qx+14, sy+2, upstr, S_TEXT_DIM, 0x00000000);
        }

        /* Rearrange mode toggle button at bottom of popup */
        sy += 24;
        vga_bb_draw_hline(qx+14, sy-4, qw-28, S_SEPARATOR);
        {
            int rb_hover = (msx >= qx+14 && msx < qx+qw-14 && msy >= sy && msy < sy+24);
            vga_bb_fill_rounded_rect(qx+14, sy, qw-28, 24, 8,
                                     rearrange_mode ? S_NEON_MAGENTA : (rb_hover ? S_BG_HOVER : S_BG_ALT));
            const char *rlbl = rearrange_mode ? "Exit Rearrange" : "Rearrange Tray";
            int rlw = (int)strlen(rlbl) * CW;
            vga_bb_draw_string_2x(qx + (qw - rlw)/2, sy+4, rlbl,
                                  rearrange_mode ? S_BG_DEEP : S_TEXT, 0x00000000);
        }
    }
}


/* ── Context Menu ─────────────────────────────────────────── */
static int ctx_menu_open = 0;
static int ctx_menu_x = 0;
static int ctx_menu_y = 0;
#define CTX_W 240
#define CTX_ITEMS 4
#define CTX_ITEM_H 36
#define CTX_H (CTX_ITEMS * CTX_ITEM_H + 8)
static const char *ctx_labels[CTX_ITEMS] = { "LoveLetter", "Heartbeat", "SwanDraw", "Refresh" };

static void draw_context_menu(void) {
    if (!ctx_menu_open) return;
    ui_soft_shadow(ctx_menu_x, ctx_menu_y, CTX_W, CTX_H, 10, 4);
    ui_frosted_panel(ctx_menu_x, ctx_menu_y, CTX_W, CTX_H, 10, B_KICKOFF, S_GLASS_BORDER);
    ui_neon_border(ctx_menu_x, ctx_menu_y, CTX_W, CTX_H, 10, S_NEON_CYAN);
    mouse_state_t ms; mouse_get_state(&ms);
    for (int i=0; i<CTX_ITEMS; i++) {
        int iy = ctx_menu_y + 4 + i*CTX_ITEM_H;
        int hover = (ms.x >= ctx_menu_x+4 && ms.x < ctx_menu_x+CTX_W-4 && ms.y >= iy && ms.y < iy+CTX_ITEM_H);
        if (hover) {
            vga_bb_fill_rounded_rect(ctx_menu_x+4, iy+2, CTX_W-8, CTX_ITEM_H-4, 6, S_KICKOFF_HL);
            vga_bb_fill_rounded_rect(ctx_menu_x+4, iy+CTX_ITEM_H-4, CTX_W-8, 2, 1, S_NEON_CYAN);
        }
        /* Neon dot indicator */
        vga_bb_fill_circle(ctx_menu_x+16, iy+CTX_ITEM_H/2, 4, S_NEON_CYAN);
        vga_bb_fill_circle(ctx_menu_x+16, iy+CTX_ITEM_H/2, 2, S_BG_DEEP);
        vga_bb_draw_string_2x(ctx_menu_x+28, iy+10, ctx_labels[i], B_TEXT, 0x00000000);
    }
}

/* ── Kickoff Menu (Neon Aurora Style) ─────────────────────── */
static void draw_kickoff(void) {
    if (!kickoff_open) return;
    int mx_top = dock_y - KO_H - 14;
    int mx_left = kickoff_x;

    /* Frosted glass + neon border */
    ui_soft_shadow(mx_left, mx_top, KO_W, KO_H, 14, 5);
    ui_frosted_panel(mx_left, mx_top, KO_W, KO_H, 12, B_KICKOFF, S_GLASS_BORDER);
    ui_neon_border(mx_left, mx_top, KO_W, KO_H, 12, S_NEON_CYAN);

    /* Header card with user info */
    vga_bb_fill_rounded_rect(mx_left+8, mx_top+8, KO_W-16, KO_HEADER-16, 10, 0x18FFFFFF);
    /* Neon avatar ring */
    vga_bb_fill_circle_alpha(mx_left+36, mx_top+32, 18, S_GLOW_PULSE);
    vga_bb_fill_circle(mx_left+36, mx_top+32, 16, S_NEON_CYAN);
    vga_bb_fill_circle(mx_left+36, mx_top+32, 14, B_BG_DARK);
    vga_bb_draw_string_2x(mx_left+27, mx_top+27, user_current()[0] ? (char[]){user_current()[0],0} : "U", 0x80000000, 0);
    vga_bb_draw_string_2x(mx_left+26, mx_top+26, user_current()[0] ? (char[]){user_current()[0],0} : "U", S_NEON_CYAN, 0);
    /* Username */
    vga_bb_draw_string_2x(mx_left+63, mx_top+18, user_current(), 0x80000000, 0);
    vga_bb_draw_string_2x(mx_left+62, mx_top+17, user_current(), B_TEXT, 0);
    /* Login count badge */
    {
        user_profile_t *prof = user_get_profile();
        char login_info[32];
        char tmp_lc[8];
        strcpy(login_info, "Login #");
        itoa(prof->login_count, tmp_lc, 10);
        strcat(login_info, tmp_lc);
        vga_bb_draw_string_2x(mx_left+62, mx_top+35, login_info, S_YELLOW, 0);
        /* Session timer */
        uint32_t ss = user_session_seconds();
        uint32_t sm = ss / 60; uint32_t sh = sm / 60;
        ss %= 60; sm %= 60;
        char sess[20]; char stmp[8];
        strcpy(sess, "Session: ");
        itoa(sh, stmp, 10); strcat(sess, stmp); strcat(sess, "h");
        itoa(sm, stmp, 10); strcat(sess, stmp); strcat(sess, "m");
        vga_bb_draw_string_2x(mx_left+62, mx_top+52, sess, B_TEXT_DIM, 0);
    }

    /* Separator with neon accent */
    vga_bb_draw_hline(mx_left+12, mx_top + KO_HEADER - 4, KO_W-24, B_SEPARATOR);
    vga_bb_draw_hline(mx_left+12, mx_top + KO_HEADER - 3, 40, S_NEON_CYAN);

    /* Menu items */
    mouse_state_t ms; mouse_get_state(&ms);
    /* Per-item neon accent colors */
    uint32_t ko_colors[KO_ITEMS] = {S_NEON_PURPLE, S_GREEN, S_YELLOW, S_NEON_CYAN, S_PINK, S_RED, S_BLUE, S_GREEN, S_ORANGE, S_BLUE, S_NEON_MAGENTA, S_PINK, S_YELLOW, 0, S_RED};
    for (int i = 0; i < KO_ITEMS; i++) {
        int iy = mx_top + KO_HEADER + i * KO_ITEM_H;
        if (ko_labels[i][0] == '-') {
            vga_bb_draw_hline(mx_left+16, iy + KO_ITEM_H/2, KO_W - 32, B_SEPARATOR);
            continue;
        }
        int hover = (ms.x >= mx_left+4 && ms.x < mx_left+KO_W-4 &&
                     ms.y >= iy && ms.y < iy + KO_ITEM_H);
        if (hover) {
            vga_bb_fill_rounded_rect(mx_left+4, iy+2, KO_W-8, KO_ITEM_H-4, 6, S_KICKOFF_HL);
            /* Neon underline on hover */
            vga_bb_fill_rounded_rect(mx_left+8, iy+KO_ITEM_H-4, KO_W-16, 2, 1, ko_colors[i]);
        }

        /* Neon-colored icon dot */
        uint32_t ic = ko_colors[i];
        vga_bb_fill_circle(mx_left+24, iy + KO_ITEM_H/2, 6, ic);
        vga_bb_fill_circle(mx_left+24, iy + KO_ITEM_H/2, 4, B_BG_DARK);
        vga_bb_fill_circle(mx_left+24, iy + KO_ITEM_H/2, 2, ic);

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

    /* ── Open animation — scale up effect ── */
    uint32_t now_t = timer_get_ticks();
    int anim_elapsed = (int)(now_t - w->open_anim_tick);
    int anim_active = (anim_elapsed < 20); /* 200ms at 100Hz */
    if (anim_active) {
        /* Scale factor: 70% → 100% over 20 ticks */
        int scale = 700 + (anim_elapsed * 300) / 20; /* 700-1000 (0.7x to 1.0x) */
        if (scale > 1000) scale = 1000;
        /* Apply scale to dimensions — shrink from center */
        int orig_w = w->w, orig_h = w->h;
        int orig_x = w->x, orig_y = w->y;
        int nw = (orig_w * scale) / 1000;
        int nh = (orig_h * scale) / 1000;
        int nx = orig_x + (orig_w - nw) / 2;
        int ny = orig_y + (orig_h - nh) / 2;
        /* Temporarily adjust for drawing */
        w->x = nx; w->y = ny; w->w = nw; w->h = nh;
        /* Alpha overlay for fade-in */
        int alpha = (anim_elapsed * 255) / 20;
        if (alpha > 255) alpha = 255;
        (void)alpha; /* Used conceptually; VGA doesn't have per-window alpha */
        /* Draw at scaled size, then restore */
        ui_window_shadow(w->x, w->y, w->w, w->h);
        vga_bb_fill_rounded_rect(w->x, w->y, w->w, w->h, 10, B_WIN_BG);
        if (focused) ui_neon_border(w->x, w->y, w->w, w->h, 10, S_NEON_CYAN);
        else vga_bb_draw_rect_outline(w->x, w->y, w->w, w->h, B_BORDER);
        /* Restore original dims for next frame */
        w->x = orig_x; w->y = orig_y; w->w = orig_w; w->h = orig_h;
        return; /* Don't draw content during animation */
    }

    /* Multi-layer soft shadow */
    ui_window_shadow(w->x, w->y, w->w, w->h);

    /* Window body */
    vga_bb_fill_rounded_rect(w->x, w->y, w->w, w->h, 10, B_WIN_BG);

    /* Border — neon glow on focus with breathing pulse */
    if (focused) {
        /* Breathing neon border — alpha pulses */
        int breath = sine_approx((now_t * 4) % 360);
        int glow_a = 0x20 + (breath * 0x18) / 100;
        if (glow_a < 0x10) glow_a = 0x10;
        uint32_t glow_c = ((uint32_t)glow_a << 24) | (S_NEON_CYAN & 0x00FFFFFF);
        vga_bb_draw_rect_outline(w->x-1, w->y-1, w->w+2, w->h+2, glow_c);
        ui_neon_border(w->x, w->y, w->w, w->h, 10, S_NEON_CYAN);
    } else {
        vga_bb_draw_rect_outline(w->x, w->y, w->w, w->h, B_BORDER);
    }
    /* Inner top highlight for depth */
    vga_bb_draw_hline(w->x+10, w->y+1, w->w-20, S_GLASS_BORDER);

    /* Title bar — gradient on focus with shimmer effect */
    if (focused) {
        vga_bb_fill_rounded_rect_gradient(w->x+1, w->y+1, w->w-2, TITLEBAR_H, 0, S_TITLEBAR_F, S_BG_DARK);
        /* Shimmer sweep across title bar */
        int shimmer_x = (int)((now_t * 3) % (uint32_t)(w->w + 60)) - 30;
        for (int sx = shimmer_x - 15; sx < shimmer_x + 15 && sx < w->w - 4; sx++) {
            if (sx < 2) continue;
            int dist = sx - shimmer_x; if (dist < 0) dist = -dist;
            int sa = 0x12 - (dist * 0x12) / 15;
            if (sa > 0) {
                uint32_t sc = ((uint32_t)sa << 24) | 0x00FFFFFF;
                vga_bb_putpixel(w->x + 1 + sx, w->y + 2, sc);
                vga_bb_putpixel(w->x + 1 + sx, w->y + 3, sc);
            }
        }
    } else {
        vga_bb_fill_rect(w->x+1, w->y+1, w->w-2, TITLEBAR_H, B_TITLEBAR);
    }
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
        /* Swan emblem */
        vga_bb_fill_circle(cx+cw/2, ay+30, 28, S_NEON_CYAN);
        vga_bb_fill_circle(cx+cw/2, ay+30, 24, B_BG_DARK);
        vga_bb_draw_string_2x(cx+cw/2-7, ay+23, "S", S_NEON_CYAN, 0x00000000);
        ay += 66;
        vga_bb_draw_string_2x(cx+cw/2-3*CW, ay, "SwanOS", S_NEON_CYAN, 0x00000000);
        ay += CH+4;
        vga_bb_draw_string_2x(cx+cw/2-5*CW, ay, "Version 3.0", B_TEXT, 0x00000000);
        ay += CH+6;
        vga_bb_draw_hline(cx+20, ay, cw-40, B_SEPARATOR);
        ay += 10;
        /* Swan philosophy */
        vga_bb_draw_string_2x(cx+16, ay, "The Swan - Symbol of", S_YELLOW, 0x00000000);
        ay += CH+4;
        vga_bb_draw_string_2x(cx+16, ay, "ultimate love, grace", S_YELLOW, 0x00000000);
        ay += CH+4;
        vga_bb_draw_string_2x(cx+16, ay, "and eternal devotion.", S_YELLOW, 0x00000000);
        ay += CH+10;
        vga_bb_draw_string_2x(cx+16, ay, "Where Love Meets", S_NEON_PURPLE, 0x00000000);
        ay += CH+4;
        vga_bb_draw_string_2x(cx+16, ay, "Technology.", S_NEON_PURPLE, 0x00000000);
        ay += CH+10;
        vga_bb_draw_hline(cx+20, ay, cw-40, B_SEPARATOR);
        ay += 8;
        vga_bb_draw_string_2x(cx+16, ay, "AI-Powered  Secure", B_GREEN, 0x00000000);
        ay += CH+4;
        vga_bb_draw_string_2x(cx+16, ay, "Bare-Metal x86", B_TEXT, 0x00000000);
        ay += CH+4;
        char res[20]; itoa(GFX_W, res, 10); strcat(res, "x");
        char t2[8]; itoa(GFX_H, t2, 10); strcat(res, t2);
        vga_bb_draw_string_2x(cx+16, ay, res, B_TEXT_DIM, 0x00000000);
        ay += CH+4;
        char mb[20]; char tmp[8];
        strcpy(mb, "Mem: "); itoa(mem_total()/1024, tmp, 10); strcat(mb, tmp); strcat(mb, "K");
        vga_bb_draw_string_2x(cx+16, ay, mb, B_TEXT_DIM, 0x00000000);
        ay += CH+6;
        vga_bb_draw_string_2x(cx+16, ay, "User:", B_TEXT_DIM, 0x00000000);
        vga_bb_draw_string_2x(cx+16+6*CW, ay, user_current(), B_ACCENT, 0x00000000);
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
    else if (w->type == WIN_AUDIT) {
        vga_bb_fill_rect(cx, cy, cw, ch, B_BG_DARK);
        /* Header */
        vga_bb_fill_rect(cx, cy, cw, CH+8, B_BG_ALT);
        vga_bb_fill_circle(cx+20, cy+12, 6, S_NEON_MAGENTA);
        vga_bb_draw_string_2x(cx+32, cy+4, "SwanWatch", S_NEON_MAGENTA, 0x00000000);
        /* Event count badge */
        char ecnt[12]; char etmp[8];
        int ac = audit_get_count();
        strcpy(ecnt, ""); itoa(ac, etmp, 10); strcat(ecnt, etmp); strcat(ecnt, " events");
        int ecnw = (int)strlen(ecnt) * CW;
        vga_bb_fill_rounded_rect(cx+cw-ecnw-20, cy+2, ecnw+12, CH+4, 6, S_NEON_MAGENTA);
        vga_bb_draw_string_2x(cx+cw-ecnw-14, cy+4, ecnt, B_BG_DARK, 0x00000000);
        vga_bb_draw_hline(cx, cy+CH+8, cw, B_SEPARATOR);
        /* Event list */
        int ey = cy + CH + 14;
        int total = ac;
        if (total > AUDIT_MAX_ENTRIES) total = AUDIT_MAX_ENTRIES;
        int max_visible = (ch - CH - 20) / (CH + 6);
        int start = total > max_visible ? total - max_visible : 0;
        for (int i = start; i < total && ey + CH + 4 < cy + ch; i++) {
            const audit_entry_t *e = audit_get_entry(i);
            if (!e || !e->used) continue;
            /* Alternating row background */
            if ((i - start) % 2 == 0)
                vga_bb_fill_rect(cx+2, ey-2, cw-4, CH+4, 0x08FFFFFF);
            /* Timestamp */
            char ts[12]; char tt[4];
            strcpy(ts, "");
            if (e->hour < 10) strcat(ts, "0");
            itoa(e->hour, tt, 10); strcat(ts, tt); strcat(ts, ":");
            if (e->minute < 10) strcat(ts, "0");
            itoa(e->minute, tt, 10); strcat(ts, tt); strcat(ts, ":");
            if (e->second < 10) strcat(ts, "0");
            itoa(e->second, tt, 10); strcat(ts, tt);
            vga_bb_draw_string_2x(cx+8, ey, ts, B_TEXT_DIM, 0x00000000);
            /* Event type with color */
            uint32_t ec = S_TEXT;
            switch (e->type) {
                case AUDIT_LOGIN:       ec = S_GREEN; break;
                case AUDIT_LOGOUT:      ec = S_YELLOW; break;
                case AUDIT_FILE_CREATE: ec = S_NEON_CYAN; break;
                case AUDIT_FILE_DELETE: ec = S_RED; break;
                case AUDIT_APP_OPEN:    ec = S_BLUE; break;
                case AUDIT_APP_CLOSE:   ec = S_TEXT_DIM; break;
                case AUDIT_COMMAND:     ec = S_TEXT; break;
                case AUDIT_FILE_WRITE:  ec = S_GREEN; break;
                case AUDIT_SYSTEM:      ec = S_NEON_PURPLE; break;
            }
            /* Type badge */
            const char *tn = audit_type_name(e->type);
            int tw2 = (int)strlen(tn) * CW;
            vga_bb_fill_rounded_rect(cx+8*CW+16, ey-1, tw2+8, CH+2, 4, ec);
            vga_bb_draw_string_2x(cx+8*CW+20, ey, tn, B_BG_DARK, 0x00000000);
            /* User */
            int ux = cx + 8*CW + tw2 + 32;
            vga_bb_draw_string_2x(ux, ey, e->user, S_NEON_CYAN, 0x00000000);
            /* Detail */
            if (e->detail[0]) {
                int dx = ux + ((int)strlen(e->user)+1) * CW;
                int max_dc = (cx + cw - dx - 8) / CW;
                if (max_dc > 0) {
                    char dtrunc[40];
                    strncpy(dtrunc, e->detail, max_dc > 39 ? 39 : max_dc);
                    dtrunc[max_dc > 39 ? 39 : max_dc] = '\0';
                    vga_bb_draw_string_2x(dx, ey, dtrunc, B_TEXT_DIM, 0x00000000);
                }
            }
            ey += CH + 6;
        }
    }
    else if (w->type == WIN_DRAW) {
        /* SwanDraw — Pixel Art Canvas */
        vga_bb_fill_rect(cx, cy, cw, ch, B_BG_DARK);
        /* Color palette bar */
        static const uint32_t draw_colors[8] = {
            0xFFFFFFFF, 0xFF000000, 0xFFFF4444, 0xFF44FF44,
            0xFF4488FF, 0xFFFFFF44, 0xFFFF44FF, 0xFF44FFFF
        };
        int palette_y = cy + 4;
        for (int ci = 0; ci < 8; ci++) {
            int px = cx + 8 + ci * 34;
            vga_bb_fill_rounded_rect(px, palette_y, 28, 28, 4, draw_colors[ci]);
            if (ci == w->draw_color) {
                vga_bb_draw_rect_outline(px-2, palette_y-2, 32, 32, S_NEON_CYAN);
                vga_bb_draw_rect_outline(px-1, palette_y-1, 30, 30, S_NEON_CYAN);
            }
        }
        /* Clear button */
        vga_bb_fill_rounded_rect(cx+8+8*34+10, palette_y, 60, 28, 6, S_RED);
        vga_bb_draw_string_2x(cx+8+8*34+16, palette_y+6, "CLR", B_BG_DARK, 0x00000000);
        vga_bb_draw_hline(cx, palette_y+34, cw, B_SEPARATOR);
        /* Canvas area */
        int canvas_x = cx + 8, canvas_y = palette_y + 40;
        int canvas_w = cw - 16, canvas_h = ch - 52;
        vga_bb_fill_rect(canvas_x, canvas_y, canvas_w, canvas_h, 0xFF202020);
        vga_bb_draw_rect_outline(canvas_x, canvas_y, canvas_w, canvas_h, B_BORDER);
        /* Draw stored pixel data from note_text buffer (used as pixel grid) */
        /* Grid: 64 x 48 cells, each byte stores color index */
        int cell_w = canvas_w / 64;
        int cell_h = canvas_h / 48;
        if (cell_w < 2) cell_w = 2;
        if (cell_h < 2) cell_h = 2;
        for (int py = 0; py < 48; py++) {
            for (int px = 0; px < 64; px++) {
                int idx = py * 64 + px;
                if (idx >= 512) break;
                int ci = (uint8_t)w->note_text[idx];
                if (ci > 0 && ci <= 8) {
                    vga_bb_fill_rect(canvas_x + px*cell_w, canvas_y + py*cell_h,
                                     cell_w, cell_h, draw_colors[ci-1]);
                }
            }
        }
        /* Grid lines (subtle) */
        for (int gx = 0; gx <= 64; gx += 8)
            vga_bb_draw_vline(canvas_x + gx*cell_w, canvas_y, canvas_h, 0x20FFFFFF);
        for (int gy = 0; gy <= 48; gy += 8)
            vga_bb_draw_hline(canvas_x, canvas_y + gy*cell_h, canvas_w, 0x20FFFFFF);
        /* Status */
        vga_bb_draw_string_2x(cx+8, cy+ch-CH-4, "Click to paint", B_TEXT_DIM, 0x00000000);
    }
    else if (w->type == WIN_CLOCK) {
        /* SwanClock — Stopwatch + Clock */
        vga_bb_fill_rect(cx, cy, cw, ch, B_BG_DARK);
        /* Large analog clock face */
        int clock_cx = cx + cw/2;
        int clock_cy = cy + 120;
        int clock_r = 90;
        vga_bb_fill_circle(clock_cx, clock_cy, clock_r+4, S_NEON_CYAN);
        vga_bb_fill_circle(clock_cx, clock_cy, clock_r, B_BG_DARK);
        /* Hour markers */
        for (int h = 0; h < 12; h++) {
            int deg = h * 30;
            int mx2 = clock_cx + (sine_approx((deg+90)%360) * (clock_r-10)) / 100;
            int my2 = clock_cy - (sine_approx(deg%360) * (clock_r-10)) / 100;
            vga_bb_fill_circle(mx2, my2, 3, S_TEXT);
        }
        /* Current time hands */
        rtc_time_t ct; rtc_read(&ct);
        /* Hour hand */
        int h_deg = ((ct.hour % 12) * 30 + ct.minute / 2);
        int hx = clock_cx + (sine_approx((h_deg+90)%360) * 50) / 100;
        int hy = clock_cy - (sine_approx(h_deg%360) * 50) / 100;
        vga_bb_fill_rect(clock_cx < hx ? clock_cx : hx, clock_cy < hy ? clock_cy : hy,
                         (hx > clock_cx ? hx-clock_cx : clock_cx-hx) + 3,
                         (hy > clock_cy ? hy-clock_cy : clock_cy-hy) + 3, S_TEXT);
        /* Minute hand */
        int m_deg = ct.minute * 6;
        int mmx = clock_cx + (sine_approx((m_deg+90)%360) * 70) / 100;
        int mmy = clock_cy - (sine_approx(m_deg%360) * 70) / 100;
        vga_bb_fill_rect(clock_cx < mmx ? clock_cx : mmx, clock_cy < mmy ? clock_cy : mmy,
                         (mmx > clock_cx ? mmx-clock_cx : clock_cx-mmx) + 2,
                         (mmy > clock_cy ? mmy-clock_cy : clock_cy-mmy) + 2, S_NEON_CYAN);
        /* Center dot */
        vga_bb_fill_circle(clock_cx, clock_cy, 4, S_YELLOW);
        /* Digital time below */
        char clkbuf[10]; rtc_format_time(&ct, clkbuf);
        int clkw = (int)strlen(clkbuf) * CW;
        vga_bb_draw_string_2x(clock_cx - clkw/2, clock_cy + clock_r + 16, clkbuf, S_TEXT, 0x00000000);
        /* Separator */
        int sep_y = clock_cy + clock_r + 40;
        vga_bb_draw_hline(cx+20, sep_y, cw-40, B_SEPARATOR);
        /* Stopwatch section */
        int sw_y = sep_y + 12;
        vga_bb_draw_string_2x(cx+cw/2-5*CW, sw_y, "Stopwatch", S_NEON_PURPLE, 0x00000000);
        sw_y += CH + 8;
        /* Display stopwatch time */
        int sw_ticks = (int)w->clock_sw_elapsed;
        if (w->clock_sw_running) sw_ticks += (int)(timer_get_ticks() - w->clock_sw_start);
        int sw_sec = sw_ticks / 100;
        int sw_min = sw_sec / 60;
        int sw_cs = sw_ticks % 100;
        sw_sec %= 60;
        char swbuf[20]; char stmp[8];
        strcpy(swbuf, "");
        if (sw_min < 10) strcat(swbuf, "0");
        itoa(sw_min, stmp, 10); strcat(swbuf, stmp); strcat(swbuf, ":");
        if (sw_sec < 10) strcat(swbuf, "0");
        itoa(sw_sec, stmp, 10); strcat(swbuf, stmp); strcat(swbuf, ".");
        if (sw_cs < 10) strcat(swbuf, "0");
        itoa(sw_cs, stmp, 10); strcat(swbuf, stmp);
        int sww = (int)strlen(swbuf) * CW;
        vga_bb_draw_string_2x(cx+cw/2-sww/2, sw_y, swbuf, S_TEXT, 0x00000000);
        sw_y += CH + 12;
        /* Buttons: Start/Stop | Reset */
        mouse_state_t cms; mouse_get_state(&cms);
        int btn_w = 100, btn_h = 32;
        int btn1_x = cx + cw/2 - btn_w - 10;
        int btn2_x = cx + cw/2 + 10;
        int b1_hover = (cms.x >= btn1_x && cms.x < btn1_x+btn_w && cms.y >= sw_y && cms.y < sw_y+btn_h);
        int b2_hover = (cms.x >= btn2_x && cms.x < btn2_x+btn_w && cms.y >= sw_y && cms.y < sw_y+btn_h);
        const char *b1_lbl = w->clock_sw_running ? "Stop" : "Start";
        uint32_t b1_c = w->clock_sw_running ? S_RED : S_GREEN;
        vga_bb_fill_rounded_rect(btn1_x, sw_y, btn_w, btn_h, 8, b1_hover ? b1_c : (b1_c & 0xC0FFFFFF));
        int b1w = (int)strlen(b1_lbl) * CW;
        vga_bb_draw_string_2x(btn1_x+(btn_w-b1w)/2, sw_y+8, b1_lbl, B_BG_DARK, 0x00000000);
        vga_bb_fill_rounded_rect(btn2_x, sw_y, btn_w, btn_h, 8, b2_hover ? S_YELLOW : 0xC0999900);
        int b2w = (int)strlen("Reset") * CW;
        vga_bb_draw_string_2x(btn2_x+(btn_w-b2w)/2, sw_y+8, "Reset", B_BG_DARK, 0x00000000);
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
    audit_log(AUDIT_APP_CLOSE, windows[wi].title);
    windows[wi].active=0;
    int pos=-1;
    for (int i=0;i<win_order_count;i++) if (win_order[i]==wi) {pos=i;break;}
    if (pos>=0) { for (int i=pos;i<win_order_count-1;i++) win_order[i]=win_order[i+1]; win_order_count--; }
    win_focus=(win_order_count>0)?win_order[win_order_count-1]:-1;
}

static void open_window(int type) {
    for (int i=0;i<MAX_WINDOWS;i++) { if (windows[i].active && windows[i].type==type && windows[i].workspace==current_workspace) { bring_to_front(i); return; } }
    int wi=find_free_window(); if (wi<0) return;
    window_t *w=&windows[wi]; memset(w,0,sizeof(window_t)); w->active=1; w->type=type; w->workspace=current_workspace;
    switch (type) {
        case WIN_TERM: strcpy(w->title,"SwanShell"); w->x=280;w->y=120;w->w=820;w->h=520;
            term_add_line(w,"SwanShell Terminal"); term_add_line(w,"Type 'help'"); break;
        case WIN_FILES: strcpy(w->title,"Feather"); w->x=320;w->y=180;w->w=820;w->h=520; w->file_sel=0; break;
        case WIN_NOTES: strcpy(w->title,"LoveLetter"); w->x=360;w->y=220;w->w=820;w->h=520;
            strcpy(w->note_file,"notes.txt"); fs_read("notes.txt",w->note_text,sizeof(w->note_text)-1);
            w->note_len=strlen(w->note_text); w->note_cursor=w->note_len; break;
        case WIN_ABOUT: strcpy(w->title,"SwanSong"); w->x=420;w->y=200;w->w=620;w->h=520; break;
        case WIN_AI: strcpy(w->title,"SwanSoul"); w->x=240;w->y=80;w->w=840;w->h=620;
            term_add_line(w,"SwanSoul AI"); term_add_line(w,"Ask me anything!"); break;
        case WIN_CALC: strcpy(w->title,"SwanCount"); w->x=500;w->y=160;w->w=320;w->h=460; w->input[0]='0'; w->input[1]='\0'; w->calc_new=1; break;
        case WIN_SYSMON: strcpy(w->title,"Heartbeat"); w->x=600;w->y=300;w->w=560;w->h=380; w->sysmon_head=0; memset(w->sysmon_history,0,sizeof(w->sysmon_history)); break;
        case WIN_STORE: strcpy(w->title,"SwanNest"); w->x=200;w->y=60;w->w=860;w->h=560; w->store_sel=0; memset(w->store_downloaded,0,sizeof(w->store_downloaded)); break;
        case WIN_BROWSER: strcpy(w->title,"SwanLake"); w->x=180;w->y=40;w->w=900;w->h=640; w->browser_tab=0; break;
        case WIN_NETWORK: strcpy(w->title,"WingLink"); w->x=300;w->y=100;w->w=700;w->h=580; break;
        case WIN_AUDIT: strcpy(w->title,"SwanWatch"); w->x=260;w->y=120;w->w=820;w->h=520; break;
        case WIN_DRAW: strcpy(w->title,"SwanDraw"); w->x=200;w->y=80;w->w=680;w->h=520;
            w->draw_color=0; memset(w->draw_canvas,0,sizeof(w->draw_canvas)); break;
        case WIN_CLOCK: strcpy(w->title,"SwanClock"); w->x=500;w->y=120;w->w=400;w->h=460;
            w->clock_sw_running=0; w->clock_sw_start=0; w->clock_sw_elapsed=0; break;
    }
    w->open_anim_tick = timer_get_ticks();  /* Start open animation */
    win_order[win_order_count++]=wi; win_focus=wi;
    /* Audit log the app open */
    audit_log(AUDIT_APP_OPEN, w->title);
    toast_show(w->title, icon_accent(type));
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
            else if (idx == 2) open_window(WIN_DRAW);
            else if (idx == 3) wp_cached = 0;
        }
        ctx_menu_open = 0; return 0;
    }
    /* Workspace indicator clicks (top-left) */
    if (my >= 40 && my < 80 && mx >= 40 && mx < 200) {
        int idx = (mx - 40) / 53;
        if (idx >= 0 && idx < MAX_WORKSPACES && idx != current_workspace) {
            current_workspace = idx;
            win_focus = -1;
            for (int zi = win_order_count-1; zi >= 0; zi--) {
                int wi = win_order[zi];
                if (windows[wi].active && windows[wi].workspace == current_workspace) { win_focus = wi; break; }
            }
            return 0;
        }
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
    /* Panel launcher pill */
    if (my>=dock_y && my<dock_y+PANEL_H && mx>=dock_x+4 && mx<dock_x+60) { kickoff_open=!kickoff_open; quick_settings_open=0; return 0; }
    /* Quick settings rearrange button click */
    if (quick_settings_open) {
        int qx = dock_x + dock_w - 290, qy = dock_y - 320;
        int qw = 270;
        /* Compute rearrange button Y (same as draw_panel calculation) */
        int rby = qy + 42 + 40 + 48 + 72 + 32 + 34 + 24;
        if (mx >= qx+14 && mx < qx+qw-14 && my >= rby && my < rby+24) {
            rearrange_mode = !rearrange_mode;
            return 0;
        }
    }
    /* Panel clicks */
    if (my>=dock_y && my<dock_y+PANEL_H) {
        /* Clock pill → toggle quick settings (rightmost ~136px) */
        if (mx >= dock_x + dock_w - 136) { quick_settings_open=!quick_settings_open; return 0; }
        /* Network tray region */
        int tray_r2 = dock_x + dock_w - 140;
        int t_start = tray_r2 - 360;
        int net_region = t_start + 8 + 48 + 46 + 15;
        if (mx >= net_region && mx < net_region + 44 && !rearrange_mode) { open_window(WIN_NETWORK); return 0; }
        /* Task buttons */
        int launcher_w2 = 56;
        int bx=dock_x + 4 + launcher_w2 + 6 + 8; /* Match draw_panel tasks_x + 8 */
        for (int i=0;i<win_order_count;i++) { int wi=win_order[i]; if(!windows[wi].active) continue;
            int bw=(int)strlen(windows[wi].title)*CW+32; if(bw>200) bw=200;
            if (mx>=bx&&mx<bx+bw) { bring_to_front(wi); return 0; } bx+=bw+6; }
        return 0;
    }
    /* Close quick settings on click outside */
    if (quick_settings_open) { quick_settings_open=0; }
    /* Windows (top z first) */
    for (int zi=win_order_count-1;zi>=0;zi--) {
        int wi=win_order[zi]; window_t *w=&windows[wi]; if(!w->active || w->workspace != current_workspace) continue;
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
            /* SwanDraw click — palette, clear, and canvas painting */
            if (w->type == WIN_DRAW) {
                int dcx = w->x+4, dcy = w->y+TITLEBAR_H+4, dcw = w->w-8;
                int palette_y2 = dcy + 4;
                /* Palette color selection */
                for (int ci = 0; ci < 8; ci++) {
                    int ppx = dcx + 8 + ci * 34;
                    if (mx >= ppx && mx < ppx+28 && my >= palette_y2 && my < palette_y2+28) {
                        w->draw_color = ci;
                        return 0;
                    }
                }
                /* Clear button */
                int clr_x = dcx + 8 + 8*34 + 10;
                if (mx >= clr_x && mx < clr_x+60 && my >= palette_y2 && my < palette_y2+28) {
                    memset(w->note_text, 0, 512);
                    return 0;
                }
                /* Canvas painting */
                int cv_x = dcx + 8, cv_y = palette_y2 + 40;
                int cv_w = dcw - 16, cv_h = w->h - TITLEBAR_H - 8 - 52;
                int cell_w2 = cv_w / 64; if (cell_w2 < 2) cell_w2 = 2;
                int cell_h2 = cv_h / 48; if (cell_h2 < 2) cell_h2 = 2;
                if (mx >= cv_x && mx < cv_x + 64*cell_w2 && my >= cv_y && my < cv_y + 48*cell_h2) {
                    int px2 = (mx - cv_x) / cell_w2;
                    int py2 = (my - cv_y) / cell_h2;
                    if (px2 >= 0 && px2 < 64 && py2 >= 0 && py2 < 48) {
                        int idx2 = py2 * 64 + px2;
                        if (idx2 < 512) w->note_text[idx2] = (char)(w->draw_color + 1);
                    }
                    return 0;
                }
            }
            /* SwanClock click — stopwatch start/stop/reset */
            if (w->type == WIN_CLOCK) {
                int ccx = w->x+4, ccy = w->y+TITLEBAR_H+4, ccw = w->w-8;
                int clock_r2 = 90;
                int sep_y2 = ccy + 120 + clock_r2 + 40;
                int sw_y2 = sep_y2 + 12 + CH + 8 + CH + 12;
                int btn_w2 = 100, btn_h2 = 32;
                int btn1_x2 = ccx + ccw/2 - btn_w2 - 10;
                int btn2_x2 = ccx + ccw/2 + 10;
                /* Start/Stop button */
                if (mx >= btn1_x2 && mx < btn1_x2+btn_w2 && my >= sw_y2 && my < sw_y2+btn_h2) {
                    if (w->clock_sw_running) {
                        /* Stop: accumulate elapsed */
                        w->clock_sw_elapsed += timer_get_ticks() - w->clock_sw_start;
                        w->clock_sw_running = 0;
                    } else {
                        /* Start */
                        w->clock_sw_start = timer_get_ticks();
                        w->clock_sw_running = 1;
                    }
                    return 0;
                }
                /* Reset button */
                if (mx >= btn2_x2 && mx < btn2_x2+btn_w2 && my >= sw_y2 && my < sw_y2+btn_h2) {
                    w->clock_sw_running = 0;
                    w->clock_sw_elapsed = 0;
                    w->clock_sw_start = 0;
                    return 0;
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
            /* SwanDraw — canvas click */
            if (w->type == WIN_DRAW) {
                int dcx = w->x+4, dcy = w->y+TITLEBAR_H+4;
                int dcw = w->w-8;
                int palette_y = dcy + 4;
                for (int ci = 0; ci < 8; ci++) {
                    int px2 = dcx + 8 + ci * 34;
                    if (mx >= px2 && mx < px2+28 && my >= palette_y && my < palette_y+28) {
                        w->calc_v1 = ci; return 0;
                    }
                }
                if (mx >= dcx+8+8*34+10 && mx < dcx+8+8*34+70 && my >= palette_y && my < palette_y+28) {
                    memset(w->note_text, 0, 512); return 0;
                }
                int canvas_x = dcx + 8, canvas_y = palette_y + 40;
                int canvas_w = dcw - 16, canvas_h = (w->h - TITLEBAR_H - 8) - 52;
                int cell_w = canvas_w / 64; int cell_h = canvas_h / 48;
                if (cell_w < 2) cell_w = 2; if (cell_h < 2) cell_h = 2;
                if (mx >= canvas_x && mx < canvas_x+canvas_w && my >= canvas_y && my < canvas_y+canvas_h) {
                    int gx2 = (mx - canvas_x) / cell_w;
                    int gy2 = (my - canvas_y) / cell_h;
                    if (gx2 >= 0 && gx2 < 64 && gy2 >= 0 && gy2 < 48) {
                        int idx = gy2 * 64 + gx2;
                        if (idx < 512) w->note_text[idx] = (char)(w->calc_v1 + 1);
                    }
                    return 0;
                }
            }
            /* SwanClock — button clicks */
            if (w->type == WIN_CLOCK) {
                int ccx2 = w->x+4, ccw2 = w->w-8;
                int clock_cy2 = w->y+TITLEBAR_H+4 + 120;
                int sep_y2 = clock_cy2 + 90 + 40;
                int sw_y2 = sep_y2 + 12 + CH + 8 + CH + 12;
                int bw2 = 100, bh2 = 32;
                int btn1x = ccx2 + ccw2/2 - bw2 - 10;
                int btn2x = ccx2 + ccw2/2 + 10;
                if (mx >= btn1x && mx < btn1x+bw2 && my >= sw_y2 && my < sw_y2+bh2) {
                    w->calc_op = !w->calc_op; return 0;
                }
                if (mx >= btn2x && mx < btn2x+bw2 && my >= sw_y2 && my < sw_y2+bh2) {
                    w->calc_v1 = 0; w->calc_op = 0; return 0;
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

/* ── Draw Notification Toasts ────────────────────────── */
static void draw_toasts(void) {
    uint32_t now = timer_get_ticks();
    int ty = 20;
    for (int i = 0; i < MAX_TOASTS; i++) {
        if (!toasts[i].active) continue;
        uint32_t elapsed = now - toasts[i].start_tick;
        if (elapsed > TOAST_DURATION) {
            toasts[i].active = 0;
            continue;
        }
        /* Fade calculation */
        int alpha = 0xFF;
        if (elapsed > TOAST_DURATION - 60) {
            alpha = ((TOAST_DURATION - elapsed) * 0xFF) / 60;
        }
        int tw2 = (int)strlen(toasts[i].text) * CW + 40;
        int tx = SCRW - tw2 - 20;
        /* Toast card with neon accent */
        uint32_t bg_a = (((uint32_t)alpha * 0xD0) / 0xFF) << 24 | (S_BG & 0x00FFFFFF);
        vga_bb_fill_rounded_rect(tx, ty, tw2, 36, 10, bg_a);
        vga_bb_fill_rounded_rect(tx, ty, 4, 36, 2, toasts[i].color);
        /* Icon dot */
        vga_bb_fill_circle(tx+16, ty+18, 5, toasts[i].color);
        vga_bb_fill_circle(tx+16, ty+18, 3, S_BG_DEEP);
        /* Text */
        vga_bb_draw_string_2x(tx+28, ty+10, toasts[i].text, S_TEXT, 0x00000000);
        ty += 44;
    }
}

/* ── Widgets ──────────────────────────────────────────── */
static void draw_widgets(void) {
    rtc_time_t t; rtc_read(&t);

    /* Greeting Widget — dynamic based on time of day */
    {
        int gx = SCRW - 260, gy = 20;
        ui_card(gx, gy, 240, 52, 14, 0x30FFFFFF);
        /* Determine greeting and color */
        const char *greeting;
        uint32_t gc;
        if (t.hour < 6) { greeting = "Good Night"; gc = S_NEON_PURPLE; }
        else if (t.hour < 12) { greeting = "Good Morning"; gc = S_YELLOW; }
        else if (t.hour < 17) { greeting = "Good Afternoon"; gc = S_ORANGE; }
        else if (t.hour < 21) { greeting = "Good Evening"; gc = S_NEON_CYAN; }
        else { greeting = "Good Night"; gc = S_NEON_PURPLE; }
        vga_bb_fill_rounded_rect(gx, gy, 4, 52, 2, gc);
        vga_bb_draw_string_2x(gx+14, gy+8, greeting, gc, 0);
        /* Username */
        char gname[24];
        strcpy(gname, "");
        strcat(gname, user_current());
        vga_bb_draw_string_2x(gx+14, gy+28, gname, S_TEXT_DIM, 0);
    }

    /* Clock Widget */
    int cx = SCRW - 180, cy = 86;
    ui_card(cx, cy, 160, 140, 12, 0x35FFFFFF);
    ui_neon_border(cx, cy, 160, 140, 12, S_NEON_CYAN);
    vga_bb_fill_circle(cx+80, cy+60, 50, S_BG_DARK);
    vga_bb_draw_circle(cx+80, cy+60, 50, S_NEON_CYAN);
    char tbuf[16]; rtc_format_time(&t, tbuf);
    vga_bb_draw_string_2x(cx+30, cy+50, tbuf, S_TEXT, 0);
    /* Date under clock */
    char datebuf[12]; rtc_format_date(&t, datebuf);
    vga_bb_draw_string(cx+40, cy+110, datebuf, S_TEXT_DIM, 0);

    /* Stats Widget with session timer */
    int sx = SCRW - 180, sy = 240;
    ui_card(sx, sy, 160, 150, 12, 0x35FFFFFF);
    ui_neon_border(sx, sy, 160, 150, 12, S_NEON_PURPLE);
    vga_bb_draw_string_2x(sx+16, sy+12, "SYSTEM", S_TEXT, 0);
    int pct = (mem_total() > 0) ? (mem_used() * 100) / mem_total() : 0;
    vga_bb_draw_string(sx+16, sy+36, "Memory", S_TEXT_DIM, 0);
    ui_progress_bar(sx+16, sy+50, 128, 10, pct, 100, S_NEON_PURPLE, S_BG_DEEP);
    char mbuf[32]; strcpy(mbuf, ""); char nb[10]; itoa(mem_used()/1024, nb, 10); strcat(mbuf, nb); strcat(mbuf, " / ");
    itoa(mem_total()/1024, nb, 10); strcat(mbuf, nb); strcat(mbuf, " KB");
    vga_bb_draw_string(sx+16, sy+66, mbuf, S_TEXT_DIM, 0);
    /* Session timer */
    vga_bb_draw_hline(sx+16, sy+82, 128, S_SEPARATOR);
    vga_bb_draw_string(sx+16, sy+90, "Session", S_TEXT_DIM, 0);
    {
        uint32_t ss = user_session_seconds();
        uint32_t sm = ss / 60; uint32_t sh = sm / 60;
        ss %= 60; sm %= 60;
        char stbuf[20]; char stmp[8];
        strcpy(stbuf, "");
        itoa(sh, stmp, 10); strcat(stbuf, stmp); strcat(stbuf, "h ");
        itoa(sm, stmp, 10); strcat(stbuf, stmp); strcat(stbuf, "m ");
        itoa(ss, stmp, 10); strcat(stbuf, stmp); strcat(stbuf, "s");
        vga_bb_draw_string_2x(sx+16, sy+104, stbuf, S_NEON_CYAN, 0);
    }
    /* Audit event count */
    vga_bb_draw_hline(sx+16, sy+124, 128, S_SEPARATOR);
    {
        char abuf[20]; char atmp[8];
        strcpy(abuf, ""); itoa(audit_get_count(), atmp, 10); strcat(abuf, atmp); strcat(abuf, " events");
        vga_bb_draw_string(sx+16, sy+132, abuf, S_NEON_MAGENTA, 0);
    }

    /* Security Shield Widget */
    {
        int shx = SCRW - 180, shy = 404;
        ui_card(shx, shy, 160, 110, 12, 0x35FFFFFF);
        ui_neon_border(shx, shy, 160, 110, 12, S_GREEN);
        /* Shield icon */
        vga_bb_fill_rounded_rect(shx+16, shy+12, 20, 24, 6, S_GREEN);
        vga_bb_fill_rounded_rect(shx+18, shy+14, 16, 20, 4, S_BG_DEEP);
        vga_bb_draw_string_2x(shx+22, shy+17, "S", S_GREEN, 0);
        vga_bb_draw_string_2x(shx+42, shy+14, "SECURE", S_GREEN, 0);
        vga_bb_draw_hline(shx+16, shy+42, 128, S_SEPARATOR);
        vga_bb_draw_string(shx+16, shy+50, "Ring 3 Isolation", S_TEXT_DIM, 0);
        ui_status_dot(shx+130, shy+54, 4, S_GREEN);
        vga_bb_draw_string(shx+16, shy+64, "Kernel Protected", S_TEXT_DIM, 0);
        ui_status_dot(shx+130, shy+68, 4, S_GREEN);
        vga_bb_draw_string(shx+16, shy+78, "Memory Guarded", S_TEXT_DIM, 0);
        ui_status_dot(shx+130, shy+82, 4, S_GREEN);
        vga_bb_draw_string(shx+16, shy+92, "Audit Active", S_TEXT_DIM, 0);
        ui_status_dot(shx+130, shy+96, 4, S_GREEN);
    }
}

static void draw_workspace_indicator(void) {
    int wx = 40, wy = 40;
    ui_glass_panel(wx, wy, 160, 40, 20, 0x40FFFFFF, S_BORDER);
    for (int i=0; i<3; i++) {
        int px = wx + 30 + i*50;
        int py = wy + 20;
        uint32_t c = (i == current_workspace) ? S_NEON_CYAN : S_TEXT_DIM;
        vga_bb_fill_circle(px, py, (i == current_workspace) ? 8 : 5, c);
    }
}

/* ── Full desktop draw ────────────────────────────────── */
static void draw_desktop(void) {
    draw_wallpaper();
    draw_icons();
    draw_workspace_indicator();
    draw_widgets();
    /* Only draw active windows to save cycles */
    for (int i = 0; i < win_order_count; i++) {
        int wi = win_order[i];
        if (windows[wi].active && windows[wi].workspace == current_workspace) draw_window(wi);
    }
    draw_panel();
    draw_kickoff();
    draw_context_menu();
    draw_toasts();
    mouse_state_t ms; mouse_get_state(&ms);
    draw_cursor(ms.x, ms.y);
    vga_flip();
}

/* ── Main loop ────────────────────────────────────────────── */
void desktop_run(void) {
    serial_write("desktop_run: start\n"); screen_set_serial_mirror(0xFF000000);
    memset(windows,0,sizeof(windows)); win_count=0; win_focus=-1; win_order_count=0; kickoff_open=0; dragging=0; wp_cached=0;
    memset(toasts, 0, sizeof(toasts));
    audit_log(AUDIT_SYSTEM, "Desktop started");
    /* Welcome splash */
    toast_show("Welcome to SwanOS!", S_NEON_CYAN);
    toast_show("Where Love Meets Technology", S_PINK);
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
            /* Right click on taskbar → toggle rearrange mode */
            if (ms.y >= dock_y && ms.y < dock_y + PANEL_H &&
                ms.x >= dock_x && ms.x < dock_x + dock_w) {
                rearrange_mode = !rearrange_mode;
                needs_redraw = 1;
            }
            /* Right click elsewhere → Context menu */
            else if (!ctx_menu_open) {
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
                if(ny+windows[drag_win].h>SCRH-80) ny=SCRH-80-windows[drag_win].h;
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
        /* Animated wallpaper periodic refresh */
        if (ticks - wp_last_tick > 150) {
            wp_phase += 16;
            wp_cached = 0;
            wp_last_tick = ticks;
            needs_redraw = 1;
        }

        /* SwanClock: force redraw while stopwatch is running */
        for (int i=0; i<MAX_WINDOWS; i++) {
            if (windows[i].active && windows[i].type == WIN_CLOCK && windows[i].clock_sw_running)
                needs_redraw = 1;
        }

        /* SwanDraw: continuous painting while mouse held down */
        if ((ms.buttons & 1) && !dragging && win_focus >= 0 && windows[win_focus].active && windows[win_focus].type == WIN_DRAW) {
            window_t *dw = &windows[win_focus];
            int dcx3 = dw->x+4, dcy3 = dw->y+TITLEBAR_H+4, dcw3 = dw->w-8;
            int pal_y3 = dcy3 + 4;
            int cvx3 = dcx3 + 8, cvy3 = pal_y3 + 40;
            int cvw3 = dcw3 - 16, cvh3 = dw->h - TITLEBAR_H - 8 - 52;
            int cew3 = cvw3 / 64; if (cew3 < 2) cew3 = 2;
            int ceh3 = cvh3 / 48; if (ceh3 < 2) ceh3 = 2;
            if (ms.x >= cvx3 && ms.x < cvx3 + 64*cew3 && ms.y >= cvy3 && ms.y < cvy3 + 48*ceh3) {
                int ppx = (ms.x - cvx3) / cew3;
                int ppy = (ms.y - cvy3) / ceh3;
                if (ppx >= 0 && ppx < 64 && ppy >= 0 && ppy < 48) {
                    int pidx = ppy * 64 + ppx;
                    if (pidx < 512) { dw->note_text[pidx] = (char)(dw->draw_color + 1); needs_redraw = 1; }
                }
            }
        }

        /* Sysmon history update (every 1 second roughly = 100 ticks at 100Hz) */
        if (ticks - sysmon_tick > 100) {
            for (int i=0; i<MAX_WINDOWS; i++) {
                if (windows[i].active && windows[i].type == WIN_SYSMON) {
                    windows[i].sysmon_history[windows[i].sysmon_head] = mem_used() / 1024;
                    windows[i].sysmon_head = (windows[i].sysmon_head + 1) % 60;
                    needs_redraw = 1;
                }
            }
            /* Update tray memory sparkline (every ~2 sec use same tick) */
            if (ticks - tray_mem_tick > 200) {
                int mt = mem_total();
                int pct = (mt > 0) ? (mem_used() * 100) / mt : 0;
                if (pct > 100) pct = 100;
                tray_mem_history[tray_mem_head] = pct;
                tray_mem_head = (tray_mem_head + 1) % 20;
                tray_mem_tick = ticks;
                needs_redraw = 1;
            }
            sysmon_tick = ticks;
            /* Periodic user profile save */
            user_periodic_save();
            /* Stopwatch tick for SwanClock */
            for (int i=0; i<MAX_WINDOWS; i++) {
                if (windows[i].active && windows[i].type == WIN_CLOCK && windows[i].calc_op) {
                    windows[i].calc_v1++;
                    needs_redraw = 1;
                }
            }
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
