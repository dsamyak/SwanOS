/* ============================================================
 * SwanOS — VGA Text-Mode GUI
 * Rich TUI with sidebar, chat area, input bar, status bar.
 * Uses box-drawing characters and 16 VGA colors.
 *
 *  Layout (80×25):
 *  Row  0:   Title bar         ╔═ SwanOS v2.0 ═══ user ● ONLINE ═╗
 *  Row  1-20: Sidebar(20) │ Chat area(58)
 *  Row 21:   Separator         ╠══════════════════════════════════╣
 *  Row 22:   Input bar         ║ > _                              ║
 *  Row 23:   Status bar        ╠══════════════════════════════════╣
 *  Row 24:   Hints             ╚═ help│status│clear│ask│shutdown ═╝
 * ============================================================ */

#include "gui.h"
#include "screen.h"
#include "keyboard.h"
#include "string.h"
#include "timer.h"
#include "fs.h"
#include "user.h"
#include "llm.h"
#include "ports.h"

/* ── Layout constants ──────────────────────────────────── */
#define W          80
#define H          25
#define SIDEBAR_W  22   /* columns 0-21 */
#define CHAT_LEFT  23   /* columns 23-78 */
#define CHAT_W     55
#define CHAT_TOP   2
#define CHAT_BOT   19
#define CHAT_LINES (CHAT_BOT - CHAT_TOP + 1)
#define INPUT_ROW  22
#define STATUS_ROW 23
#define HINTS_ROW  24

/* Colors */
#define C_TITLE_FG    VGA_WHITE
#define C_TITLE_BG    VGA_BLUE
#define C_BORDER_FG   VGA_CYAN
#define C_BORDER_BG   VGA_BLACK
#define C_SIDEBAR_FG  VGA_LIGHT_GREY
#define C_SIDEBAR_BG  VGA_BLACK
#define C_LABEL_FG    VGA_YELLOW
#define C_VALUE_FG    VGA_WHITE
#define C_CHAT_FG     VGA_WHITE
#define C_CHAT_BG     VGA_BLACK
#define C_USER_FG     VGA_GREEN
#define C_AI_FG       VGA_CYAN
#define C_SYS_FG      VGA_DARK_GREY
#define C_INPUT_FG    VGA_WHITE
#define C_INPUT_BG    VGA_BLACK
#define C_STATUS_FG   VGA_LIGHT_GREY
#define C_STATUS_BG   VGA_BLUE
#define C_HINT_FG     VGA_DARK_GREY
#define C_HINT_BG     VGA_BLACK
#define C_FILE_FG     VGA_LIGHT_CYAN
#define C_DIR_FG      VGA_YELLOW
#define C_GREEN       VGA_GREEN
#define C_RED         VGA_RED
#define C_BLK         VGA_BLACK

/* Chat buffer */
#define MAX_CHAT      64
#define MAX_MSG_LEN   256

typedef struct {
    char text[MAX_MSG_LEN];
    int  role; /* 0=system, 1=user, 2=ai */
} chat_msg_t;

static chat_msg_t chat[MAX_CHAT];
static int chat_count = 0;
static int chat_scroll = 0;

/* Input buffer */
#define INPUT_MAX 200
static char input_buf[INPUT_MAX];
static int  input_pos = 0;

/* ── Drawing primitives ──────────────────────────────────── */

static void draw_hline(int row, int c1, int c2, uint8_t fg) {
    for (int c = c1; c <= c2; c++)
        screen_put_char_at(row, c, (char)BOX_DH, fg, C_BLK);
}

static void draw_vline(int col, int r1, int r2, uint8_t fg) {
    for (int r = r1; r <= r2; r++)
        screen_put_char_at(r, col, (char)BOX_DV, fg, C_BLK);
}

/* ── Title bar ───────────────────────────────────────────── */

static void draw_title(void) {
    screen_fill_row(0, 0, W-1, ' ', C_TITLE_FG, C_TITLE_BG);
    screen_put_str_at(0, 1, " SwanOS v2.0 ", C_TITLE_FG, C_TITLE_BG);

    /* User + status on right */
    char right[40];
    strcpy(right, user_current());
    strcat(right, "  ");

    int rlen = strlen(right);
    screen_put_str_at(0, W - rlen - 12, right, VGA_GREEN, C_TITLE_BG);
    screen_put_char_at(0, W - 11, (char)BULLET, VGA_GREEN, C_TITLE_BG);
    screen_put_str_at(0, W - 10, " ONLINE ", VGA_GREEN, C_TITLE_BG);
}

/* ── Borders ─────────────────────────────────────────────── */

static void draw_borders(void) {
    /* Top border below title */
    screen_put_char_at(1, 0, (char)BOX_DTL, C_BORDER_FG, C_BLK);
    draw_hline(1, 1, SIDEBAR_W-1, C_BORDER_FG);
    screen_put_char_at(1, SIDEBAR_W, (char)BOX_DTT, C_BORDER_FG, C_BLK);
    draw_hline(1, SIDEBAR_W+1, W-2, C_BORDER_FG);
    screen_put_char_at(1, W-1, (char)BOX_DTR, C_BORDER_FG, C_BLK);

    /* Left border */
    draw_vline(0, 2, CHAT_BOT+1, C_BORDER_FG);
    /* Sidebar/chat divider */
    draw_vline(SIDEBAR_W, 2, CHAT_BOT+1, C_BORDER_FG);
    /* Right border */
    draw_vline(W-1, 2, CHAT_BOT+1, C_BORDER_FG);

    /* Separator above input */
    screen_put_char_at(CHAT_BOT+2, 0, (char)BOX_DLT, C_BORDER_FG, C_BLK);
    draw_hline(CHAT_BOT+2, 1, W-2, C_BORDER_FG);
    screen_put_char_at(CHAT_BOT+2, SIDEBAR_W, (char)BOX_DBT, C_BORDER_FG, C_BLK);
    screen_put_char_at(CHAT_BOT+2, W-1, (char)BOX_DRT, C_BORDER_FG, C_BLK);

    /* Input row borders */
    screen_put_char_at(INPUT_ROW, 0, (char)BOX_DV, C_BORDER_FG, C_BLK);
    screen_put_char_at(INPUT_ROW, W-1, (char)BOX_DV, C_BORDER_FG, C_BLK);

    /* Status bar row */
    screen_fill_row(STATUS_ROW, 0, W-1, ' ', C_STATUS_FG, C_STATUS_BG);

    /* Bottom hints row */
    screen_put_char_at(HINTS_ROW, 0, (char)BOX_DBL, C_BORDER_FG, C_BLK);
    draw_hline(HINTS_ROW, 1, W-2, C_BORDER_FG);
    screen_put_char_at(HINTS_ROW, W-1, (char)BOX_DBR, C_BORDER_FG, C_BLK);
}

/* ── Sidebar ─────────────────────────────────────────────── */

static void draw_sidebar(void) {
    /* Clear sidebar area */
    screen_fill_rect(2, 1, CHAT_BOT+1, SIDEBAR_W-1, ' ', C_SIDEBAR_FG, C_SIDEBAR_BG);

    int row = 2;

    /* Section: SYSTEM */
    screen_put_str_at(row, 2, "SYSTEM", C_LABEL_FG, C_BLK);
    row++;
    for (int c = 2; c < SIDEBAR_W-1; c++)
        screen_put_char_at(row, c, (char)BOX_H, VGA_DARK_GREY, C_BLK);
    row++;

    /* Model */
    screen_put_str_at(row, 2, "Model:", VGA_DARK_GREY, C_BLK);
    screen_put_str_at(row, 9, "Groq LLM", C_VALUE_FG, C_BLK);
    row++;

    /* Uptime */
    uint32_t secs = timer_get_seconds();
    uint32_t mins = secs / 60;
    uint32_t hrs = mins / 60;
    secs %= 60; mins %= 60;
    char uptime[16];
    char tmp[8];
    uptime[0] = '\0';
    itoa(hrs, tmp, 10); strcat(uptime, tmp); strcat(uptime, "h ");
    itoa(mins, tmp, 10); strcat(uptime, tmp); strcat(uptime, "m ");
    itoa(secs, tmp, 10); strcat(uptime, tmp); strcat(uptime, "s");

    screen_put_str_at(row, 2, "Up:", VGA_DARK_GREY, C_BLK);
    screen_put_str_at(row, 6, uptime, C_VALUE_FG, C_BLK);
    row++;

    /* Status */
    screen_put_str_at(row, 2, "Status:", VGA_DARK_GREY, C_BLK);
    screen_put_char_at(row, 10, (char)BULLET, C_GREEN, C_BLK);
    screen_put_str_at(row, 12, "Online", C_GREEN, C_BLK);
    row++;

    /* Chat turns */
    char turns[8];
    itoa(chat_count, turns, 10);
    screen_put_str_at(row, 2, "Chat:", VGA_DARK_GREY, C_BLK);
    screen_put_str_at(row, 8, turns, C_VALUE_FG, C_BLK);
    screen_put_str_at(row, 8 + strlen(turns), " msgs", VGA_DARK_GREY, C_BLK);
    row += 2;

    /* Section: FILES */
    screen_put_str_at(row, 2, "FILES", C_LABEL_FG, C_BLK);
    row++;
    for (int c = 2; c < SIDEBAR_W-1; c++)
        screen_put_char_at(row, c, (char)BOX_H, VGA_DARK_GREY, C_BLK);
    row++;

    /* List files from root */
    char listing[1024];
    fs_list("/", listing, sizeof(listing));

    /* Parse and display file list */
    char *p = listing;
    while (*p && row <= CHAT_BOT) {
        /* Skip leading spaces */
        while (*p == ' ') p++;
        if (*p == '\0' || *p == '\n') { if (*p) p++; continue; }

        int is_dir = (strncmp(p, "[DIR]", 5) == 0);
        /* Skip marker */
        char *name_start = strchr(p, ']');
        if (name_start) {
            name_start += 2; /* skip "] " */
        } else {
            name_start = p;
        }

        /* Extract name */
        char fname[20];
        int fi = 0;
        while (*name_start && *name_start != '\n' && fi < 18) {
            fname[fi++] = *name_start++;
        }
        fname[fi] = '\0';

        if (is_dir) {
            screen_put_str_at(row, 3, "+", VGA_DARK_GREY, C_BLK);
            screen_put_str_at(row, 5, fname, C_DIR_FG, C_BLK);
        } else {
            screen_put_str_at(row, 3, "-", VGA_DARK_GREY, C_BLK);
            screen_put_str_at(row, 5, fname, C_FILE_FG, C_BLK);
        }
        row++;

        /* Move to next line */
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
    }
}

/* ── Chat area ───────────────────────────────────────────── */

static void add_chat(const char *text, int role) {
    if (chat_count >= MAX_CHAT) {
        /* Shift everything up */
        for (int i = 0; i < MAX_CHAT - 1; i++)
            chat[i] = chat[i + 1];
        chat_count = MAX_CHAT - 1;
    }
    strncpy(chat[chat_count].text, text, MAX_MSG_LEN - 1);
    chat[chat_count].text[MAX_MSG_LEN - 1] = '\0';
    chat[chat_count].role = role;
    chat_count++;

    /* Auto-scroll to bottom */
    if (chat_count > CHAT_LINES) {
        chat_scroll = chat_count - CHAT_LINES;
    }
}

static void draw_chat(void) {
    /* Clear chat area */
    screen_fill_rect(CHAT_TOP, CHAT_LEFT, CHAT_BOT, W-2, ' ', C_CHAT_FG, C_CHAT_BG);

    int draw_row = CHAT_TOP;
    int start = chat_scroll;

    for (int i = start; i < chat_count && draw_row <= CHAT_BOT; i++) {
        uint8_t name_fg, text_fg;
        const char *prefix;

        if (chat[i].role == 1) {      /* user */
            prefix = "You > ";
            name_fg = C_USER_FG;
            text_fg = VGA_WHITE;
        } else if (chat[i].role == 2) { /* AI */
            prefix = "AI  > ";
            name_fg = C_AI_FG;
            text_fg = VGA_LIGHT_GREY;
        } else {                        /* system */
            prefix = "  ";
            name_fg = C_SYS_FG;
            text_fg = C_SYS_FG;
        }

        /* Print prefix */
        screen_put_str_at(draw_row, CHAT_LEFT, prefix, name_fg, C_CHAT_BG);

        /* Print message text, wrapping lines */
        int col = CHAT_LEFT + strlen(prefix);
        const char *p = chat[i].text;
        while (*p && draw_row <= CHAT_BOT) {
            if (*p == '\n' || col >= W - 2) {
                draw_row++;
                col = CHAT_LEFT + 6; /* indent continuation */
                if (*p == '\n') p++;
                continue;
            }
            screen_put_char_at(draw_row, col, *p, text_fg, C_CHAT_BG);
            col++;
            p++;
        }
        draw_row++;
    }
}

/* ── Input bar ───────────────────────────────────────────── */

static void draw_input(void) {
    screen_fill_row(INPUT_ROW, 1, W-2, ' ', C_INPUT_FG, C_INPUT_BG);
    screen_put_char_at(INPUT_ROW, 2, (char)ARROW_RIGHT, C_AI_FG, C_BLK);
    screen_put_str_at(INPUT_ROW, 4, input_buf, C_INPUT_FG, C_INPUT_BG);
    screen_set_cursor(INPUT_ROW, 4 + input_pos);
}

/* ── Status bar ──────────────────────────────────────────── */

static void draw_status(void) {
    screen_fill_row(STATUS_ROW, 0, W-1, ' ', C_STATUS_FG, C_STATUS_BG);
    screen_put_str_at(STATUS_ROW, 1, " SwanOS v2.0  |", VGA_WHITE, C_STATUS_BG);
    screen_put_str_at(STATUS_ROW, 18, "  Groq LLM  |  Serial Bridge", C_STATUS_FG, C_STATUS_BG);

    /* Uptime on right */
    uint32_t secs = timer_get_seconds();
    uint32_t mins = secs / 60;
    uint32_t hrs = mins / 60;
    secs %= 60; mins %= 60;
    char uptime[20];
    char tmp[8];
    strcpy(uptime, "Up:");
    itoa(hrs, tmp, 10); strcat(uptime, tmp); strcat(uptime, "h");
    itoa(mins, tmp, 10); strcat(uptime, tmp); strcat(uptime, "m");
    int ulen = strlen(uptime);
    screen_put_str_at(STATUS_ROW, W - ulen - 2, uptime, VGA_WHITE, C_STATUS_BG);
}

/* ── Hints bar ───────────────────────────────────────────── */

static void draw_hints(void) {
    screen_put_str_at(HINTS_ROW, 3,
        "help", VGA_WHITE, C_BLK);
    screen_put_str_at(HINTS_ROW, 8,
        "|", VGA_DARK_GREY, C_BLK);
    screen_put_str_at(HINTS_ROW, 10,
        "ask <q>", VGA_WHITE, C_BLK);
    screen_put_str_at(HINTS_ROW, 18,
        "|", VGA_DARK_GREY, C_BLK);
    screen_put_str_at(HINTS_ROW, 20,
        "ls", VGA_WHITE, C_BLK);
    screen_put_str_at(HINTS_ROW, 23,
        "|", VGA_DARK_GREY, C_BLK);
    screen_put_str_at(HINTS_ROW, 25,
        "clear", VGA_WHITE, C_BLK);
    screen_put_str_at(HINTS_ROW, 31,
        "|", VGA_DARK_GREY, C_BLK);
    screen_put_str_at(HINTS_ROW, 33,
        "cli", VGA_WHITE, C_BLK);
    screen_put_str_at(HINTS_ROW, 37,
        "|", VGA_DARK_GREY, C_BLK);
    screen_put_str_at(HINTS_ROW, 39,
        "status", VGA_WHITE, C_BLK);
    screen_put_str_at(HINTS_ROW, 46,
        "|", VGA_DARK_GREY, C_BLK);
    screen_put_str_at(HINTS_ROW, 48,
        "shutdown", VGA_WHITE, C_BLK);
}

/* ── Full screen redraw ──────────────────────────────────── */

static void draw_full(void) {
    screen_fill_rect(0, 0, H-1, W-1, ' ', VGA_WHITE, C_BLK);
    draw_title();
    draw_borders();
    draw_sidebar();
    draw_chat();
    draw_input();
    draw_status();
    draw_hints();
    screen_show_cursor();
    screen_set_cursor(INPUT_ROW, 4 + input_pos);
}

/* ── Command processor (GUI mode) ───────────────────────── */

static int gui_process_cmd(char *cmd) {
    char *arg = cmd;
    while (*arg && !isspace(*arg)) arg++;
    if (*arg) { *arg = '\0'; arg++; }
    arg = trim(arg);

    if (strcmp(cmd, "shutdown") == 0 || strcmp(cmd, "exit") == 0) {
        return -1;
    }
    if (strcmp(cmd, "reboot") == 0) {
        return -2;
    }
    if (strcmp(cmd, "cli") == 0) {
        return -3; /* switch to CLI */
    }
    if (strcmp(cmd, "login") == 0) {
        return -4;
    }
    if (strcmp(cmd, "clear") == 0) {
        chat_count = 0;
        chat_scroll = 0;
        add_chat("Chat cleared.", 0);
        return 0;
    }
    if (strcmp(cmd, "help") == 0) {
        add_chat("Commands:", 0);
        add_chat("ask <q>  - Ask the AI", 0);
        add_chat("ls/cat/write/mkdir/rm - Files", 0);
        add_chat("calc <expr> - Calculator", 0);
        add_chat("status - System info", 0);
        add_chat("clear - Clear chat", 0);
        add_chat("cli - Switch to CLI mode", 0);
        add_chat("login - Switch user", 0);
        add_chat("shutdown - Power off", 0);
        return 0;
    }
    if (strcmp(cmd, "ask") == 0) {
        if (arg[0] == '\0') {
            add_chat("Usage: ask <question>", 0);
            return 0;
        }
        add_chat(arg, 1); /* user message */
        draw_chat(); draw_input();
        screen_set_cursor(INPUT_ROW, 4);

        /* Show thinking indicator */
        add_chat("Thinking...", 0);
        draw_chat();

        char response[2048];
        llm_query(arg, response, sizeof(response));

        /* Remove thinking message */
        chat_count--;
        add_chat(response, 2); /* AI response */
        return 0;
    }
    if (strcmp(cmd, "status") == 0) {
        uint32_t secs = timer_get_seconds();
        uint32_t mins = secs / 60;
        uint32_t hrs = mins / 60;
        secs %= 60; mins %= 60;
        char buf[80]; char tmp[8];
        strcpy(buf, "Uptime: ");
        itoa(hrs, tmp, 10); strcat(buf, tmp); strcat(buf, "h ");
        itoa(mins, tmp, 10); strcat(buf, tmp); strcat(buf, "m ");
        itoa(secs, tmp, 10); strcat(buf, tmp); strcat(buf, "s");
        add_chat(buf, 0);
        strcpy(buf, "User: "); strcat(buf, user_current());
        add_chat(buf, 0);
        add_chat("Model: Groq LLM | Arch: x86", 0);
        return 0;
    }
    if (strcmp(cmd, "ls") == 0) {
        char listing[512];
        fs_list(arg[0] ? arg : "/", listing, sizeof(listing));
        add_chat(listing, 0);
        return 0;
    }
    if (strcmp(cmd, "cat") == 0) {
        if (arg[0] == '\0') { add_chat("Usage: cat <file>", 0); return 0; }
        char content[512];
        int r = fs_read(arg, content, sizeof(content));
        if (r < 0) add_chat(content, 0);
        else { add_chat(content, 0); }
        return 0;
    }
    if (strcmp(cmd, "write") == 0) {
        char *content = arg;
        while (*content && !isspace(*content)) content++;
        if (*content) { *content = '\0'; content++; }
        content = trim(content);
        if (arg[0] == '\0' || content[0] == '\0') {
            add_chat("Usage: write <file> <text>", 0);
            return 0;
        }
        if (fs_write(arg, content) == 0) {
            char msg[64]; strcpy(msg, "Written: "); strcat(msg, arg);
            add_chat(msg, 0);
        } else {
            add_chat("Write failed.", 0);
        }
        return 0;
    }
    if (strcmp(cmd, "mkdir") == 0) {
        if (arg[0] == '\0') { add_chat("Usage: mkdir <name>", 0); return 0; }
        if (fs_mkdir(arg) == 0) {
            char msg[64]; strcpy(msg, "Created: "); strcat(msg, arg);
            add_chat(msg, 0);
        } else add_chat("Failed (exists?).", 0);
        return 0;
    }
    if (strcmp(cmd, "rm") == 0) {
        if (arg[0] == '\0') { add_chat("Usage: rm <file>", 0); return 0; }
        int r = fs_delete(arg);
        if (r == 0) { char msg[64]; strcpy(msg, "Deleted: "); strcat(msg, arg); add_chat(msg, 0); }
        else if (r == -2) add_chat("Dir not empty.", 0);
        else add_chat("Not found.", 0);
        return 0;
    }
    if (strcmp(cmd, "calc") == 0) {
        if (arg[0] == '\0') { add_chat("Usage: calc <expr>", 0); return 0; }
        /* Simple eval */
        int result = 0, num = 0, sign = 1; char op = '+'; int has = 0;
        const char *e = arg;
        while (*e) {
            if (isdigit(*e)) { num = num*10 + (*e-'0'); has = 1; }
            else if (*e == '+' || *e == '-' || *e == '*' || *e == '/') {
                if (has) { if (op=='+') result+=sign*num; else if (op=='-') result-=num; else if (op=='*') result*=num; else if (op=='/'&&num) result/=num; }
                op=*e; if (op=='+'||op=='-'){sign=(op=='-')?-1:1;op='+';} num=0;has=0;
            }
            e++;
        }
        if (has) { if (op=='+') result+=sign*num; else if (op=='-') result-=num; else if (op=='*') result*=num; else if (op=='/'&&num) result/=num; }
        char msg[32]; char nb[16]; strcpy(msg, "= "); itoa(result, nb, 10); strcat(msg, nb);
        add_chat(msg, 0);
        return 0;
    }
    if (strcmp(cmd, "echo") == 0) {
        add_chat(arg, 0);
        return 0;
    }
    if (strcmp(cmd, "whoami") == 0) {
        char msg[32]; strcpy(msg, "User: "); strcat(msg, user_current());
        add_chat(msg, 0);
        return 0;
    }

    /* Unknown */
    char msg[64]; strcpy(msg, "Unknown: "); strcat(msg, cmd);
    add_chat(msg, 0);
    add_chat("Type 'help' for commands.", 0);
    return 0;
}

/* ── GUI main loop ───────────────────────────────────────── */

void gui_run(void) {
    chat_count = 0;
    chat_scroll = 0;
    input_pos = 0;
    input_buf[0] = '\0';

    /* Clear screen fully */
    screen_fill_rect(0, 0, H-1, W-1, ' ', VGA_WHITE, VGA_BLACK);

    add_chat("Welcome to SwanOS! Type 'help' or 'ask <question>'.", 0);
    draw_full();

    uint32_t last_status_tick = 0;

    while (1) {
        char c = keyboard_getchar();

        if (c == '\n') {
            /* Process command */
            char *cmd = trim(input_buf);
            if (cmd[0] != '\0') {
                int result = gui_process_cmd(cmd);

                /* Redraw everything */
                draw_sidebar();
                draw_chat();

                if (result == -1) {
                    /* Shutdown */
                    screen_fill_rect(0, 0, H-1, W-1, ' ', VGA_WHITE, VGA_BLACK);
                    screen_put_str_at(12, 30, "Shutting down...", VGA_YELLOW, VGA_BLACK);
                    __asm__ volatile ("cli; hlt");
                    while (1);
                }
                if (result == -2) {
                    uint8_t good = 0x02;
                    while (good & 0x02) good = inb(0x64);
                    outb(0x64, 0xFE);
                    __asm__ volatile ("cli; hlt");
                }
                if (result == -3) {
                    return; /* back to kernel → CLI mode */
                }
                if (result == -4) {
                    return; /* back to kernel → re-login */
                }
            }
            input_pos = 0;
            input_buf[0] = '\0';
            draw_input();
        }
        else if (c == '\b') {
            if (input_pos > 0) {
                input_pos--;
                input_buf[input_pos] = '\0';
                draw_input();
            }
        }
        else if (c >= ' ' && input_pos < INPUT_MAX - 1) {
            input_buf[input_pos++] = c;
            input_buf[input_pos] = '\0';
            draw_input();
        }

        /* Periodic status/sidebar update every ~5 seconds */
        uint32_t ticks = timer_get_ticks();
        if (ticks - last_status_tick > 500) {
            draw_status();
            draw_sidebar();
            screen_set_cursor(INPUT_ROW, 4 + input_pos);
            last_status_tick = ticks;
        }
    }
}
