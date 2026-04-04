#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

/* ── Core Timer API ─────────────────────────────────────── */
void     timer_init(uint32_t frequency);
uint32_t timer_get_ticks(void);
uint32_t timer_get_seconds(void);
uint32_t timer_get_ms(void);       /* Millisecond-resolution timestamp */
uint32_t timer_get_frequency(void); /* Get configured tick frequency */

/* ── Periodic Callback System ───────────────────────────── */
/* Register a function to be called every `interval_ticks` timer ticks.
   Max 8 callbacks. Returns slot index on success, -1 on failure. */
#define TIMER_MAX_CALLBACKS 8

typedef void (*timer_callback_fn)(void);

int  timer_register_periodic(uint32_t interval_ticks, timer_callback_fn cb);
void timer_unregister_periodic(int slot);

#endif
