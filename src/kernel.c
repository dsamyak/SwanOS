/* ============================================================
 * SwanOS — Kernel Main
 * Premium graphical boot → styled text mode → login → shell/GUI
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
#include "vga_gfx.h"
#include "mouse.h"
#include "desktop.h"

/* ── Modern Boot Splash ──────────────────────────────────── */

/* Draw a minimal geometric swan icon */
static void draw_swan_icon(int cx, int cy) {
    /* Outer ring (brand circle) */
    vga_draw_ring(cx, cy, 28, 2, 15);

    /* Swan body: a smooth curve inside the circle */
    for (int y = -8; y <= 10; y++) {
        for (int x = -14; x <= 14; x++) {
            int ex = x - 2;
            int ey = y - 2;
            if ((ex * ex * 64 + ey * ey * 196) <= 196 * 64)
                vga_putpixel(cx + x, cy + y, 7);
        }
    }

    /* Neck: smooth curve going up-left */
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
        vga_draw_hline(cx + 1 + i, cy - 23, 5 - i, 19);
    }

    /* Eye: single dark pixel */
    vga_putpixel(cx - 1, cy - 23, 1);
    vga_putpixel(cx, cy - 23, 1);
}

/* Starfield background with twinkling */
static void draw_starfield(void) {
    /* Seed for pseudo-random star placement */
    uint32_t seed = 0xDEAD;
    for (int i = 0; i < 80; i++) {
        seed = seed * 1103515245 + 12345;
        int x = (seed >> 16) % GFX_W;
        seed = seed * 1103515245 + 12345;
        int y = (seed >> 16) % GFX_H;
        seed = seed * 1103515245 + 12345;
        uint8_t brightness = (seed >> 16) % 4;
        /* Different star brightnesses for depth effect */
        uint8_t color;
        if (brightness == 0) color = 3;       /* dim grey-blue */
        else if (brightness == 1) color = 4;  /* medium grey */
        else if (brightness == 2) color = 5;  /* light grey */
        else color = 7;                        /* bright white */
        vga_putpixel(x, y, color);
    }
}

/* Draw subtle dark gradient background */
static void draw_modern_bg(void) {
    for (int y = 0; y < GFX_H; y++) {
        int dy = y - GFX_H / 2;
        int dist = (dy * dy) / 200;
        if (dist > 5) dist = 5;
        uint8_t color = 80 + (dist < 9 ? dist : 8);
        for (int x = 0; x < GFX_W; x++)
            vga_putpixel(x, y, color);
    }
    /* Add starfield on top of gradient */
    draw_starfield();
}

/* Animated loading ring spinner */
static void draw_spinner(int cx, int cy, int r, int frame) {
    int segments = 12;
    for (int i = 0; i < segments; i++) {
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

        int dist_from_head = (i - frame % segments + segments) % segments;
        uint8_t color;
        if (dist_from_head < 3)
            color = 19 - dist_from_head;
        else if (dist_from_head < 6)
            color = 14 - dist_from_head;
        else
            color = 70;

        vga_fill_circle(cx + dx, cy + dy, 2, color);
    }
}

/* Draw a thin horizontal accent line with gradient */
static void draw_accent_line(int y, int x1, int x2, int center) {
    for (int x = x1; x <= x2; x++) {
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

/* Modern loading bar with gradient fill */
static void draw_loading_bar(int x, int y, int w, int h, int progress, int total) {
    vga_fill_rect(x, y, w, h, 2);

    int filled = (progress * w) / total;
    for (int i = 0; i < filled; i++) {
        uint8_t c;
        int pct = (i * 30) / w;
        if (pct > 9) pct = 9;
        c = 10 + pct;
        for (int j = 0; j < h; j++)
            vga_putpixel(x + i, y + j, c);
    }

    if (filled > 0 && filled < w) {
        for (int j = 0; j < h; j++)
            vga_putpixel(x + filled, y + j, 7);
    }
}

static void gfx_boot_splash(void) {
    screen_set_serial_mirror(0);

    vga_gfx_init();
    vga_clear(0);

    /* Draw dark gradient background with starfield */
    draw_modern_bg();

    /* Fade in from black */
    vga_fade_from_black(12);

    /* ── Phase 1: Swan icon appears ── */
    screen_delay(300);
    int icon_cx = GFX_W / 2;
    int icon_cy = 55;

    /* Draw icon ring first, then fill */
    vga_draw_ring(icon_cx, icon_cy, 28, 2, 3);
    screen_delay(200);
    vga_draw_ring(icon_cx, icon_cy, 28, 2, 15);
    screen_delay(100);

    /* Swan body appears */
    draw_swan_icon(icon_cx, icon_cy);
    screen_delay(300);

    /* ── Phase 2: Title text ── */
    const char *title = "SWANOS";
    int title_w = 6 * 18;
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
    vga_draw_string(sub1_x, line_y + 8, sub1, 15);

    screen_delay(80);
    const char *sub2 = "OPERATING SYSTEM";
    int sub2_w = 16 * 9;
    int sub2_x = (GFX_W - sub2_w) / 2;
    vga_draw_string(sub2_x, line_y + 20, sub2, 5);

    /* Version badge */
    screen_delay(60);
    const char *ver = "V2.0";
    int ver_w = 4 * 9;
    int ver_x = (GFX_W - ver_w) / 2;
    vga_draw_string(ver_x, line_y + 32, ver, 3);

    /* ── Phase 4: Loading spinner + bar ── */
    screen_delay(200);
    int bar_x = (GFX_W - 160) / 2;
    int bar_y = 170;
    int bar_w = 160;
    int bar_h = 4;

    const char *lbl = "INITIALIZING";
    int lbl_w = 12 * 9;
    int lbl_x = (GFX_W - lbl_w) / 2;
    vga_draw_string(lbl_x, bar_y - 14, lbl, 4);

    /* Animate loading bar with spinner */
    int spinner_cx = GFX_W / 2;
    int spinner_cy = bar_y + 18;
    int total_steps = 40;

    for (int step = 0; step <= total_steps; step++) {
        draw_loading_bar(bar_x, bar_y, bar_w, bar_h, step, total_steps);

        vga_fill_rect(spinner_cx - 16, spinner_cy - 16, 32, 32, 80);
        draw_spinner(spinner_cx, spinner_cy, 10, step);

        vga_vsync();
        screen_delay(30);
    }

    /* Bar complete: flash to white briefly */
    draw_loading_bar(bar_x, bar_y, bar_w, bar_h, 1, 1);
    vga_fill_rect(lbl_x, bar_y - 14, lbl_w, 9, 80);
    const char *rdy = "READY";
    int rdy_w = 5 * 9;
    int rdy_x = (GFX_W - rdy_w) / 2;
    vga_draw_string(rdy_x, bar_y - 14, rdy, 35);

    /* Clear spinner, show checkmark-like indicator */
    vga_fill_rect(spinner_cx - 16, spinner_cy - 16, 32, 32, 80);
    vga_fill_circle(spinner_cx, spinner_cy, 8, 35);
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

/* ── Styled Text-mode Boot Sequence ─────────────────────── */

static int boot_step = 0;
#define BOOT_TOTAL 8

/* Draw the boot progress bar at the bottom */
static void draw_boot_progress(void) {
    int bar_row = 20;
    int bar_col = 20;
    int bar_width = 40;

    screen_put_str_at(bar_row, 5, "Loading:", VGA_DARK_GREY, VGA_BLACK);

    /* Progress background */
    for (int i = 0; i < bar_width; i++)
        screen_put_char_at(bar_row, bar_col + i, (char)176, VGA_DARK_GREY, VGA_BLACK);

    /* Filled portion */
    int filled = (boot_step * bar_width) / BOOT_TOTAL;
    for (int i = 0; i < filled; i++)
        screen_put_char_at(bar_row, bar_col + i, (char)219, VGA_CYAN, VGA_BLACK);

    /* Percentage */
    char pct_buf[8]; char tmp[8];
    int pct = (boot_step * 100) / BOOT_TOTAL;
    itoa(pct, tmp, 10);
    strcpy(pct_buf, tmp);
    strcat(pct_buf, "%");
    screen_put_str_at(bar_row, bar_col + bar_width + 2, "     ", VGA_WHITE, VGA_BLACK);
    screen_put_str_at(bar_row, bar_col + bar_width + 2, pct_buf, VGA_WHITE, VGA_BLACK);
}

static void boot_status(const char *msg) {
    boot_step++;

    /* Step indicator */
    screen_set_color(VGA_DARK_GREY, VGA_BLACK);
    screen_print("   ");
    screen_set_color(VGA_GREEN, VGA_BLACK);
    screen_putchar((char)254);  /* ■ bullet */
    screen_print(" ");
    screen_set_color(VGA_GREEN, VGA_BLACK);
    screen_print("OK ");
    screen_set_color(VGA_DARK_GREY, VGA_BLACK);
    screen_putchar((char)179);  /* │ */
    screen_putchar(' ');
    screen_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    screen_print(msg);
    screen_print("\n");

    /* Update progress bar */
    draw_boot_progress();

    /* Small delay for visual effect */
    screen_delay(80);
}

/* ── Styled mode selection ───────────────────────────────── */

static void draw_mode_card(int row, int col, int width, const char *num,
                           const char *title, const char *desc,
                           uint8_t accent, int selected) {
    uint8_t border_color = selected ? accent : VGA_DARK_GREY;
    uint8_t bg = VGA_BLACK;

    /* Top border */
    screen_put_char_at(row, col, (char)218, border_color, bg);
    for (int i = 1; i < width - 1; i++)
        screen_put_char_at(row, col + i, (char)196, border_color, bg);
    screen_put_char_at(row, col + width - 1, (char)191, border_color, bg);

    /* Content row 1: number + title */
    screen_put_char_at(row + 1, col, (char)179, border_color, bg);
    screen_put_str_at(row + 1, col + 2, num, accent, bg);
    screen_put_str_at(row + 1, col + 5, title, VGA_WHITE, bg);
    for (int i = 5 + (int)strlen(title); i < width - 1; i++)
        screen_put_char_at(row + 1, col + i, ' ', VGA_WHITE, bg);
    screen_put_char_at(row + 1, col + width - 1, (char)179, border_color, bg);

    /* Content row 2: description */
    screen_put_char_at(row + 2, col, (char)179, border_color, bg);
    screen_put_char_at(row + 2, col + 2, (char)250, VGA_DARK_GREY, bg); /* · */
    screen_put_str_at(row + 2, col + 4, desc, VGA_DARK_GREY, bg);
    for (int i = 4 + (int)strlen(desc); i < width - 1; i++)
        screen_put_char_at(row + 2, col + i, ' ', VGA_WHITE, bg);
    screen_put_char_at(row + 2, col + width - 1, (char)179, border_color, bg);

    /* Bottom border */
    screen_put_char_at(row + 3, col, (char)192, border_color, bg);
    for (int i = 1; i < width - 1; i++)
        screen_put_char_at(row + 3, col + i, (char)196, border_color, bg);
    screen_put_char_at(row + 3, col + width - 1, (char)217, border_color, bg);
}

static int select_mode(void) {
    /* Clear progress bar area */
    for (int r = 18; r < 25; r++)
        screen_fill_row(r, 0, 79, ' ', VGA_WHITE, VGA_BLACK);

    /* Section divider */
    screen_set_color(VGA_DARK_GREY, VGA_BLACK);
    screen_print("\n   ");
    for (int i = 0; i < 58; i++) screen_putchar((char)196);
    screen_print("\n\n");

    /* Header */
    screen_set_color(VGA_CYAN, VGA_BLACK);
    screen_print("   ");
    screen_putchar((char)254);
    screen_print(" Select Interface\n\n");
    screen_set_color(VGA_WHITE, VGA_BLACK);

    /* Draw both mode cards side by side */
    int card_row = screen_get_row();
    draw_mode_card(card_row, 6,  32, "[1]", "GUI Mode",
                   "Panels, sidebar, visual UI", VGA_CYAN, 1);
    draw_mode_card(card_row, 42, 32, "[2]", "CLI Mode",
                   "Classic command-line shell", VGA_GREEN, 1);

    /* Hint */
    screen_set_cursor(card_row + 5, 0);
    screen_set_color(VGA_DARK_GREY, VGA_BLACK);
    screen_print("\n   Press ");
    screen_set_color(VGA_YELLOW, VGA_BLACK);
    screen_print("1");
    screen_set_color(VGA_DARK_GREY, VGA_BLACK);
    screen_print(" or ");
    screen_set_color(VGA_YELLOW, VGA_BLACK);
    screen_print("2");
    screen_set_color(VGA_DARK_GREY, VGA_BLACK);
    screen_print(" to continue: ");
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

    /* ── Graphical boot splash (returns in text mode) ── */
    gfx_boot_splash();

    /* Ensure clean text-mode screen */
    screen_clear();

    /* ── Styled boot header ── */
    screen_set_color(VGA_DARK_GREY, VGA_BLACK);
    screen_print("   ");
    for (int i = 0; i < 58; i++) screen_putchar((char)205); /* ═ */
    screen_print("\n");
    screen_set_color(VGA_CYAN, VGA_BLACK);
    screen_print("   ");
    screen_putchar((char)6);  /* ♠ swan */
    screen_print(" SWAN");
    screen_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    screen_print("OS");
    screen_set_color(VGA_DARK_GREY, VGA_BLACK);
    screen_print(" v2.0  ");
    screen_putchar((char)250); /* · */
    screen_print("  LLM-Powered Operating System\n");
    screen_set_color(VGA_DARK_GREY, VGA_BLACK);
    screen_print("   ");
    for (int i = 0; i < 58; i++) screen_putchar((char)205);
    screen_print("\n\n");
    screen_set_color(VGA_WHITE, VGA_BLACK);

    /* ── Boot subsystems with progress ── */
    boot_status("VGA display initialized");

    idt_init();
    boot_status("Interrupt Descriptor Table loaded");

    timer_init(100);
    boot_status("PIT timer @ 100 Hz");

    serial_init();
    boot_status("COM1 serial port ready");

    keyboard_init();
    boot_status("PS/2 keyboard driver loaded");

    memory_init();
    boot_status("Memory allocator ready (4 MB heap)");

    mouse_init();
    boot_status("PS/2 mouse driver loaded");

    fs_init();
    fs_write("readme.txt",
        "Welcome to SwanOS!\n"
        "A bare-metal AI-powered operating system.\n"
        "Type 'help' for commands, 'ask <q>' to talk to AI.");
    fs_mkdir("documents");
    fs_mkdir("programs");
    boot_status("In-memory filesystem mounted");

    user_init();

    /* Clear progress bar and show completion */
    for (int r = 19; r < 22; r++)
        screen_fill_row(r, 0, 79, ' ', VGA_WHITE, VGA_BLACK);

    screen_print("\n   ");
    screen_set_color(VGA_GREEN, VGA_BLACK);
    screen_putchar((char)254);
    screen_print(" All systems online");
    screen_set_color(VGA_DARK_GREY, VGA_BLACK);
    screen_print("  ─  Ready\n");
    screen_set_color(VGA_WHITE, VGA_BLACK);

    int mode = select_mode();

    while (1) {
        if (mode == 1) {
            screen_clear();
            screen_set_color(VGA_CYAN, VGA_BLACK);
            screen_print("\n   ");
            screen_putchar((char)6);
            screen_print(" SWAN OS - Login\n\n");
            screen_set_color(VGA_WHITE, VGA_BLACK);
        }

        if (!user_login()) continue;
        screen_print("\n");

        if (mode == 1) {
            desktop_run();
            mode = 2;
            screen_clear();
            screen_set_color(VGA_DARK_GREY, VGA_BLACK);
            screen_print("   ");
            for (int i = 0; i < 50; i++) screen_putchar((char)196);
            screen_print("\n");
            screen_set_color(VGA_CYAN, VGA_BLACK);
            screen_print("   ");
            screen_putchar((char)254);
            screen_print(" Switched to CLI mode\n");
            screen_set_color(VGA_DARK_GREY, VGA_BLACK);
            screen_print("   Type 'gui' to switch back\n");
            screen_set_color(VGA_DARK_GREY, VGA_BLACK);
            screen_print("   ");
            for (int i = 0; i < 50; i++) screen_putchar((char)196);
            screen_print("\n\n");
            screen_set_color(VGA_WHITE, VGA_BLACK);
        }

        if (mode == 2) {
            int sh_ret = shell_run();
            /* Check if shell exited to switch to GUI */
            if (sh_ret == -4) {
                mode = 1;
            } else {
                /* User logged out, fall back to mode select */
                mode = select_mode();
            }
        }
    }
}
