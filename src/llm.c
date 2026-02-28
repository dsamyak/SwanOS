/* ============================================================
 * SwanOS — LLM Client
 * Sends queries to the host-side bridge via COM1 serial port.
 * Protocol:
 *   OS → Bridge:  query text + \x04 (EOT)
 *   Bridge → OS:  response text + \x04 (EOT)
 * ============================================================ */

#include "llm.h"
#include "serial.h"
#include "screen.h"
#include "string.h"

int llm_query(const char *question, char *response, int max_len) {
    screen_set_color(VGA_DARK_GREY, VGA_BLACK);
    screen_print("  [connecting to AI...]\n");
    screen_set_color(VGA_WHITE, VGA_BLACK);

    /* Send query over serial */
    serial_write(question);

    /* Read response (30 second timeout) */
    int len = serial_read_line(response, max_len, 30);

    if (len == 0) {
        strcpy(response, "No response from AI bridge. Is bridge.py running?");
        return -1;
    }

    return len;
}
