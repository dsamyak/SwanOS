/* ============================================================
 * SwanOS — PS/2 Keyboard Driver
 * IRQ1 handler, scancode→ASCII translation, line buffer
 * ============================================================ */

#include "keyboard.h"
#include "idt.h"
#include "ports.h"
#include "screen.h"

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

static void keyboard_callback(registers_t *regs) {
    (void)regs;
    uint8_t scancode = inb(0x60);

    /* Track shift key */
    if (scancode == 0x2A || scancode == 0x36) { shift_pressed = 1; return; }
    if (scancode == 0xAA || scancode == 0xB6) { shift_pressed = 0; return; }

    /* Ignore key releases (bit 7 set) */
    if (scancode & 0x80) return;

    char c;
    if (scancode < 128) {
        c = shift_pressed ? scancode_to_ascii_shift[scancode] : scancode_to_ascii[scancode];
    } else {
        return;
    }

    if (c == 0) return;

    /* Add to circular buffer */
    int next = (kb_head + 1) % KB_BUFFER_SIZE;
    if (next != kb_tail) {
        kb_buffer[kb_head] = c;
        kb_head = next;
    }
}

void keyboard_init(void) {
    kb_head = 0;
    kb_tail = 0;
    register_interrupt_handler(33, keyboard_callback); /* IRQ1 → INT 33 */
}

char keyboard_getchar(void) {
    /* Busy-wait for a character */
    while (kb_head == kb_tail) {
        __asm__ volatile ("hlt"); /* halt until next interrupt */
    }
    char c = kb_buffer[kb_tail];
    kb_tail = (kb_tail + 1) % KB_BUFFER_SIZE;
    return c;
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
