/* ============================================================
 * SwanOS — LLM Client (Groq API via Serial Bridge)
 * Manages API key storage and sends queries to host-side
 * bridge via COM1 serial port.
 *
 * Protocol:
 *   OS → Bridge:  \x01K + api_key + \x04        (set API key)
 *   OS → Bridge:  \x01Q + query text + \x04      (LLM query)
 *   Bridge → OS:  response text + \x04            (LLM response)
 * ============================================================ */

#include "llm.h"
#include "serial.h"
#include "screen.h"
#include "string.h"
#include "fs.h"

#define API_KEY_FILE ".apikey"
#define MAX_KEY_LEN  128

static char api_key[MAX_KEY_LEN];
static int  key_loaded = 0;

void llm_init(void) {
    api_key[0] = '\0';
    key_loaded = 0;

    /* Try to load API key from filesystem */
    char buf[MAX_KEY_LEN];
    int r = fs_read(API_KEY_FILE, buf, sizeof(buf) - 1);
    if (r > 0 && buf[0] != '\0') {
        /* Strip newlines/spaces */
        int len = strlen(buf);
        while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == ' ' || buf[len-1] == '\r'))
            buf[--len] = '\0';
        if (len > 0) {
            strcpy(api_key, buf);
            key_loaded = 1;
        }
    }

    /* Send key to bridge if available */
    if (key_loaded) {
        llm_send_key();
    }
}

void llm_set_api_key(const char *key) {
    if (!key || key[0] == '\0') return;

    /* Store in memory */
    strncpy(api_key, key, MAX_KEY_LEN - 1);
    api_key[MAX_KEY_LEN - 1] = '\0';
    key_loaded = 1;

    /* Persist to filesystem */
    fs_write(API_KEY_FILE, api_key);

    /* Send to bridge immediately */
    llm_send_key();
}

const char *llm_get_api_key(void) {
    return api_key;
}

int llm_ready(void) {
    return key_loaded && api_key[0] != '\0';
}

void llm_send_key(void) {
    if (!key_loaded) return;

    /* Send key command: \x01K<key>\x04 */
    serial_write_char('\x01');
    serial_write_char('K');
    const char *p = api_key;
    while (*p) {
        serial_write_char(*p++);
    }
    serial_write_char('\x04');
}

int llm_query(const char *question, char *response, int max_len) {
    if (!llm_ready()) {
        strcpy(response, "No API key set. Use 'setkey <KEY>' to configure.");
        return -1;
    }

    /* Send query over serial: \x01Q<query>\x04 */
    serial_write_char('\x01');
    serial_write_char('Q');
    const char *p = question;
    while (*p) {
        serial_write_char(*p++);
    }
    serial_write_char('\x04');

    /* Read response (30 second timeout) */
    int len = serial_read_line(response, max_len, 30);

    if (len == 0) {
        strcpy(response, "No response from AI bridge. Is llm_bridge.py running?");
        return -1;
    }

    return len;
}

/* ── Host Persistent Storage via Bridge ──────────────────── */

void llm_host_save(const char *name, const char *content) {
    /* Send \x01V<name>|<content>\x04 */
    serial_write_char('\x01');
    serial_write_char('V');
    const char *p = name;
    while (*p) serial_write_char(*p++);
    serial_write_char('|');
    p = content;
    while (*p) serial_write_char(*p++);
    serial_write_char('\x04');
}

int llm_host_load(const char *name, char *buf, int max_len) {
    /* Send \x01L<name>\x04 */
    serial_write_char('\x01');
    serial_write_char('L');
    const char *p = name;
    while (*p) serial_write_char(*p++);
    serial_write_char('\x04');

    /* Read response */
    int len = serial_read_line(buf, max_len, 5); /* 5 sec timeout */
    if (len == 0) {
        buf[0] = '\0';
        return -1;
    }
    return len;
}

void llm_host_audit(const char *event) {
    /* Send \x01A<event>\x04 */
    serial_write_char('\x01');
    serial_write_char('A');
    const char *p = event;
    while (*p) serial_write_char(*p++);
    serial_write_char('\x04');
}
