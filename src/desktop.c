/* ============================================================
 * SwanOS — Desktop Environment
 * Full OS-like desktop: wallpaper, icons, taskbar, windows,
 * mouse cursor, start menu, and built-in apps.
 * 320x200, 256 colors, double-buffered.
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

/* ── Layout constants ─────────────────────────────────────── */
#define SCRW       GFX_W
#define SCRH       GFX_H
#define TASKBAR_H  40
#define DESK_H     (SCRH - TASKBAR_H)  /* 186 */
#define TITLEBAR_H 30
#define CHAR_W     8
#define CHAR_H     8

/* ── Palette indices (from modern palette) ────────────────── */
#define C_DESK_BG    0xFF1F1F2F
#define C_TASKBAR    0xEE101015
#define C_WIN_TITLE  0xDD20202F
#define C_WIN_BG     0xEE151515
#define C_WIN_BORDER 0xFF404050
#define C_TEXT       0xFFAAAAAA
#define C_BRIGHT     0xFFFFFFFF
#define C_DIM        0xFF555555
#define C_CYAN       0xFF00EEEE
#define C_GREEN      0xFF00EE00
#define C_GOLD       0xFFEEEE00
#define C_RED        0xFFEE0000
#define C_ICON_BG    0x88000000
#define C_MENU_BG    0xEE202020
#define C_MENU_HL    0xFF404050
#define C_CURSOR1    0xFFFFFFFF
#define C_CURSOR2    0xFF000000
#define C_BTN_START  0xFF00EEEE

/* ── Icon definitions ─────────────────────────────────────── */
#define ICON_W     64
#define ICON_H     64
#define MAX_ICONS  6

typedef struct {
    int x, y;
    const char *label;
    int app_id;  /* 0=terminal,1=files,2=notes,3=about,4=snake,5=settings */
} desktop_icon_t;

static desktop_icon_t icons[MAX_ICONS] = {
    {32, 32, "AI Chat", 5},
    {32, 128, "Terminal", 0},
    {32, 224, "Files", 1},
    {32, 320, "Notes", 2},
    {32, 416, "About", 3},
};
static int num_icons = 5;

/* ── Window system ────────────────────────────────────────── */
#define MAX_WINDOWS 6
#define WIN_TERM    0
#define WIN_FILES   1
#define WIN_NOTES   2
#define WIN_ABOUT   3
#define WIN_AI      4

typedef struct {
    int active;
    int x, y, w, h;
    int type;           /* WIN_TERM, WIN_FILES, etc. */
    char title[20];
    /* Terminal state */
    char lines[16][40];
    int line_count;
    int scroll;
    char input[80];
    int input_pos;
    /* Notes state */
    char note_text[512];
    int note_len;
    int note_cursor;
    char note_file[20];
    /* Files state */
    int file_sel;
} window_t;

static window_t windows[MAX_WINDOWS];
static int win_count = 0;
static int win_focus = -1;  /* index of focused window */
static int win_order[MAX_WINDOWS]; /* z-order, [0]=bottom */
static int win_order_count = 0;

/* ── Dragging state ───────────────────────────────────────── */
static int dragging = 0;
static int drag_win = -1;
static int drag_ox, drag_oy;

/* ── Start menu ───────────────────────────────────────────── */
static int start_menu_open = 0;
#define MENU_W    70
#define MENU_ITEMS 7
#define MENU_ITEM_H 12
#define MENU_H    (MENU_ITEMS * MENU_ITEM_H + 4)

static const char *menu_labels[MENU_ITEMS] = {
    "AI Chat", "Terminal", "Files", "Notes", "About",
    "--------", "Shutdown"
};
static int menu_app_ids[MENU_ITEMS] = {5,0,1,2,3,-1,-2};

/* ── Mouse cursor save buffer ─────────────────────────────── */
static int prev_mx = -1, prev_my = -1;

/* ── Forward declarations ─────────────────────────────────── */
static void open_window(int type);
static void draw_desktop(void);
static int  handle_click(int mx, int my);
static void term_add_line(window_t *w, const char *text);
static void term_process_cmd(window_t *w);

/* ── Desktop palette setup ────────────────────────────────── */
static void setup_desktop_palette(void) {
    // palette
    // palette
    // palette
    // palette
    // palette
    // palette
    // palette
    // palette /* close btn */
    // palette   /* green accent */
    // palette  /* gold accent */
}

/* ── Draw wallpaper ───────────────────────────────────────── */
static void draw_wallpaper(void) {
    for (int y = 0; y < DESK_H; y++) {
        /* Gradient from dark blue to slightly lighter */
        int shade = 80 + (y * 5) / DESK_H;
        if (shade > 85) shade = 85;
        vga_bb_draw_hline(0, y, SCRW, (uint8_t)shade);
    }
    /* Starfield */
    uint32_t seed = 0xCAFE;
    for (int i = 0; i < 40; i++) {
        seed = seed * 1103515245 + 12345;
        int sx = (seed >> 16) % SCRW;
        seed = seed * 1103515245 + 12345;
        int sy = (seed >> 16) % DESK_H;
        seed = seed * 1103515245 + 12345;
        uint8_t c = ((seed >> 16) % 3 == 0) ? 7 : 4;
        vga_bb_putpixel(sx, sy, c);
    }
}

/* ── Draw icon ────────────────────────────────────────────── */
static void draw_icon_glyph(int x, int y, int app_id) {
    int cx = x + 12, cy = y + 2;
    switch (app_id) {
        case 0: /* Terminal: monitor shape */
            vga_bb_fill_rect(cx-5, cy, 14, 10, C_DIM);
            vga_bb_fill_rect(cx-4, cy+1, 12, 8, 0xFF000000);
            vga_bb_draw_string(cx-2, cy+2, ">_", C_GREEN, 0xFF000000);
            vga_bb_draw_hline(cx-1, cy+11, 6, C_DIM);
            break;
        case 1: /* Files: folder */
            vga_bb_fill_rect(cx-5, cy+1, 5, 2, C_GOLD);
            vga_bb_fill_rect(cx-5, cy+3, 14, 8, C_GOLD);
            vga_bb_fill_rect(cx-4, cy+4, 12, 6, 42);
            break;
        case 2: /* Notes: notepad */
            vga_bb_fill_rect(cx-4, cy, 12, 12, C_BRIGHT);
            vga_bb_fill_rect(cx-3, cy+1, 10, 10, 6);
            for (int i = 0; i < 3; i++)
                vga_bb_draw_hline(cx-2, cy+3+i*3, 8, C_DIM);
            break;
        case 3: /* About: info circle */
            vga_bb_fill_rect(cx-1, cy, 6, 12, C_CYAN);
            vga_bb_fill_rect(cx, cy+1, 4, 10, C_WIN_TITLE);
            vga_bb_draw_string(cx, cy+2, "i", C_BRIGHT, C_WIN_TITLE);
            break;
        case 4: /* Snake */
            vga_bb_fill_rect(cx-3, cy+2, 3, 3, C_GREEN);
            vga_bb_fill_rect(cx, cy+2, 3, 3, 32);
            vga_bb_fill_rect(cx+3, cy+2, 3, 3, 32);
            vga_bb_fill_rect(cx+3, cy+5, 3, 3, 32);
            vga_bb_putpixel(cx-2, cy+3, 0xFF000000);
            break;
        case 5: /* AI Chat: brain/sparkle icon */
            vga_bb_fill_rect(cx-4, cy+1, 12, 10, C_CYAN);
            vga_bb_fill_rect(cx-3, cy+2, 10, 8, C_WIN_TITLE);
            vga_bb_draw_string(cx-2, cy+3, "AI", C_BRIGHT, C_WIN_TITLE);
            /* Sparkle dots */
            vga_bb_putpixel(cx+8, cy, C_GOLD);
            vga_bb_putpixel(cx+9, cy+1, C_GOLD);
            vga_bb_putpixel(cx-5, cy+10, C_CYAN);
            break;
    }
}

static void draw_icons(void) {
    for (int i = 0; i < num_icons; i++) {
        desktop_icon_t *ic = &icons[i];
        draw_icon_glyph(ic->x, ic->y, ic->app_id);
        int lx = ic->x + (ICON_W - (int)strlen(ic->label) * CHAR_W) / 2;
        if (lx < 0) lx = 0;
        vga_bb_draw_string(lx, ic->y + 16, ic->label, C_BRIGHT, C_DESK_BG);
    }
}

/* ── Draw taskbar ─────────────────────────────────────────── */
static void draw_taskbar(void) {
    int ty = DESK_H;
    vga_bb_fill_rect(0, ty, SCRW, TASKBAR_H, C_TASKBAR);
    vga_bb_draw_hline(0, ty, SCRW, C_DIM);

    /* Start button */
    vga_bb_fill_rect(1, ty+1, 36, TASKBAR_H-2, C_BTN_START);
    vga_bb_draw_string(3, ty+3, "Swan", C_BRIGHT, C_BTN_START);

    /* Window buttons */
    int bx = 40;
    for (int i = 0; i < win_order_count && bx < SCRW - 60; i++) {
        int wi = win_order[i];
        if (!windows[wi].active) continue;
        uint8_t bg = (wi == win_focus) ? C_MENU_HL : C_TASKBAR;
        int bw = (int)strlen(windows[wi].title) * CHAR_W + 6;
        if (bw > 60) bw = 60;
        vga_bb_fill_rect(bx, ty+2, bw, TASKBAR_H-4, bg);
        vga_bb_draw_string(bx+3, ty+3, windows[wi].title, C_BRIGHT, bg);
        bx += bw + 2;
    }

    /* Clock on right */
    rtc_time_t rtc;
    rtc_read(&rtc);
    char clk[10];
    rtc_format_time(&rtc, clk);
    int cx = SCRW - (int)strlen(clk) * CHAR_W - 4;
    vga_bb_draw_string(cx, ty+3, clk, C_TEXT, C_TASKBAR);

    /* Memory */
    char mb[12]; char tmp[8];
    itoa(mem_used()/1024, tmp, 10);
    strcpy(mb, tmp); strcat(mb, "K");
    int mx2 = cx - (int)strlen(mb) * CHAR_W - 8;
    vga_bb_draw_string(mx2, ty+3, mb, C_DIM, C_TASKBAR);
}

/* ── Draw start menu ──────────────────────────────────────── */
static void draw_start_menu(void) {
    if (!start_menu_open) return;
    int mx_top = DESK_H - MENU_H;
    vga_bb_fill_rect(0, mx_top, MENU_W, MENU_H, C_MENU_BG);
    vga_bb_draw_rect_outline(0, mx_top, MENU_W, MENU_H, C_DIM);

    mouse_state_t ms;
    mouse_get_state(&ms);

    for (int i = 0; i < MENU_ITEMS; i++) {
        int iy = mx_top + 2 + i * MENU_ITEM_H;
        if (menu_labels[i][0] == '-') {
            vga_bb_draw_hline(4, iy + 5, MENU_W - 8, C_DIM);
            continue;
        }
        /* Highlight on hover */
        int hover = (ms.x >= 0 && ms.x < MENU_W &&
                     ms.y >= iy && ms.y < iy + MENU_ITEM_H);
        if (hover) {
            vga_bb_fill_rect(1, iy, MENU_W-2, MENU_ITEM_H, C_MENU_HL);
        }
        uint8_t fc = (i == MENU_ITEMS - 1) ? C_RED : C_BRIGHT;
        vga_bb_draw_string(6, iy + 2, menu_labels[i], fc,
                           hover ? C_MENU_HL : C_MENU_BG);
    }
}

/* ── Window drawing ───────────────────────────────────────── */

static void draw_window(int wi) {
    window_t *w = &windows[wi];
    if (!w->active) return;
    int focused = (wi == win_focus);

    /* Shadow */
    vga_bb_fill_rect_alpha(w->x+8, w->y+8, w->w, w->h, 0x88000000);

    /* Border */
    vga_bb_fill_rect(w->x, w->y, w->w, w->h, C_WIN_BG);
    vga_bb_draw_rect_outline(w->x, w->y, w->w, w->h,
                             focused ? C_CYAN : C_WIN_BORDER);

    /* Title bar */
    uint8_t tb_col = focused ? C_WIN_TITLE : C_DIM;
    vga_bb_fill_rect(w->x+1, w->y+1, w->w-2, TITLEBAR_H, tb_col);
    vga_bb_draw_string(w->x+3, w->y+2, w->title, C_BRIGHT, tb_col);

    /* Close button [X] */
    int cbx = w->x + w->w - 12;
    vga_bb_fill_rect(cbx, w->y+6, 18, 18, 0xFFEE2222);
    vga_bb_draw_string(cbx+4, w->y+8, "X", C_BRIGHT, 0xFFEE2222);

    /* Content area */
    int cx = w->x + 2;
    int cy = w->y + TITLEBAR_H + 2;
    int cw = w->w - 4;
    int ch = w->h - TITLEBAR_H - 4;

    if (w->type == WIN_TERM) {
        /* Terminal: scrollable text output + input line */
        vga_bb_fill_rect_alpha(cx, cy, cw, ch, 0x88000000);
        int max_lines = (ch - CHAR_H - 2) / CHAR_H;
        int start_line = 0;
        if (w->line_count > max_lines)
            start_line = w->line_count - max_lines;
        int ly = cy + 1;
        for (int i = start_line; i < w->line_count && i < start_line + max_lines; i++) {
            int max_ch = (cw - 4) / CHAR_W;
            for (int j = 0; j < max_ch && w->lines[i][j]; j++) {
                vga_bb_draw_char(cx + 2 + j * CHAR_W, ly,
                                 w->lines[i][j], C_GREEN, 0x00000000);
            }
            ly += CHAR_H;
        }
        /* Input line */
        int iy = cy + ch - CHAR_H - 1;
        vga_bb_draw_hline(cx, iy - 1, cw, C_DIM);
        vga_bb_draw_string(cx+1, iy, ">", C_CYAN, 0x00000000);
        int max_in = (cw - CHAR_W * 2) / CHAR_W;
        int s = 0;
        if (w->input_pos > max_in) s = w->input_pos - max_in;
        for (int i = s; i < w->input_pos; i++) {
            vga_bb_draw_char(cx + CHAR_W + 2 + (i-s)*CHAR_W, iy,
                             w->input[i], C_BRIGHT, 0xFF000000);
        }
        /* Cursor blink */
        int cur_x = cx + CHAR_W + 2 + (w->input_pos - s) * CHAR_W;
        if (cur_x < cx + cw - 2) {
            uint8_t cc = (timer_get_ticks() / 30) % 2 ? C_CYAN : 0;
            vga_bb_fill_rect(cur_x, iy, CHAR_W-1, CHAR_H-1, cc);
        }
    }
    else if (w->type == WIN_FILES) {
        /* File manager */
        vga_bb_fill_rect_alpha(cx, cy, cw, ch, 0x88000000);
        char listing[512];
        fs_list("/", listing, sizeof(listing));
        char *p = listing;
        int fy = cy + 2;
        int fi = 0;
        while (*p && fy + CHAR_H <= cy + ch) {
            while (*p == ' ') p++;
            if (*p == '\0' || *p == '\n') { if (*p) p++; continue; }
            int is_dir = (strncmp(p, "[DIR]", 5) == 0);
            char fname[24]; int fni = 0;
            char *ns = strchr(p, ']');
            if (ns) ns += 2; else ns = p;
            while (*ns && *ns != '\n' && fni < 22) fname[fni++] = *ns++;
            fname[fni] = '\0';
            uint8_t fc = is_dir ? C_GOLD : C_CYAN;
            uint8_t bg_c = (fi == w->file_sel) ? C_MENU_HL : 0;
            if (fi == w->file_sel)
                vga_bb_fill_rect(cx+1, fy-1, cw-2, CHAR_H+1, bg_c);
            const char *prefix = is_dir ? "+ " : "  ";
            vga_bb_draw_string(cx+3, fy, prefix, C_DIM, bg_c);
            vga_bb_draw_string(cx+3+2*CHAR_W, fy, fname, fc, bg_c);
            fy += CHAR_H + 1;
            fi++;
            while (*p && *p != '\n') p++;
            if (*p == '\n') p++;
        }
    }
    else if (w->type == WIN_NOTES) {
        /* Simple text editor */
        vga_bb_fill_rect_alpha(cx, cy, cw, ch, 0x88000000);
        /* Header */
        vga_bb_draw_string(cx+2, cy+1, "File:", C_DIM, 0xFF000000);
        vga_bb_draw_string(cx+2+5*CHAR_W, cy+1,
                           w->note_file[0] ? w->note_file : "untitled", C_CYAN, 0xFF000000);
        vga_bb_draw_hline(cx, cy + CHAR_H + 2, cw, C_DIM);
        /* Text content */
        int ty2 = cy + CHAR_H + 4;
        int tx = cx + 2;
        int col = 0;
        int max_col = (cw - 4) / CHAR_W;
        for (int i = 0; i < w->note_len && ty2 + CHAR_H <= cy + ch - 2; i++) {
            if (w->note_text[i] == '\n' || col >= max_col) {
                ty2 += CHAR_H;
                col = 0;
                tx = cx + 2;
                if (w->note_text[i] == '\n') continue;
            }
            vga_bb_draw_char(tx, ty2, w->note_text[i], C_TEXT, 0xFF000000);
            tx += CHAR_W;
            col++;
        }
        /* Cursor */
        if (focused) {
            uint8_t cc = (timer_get_ticks() / 30) % 2 ? C_GREEN : 0;
            vga_bb_fill_rect(tx, ty2, 2, CHAR_H-1, cc);
        }
        /* Save hint */
        vga_bb_draw_string(cx+2, cy+ch-CHAR_H-1, "Ctrl+S:save", C_DIM, 0xFF000000);
    }
    else if (w->type == WIN_ABOUT) {
        vga_bb_fill_rect(cx, cy, cw, ch, C_WIN_BG);
        int ay = cy + 4;
        vga_bb_draw_string(cx+4, ay, "SWANOS", C_CYAN, C_WIN_BG);
        ay += CHAR_H + 2;
        vga_bb_draw_string(cx+4, ay, "Version 3.0", C_TEXT, C_WIN_BG);
        ay += CHAR_H + 2;
        vga_bb_draw_hline(cx+4, ay, cw-8, C_DIM);
        ay += 4;
        vga_bb_draw_string(cx+4, ay, "AI-Powered OS", C_GREEN, C_WIN_BG);
        ay += CHAR_H;
        vga_bb_draw_string(cx+4, ay, "Bare-Metal x86", C_TEXT, C_WIN_BG);
        ay += CHAR_H + 2;
        vga_bb_draw_string(cx+4, ay, "VGA 320x200", C_DIM, C_WIN_BG);
        ay += CHAR_H;
        char mb2[20]; char tmp[8];
        strcpy(mb2, "Mem: ");
        itoa(mem_total()/1024, tmp, 10);
        strcat(mb2, tmp); strcat(mb2, "K");
        vga_bb_draw_string(cx+4, ay, mb2, C_DIM, C_WIN_BG);
        ay += CHAR_H + 4;
        vga_bb_draw_string(cx+4, ay, "User: ", C_DIM, C_WIN_BG);
        vga_bb_draw_string(cx+4+6*CHAR_W, ay, user_current(), C_CYAN, C_WIN_BG);
    }
    else if (w->type == WIN_AI) {
        /* AI Chat window — same layout as terminal but AI-themed */
        vga_bb_fill_rect_alpha(cx, cy, cw, ch, 0x88000000);
        /* Header */
        vga_bb_fill_rect(cx, cy, cw, CHAR_H + 2, C_WIN_TITLE);
        vga_bb_draw_string(cx+2, cy+1, "AI Assistant", C_CYAN, C_WIN_TITLE);
        /* Chat lines */
        int max_lines = (ch - CHAR_H * 2 - 6) / CHAR_H;
        int start_line = 0;
        if (w->line_count > max_lines)
            start_line = w->line_count - max_lines;
        int ly2 = cy + CHAR_H + 4;
        for (int i = start_line; i < w->line_count && i < start_line + max_lines; i++) {
            int max_ch2 = (cw - 4) / CHAR_W;
            /* Alternate colors for user vs AI */
            uint8_t lc = (w->lines[i][0] == '>') ? C_GREEN : C_CYAN;
            for (int j = 0; j < max_ch2 && w->lines[i][j]; j++) {
                vga_bb_draw_char(cx + 2 + j * CHAR_W, ly2,
                                 w->lines[i][j], lc, 0xFF000000);
            }
            ly2 += CHAR_H;
        }
        /* Input line */
        int iy = cy + ch - CHAR_H - 1;
        vga_bb_draw_hline(cx, iy - 1, cw, C_DIM);
        vga_bb_draw_string(cx+1, iy, "?", C_GOLD, 0xFF000000);
        int max_in = (cw - CHAR_W * 2) / CHAR_W;
        int s = 0;
        if (w->input_pos > max_in) s = w->input_pos - max_in;
        for (int i = s; i < w->input_pos; i++) {
            vga_bb_draw_char(cx + CHAR_W + 2 + (i-s)*CHAR_W, iy,
                             w->input[i], C_BRIGHT, 0xFF000000);
        }
        /* Cursor blink */
        int cur_x2 = cx + CHAR_W + 2 + (w->input_pos - s) * CHAR_W;
        if (cur_x2 < cx + cw - 2) {
            uint8_t cc = (timer_get_ticks() / 30) % 2 ? C_GOLD : 0;
            vga_bb_fill_rect(cur_x2, iy, CHAR_W-1, CHAR_H-1, cc);
        }
    }
}

/* ── Mouse cursor (8x8 arrow) ─────────────────────────────── */
static const uint8_t cursor_sprite[8][8] = {
    {1,0,0,0,0,0,0,0},
    {1,1,0,0,0,0,0,0},
    {1,2,1,0,0,0,0,0},
    {1,2,2,1,0,0,0,0},
    {1,2,2,2,1,0,0,0},
    {1,2,2,2,2,1,0,0},
    {1,2,1,1,0,0,0,0},
    {1,0,0,1,1,0,0,0},
};

static void draw_cursor(int mx, int my) {
    for (int dy = 0; dy < 8; dy++) {
        for (int dx = 0; dx < 8; dx++) {
            if (cursor_sprite[dy][dx] == 0) continue;
            int px = mx + dx, py = my + dy;
            if (px >= SCRW || py >= SCRH) continue;
            uint8_t c = (cursor_sprite[dy][dx] == 1) ? C_CURSOR2 : C_CURSOR1;
            vga_bb_putpixel(px, py, c);
        }
    }
}

/* ── Window management ────────────────────────────────────── */

static int find_free_window(void) {
    for (int i = 0; i < MAX_WINDOWS; i++)
        if (!windows[i].active) return i;
    return -1;
}

static void bring_to_front(int wi) {
    /* Remove from order */
    int pos = -1;
    for (int i = 0; i < win_order_count; i++)
        if (win_order[i] == wi) { pos = i; break; }
    if (pos >= 0) {
        for (int i = pos; i < win_order_count - 1; i++)
            win_order[i] = win_order[i+1];
    } else {
        if (win_order_count >= MAX_WINDOWS) return;
    }
    win_order[win_order_count > 0 ? win_order_count - 1 : 0] = wi;
    if (pos < 0 && win_order_count < MAX_WINDOWS) win_order_count++;
    win_focus = wi;
}

static void close_window(int wi) {
    windows[wi].active = 0;
    /* Remove from order */
    int pos = -1;
    for (int i = 0; i < win_order_count; i++)
        if (win_order[i] == wi) { pos = i; break; }
    if (pos >= 0) {
        for (int i = pos; i < win_order_count - 1; i++)
            win_order[i] = win_order[i+1];
        win_order_count--;
    }
    win_focus = (win_order_count > 0) ? win_order[win_order_count-1] : -1;
}

static void open_window(int type) {
    /* Check if already open */
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (windows[i].active && windows[i].type == type) {
            bring_to_front(i);
            return;
        }
    }
    int wi = find_free_window();
    if (wi < 0) return;

    window_t *w = &windows[wi];
    memset(w, 0, sizeof(window_t));
    w->active = 1;
    w->type = type;

    /* Position based on type to avoid overlap */
    switch (type) {
        case WIN_TERM:
            strcpy(w->title, "Terminal");
            w->x = 250; w->y = 150; w->w = 800; w->h = 500;
            term_add_line(w, "SwanOS Terminal");
            term_add_line(w, "Type 'help'");
            break;
        case WIN_FILES:
            strcpy(w->title, "Files");
            w->x = 300; w->y = 200; w->w = 800; w->h = 500;
            w->file_sel = 0;
            break;
        case WIN_NOTES:
            strcpy(w->title, "Notes");
            w->x = 350; w->y = 250; w->w = 800; w->h = 500;
            strcpy(w->note_file, "notes.txt");
            /* Try to load existing */
            fs_read("notes.txt", w->note_text, sizeof(w->note_text) - 1);
            w->note_len = strlen(w->note_text);
            w->note_cursor = w->note_len;
            break;
        case WIN_ABOUT:
            strcpy(w->title, "About");
            w->x = 400; w->y = 300; w->w = 600; w->h = 400;
            break;
        case WIN_AI:
            strcpy(w->title, "AI Chat");
            w->x = 200; w->y = 100; w->w = 800; w->h = 600;
            term_add_line(w, "SwanOS AI Assistant");
            term_add_line(w, "Ask me anything!");
            break;
    }

    win_order[win_order_count++] = wi;
    win_focus = wi;
}

/* ── Terminal helpers ─────────────────────────────────────── */
static void term_add_line(window_t *w, const char *text) {
    if (w->line_count >= 16) {
        for (int i = 0; i < 15; i++)
            strcpy(w->lines[i], w->lines[i+1]);
        w->line_count = 15;
    }
    strncpy(w->lines[w->line_count], text, 39);
    w->lines[w->line_count][39] = '\0';
    w->line_count++;
}

static void term_process_cmd(window_t *w) {
    char *cmd = w->input;
    while (*cmd == ' ') cmd++;
    if (cmd[0] == '\0') return;

    /* Echo command */
    char echo[44]; strcpy(echo, "> "); strcat(echo, cmd);
    term_add_line(w, echo);

    /* Parse */
    char cmdcopy[80]; strcpy(cmdcopy, cmd);
    char *arg = cmdcopy;
    while (*arg && *arg != ' ') arg++;
    if (*arg) { *arg = '\0'; arg++; }
    while (*arg == ' ') arg++;

    if (strcmp(cmdcopy, "help") == 0) {
        term_add_line(w, "Commands:");
        term_add_line(w, " ls cat write mkdir rm");
        term_add_line(w, " calc date mem whoami");
        term_add_line(w, " ask <question>");
        term_add_line(w, " clear exit");
    } else if (strcmp(cmdcopy, "clear") == 0) {
        w->line_count = 0;
    } else if (strcmp(cmdcopy, "exit") == 0) {
        for (int i = 0; i < MAX_WINDOWS; i++)
            if (&windows[i] == w) { close_window(i); break; }
    } else if (strcmp(cmdcopy, "ls") == 0) {
        char listing[256];
        fs_list(arg[0] ? arg : "/", listing, sizeof(listing));
        char *p = listing;
        while (*p) {
            char line[40]; int li = 0;
            while (*p && *p != '\n' && li < 38) line[li++] = *p++;
            line[li] = '\0';
            if (li > 0) term_add_line(w, line);
            if (*p == '\n') p++;
        }
    } else if (strcmp(cmdcopy, "cat") == 0) {
        if (!arg[0]) { term_add_line(w, "Usage: cat <file>"); }
        else {
            char content[256];
            int r = fs_read(arg, content, sizeof(content));
            if (r >= 0) term_add_line(w, content);
            else term_add_line(w, "File not found");
        }
    } else if (strcmp(cmdcopy, "write") == 0) {
        char *ct = arg;
        while (*ct && *ct != ' ') ct++;
        if (*ct) { *ct = '\0'; ct++; }
        if (!arg[0] || !ct[0]) term_add_line(w, "Usage: write <f> <text>");
        else {
            fs_write(arg, ct);
            term_add_line(w, "Written.");
        }
    } else if (strcmp(cmdcopy, "mkdir") == 0) {
        if (!arg[0]) term_add_line(w, "Usage: mkdir <name>");
        else { fs_mkdir(arg); term_add_line(w, "Created."); }
    } else if (strcmp(cmdcopy, "rm") == 0) {
        if (!arg[0]) term_add_line(w, "Usage: rm <file>");
        else { fs_delete(arg); term_add_line(w, "Deleted."); }
    } else if (strcmp(cmdcopy, "date") == 0) {
        rtc_time_t t; rtc_read(&t);
        char dbuf[12], tbuf[10];
        rtc_format_date(&t, dbuf);
        rtc_format_time(&t, tbuf);
        char msg[30]; strcpy(msg, dbuf); strcat(msg, " "); strcat(msg, tbuf);
        term_add_line(w, msg);
    } else if (strcmp(cmdcopy, "mem") == 0) {
        char msg[40]; char tmp[8];
        strcpy(msg, "Used: "); itoa(mem_used()/1024, tmp, 10);
        strcat(msg, tmp); strcat(msg, "K / ");
        itoa(mem_total()/1024, tmp, 10); strcat(msg, tmp); strcat(msg, "K");
        term_add_line(w, msg);
    } else if (strcmp(cmdcopy, "whoami") == 0) {
        char msg[30]; strcpy(msg, "User: "); strcat(msg, user_current());
        term_add_line(w, msg);
    } else if (strcmp(cmdcopy, "calc") == 0) {
        if (!arg[0]) { term_add_line(w, "Usage: calc <expr>"); }
        else {
            int result = 0, num = 0; char op = '+'; int has = 0;
            const char *e = arg;
            while (*e) {
                if (*e >= '0' && *e <= '9') { num = num*10+(*e-'0'); has=1; }
                else if (*e=='+' || *e=='-' || *e=='*' || *e=='/') {
                    if (has) {
                        if (op=='+') result+=num; else if (op=='-') result-=num;
                        else if (op=='*') result*=num; else if (op=='/' && num) result/=num;
                    }
                    op = *e; num = 0; has = 0;
                }
                e++;
            }
            if (has) {
                if (op=='+') result+=num; else if (op=='-') result-=num;
                else if (op=='*') result*=num; else if (op=='/' && num) result/=num;
            }
            char msg[20]; char nb[12];
            strcpy(msg, "= "); itoa(result, nb, 10); strcat(msg, nb);
            term_add_line(w, msg);
        }
    } else if (strcmp(cmdcopy, "ask") == 0) {
        if (!arg[0]) { term_add_line(w, "Usage: ask <q>"); }
        else {
            term_add_line(w, "Thinking...");
            char resp[512];
            llm_query(arg, resp, sizeof(resp));
            /* Split response into lines */
            char *rp = resp;
            while (*rp) {
                char line[40]; int li = 0;
                while (*rp && *rp != '\n' && li < 38) line[li++] = *rp++;
                line[li] = '\0';
                if (li > 0) term_add_line(w, line);
                if (*rp == '\n') rp++;
            }
        }
    } else {
        /* LLM-first: send unknown commands to AI */
        term_add_line(w, "[AI] Thinking...");
        char resp[512];
        llm_query(w->input_pos > 0 ? cmd : cmdcopy, resp, sizeof(resp));
        /* Remove the "Thinking" line */
        if (w->line_count > 0) w->line_count--;
        /* Split response */
        char *rp = resp;
        while (*rp) {
            char line[40]; int li = 0;
            while (*rp && *rp != '\n' && li < 38) line[li++] = *rp++;
            line[li] = '\0';
            if (li > 0) term_add_line(w, line);
            if (*rp == '\n') rp++;
        }
    }

    w->input_pos = 0;
    w->input[0] = '\0';
}

/* ── Click handling ───────────────────────────────────────── */

static int handle_click(int mx, int my) {
    /* Start menu clicks */
    if (start_menu_open) {
        int mtop = DESK_H - MENU_H;
        if (mx >= 0 && mx < MENU_W && my >= mtop && my < DESK_H) {
            int idx = (my - mtop - 2) / MENU_ITEM_H;
            if (idx >= 0 && idx < MENU_ITEMS && menu_labels[idx][0] != '-') {
                start_menu_open = 0;
                int aid = menu_app_ids[idx];
                if (aid == -2) return -1; /* shutdown */
                if (aid == 5) { open_window(WIN_AI); return 0; }
                if (aid >= 0 && aid <= 4) open_window(aid);
                return 0;
            }
        }
        start_menu_open = 0;
        return 0;
    }

    /* Taskbar start button */
    if (my >= DESK_H && mx < 37) {
        start_menu_open = !start_menu_open;
        return 0;
    }

    /* Taskbar window buttons */
    if (my >= DESK_H) {
        int bx = 40;
        for (int i = 0; i < win_order_count; i++) {
            int wi = win_order[i];
            if (!windows[wi].active) continue;
            int bw = (int)strlen(windows[wi].title) * CHAR_W + 6;
            if (bw > 60) bw = 60;
            if (mx >= bx && mx < bx + bw) {
                bring_to_front(wi);
                return 0;
            }
            bx += bw + 2;
        }
        return 0;
    }

    /* Check windows (top to bottom in z-order) */
    for (int zi = win_order_count - 1; zi >= 0; zi--) {
        int wi = win_order[zi];
        window_t *w = &windows[wi];
        if (!w->active) continue;
        if (mx >= w->x && mx < w->x + w->w &&
            my >= w->y && my < w->y + w->h) {

            bring_to_front(wi);

            /* Close button */
            int cbx = w->x + w->w - 12;
            if (mx >= cbx && mx < cbx + 9 &&
                my >= w->y + 2 && my < w->y + 10) {
                close_window(wi);
                return 0;
            }

            /* Title bar → start drag */
            if (my >= w->y && my < w->y + TITLEBAR_H) {
                dragging = 1;
                drag_win = wi;
                drag_ox = mx - w->x;
                drag_oy = my - w->y;
                return 0;
            }

            /* File manager selection */
            if (w->type == WIN_FILES) {
                int cy = w->y + TITLEBAR_H + 4;
                int idx = (my - cy) / (CHAR_H + 1);
                if (idx >= 0) w->file_sel = idx;
            }

            return 0;
        }
    }

    /* Desktop icon clicks */
    for (int i = 0; i < num_icons; i++) {
        desktop_icon_t *ic = &icons[i];
        if (mx >= ic->x && mx < ic->x + ICON_W &&
            my >= ic->y && my < ic->y + ICON_H) {
            if (ic->app_id == 5) { open_window(WIN_AI); return 0; }
            if (ic->app_id == 4) return -4; /* snake */
            if (ic->app_id <= 3) open_window(ic->app_id);
            return 0;
        }
    }

    start_menu_open = 0;
    return 0;
}

/* ── Full desktop draw ────────────────────────────────────── */
static void draw_desktop(void) {
    draw_wallpaper();
    draw_icons();

    /* Draw windows in z-order (bottom first) */
    for (int i = 0; i < win_order_count; i++) {
        draw_window(win_order[i]);
    }

    draw_taskbar();
    draw_start_menu();

    /* Mouse cursor on top */
    mouse_state_t ms;
    mouse_get_state(&ms);
    draw_cursor(ms.x, ms.y);

    vga_flip();
}

/* ── Main desktop loop ────────────────────────────────────── */
void desktop_run(void) {
    /* Init graphics */
    serial_write("desktop_run: start\n");
    screen_set_serial_mirror(0xFF000000);
    //vga_gfx_init
    // setup_desktop_palette();

    /* Reset window state */
    serial_write("desktop_run: init windows\n");
    memset(windows, 0, sizeof(windows));
    win_count = 0;
    win_focus = -1;
    win_order_count = 0;
    start_menu_open = 0;
    dragging = 0;

    /* Add default windows */
    serial_write("desktop_run: open_window\n");
    open_window(WIN_ABOUT);
    open_window(WIN_TERM);

    /* Initial draw */
    serial_write("desktop_run: draw_desktop\n");
    draw_desktop();
    serial_write("desktop_run: initial draw done\n");
    // vga_fade_from_black(8);

    uint32_t last_draw = 0;
    int needs_redraw = 1;

    serial_write("desktop_run: enter loop\n");

    while (1) {
        mouse_state_t ms;
        mouse_get_state(&ms);

        /* Handle mouse dragging */
        if (dragging) {
            if (mouse_left_pressed()) {
                int nx = ms.x - drag_ox;
                int ny = ms.y - drag_oy;
                if (nx < 0) nx = 0;
                if (ny < 0) ny = 0;
                if (nx + windows[drag_win].w > SCRW)
                    nx = SCRW - windows[drag_win].w;
                if (ny + windows[drag_win].h > DESK_H)
                    ny = DESK_H - windows[drag_win].h;
                windows[drag_win].x = nx;
                windows[drag_win].y = ny;
                needs_redraw = 1;
            } else {
                dragging = 0;
            }
        }

        /* Handle mouse clicks */
        if (ms.clicked) {
            mouse_clear_events();
            if (!dragging) {
                int result = handle_click(ms.x, ms.y);
                if (result == -1) {
                    /* Shutdown */
                    // vga_fade_to_black(8);
                    //vga_gfx_exit
                    screen_init();
                    screen_set_serial_mirror(1);
                    screen_clear();
                    screen_set_color(VGA_DARK_GREY, VGA_BLACK);
                    screen_print("\n\n   Shutting down...\n");
                    screen_delay(500);
                    __asm__ volatile("cli; hlt");
                    while(1);
                }
                if (result == -4) {
                    /* Snake game */
                    game_snake();
                    //vga_gfx_init
                    // setup_desktop_palette();
                }
                needs_redraw = 1;
            }
        }

        /* Handle mouse movement */
        if (ms.moved) {
            mouse_clear_events();
            needs_redraw = 1;
        }

        /* Handle keyboard for focused window */
        if (keyboard_has_key()) {
            char c = keyboard_getchar();
            needs_redraw = 1;

            if (win_focus >= 0 && windows[win_focus].active) {
                window_t *fw = &windows[win_focus];

                if (fw->type == WIN_TERM) {
                    if (c == '\n') {
                        term_process_cmd(fw);
                    } else if (c == '\b') {
                        if (fw->input_pos > 0) {
                            fw->input_pos--;
                            fw->input[fw->input_pos] = '\0';
                        }
                    } else if (c >= ' ' && fw->input_pos < 78) {
                        fw->input[fw->input_pos++] = c;
                        fw->input[fw->input_pos] = '\0';
                    }
                }
                else if (fw->type == WIN_NOTES) {
                    if (c == 19) { /* Ctrl+S */
                        fs_write(fw->note_file, fw->note_text);
                        /* Flash title to confirm */
                    } else if (c == '\b') {
                        if (fw->note_len > 0) {
                            fw->note_len--;
                            fw->note_text[fw->note_len] = '\0';
                        }
                    } else if (c == '\n') {
                        if (fw->note_len < 510) {
                            fw->note_text[fw->note_len++] = '\n';
                            fw->note_text[fw->note_len] = '\0';
                        }
                    } else if (c >= ' ' && fw->note_len < 510) {
                        fw->note_text[fw->note_len++] = c;
                        fw->note_text[fw->note_len] = '\0';
                    }
                }
                else if (fw->type == WIN_FILES) {
                    if ((uint8_t)c == KEY_UP && fw->file_sel > 0) fw->file_sel--;
                    if ((uint8_t)c == KEY_DOWN) fw->file_sel++;
                }
                else if (fw->type == WIN_AI) {
                    if (c == '\n') {
                        if (fw->input_pos > 0) {
                            char echo[44]; strcpy(echo, "> "); strcat(echo, fw->input);
                            term_add_line(fw, echo);
                            term_add_line(fw, "[AI] ...");
                            /* Send to LLM */
                            char resp[512];
                            llm_query(fw->input, resp, sizeof(resp));
                            /* Remove thinking indicator */
                            if (fw->line_count > 0) fw->line_count--;
                            /* Add response lines */
                            char *rp = resp;
                            while (*rp) {
                                char line[40]; int li = 0;
                                while (*rp && *rp != '\n' && li < 38) line[li++] = *rp++;
                                line[li] = '\0';
                                if (li > 0) term_add_line(fw, line);
                                if (*rp == '\n') rp++;
                            }
                            fw->input_pos = 0;
                            fw->input[0] = '\0';
                        }
                    } else if (c == '\b') {
                        if (fw->input_pos > 0) {
                            fw->input_pos--;
                            fw->input[fw->input_pos] = '\0';
                        }
                    } else if (c >= ' ' && fw->input_pos < 78) {
                        fw->input[fw->input_pos++] = c;
                        fw->input[fw->input_pos] = '\0';
                    }
                }
            }
        }

        /* Periodic redraw for clock/cursor blink */
        uint32_t ticks = timer_get_ticks();
        if (ticks - last_draw > 10 || needs_redraw) {
            draw_desktop();
            last_draw = ticks;
            needs_redraw = 0;
        }

        __asm__ volatile("hlt");
    }
}
