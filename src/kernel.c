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

/* ── Main boot splash ─────────────────────────────────────── */
static void gfx_boot_splash(void) {
    screen_set_serial_mirror(0);
    vga_gfx_init();
    vga_clear(0);

    /* Setup special splash palette entries */
    vga_set_palette(96, 2, 8, 20);   /* deep blue glow */
    vga_set_palette(97, 4, 14, 30);  /* medium blue glow */
    vga_set_palette(98, 8, 20, 40);  /* bright blue glow */

    int cx = GFX_W / 2;  /* 160 */

    /* ══════ PHASE 1: Background + Particles fade in ══════ */
    draw_splash_bg();
    vga_fade_from_black(8);

    /* Emit particles from center */
    particle_count = 0;
    for (int f = 0; f < 30; f++) {
        if (f % 2 == 0) {
            for (int p = 0; p < 3; p++)
                spawn_particle(cx, 60);
        }
        update_particles();
        /* Redraw bg + particles */
        draw_splash_bg();
        draw_particles();
        vga_vsync();
        screen_delay(20);
    }

    /* ══════ PHASE 2: Neural network animation ══════ */
    for (int f = 0; f < 40; f++) {
        draw_splash_bg();
        draw_particles();
        update_particles();
        draw_neural_net(cx, 65, f);

        /* Pulsing rings behind network */
        draw_pulse_rings(cx, 65, f);

        vga_vsync();
        screen_delay(25);
    }

    /* ══════ PHASE 3: Title reveal with glow ══════ */
    /* Clear neural net area, keep bg */
    draw_splash_bg();

    /* Draw the swan icon centered above title */
    int icon_cy = 45;
    /* Outer glow ring */
    vga_draw_ring(cx, icon_cy, 30, 1, 10);
    screen_delay(50);
    vga_draw_ring(cx, icon_cy, 28, 2, 15);
    screen_delay(50);

    /* Swan body */
    for (int y = -8; y <= 10; y++) {
        for (int x = -14; x <= 14; x++) {
            int ex = x - 2, ey = y - 2;
            if ((ex * ex * 64 + ey * ey * 196) <= 196 * 64)
                vga_putpixel(cx + x, icon_cy + y, 7);
        }
    }
    /* Neck */
    int neck[][2] = {{-6,0},{-8,-4},{-10,-8},{-11,-12},{-10,-16},{-8,-19},{-5,-21}};
    for (int i = 0; i < 7; i++)
        vga_fill_circle(cx + neck[i][0], icon_cy + neck[i][1], 3, 7);
    /* Head + beak */
    vga_fill_circle(cx - 3, icon_cy - 22, 4, 7);
    for (int i = 0; i < 5; i++)
        vga_draw_hline(cx + 1 + i, icon_cy - 23, 5 - i, 19);
    vga_putpixel(cx - 1, icon_cy - 23, 1);
    vga_putpixel(cx, icon_cy - 23, 1);
    screen_delay(200);

    /* Title: "SWANOS" in 3x scale for impact */
    const char *title = "SWANOS";
    int tw = 6 * 27; /* 3x scale: 9px * 3 per char */
    int tx = (GFX_W - tw) / 2;
    int ty = 82;
    for (int i = 0; title[i]; i++) {
        vga_draw_char_3x(tx + i * 27, ty, title[i], 7);
        screen_delay(50);
    }

    /* Glowing accent line */
    screen_delay(80);
    int ly = ty + 28;
    draw_glow_line(ly, cx - 70, cx + 70, cx);
    draw_glow_line(ly + 1, cx - 65, cx + 65, cx);

    /* ══════ PHASE 4: LLM branding ══════ */
    screen_delay(100);

    /* "ARTIFICIAL INTELLIGENCE" subtitle */
    const char *ai1 = "ARTIFICIAL INTELLIGENCE";
    int ai1_w = 23 * 7;
    int ai1_x = (GFX_W - ai1_w) / 2;
    vga_draw_string(ai1_x, ly + 6, ai1, 15);

    screen_delay(60);
    const char *ai2 = "OPERATING SYSTEM";
    int ai2_w = 16 * 7;
    int ai2_x = (GFX_W - ai2_w) / 2;
    vga_draw_string(ai2_x, ly + 17, ai2, 5);

    /* Version + model badge */
    screen_delay(40);
    const char *badge = "v3.0  |  LLM-Powered  |  x86";
    int badge_w = 29 * 7;
    int badge_x = (GFX_W - badge_w) / 2;
    vga_draw_string(badge_x, ly + 30, badge, 3);

    /* ══════ PHASE 5: Boot progress with spinner ══════ */
    screen_delay(150);
    int bar_x = (GFX_W - 180) / 2;
    int bar_y = 168;
    int bar_w = 180;
    int bar_h = 3;

    const char *loading_msgs[] = {
        "Loading neural engine...",
        "Initializing AI core...",
        "Preparing workspace...",
        "Starting SwanOS...",
    };

    int spinner_cx = cx;
    int spinner_cy = bar_y + 16;
    int total_steps = 48;

    for (int step = 0; step <= total_steps; step++) {
        /* Update loading message */
        int msg_idx = (step * 4) / (total_steps + 1);
        if (msg_idx > 3) msg_idx = 3;

        /* Clear status text area */
        vga_fill_rect(0, bar_y - 14, GFX_W, 10, 80);
        int msg_w = strlen(loading_msgs[msg_idx]) * 7;
        int msg_x = (GFX_W - msg_w) / 2;
        vga_draw_string(msg_x, bar_y - 12, loading_msgs[msg_idx], 4);

        draw_glow_bar(bar_x, bar_y, bar_w, bar_h, step, total_steps);

        /* Spinner */
        vga_fill_rect(spinner_cx - 12, spinner_cy - 12, 24, 24, 80);
        draw_dot_spinner(spinner_cx, spinner_cy, 8, step);

        /* Generate some particles near the bar for effect */
        if (step % 6 == 0) {
            int px = bar_x + (step * bar_w) / total_steps;
            spawn_particle(px, bar_y);
        }
        update_particles();
        draw_particles();

        vga_vsync();
        screen_delay(35);
    }

    /* ══════ PHASE 6: Completion ══════ */
    vga_fill_rect(0, bar_y - 14, GFX_W, 10, 80);
    const char *ready = "AI SYSTEMS ONLINE";
    int rw = 17 * 7;
    int rx = (GFX_W - rw) / 2;
    vga_draw_string(rx, bar_y - 12, ready, 35);

    /* Replace spinner with checkmark */
    vga_fill_rect(spinner_cx - 12, spinner_cy - 12, 24, 24, 80);
    vga_fill_circle(spinner_cx, spinner_cy, 7, 35);
    /* Checkmark */
    vga_putpixel(spinner_cx - 3, spinner_cy, 7);
    vga_putpixel(spinner_cx - 2, spinner_cy + 1, 7);
    vga_putpixel(spinner_cx - 1, spinner_cy + 2, 7);
    vga_putpixel(spinner_cx, spinner_cy + 1, 7);
    vga_putpixel(spinner_cx + 1, spinner_cy, 7);
    vga_putpixel(spinner_cx + 2, spinner_cy - 1, 7);
    vga_putpixel(spinner_cx + 3, spinner_cy - 2, 7);

    /* Final particle burst */
    for (int p = 0; p < 15; p++)
        spawn_particle(spinner_cx, spinner_cy);
    for (int f = 0; f < 20; f++) {
        update_particles();
        draw_particles();
        vga_vsync();
        screen_delay(20);
    }

    screen_delay(800);

    /* Smooth fade out */
    vga_fade_to_black(10);

    /* Switch back to text mode */
    vga_gfx_exit();
    screen_init();
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
