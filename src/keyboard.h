#ifndef KEYBOARD_H
#define KEYBOARD_H

/* Special key codes (non-ASCII) */
#define KEY_UP    0x80
#define KEY_DOWN  0x81
#define KEY_LEFT  0x82
#define KEY_RIGHT 0x83
#define KEY_F1    0x84
#define KEY_F2    0x85
#define KEY_F3    0x86
#define KEY_CTRL_SPACE 0x87
#define KEY_TAB   0x09

void keyboard_init(void);
char keyboard_getchar(void);     /* Blocking: waits for a key */
int  keyboard_has_key(void);     /* Non-blocking: 1 if key available */
void keyboard_flush(void);       /* Flush all buffered input */
int  keyboard_read_line(char *buf, int max_len);  /* Read a line with editing */

#endif
