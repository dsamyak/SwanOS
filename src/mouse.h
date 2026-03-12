/* ============================================================
 * SwanOS — PS/2 Mouse Driver
 * IRQ12-based mouse input with position and button tracking.
 * ============================================================ */

#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>

typedef struct {
    int x, y;              /* absolute position (clamped to screen) */
    uint8_t buttons;       /* bit0=left, bit1=right, bit2=middle */
    int moved;             /* set to 1 when position changes */
    int clicked;           /* set to 1 on new button press */
} mouse_state_t;

void mouse_init(void);
void mouse_get_state(mouse_state_t *state);
int  mouse_left_pressed(void);
int  mouse_right_pressed(void);
void mouse_clear_events(void);

#endif
