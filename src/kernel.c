/* ============================================================
 * SwanOS — Kernel Main
 * Modern graphical boot → text mode → login → shell/GUI
 * ============================================================ */

#include <stdint.h>
#include "screen.h"
#include "string.h"
#include "idt.h"
#include "timer.h"
#include "keyboard.h"
#include "serial.h"
#include "memory.h"
#include "fs.h"
#include "user.h"
#include "shell.h"
#include "gui.h"
#include "vga_gfx.h"

/* ── Modern Boot Splash ──────────────────────────────────── */

/* Draw a minimal geometric swan icon */
static void draw_swan_icon(int cx, int cy) {
    /* Outer ring (brand circle) */
    vga_draw_ring(cx, cy, 28, 2, 15);

    /* Swan body: a smooth curve inside the circle */
    /* Body = elliptical arc in bottom-right */
    for (int y = -8; y <= 10; y++) {
        for (int x = -14; x <= 14; x++) {
            /* Ellipse check for body */
            int ex = x - 2;
            int ey = y - 2;
            if ((ex * ex * 64 + ey * ey * 196) <= 196 * 64)
                vga_putpixel(cx + x, cy + y, 7); /* white */
        }
    }

    /* Neck: smooth curve going up-left */
    /* Using thick line segments approximating a curve */
    int neck_segs[][2] = {
        {-6, 0}, {-8, -4}, {-10, -8}, {-11, -12},
        {-10, -16}, {-8, -19}, {-5, -21},
    };
    for (int i = 0; i < 7; i++) {
        vga_fill_circle(cx + neck_segs[i][0], cy + neck_segs[i][1], 3, 7);
    }

    /* Head: small circle at top of neck */
    vga_fill_circle(cx - 3, cy - 22, 4, 7);

    /* Beak: small triangular accent */
    for (int i = 0; i < 5; i++) {
        vga_draw_hline(cx + 1 + i, cy - 23, 5 - i, 19); /* cyan accent */
    }

    /* Eye: single dark pixel */
    vga_putpixel(cx - 1, cy - 23, 1);
    vga_putpixel(cx, cy - 23, 1);
}

/* Draw subtle background gradient */
static void draw_modern_bg(void) {
    for (int y = 0; y < GFX_H; y++) {
        /* Very subtle dark gradient: darker at edges, slightly lighter in center */
        uint8_t base = 1; /* near-black */
        /* Radial-ish subtle gradient */
        int dy = y - GFX_H / 2;
        int dist = (dy * dy) / 200;
        if (dist > 5) dist = 5;
        uint8_t color = base; /* keep it very dark */
        /* Use palette indices 80-89 for subtle bg variation */
        color = 80 + (dist < 9 ? dist : 8);
        for (int x = 0; x < GFX_W; x++)
            vga_putpixel(x, y, color);
    }
}

/* Animated loading ring spinner */
static void draw_spinner(int cx, int cy, int r, int frame) {
    int segments = 12;
    for (int i = 0; i < segments; i++) {
        /* Calculate segment position around the circle */
        int angle_idx = (i * 360) / segments;
        /* Simple trig using lookup: cos/sin approximation with integer math */
        /* Positions pre-calculated for 12 segments (every 30 degrees) */
        /* Using fixed offsets for 12 points on a circle */
        int dx, dy;
        switch (i) {
            case 0:  dx = 0;  dy = -r; break;
            case 1:  dx = r/2;  dy = -(r*7)/8; break;
            case 2:  dx = (r*7)/8; dy = -r/2; break;
            case 3:  dx = r;  dy = 0; break;
            case 4:  dx = (r*7)/8; dy = r/2; break;
            case 5:  dx = r/2;  dy = (r*7)/8; break;
            case 6:  dx = 0;  dy = r; break;
            case 7:  dx = -r/2; dy = (r*7)/8; break;
            case 8:  dx = -(r*7)/8; dy = r/2; break;
            case 9:  dx = -r; dy = 0; break;
            case 10: dx = -(r*7)/8; dy = -r/2; break;
            case 11: dx = -r/2; dy = -(r*7)/8; break;
            default: dx = 0; dy = 0; break;
        }

        /* Color: bright for the "head" segments, dim for the "tail" */
        int dist_from_head = (i - frame % segments + segments) % segments;
        uint8_t color;
        if (dist_from_head < 3)
            color = 19 - dist_from_head; /* bright cyan */
        else if (dist_from_head < 6)
            color = 14 - dist_from_head; /* medium */
        else
            color = 70; /* dim */

        (void)angle_idx;
        vga_fill_circle(cx + dx, cy + dy, 2, color);
    }
}

/* Draw a thin horizontal accent line */
static void draw_accent_line(int y, int x1, int x2, int center) {
    for (int x = x1; x <= x2; x++) {
        /* Gradient: fade from edges to bright center */
        int dist = (x < center) ? (center - x) : (x - center);
        int max_dist = (x2 - x1) / 2;
        uint8_t color;
        if (max_dist > 0) {
            int intensity = 10 - (dist * 8) / max_dist;
            if (intensity < 3) intensity = 3;
            color = 10 + (intensity < 9 ? intensity : 8);
        } else {
            color = 15;
        }
        vga_putpixel(x, y, color);
    }
}

/* Modern loading bar */
static void draw_loading_bar(int x, int y, int w, int h, int progress, int total) {
    /* Background track */
    vga_fill_rect(x, y, w, h, 2);

    /* Filled portion with gradient */
    int filled = (progress * w) / total;
    for (int i = 0; i < filled; i++) {
        /* Gradient: blue → cyan → teal */
        uint8_t c;
        int pct = (i * 30) / w;
        if (pct > 9) pct = 9;
        c = 10 + pct; /* cyan gradient */
        for (int j = 0; j < h; j++)
            vga_putpixel(x + i, y + j, c);
    }

    /* Rounded ends: brighten the leading edge */
    if (filled > 0 && filled < w) {
        for (int j = 0; j < h; j++)
            vga_putpixel(x + filled, y + j, 7); /* bright white edge */
    }
}

static void gfx_boot_splash(void) {
    /* Disable serial mirroring during graphics mode */
    screen_set_serial_mirror(0);

    vga_gfx_init();
    vga_clear(0);

    /* Draw dark gradient background */
    draw_modern_bg();

    /* Fade in from black */
    vga_fade_from_black(12);

    /* ── Phase 1: Swan icon appears ── */
    screen_delay(300);
    int icon_cx = GFX_W / 2;
    int icon_cy = 55;

    /* Draw icon ring first, then fill */
    vga_draw_ring(icon_cx, icon_cy, 28, 2, 3); /* dim ring first */
    screen_delay(200);
    vga_draw_ring(icon_cx, icon_cy, 28, 2, 15); /* cyan ring */
    screen_delay(100);

    /* Swan body appears */
    draw_swan_icon(icon_cx, icon_cy);
    screen_delay(300);

    /* ── Phase 2: Title text ── */
    /* "SWANOS" in large 2x font, centered */
    const char *title = "SWANOS";
    int title_w = 6 * 18; /* 6 chars * 18px per char at 2x */
    int title_x = (GFX_W - title_w) / 2;
    int title_y = 95;

    /* Type each letter with a delay */
    for (int i = 0; title[i]; i++) {
        vga_draw_char_2x(title_x + i * 18, title_y, title[i], 7);
        screen_delay(60);
    }

    /* Accent line under title */
    screen_delay(100);
    int line_y = title_y + 20;
    draw_accent_line(line_y, icon_cx - 60, icon_cx + 60, icon_cx);
    draw_accent_line(line_y + 1, icon_cx - 55, icon_cx + 55, icon_cx);

    /* ── Phase 3: Subtitle ── */
    screen_delay(150);
    const char *sub1 = "LLM-POWERED";
    int sub1_w = 11 * 9;
    int sub1_x = (GFX_W - sub1_w) / 2;
    vga_draw_string(sub1_x, line_y + 8, sub1, 15); /* bright cyan */

    screen_delay(80);
    const char *sub2 = "OPERATING SYSTEM";
    int sub2_w = 16 * 9;
    int sub2_x = (GFX_W - sub2_w) / 2;
    vga_draw_string(sub2_x, line_y + 20, sub2, 5); /* light grey */

    /* ── Phase 4: Loading spinner + bar ── */
    screen_delay(200);
    int bar_x = (GFX_W - 160) / 2;
    int bar_y = 170;
    int bar_w = 160;
    int bar_h = 4;

    /* "INITIALIZING" label */
    const char *lbl = "INITIALIZING";
    int lbl_w = 12 * 9;
    int lbl_x = (GFX_W - lbl_w) / 2;
    vga_draw_string(lbl_x, bar_y - 14, lbl, 4); /* medium grey */

    /* Animate loading bar with spinner */
    int spinner_cx = GFX_W / 2;
    int spinner_cy = bar_y + 18;
    int total_steps = 40;

    for (int step = 0; step <= total_steps; step++) {
        /* Update bar */
        draw_loading_bar(bar_x, bar_y, bar_w, bar_h, step, total_steps);

        /* Draw spinner below bar */
        /* First clear the spinner area */
        vga_fill_rect(spinner_cx - 16, spinner_cy - 16, 32, 32, 80);
        draw_spinner(spinner_cx, spinner_cy, 10, step);

        vga_vsync();
        screen_delay(30);
    }

    /* Bar complete: flash to white briefly */
    draw_loading_bar(bar_x, bar_y, bar_w, bar_h, 1, 1);
    /* Replace label with "READY" */
    vga_fill_rect(lbl_x, bar_y - 14, lbl_w, 9, 80);
    const char *rdy = "READY";
    int rdy_w = 5 * 9;
    int rdy_x = (GFX_W - rdy_w) / 2;
    vga_draw_string(rdy_x, bar_y - 14, rdy, 35); /* green */

    /* Clear spinner, show checkmark-like indicator */
    vga_fill_rect(spinner_cx - 16, spinner_cy - 16, 32, 32, 80);
    vga_fill_circle(spinner_cx, spinner_cy, 8, 35); /* green circle */
    /* Simple check shape */
    vga_putpixel(spinner_cx - 3, spinner_cy, 7);
    vga_putpixel(spinner_cx - 2, spinner_cy + 1, 7);
    vga_putpixel(spinner_cx - 1, spinner_cy + 2, 7);
    vga_putpixel(spinner_cx, spinner_cy + 1, 7);
    vga_putpixel(spinner_cx + 1, spinner_cy, 7);
    vga_putpixel(spinner_cx + 2, spinner_cy - 1, 7);
    vga_putpixel(spinner_cx + 3, spinner_cy - 2, 7);

    screen_delay(1000);

    /* Smooth fade out */
    vga_fade_to_black(15);

    /* Switch back to text mode */
    vga_gfx_exit();
    screen_init();

    /* Re-enable serial mirroring for text mode */
    screen_set_serial_mirror(1);
}

/* ── Text-mode boot status ──────────────────────────────── */

static void boot_status(const char *msg) {
    screen_set_color(VGA_GREEN, VGA_BLACK);
    screen_print("   [OK] ");
    screen_set_color(VGA_WHITE, VGA_BLACK);
    screen_print(msg);
    screen_print("\n");
}

static int select_mode(void) {
    screen_set_color(VGA_DARK_GREY, VGA_BLACK);
    screen_print("\n   ");
    for (int i = 0; i < 58; i++) screen_putchar((char)196);
    screen_print("\n\n");

    screen_set_color(VGA_CYAN, VGA_BLACK);
    screen_print("   Select interface:\n\n");

    screen_set_color(VGA_WHITE, VGA_BLACK);
    screen_print("     ");
    screen_set_color(VGA_CYAN, VGA_BLACK);
    screen_print("[1]");
    screen_set_color(VGA_WHITE, VGA_BLACK);
    screen_print("  GUI Mode   ");
    screen_set_color(VGA_DARK_GREY, VGA_BLACK);
    screen_print("- Visual interface with panels & sidebar\n");

    screen_set_color(VGA_WHITE, VGA_BLACK);
    screen_print("     ");
    screen_set_color(VGA_GREEN, VGA_BLACK);
    screen_print("[2]");
    screen_set_color(VGA_WHITE, VGA_BLACK);
    screen_print("  CLI Mode   ");
    screen_set_color(VGA_DARK_GREY, VGA_BLACK);
    screen_print("- Classic command-line interface\n\n");

    screen_set_color(VGA_CYAN, VGA_BLACK);
    screen_print("   Press 1 or 2: ");
    screen_set_color(VGA_WHITE, VGA_BLACK);

    while (1) {
        char c = keyboard_getchar();
        if (c == '1') { screen_print("GUI\n"); return 1; }
        if (c == '2') { screen_print("CLI\n"); return 2; }
    }
}

void kernel_main(uint32_t magic, uint32_t mboot_info) {
    (void)magic;
    (void)mboot_info;

    /* ── Graphical boot splash ── */
    screen_init();
    gfx_boot_splash();

    /* ── Text mode initialization ── */
    screen_set_color(VGA_CYAN, VGA_BLACK);
    screen_print("\n   SWAN OS v2.0\n");
    screen_set_color(VGA_DARK_GREY, VGA_BLACK);
    screen_print("   LLM-Powered Operating System\n\n");
    screen_set_color(VGA_WHITE, VGA_BLACK);

    boot_status("VGA display initialized");

    idt_init();
    boot_status("Interrupt Descriptor Table loaded");

    timer_init(100);
    boot_status("PIT timer initialized (100 Hz)");

    keyboard_init();
    boot_status("PS/2 keyboard driver loaded");

    serial_init();
    boot_status("COM1 serial port initialized");

    memory_init();
    boot_status("Memory allocator ready (4 MB heap)");

    fs_init();
    fs_write("readme.txt",
        "Welcome to SwanOS!\n"
        "A bare-metal AI-powered operating system.\n"
        "Type 'help' for commands, 'ask <q>' to talk to AI.");
    fs_mkdir("documents");
    fs_mkdir("programs");
    boot_status("In-memory filesystem mounted");

    user_init();
    boot_status("User manager initialized");

    screen_set_color(VGA_GREEN, VGA_BLACK);
    screen_print("\n   All systems online.\n");
    screen_set_color(VGA_WHITE, VGA_BLACK);

    int mode = select_mode();

    while (1) {
        if (mode == 1) {
            screen_clear();
            screen_set_color(VGA_CYAN, VGA_BLACK);
            screen_print("\n   SWAN OS - Login\n\n");
            screen_set_color(VGA_WHITE, VGA_BLACK);
        }

        if (!user_login()) continue;
        screen_print("\n");

        if (mode == 1) {
            gui_run();
            mode = 2;
            screen_clear();
            screen_set_color(VGA_CYAN, VGA_BLACK);
            screen_print("\n  Switched to CLI mode.\n");
            screen_print("  Type 'gui' to switch back.\n\n");
            screen_set_color(VGA_WHITE, VGA_BLACK);
        }

        if (mode == 2) {
            shell_run();
            mode = select_mode();
        }
    }
}
