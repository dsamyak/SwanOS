/* ============================================================
 * SwanOS — PIT Timer Driver (IRQ0)
 * Provides system tick counter and uptime tracking.
 * ============================================================ */

#include "timer.h"
#include "idt.h"
#include "ports.h"

static volatile uint32_t tick_count = 0;
static uint32_t tick_freq = 100; /* Hz */

static void timer_callback(registers_t *regs) {
    (void)regs;
    tick_count++;
}

void timer_init(uint32_t frequency) {
    tick_freq = frequency;
    register_interrupt_handler(32, timer_callback); /* IRQ0 → INT 32 */

    /* Configure PIT channel 0 */
    uint32_t divisor = 1193180 / frequency;
    outb(0x43, 0x36); /* Channel 0, lobyte/hibyte, rate generator */
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

uint32_t timer_get_ticks(void) {
    return tick_count;
}

uint32_t timer_get_seconds(void) {
    return tick_count / tick_freq;
}
