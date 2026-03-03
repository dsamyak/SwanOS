#ifndef SERIAL_H
#define SERIAL_H

void serial_init(void);
void serial_write_char(char c);
void serial_write(const char *str);   /* Sends str + \x04 (for LLM queries) */
void serial_putchar(char c);          /* Raw single char, no \x04 */
void serial_puts(const char *str);    /* Raw string, no \x04 */
char serial_read_char(void);
int  serial_read_line(char *buf, int max_len, int timeout_secs);
int  serial_data_ready(void);         /* Non-blocking check for data */

#endif
