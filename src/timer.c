/* ============================================================
 * SwanOS — PIT Timer Driver (IRQ0) + Periodic Callback System
 * Provides system tick counter, uptime tracking, and scheduled
 * periodic callbacks for realtime subsystem polling.
 * ============================================================ */

#include "timer.h"
#include "idt.h"
#include "ports.h"

static volatile uint32_t tick_count = 0;
static uint32_t tick_freq = 100; /* Hz */

/* ── Periodic Callback Table ─────────────────────────────── */
typedef struct {
    timer_callback_fn fn;
    uint32_t interval;    /* fire every N ticks */
    uint32_t countdown;   /* ticks until next fire */
    int      active;
} periodic_entry_t;

static periodic_entry_t periodic_table[TIMER_MAX_CALLBACKS];

static void timer_callback(registers_t *regs) {
    (void)regs;
    tick_count++;

    /* Fire periodic callbacks */
    for (int i = 0; i < TIMER_MAX_CALLBACKS; i++) {
        if (!periodic_table[i].active) continue;
        if (periodic_table[i].countdown > 0) {
            periodic_table[i].countdown--;
        } else {
            periodic_table[i].fn();
            periodic_table[i].countdown = periodic_table[i].interval;
        }
    }
}

void timer_init(uint32_t frequency) {
    tick_freq = frequency;

    /* Clear periodic callback table */
    for (int i = 0; i < TIMER_MAX_CALLBACKS; i++) {
        periodic_table[i].active = 0;
        periodic_table[i].fn = 0;
    }

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

uint32_t timer_get_ms(void) {
    /* Convert ticks to milliseconds: ticks * 1000 / freq */
    return (tick_count * 1000) / tick_freq;
}

uint32_t timer_get_frequency(void) {
    return tick_freq;
}

int timer_register_periodic(uint32_t interval_ticks, timer_callback_fn cb) {
    if (!cb || interval_ticks == 0) return -1;
    for (int i = 0; i < TIMER_MAX_CALLBACKS; i++) {
        if (!periodic_table[i].active) {
            periodic_table[i].fn = cb;
            periodic_table[i].interval = interval_ticks;
            periodic_table[i].countdown = interval_ticks;
            periodic_table[i].active = 1;
            return i;
        }
    }
    return -1; /* No free slot */
}

void timer_unregister_periodic(int slot) {
    if (slot >= 0 && slot < TIMER_MAX_CALLBACKS) {
        periodic_table[slot].active = 0;
        periodic_table[slot].fn = 0;
    }
}
