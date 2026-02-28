/* ============================================================
 * SwanOS — Serial Port Driver (COM1)
 * Used to communicate with the host-side LLM bridge.
 * Protocol: send query ending with \x04 (EOT), read response
 *           ending with \x04 (EOT).
 * ============================================================ */

#include "serial.h"
#include "ports.h"
#include "timer.h"

#define COM1 0x3F8

void serial_init(void) {
    outb(COM1 + 1, 0x00); /* Disable interrupts */
    outb(COM1 + 3, 0x80); /* Enable DLAB (set baud rate) */
    outb(COM1 + 0, 0x01); /* 115200 baud (divisor = 1) */
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03); /* 8 bits, no parity, 1 stop bit */
    outb(COM1 + 2, 0xC7); /* Enable FIFO, clear, 14-byte threshold */
    outb(COM1 + 4, 0x0B); /* IRQs enabled, RTS/DSR set */
}

static int serial_transmit_ready(void) {
    return inb(COM1 + 5) & 0x20;
}

static int serial_data_available(void) {
    return inb(COM1 + 5) & 0x01;
}

void serial_write_char(char c) {
    while (!serial_transmit_ready());
    outb(COM1, c);
}

void serial_write(const char *str) {
    while (*str) {
        serial_write_char(*str++);
    }
    /* Send EOT marker so bridge knows the message is complete */
    serial_write_char('\x04');
}

char serial_read_char(void) {
    while (!serial_data_available());
    return inb(COM1);
}

int serial_read_line(char *buf, int max_len, int timeout_secs) {
    int pos = 0;
    uint32_t start = timer_get_seconds();

    while (pos < max_len - 1) {
        /* Timeout check */
        if (timeout_secs > 0 && (timer_get_seconds() - start) > (uint32_t)timeout_secs) {
            break;
        }

        if (serial_data_available()) {
            char c = inb(COM1);

            /* EOT = end of transmission */
            if (c == '\x04') break;

            buf[pos++] = c;
            start = timer_get_seconds(); /* reset timeout on data */
        } else {
            __asm__ volatile ("hlt");
        }
    }

    buf[pos] = '\0';
    return pos;
}
