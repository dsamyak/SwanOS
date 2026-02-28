#ifndef KEYBOARD_H
#define KEYBOARD_H

void keyboard_init(void);
char keyboard_getchar(void);     /* Blocking: waits for a key */
int  keyboard_read_line(char *buf, int max_len);  /* Read a line with editing */

#endif
