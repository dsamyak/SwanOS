/* ============================================================
 * SwanOS — PS/2 Mouse Driver
 * Initializes the PS/2 auxiliary device (mouse) and handles
 * IRQ12 interrupts to track position and button state.
 * ============================================================ */

#include "mouse.h"
#include "idt.h"
#include "ports.h"
#include "vga_gfx.h"

/* ── Mouse state ──────────────────────────────────────────── */
static volatile int      mouse_x = 0;
static volatile int      mouse_y = 0;
static volatile uint8_t  mouse_buttons = 0;
static volatile int      mouse_moved_flag = 0;
static volatile int      mouse_clicked_flag = 0;

/* PS/2 mouse sends 3-byte packets */
static volatile uint8_t  mouse_cycle = 0;
static volatile int8_t   mouse_bytes[3];

/* ── PS/2 controller helpers ──────────────────────────────── */

static void mouse_wait_input(void) {
    /* Wait until the controller input buffer is empty (bit 1 clear) */
    int timeout = 100000;
    while (timeout--) {
        if (!(inb(0x64) & 0x02)) return;
    }
}

static void mouse_wait_output(void) {
    /* Wait until data is available in the output buffer (bit 0 set) */
    int timeout = 100000;
    while (timeout--) {
        if (inb(0x64) & 0x01) return;
    }
}

static void mouse_write(uint8_t data) {
    /* Tell the controller we're writing to the mouse */
    mouse_wait_input();
    outb(0x64, 0xD4);
    mouse_wait_input();
    outb(0x60, data);
}

static uint8_t mouse_read(void) {
    mouse_wait_output();
    return inb(0x60);
}

/* ── IRQ12 handler ────────────────────────────────────────── */

static void mouse_callback(registers_t *regs) {
    (void)regs;

    uint8_t status = inb(0x64);
    /* Check bit 5 (aux data) and bit 0 (data available) */
    if (!(status & 0x20)) return;

    uint8_t data = inb(0x60);

    switch (mouse_cycle) {
        case 0:
            mouse_bytes[0] = (int8_t)data;
            /* Verify bit 3 is always set in the first byte */
            if (data & 0x08) {
                mouse_cycle = 1;
            }
            break;
        case 1:
            mouse_bytes[1] = (int8_t)data;
            mouse_cycle = 2;
            break;
        case 2:
            mouse_bytes[2] = (int8_t)data;
            mouse_cycle = 0;

            /* Process the complete packet */
            uint8_t flags = (uint8_t)mouse_bytes[0];
            int dx = mouse_bytes[1];
            int dy = mouse_bytes[2];

            /* Handle sign extension from flags byte */
            if (flags & 0x10) dx |= 0xFFFFFF00;
            if (flags & 0x20) dy |= 0xFFFFFF00;

            /* Check overflow — discard packet */
            if (flags & 0xC0) break;

            /* Update position (PS/2 Y is inverted) */
            mouse_x += dx;
            mouse_y -= dy;

            /* Clamp to screen */
            if (mouse_x < 0) mouse_x = 0;
            if (mouse_x >= GFX_W) mouse_x = GFX_W - 1;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_y >= GFX_H) mouse_y = GFX_H - 1;

            /* Update buttons */
            uint8_t new_buttons = flags & 0x07;
            if (new_buttons && !mouse_buttons) {
                mouse_clicked_flag = 1;
            }
            mouse_buttons = new_buttons;

            if (dx != 0 || dy != 0)
                mouse_moved_flag = 1;

            break;
    }
}

/* ── Public API ───────────────────────────────────────────── */

void mouse_init(void) {
    uint8_t status_byte;

    mouse_x = GFX_W / 2;
    mouse_y = GFX_H / 2;

    /* Enable the auxiliary mouse device */
    mouse_wait_input();
    outb(0x64, 0xA8);

    /* Enable interrupts for mouse */
    mouse_wait_input();
    outb(0x64, 0x20);  /* Get compaq status byte */
    mouse_wait_output();
    status_byte = inb(0x60);
    status_byte |= 0x02;   /* Enable IRQ12 */
    status_byte &= ~0x20;  /* Enable mouse clock */
    mouse_wait_input();
    outb(0x64, 0x60);  /* Set compaq status byte */
    mouse_wait_input();
    outb(0x60, status_byte);

    /* Tell mouse to use default settings */
    mouse_write(0xF6);
    mouse_read(); /* ACK */

    /* Enable data reporting */
    mouse_write(0xF4);
    mouse_read(); /* ACK */

    /* Register IRQ12 handler (IRQ12 → INT 44) */
    register_interrupt_handler(44, mouse_callback);

    /* Unmask IRQ12 on the slave PIC (bit 4) */
    uint8_t mask = inb(0xA1);
    mask &= ~(1 << 4);  /* Clear bit 4 (IRQ12) */
    outb(0xA1, mask);

    /* Also make sure cascade (IRQ2) is unmasked on master */
    mask = inb(0x21);
    mask &= ~(1 << 2);  /* Clear bit 2 (cascade) */
    outb(0x21, mask);
}

void mouse_get_state(mouse_state_t *state) {
    state->x = mouse_x;
    state->y = mouse_y;
    state->buttons = mouse_buttons;
    state->moved = mouse_moved_flag;
    state->clicked = mouse_clicked_flag;
}

int mouse_left_pressed(void) {
    return mouse_buttons & 0x01;
}

int mouse_right_pressed(void) {
    return mouse_buttons & 0x02;
}

void mouse_clear_events(void) {
    mouse_moved_flag = 0;
    mouse_clicked_flag = 0;
}
