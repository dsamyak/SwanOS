/* ============================================================
 * SwanOS — PS/2 Keyboard Driver
 * IRQ1 handler, scancode→ASCII translation, line buffer.
 * Also accepts input from serial port (for GUI frontend).
 * Arrow keys emit special codes: 0x80=UP 0x81=DOWN 0x82=LEFT 0x83=RIGHT
 * ============================================================ */

#include "keyboard.h"
#include "idt.h"
#include "ports.h"
#include "screen.h"
#include "serial.h"

#define KB_BUFFER_SIZE 256

static volatile char kb_buffer[KB_BUFFER_SIZE];
static volatile int  kb_head = 0;
static volatile int  kb_tail = 0;
static int shift_pressed = 0;

/* US keyboard scancode → ASCII (lowercase) */
static const char scancode_to_ascii[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t', 'q','w','e','r','t','y','u','i','o','p','[',']', '\n',
    0, 'a','s','d','f','g','h','j','k','l',';','\'', '`',
    0, '\\','z','x','c','v','b','n','m',',','.','/', 0,
    '*', 0, ' ',
};

/* Shifted version */
static const char scancode_to_ascii_shift[128] = {
    0, 27, '!','@','#','$','%','^','&','*','(',')','_','+', '\b',
    '\t', 'Q','W','E','R','T','Y','U','I','O','P','{','}', '\n',
    0, 'A','S','D','F','G','H','J','K','L',':','"', '~',
    0, '|','Z','X','C','V','B','N','M','<','>','?', 0,
    '*', 0, ' ',
};

static void kb_push(char c) {
    int next = (kb_head + 1) % KB_BUFFER_SIZE;
    if (next != kb_tail) {
        kb_buffer[kb_head] = c;
        kb_head = next;
    }
}

static void keyboard_callback(registers_t *regs) {
    (void)regs;
    uint8_t scancode = inb(0x60);

    /* Track shift key */
    if (scancode == 0x2A || scancode == 0x36) { shift_pressed = 1; return; }
    if (scancode == 0xAA || scancode == 0xB6) { shift_pressed = 0; return; }

    /* Ignore key releases (bit 7 set) */
    if (scancode & 0x80) return;

    /* Arrow keys → special codes */
    if (scancode == 0x48) { kb_push((char)0x80); return; } /* Up    */
    if (scancode == 0x50) { kb_push((char)0x81); return; } /* Down  */
    if (scancode == 0x4B) { kb_push((char)0x82); return; } /* Left  */
    if (scancode == 0x4D) { kb_push((char)0x83); return; } /* Right */

    char c;
    if (scancode < 128) {
        c = shift_pressed ? scancode_to_ascii_shift[scancode] : scancode_to_ascii[scancode];
    } else {
        return;
    }

    if (c == 0) return;
    kb_push(c);
}

void keyboard_init(void) {
    kb_head = 0;
    kb_tail = 0;

    /* Flush the PS/2 output buffer */
    while (inb(0x64) & 1) {
        inb(0x60);
    }

    /* Enable first PS/2 port */
    outb(0x64, 0xAE);
    io_wait();

    /* Read PS/2 configuration byte */
    outb(0x64, 0x20);
    io_wait();
    uint8_t status = inb(0x60);

    /* Enable port 1 interrupt (bit 0) and translation (bit 6).
       Clear bit 4 (disable keyboard port) */
    status |= 0x41;
    status &= ~0x10;

    /* Write PS/2 configuration byte */
    outb(0x64, 0x60);
    io_wait();
    outb(0x60, status);


    register_interrupt_handler(33, keyboard_callback); /* IRQ1 → INT 33 */
}

int keyboard_has_key(void) {
    return (kb_head != kb_tail) || serial_data_ready();
}

char keyboard_getchar(void) {
    /* Check both PS/2 buffer AND serial port for input */
    while (1) {
        /* Check PS/2 keyboard buffer */
        if (kb_head != kb_tail) {
            char c = kb_buffer[kb_tail];
            kb_tail = (kb_tail + 1) % KB_BUFFER_SIZE;
            return c;
        }

        /* Check serial port for GUI input */
        if (serial_data_ready()) {
            char c = serial_read_char();
            /* Ignore control chars used by protocol, except common ones */
            if (c == '\x01') {
                /* Protocol marker — skip (shouldn't come from GUI input) */
                continue;
            }
            if (c == '\x04') {
                /* EOT — ignore in keyboard context */
                continue;
            }
            if (c == '\r') c = '\n'; /* normalize CR to LF */
            return c;
        }

        __asm__ volatile ("hlt"); /* halt until next interrupt */
    }
}

int keyboard_read_line(char *buf, int max_len) {
    int pos = 0;

    while (pos < max_len - 1) {
        char c = keyboard_getchar();

        if (c == '\n') {
            buf[pos] = '\0';
            screen_putchar('\n');
            return pos;
        }
        else if (c == '\b') {
            if (pos > 0) {
                pos--;
                screen_backspace();
            }
        }
        else if (c >= ' ') { /* printable */
            buf[pos++] = c;
            screen_putchar(c);
        }
    }

    buf[pos] = '\0';
    screen_putchar('\n');
    return pos;
}
