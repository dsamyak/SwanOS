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
#include "paging.h"
#include "gdt.h"
#include "process.h"
#include "syscall.h"
#include "multiboot.h"
#include "llm.h"
#include "network.h"

/* ── Advanced Boot Splash ────────────────────────────────── */
/* Particle system, neural network nodes, pulsing rings,
   and smooth animations for a premium LLM-OS boot. */

/* Simple pseudo-random number generator */
static uint32_t splash_seed = 0xABCD1234;
static uint32_t splash_rand(void) {
    splash_seed = splash_seed * 1103515245 + 12345;
    return (splash_seed >> 16) & 0x7FFF;
}

/* ── Particle system ─────────────────────────────────────── */
#define NUM_PARTICLES 50
typedef struct {
    int x, y;      /* position (fixed-point: /16) */
    int vx, vy;    /* velocity */
    uint8_t life;  /* frames remaining */
    uint8_t color;
} particle_t;

static particle_t particles[NUM_PARTICLES];
static int particle_count = 0;

static void spawn_particle(int cx, int cy) {
    if (particle_count >= NUM_PARTICLES) return;
    particle_t *p = &particles[particle_count++];
    p->x = cx * 16;
    p->y = cy * 16;
    p->vx = (int)(splash_rand() % 64) - 32;
    p->vy = (int)(splash_rand() % 64) - 32;
    p->life = 20 + splash_rand() % 30;
    p->color = 10 + splash_rand() % 10; /* cyan gradient */
}

static void update_particles(void) {
    for (int i = 0; i < particle_count; i++) {
        particles[i].x += particles[i].vx;
        particles[i].y += particles[i].vy;
        particles[i].vy += 1; /* gravity */
        particles[i].life--;
        if (particles[i].life == 0) {
            particles[i] = particles[--particle_count];
            i--;
        }
    }
}

static void draw_particles(void) {
    for (int i = 0; i < particle_count; i++) {
        int px = particles[i].x / 16;
        int py = particles[i].y / 16;
        if (px >= 0 && px < GFX_W && py >= 0 && py < GFX_H) {
            uint8_t c = particles[i].color;
            /* Fade with life */
            if (particles[i].life < 10) {
                c = 10 + particles[i].life / 3;
            }
            vga_putpixel(px, py, c);
            if (px + 1 < GFX_W) vga_putpixel(px + 1, py, c);
        }
    }
}

/* ── Neural network nodes ─────────────────────────────────── */
#define NUM_NODES 12
typedef struct { int x, y; } node_t;

static void draw_neural_net(int cx, int cy, int frame) {
    /* 3 layers of nodes */
    node_t nodes[NUM_NODES];
    int ni = 0;

    /* Layer 1 (left): 4 nodes */
    for (int i = 0; i < 4; i++) {
        nodes[ni].x = cx - 40;
        nodes[ni].y = cy - 24 + i * 16;
        ni++;
    }
    /* Layer 2 (center): 4 nodes */
    for (int i = 0; i < 4; i++) {
        nodes[ni].x = cx;
        nodes[ni].y = cy - 24 + i * 16;
        ni++;
    }
    /* Layer 3 (right): 4 nodes */
    for (int i = 0; i < 4; i++) {
        nodes[ni].x = cx + 40;
        nodes[ni].y = cy - 24 + i * 16;
        ni++;
    }

    /* Draw connections (lines between layers) */
    for (int l1 = 0; l1 < 4; l1++) {
        for (int l2 = 4; l2 < 8; l2++) {
            /* Animated pulse along connection */
            int active = ((frame + l1 * 3 + l2) % 12 < 4);
            uint8_t lc = active ? 12 : 2;
            /* Simple line: just horizontal + vertical segments */
            int x1 = nodes[l1].x + 3, y1 = nodes[l1].y;
            int x2 = nodes[l2].x - 3, y2 = nodes[l2].y;
            int mx = (x1 + x2) / 2;
            vga_draw_hline(x1, y1, mx - x1, lc);
            if (y1 < y2) vga_draw_vline(mx, y1, y2 - y1, lc);
            else if (y1 > y2) vga_draw_vline(mx, y2, y1 - y2, lc);
            vga_draw_hline(mx, y2, x2 - mx, lc);
        }
    }
    for (int l2 = 4; l2 < 8; l2++) {
        for (int l3 = 8; l3 < 12; l3++) {
            int active = ((frame + l2 * 2 + l3 + 6) % 12 < 4);
            uint8_t lc = active ? 12 : 2;
            int x1 = nodes[l2].x + 3, y1 = nodes[l2].y;
            int x2 = nodes[l3].x - 3, y2 = nodes[l3].y;
            int mx = (x1 + x2) / 2;
            vga_draw_hline(x1, y1, mx - x1, lc);
            if (y1 < y2) vga_draw_vline(mx, y1, y2 - y1, lc);
            else if (y1 > y2) vga_draw_vline(mx, y2, y1 - y2, lc);
            vga_draw_hline(mx, y2, x2 - mx, lc);
        }
    }

    /* Draw nodes on top */
    for (int i = 0; i < NUM_NODES; i++) {
        int pulse = ((frame + i * 5) % 20 < 10);
        uint8_t nc = pulse ? 15 : 12;
        vga_fill_circle(nodes[i].x, nodes[i].y, 3, nc);
        vga_draw_circle(nodes[i].x, nodes[i].y, 3, 10);
    }
}

/* ── Pulsing concentric rings ─────────────────────────────── */
static void draw_pulse_rings(int cx, int cy, int frame) {
    for (int r = 0; r < 4; r++) {
        int radius = 22 + r * 10 + (frame % 20) / 2;
        if (radius > 55) radius -= 40;
        int alpha_dist = radius - 22;
        uint8_t c;
        if (alpha_dist < 10) c = 13 - alpha_dist / 3;
        else c = 2;
        if (c < 2) c = 2;
        vga_draw_circle(cx, cy, radius, c);
    }
}

/* ── Gradient background for splash ───────────────────────── */
static void draw_splash_bg(void) {
    /* Radial gradient: dark at edges, subtly brighter at center */
    int cx = GFX_W / 2, cy = GFX_H / 2;
    for (int y = 0; y < GFX_H; y++) {
        for (int x = 0; x < GFX_W; x++) {
            int dx = x - cx, dy = y - cy;
            int dist = (dx * dx) / 100 + (dy * dy) / 40;
            if (dist > 30) dist = 30;
            /* Map to palette 80-85 (dark gradient) */
            int shade = 80 + (30 - dist) / 6;
            if (shade > 85) shade = 85;
            if (shade < 80) shade = 80;
            vga_putpixel(x, y, (uint8_t)shade);
        }
    }
    /* Stars */
    for (int i = 0; i < 100; i++) {
        int sx = splash_rand() % GFX_W;
        int sy = splash_rand() % GFX_H;
        uint8_t sc = (splash_rand() % 4 == 0) ? 7 : (splash_rand() % 2 ? 4 : 3);
        vga_putpixel(sx, sy, sc);
    }
}

/* ── Glowing accent lines ─────────────────────────────────── */
static void draw_glow_line(int y, int x1, int x2, int center) {
    for (int x = x1; x <= x2; x++) {
        int dist = (x < center) ? (center - x) : (x - center);
        int half = (x2 - x1) / 2;
        if (half <= 0) half = 1;
        int bright = 10 - (dist * 8) / half;
        if (bright < 0) bright = 0;
        uint8_t c = 10 + (bright < 9 ? bright : 8);
        vga_putpixel(x, y, c);
        /* Glow above and below */
        if (bright > 4) {
            if (y > 0) vga_putpixel(x, y - 1, 10 + bright / 3);
            if (y < GFX_H - 1) vga_putpixel(x, y + 1, 10 + bright / 3);
        }
    }
}

/* ── Loading bar with glow ────────────────────────────────── */
static void draw_glow_bar(int x, int y, int w, int h, int progress, int total) {
    /* Track background */
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++)
            vga_putpixel(x + i, y + j, 2);

    int filled = (progress * w) / total;
    for (int i = 0; i < filled; i++) {
        int pct = (i * 10) / w;
        if (pct > 9) pct = 9;
        uint8_t c = 10 + pct;
        for (int j = 0; j < h; j++)
            vga_putpixel(x + i, y + j, c);
    }
    /* Glow at the leading edge */
    if (filled > 0 && filled < w) {
        for (int j = -1; j <= h; j++) {
            int py = y + j;
            if (py >= 0 && py < GFX_H)
                vga_putpixel(x + filled, py, 15);
        }
    }
}

/* ── Animated spinner (ring of dots) ──────────────────────── */
static void draw_dot_spinner(int cx, int cy, int r, int frame) {
    int segs = 10;
    for (int i = 0; i < segs; i++) {
        /* Calculate position using lookup */
        int angle_idx = (i * 36) % 360;
        int dx = 0, dy = 0;
        /* Simplified trig using 12 positions */
        int pos = (i * 12) / segs;
        switch (pos) {
            case 0:  dx = 0;       dy = -r;     break;
            case 1:  dx = r/2;     dy = -(r*7)/8; break;
            case 2:  dx = (r*7)/8; dy = -r/3;   break;
            case 3:  dx = r;       dy = 0;      break;
            case 4:  dx = (r*7)/8; dy = r/3;    break;
            case 5:  dx = r/2;     dy = (r*7)/8;break;
            case 6:  dx = 0;       dy = r;      break;
            case 7:  dx = -r/2;    dy = (r*7)/8;break;
            case 8:  dx = -(r*7)/8;dy = r/3;    break;
            case 9:  dx = -r;      dy = 0;      break;
            case 10: dx = -(r*7)/8;dy = -r/3;   break;
            case 11: dx = -r/2;    dy = -(r*7)/8;break;
        }
        (void)angle_idx;

        int dist = (i - frame % segs + segs) % segs;
        uint8_t c;
        if (dist < 3) c = 15;
        else if (dist < 5) c = 12;
        else c = 3;
        vga_fill_circle(cx + dx, cy + dy, 1, c);
    }
}

static void gfx_boot_splash(void) {
    screen_set_serial_mirror(0);
    // VESA is initialized in kernel_main before this
    vga_clear(0);
    int cx = GFX_W / 2;
    int cy = GFX_H / 2;
    const char *msg = "SwanOS v3.0 - Booting AI Microkernel";
    vga_draw_string_3x(cx - (strlen(msg)*26/2), cy, msg, 0xFFFFFFFF);
    screen_delay(500);
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

void kernel_main(uint32_t magic, uint32_t mboot_info_addr) {
    (void)magic;
    multiboot_info_t *mboot = (multiboot_info_t *)mboot_info_addr;

    /* ── Initialize VESA framebuffer from multiboot info ── */
    vesa_gfx_init(mboot);
    screen_init();

    /* ── Graphical boot splash ── */
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
    screen_print(" v3.0  ");
    screen_putchar((char)250); /* · */
    screen_print("  LLM-Powered Operating System\n");
    screen_set_color(VGA_DARK_GREY, VGA_BLACK);
    screen_print("   ");
    for (int i = 0; i < 58; i++) screen_putchar((char)205);
    screen_print("\n\n");
    screen_set_color(VGA_WHITE, VGA_BLACK);

    /* ── Boot subsystems with progress ── */
    boot_status("VGA display initialized");

    gdt_init();
    boot_status("Global Descriptor Table & TSS loaded");

    idt_init();
    boot_status("Interrupt Descriptor Table loaded");

    process_init();
    boot_status("Process scheduler initialized");

    syscall_init();
    boot_status("System calls (int 0x80) enabled");

    timer_init(100);
    boot_status("PIT timer @ 100 Hz");

    serial_init();
    boot_status("COM1 serial port ready");

    keyboard_init();
    boot_status("PS/2 keyboard driver loaded");

    memory_init();
    boot_status("Memory allocator ready (4 MB heap)");

    paging_init();
    boot_status("Virtual memory paging enabled");

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

    llm_init();
    if (llm_ready())
        boot_status("Groq LLM engine initialized (API key loaded)");
    else
        boot_status("Groq LLM engine initialized (no API key - use 'setkey')");

    net_init();
    if (net_get_status()->detected)
        boot_status("Network interface detected");
    else
        boot_status("No network adapter found");

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

        keyboard_flush();            /* clear stale serial/keyboard data */
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
