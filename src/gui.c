/* ============================================================
 * SwanOS — Enhanced VGA Text-Mode GUI
 * Premium TUI with animated elements, gradient colors, badges,
 * block-character accents, and dynamic status updates.
 *
 *  Layout (80×25):
 *  Row  0:   Title bar (gradient bg, swan icon, user badge)
 *  Row  1:   Top border (double-line)
 *  Row  2-19: Sidebar(22) │ Chat area(56)
 *  Row 20:   Separator
 *  Row 21:   Input bar with styled prompt
 *  Row 22:   Separator
 *  Row 23:   Status bar (segmented, colored)
 *  Row 24:   Hints bar (badge-style shortcuts)
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
#define INPUT_ROW  21
#define SEP_ROW_1  20
#define SEP_ROW_2  22
#define STATUS_ROW 23
#define HINTS_ROW  24

/* ── Color palette ─────────────────────────────────────── */
#define C_TITLE_FG     VGA_WHITE
#define C_TITLE_BG     VGA_BLUE
#define C_TITLE_ACC    VGA_LIGHT_CYAN
#define C_BORDER_FG    VGA_CYAN
#define C_BORDER_DIM   VGA_DARK_GREY
#define C_BG           VGA_BLACK
#define C_SIDEBAR_BG   VGA_BLACK
#define C_SECT_FG      VGA_YELLOW
#define C_SECT_ACC     VGA_BROWN
#define C_VALUE_FG     VGA_WHITE
#define C_DIM          VGA_DARK_GREY
#define C_CHAT_BG      VGA_BLACK
#define C_USER_FG      VGA_GREEN
#define C_USER_BADGE   VGA_GREEN
#define C_AI_FG        VGA_LIGHT_CYAN
#define C_AI_BADGE     VGA_CYAN
#define C_SYS_FG       VGA_DARK_GREY
#define C_INPUT_FG     VGA_WHITE
#define C_INPUT_BG     VGA_BLACK
#define C_PROMPT_FG    VGA_LIGHT_CYAN
#define C_STATUS_FG    VGA_LIGHT_GREY
#define C_STATUS_BG    VGA_BLUE
#define C_HINT_KEY     VGA_YELLOW
#define C_HINT_DESC    VGA_DARK_GREY
#define C_ONLINE       VGA_GREEN
#define C_FILE_FG      VGA_LIGHT_CYAN
#define C_DIR_FG       VGA_YELLOW

/* ── Chat buffer ───────────────────────────────────────── */
#define MAX_CHAT      64
#define MAX_MSG_LEN   256

typedef struct {
    char text[MAX_MSG_LEN];
    int  role; /* 0=system, 1=user, 2=ai */
} chat_msg_t;

static chat_msg_t chat[MAX_CHAT];
static int chat_count = 0;
static int chat_scroll = 0;

/* ── Input buffer ──────────────────────────────────────── */
#define INPUT_MAX 200
static char input_buf[INPUT_MAX];
static int  input_pos = 0;

/* ── Drawing primitives ──────────────────────────────────── */

static void draw_hline_double(int row, int c1, int c2, uint8_t fg) {
    for (int c = c1; c <= c2; c++)
        screen_put_char_at(row, c, (char)BOX_DH, fg, C_BG);
}

static void draw_vline_double(int col, int r1, int r2, uint8_t fg) {
    for (int r = r1; r <= r2; r++)
        screen_put_char_at(r, col, (char)BOX_DV, fg, C_BG);
}

static void draw_hline_single(int row, int c1, int c2, uint8_t fg) {
    for (int c = c1; c <= c2; c++)
        screen_put_char_at(row, c, (char)BOX_H, fg, C_BG);
}

/* ── Title bar ────────────────────────────────────────────── */

static void draw_title(void) {
    /* Gradient background: dark blue base with accents */
    screen_fill_row(0, 0, W-1, ' ', C_TITLE_FG, C_TITLE_BG);

    /* Swan symbol + brand */
    screen_put_char_at(0, 1, (char)SPADE, VGA_LIGHT_CYAN, C_TITLE_BG);
    screen_put_str_at(0, 3, "SWAN", VGA_WHITE, C_TITLE_BG);
    screen_put_str_at(0, 7, "OS", VGA_LIGHT_CYAN, C_TITLE_BG);
    screen_put_str_at(0, 10, "v2.0", VGA_DARK_GREY, C_TITLE_BG);

    /* Separator dot */
    screen_put_char_at(0, 15, (char)BULLET, VGA_DARK_GREY, C_TITLE_BG);

    /* AI badge */
    screen_put_str_at(0, 17, "[", VGA_DARK_GREY, C_TITLE_BG);
    screen_put_str_at(0, 18, "AI", VGA_LIGHT_CYAN, C_TITLE_BG);
    screen_put_str_at(0, 20, "]", VGA_DARK_GREY, C_TITLE_BG);

    /* Right side: user + status */
    char user_badge[40];
    strcpy(user_badge, user_current());

    int ulen = strlen(user_badge);
    int rpos = W - ulen - 12;
    screen_put_char_at(0, rpos, (char)DIAMOND, VGA_LIGHT_CYAN, C_TITLE_BG);
    screen_put_str_at(0, rpos + 2, user_badge, VGA_WHITE, C_TITLE_BG);
    screen_put_char_at(0, W - 9, (char)BULLET, VGA_GREEN, C_TITLE_BG);
    screen_put_str_at(0, W - 7, "ONLINE", VGA_GREEN, C_TITLE_BG);
}

/* ── Borders ──────────────────────────────────────────────── */

static void draw_borders(void) {
    /* Top border (row 1) */
    screen_put_char_at(1, 0, (char)BOX_DTL, C_BORDER_FG, C_BG);
    draw_hline_double(1, 1, SIDEBAR_W-1, C_BORDER_FG);
    screen_put_char_at(1, SIDEBAR_W, (char)BOX_DTT, C_BORDER_FG, C_BG);
    draw_hline_double(1, SIDEBAR_W+1, W-2, C_BORDER_FG);
    screen_put_char_at(1, W-1, (char)BOX_DTR, C_BORDER_FG, C_BG);

    /* Left, middle, right vertical borders */
    draw_vline_double(0, 2, CHAT_BOT, C_BORDER_FG);
    draw_vline_double(SIDEBAR_W, 2, CHAT_BOT, C_BORDER_DIM);
    draw_vline_double(W-1, 2, CHAT_BOT, C_BORDER_FG);

    /* Separator above input (row 20) */
    screen_put_char_at(SEP_ROW_1, 0, (char)BOX_DLT, C_BORDER_FG, C_BG);
    draw_hline_double(SEP_ROW_1, 1, W-2, C_BORDER_FG);
    screen_put_char_at(SEP_ROW_1, SIDEBAR_W, (char)BOX_DBT, C_BORDER_FG, C_BG);
    screen_put_char_at(SEP_ROW_1, W-1, (char)BOX_DRT, C_BORDER_FG, C_BG);

    /* Input row borders */
    screen_put_char_at(INPUT_ROW, 0, (char)BOX_DV, C_BORDER_FG, C_BG);
    screen_put_char_at(INPUT_ROW, W-1, (char)BOX_DV, C_BORDER_FG, C_BG);

    /* Separator below input (row 22) */
    screen_put_char_at(SEP_ROW_2, 0, (char)BOX_DLT, C_BORDER_FG, C_BG);
    draw_hline_double(SEP_ROW_2, 1, W-2, C_BORDER_FG);
    screen_put_char_at(SEP_ROW_2, W-1, (char)BOX_DRT, C_BORDER_FG, C_BG);

    /* Status bar background */
    screen_fill_row(STATUS_ROW, 0, W-1, ' ', C_STATUS_FG, C_STATUS_BG);

    /* Bottom hints border */
    screen_put_char_at(HINTS_ROW, 0, (char)BOX_DBL, C_BORDER_FG, C_BG);
    draw_hline_double(HINTS_ROW, 1, W-2, C_BORDER_DIM);
    screen_put_char_at(HINTS_ROW, W-1, (char)BOX_DBR, C_BORDER_FG, C_BG);
}

/* ── Sidebar ──────────────────────────────────────────────── */

static void draw_sidebar(void) {
    /* Clear sidebar area */
    screen_fill_rect(2, 1, CHAT_BOT, SIDEBAR_W-1, ' ', VGA_WHITE, C_SIDEBAR_BG);

    int row = 2;

    /* ── Section: SYSTEM ── */
    screen_put_char_at(row, 1, (char)BLOCK_FULL, VGA_CYAN, C_BG);
    screen_put_str_at(row, 3, "SYSTEM", C_SECT_FG, C_BG);
    row++;
    draw_hline_single(row, 1, SIDEBAR_W-1, VGA_DARK_GREY);
    row++;

    /* Model */
    screen_put_char_at(row, 2, (char)DIAMOND, VGA_DARK_GREY, C_BG);
    screen_put_str_at(row, 4, "AI:", VGA_DARK_GREY, C_BG);
    screen_put_str_at(row, 8, "Groq LLM", C_VALUE_FG, C_BG);
    row++;

    /* Uptime */
    uint32_t secs = timer_get_seconds();
    uint32_t mins = secs / 60;
    uint32_t hrs = mins / 60;
    secs %= 60; mins %= 60;
    char uptime[20]; char tmp[8];
    uptime[0] = '\0';
    itoa(hrs, tmp, 10); strcat(uptime, tmp); strcat(uptime, "h ");
    itoa(mins, tmp, 10); strcat(uptime, tmp); strcat(uptime, "m ");
    itoa(secs, tmp, 10); strcat(uptime, tmp); strcat(uptime, "s");

    screen_put_char_at(row, 2, (char)DIAMOND, VGA_DARK_GREY, C_BG);
    screen_put_str_at(row, 4, "Up:", VGA_DARK_GREY, C_BG);
    screen_put_str_at(row, 8, uptime, C_VALUE_FG, C_BG);
    row++;

    /* Status indicator */
    screen_put_char_at(row, 2, (char)DIAMOND, VGA_DARK_GREY, C_BG);
    screen_put_str_at(row, 4, "Net:", VGA_DARK_GREY, C_BG);
    screen_put_char_at(row, 9, (char)BULLET, C_ONLINE, C_BG);
    screen_put_str_at(row, 11, "Online", C_ONLINE, C_BG);
    row++;

    /* Chat count */
    char turns[8]; itoa(chat_count, turns, 10);
    screen_put_char_at(row, 2, (char)DIAMOND, VGA_DARK_GREY, C_BG);
    screen_put_str_at(row, 4, "Msg:", VGA_DARK_GREY, C_BG);
    screen_put_str_at(row, 9, turns, C_VALUE_FG, C_BG);
    row += 2;

    /* ── Section: FILES ── */
    screen_put_char_at(row, 1, (char)BLOCK_FULL, VGA_YELLOW, C_BG);
    screen_put_str_at(row, 3, "FILES", C_SECT_FG, C_BG);
    row++;
    draw_hline_single(row, 1, SIDEBAR_W-1, VGA_DARK_GREY);
    row++;

    /* File listing */
    char listing[1024];
    fs_list("/", listing, sizeof(listing));

    char *p = listing;
    while (*p && row <= CHAT_BOT) {
        while (*p == ' ') p++;
        if (*p == '\0' || *p == '\n') { if (*p) p++; continue; }

        int is_dir = (strncmp(p, "[DIR]", 5) == 0);
        char *name_start = strchr(p, ']');
        if (name_start) name_start += 2;
        else name_start = p;

        char fname[18]; int fi = 0;
        while (*name_start && *name_start != '\n' && fi < 16) {
            fname[fi++] = *name_start++;
        }
        fname[fi] = '\0';

        if (is_dir) {
            screen_put_char_at(row, 2, (char)ARROW_RIGHT, VGA_DARK_GREY, C_BG);
            screen_put_char_at(row, 4, '+', VGA_YELLOW, C_BG);
            screen_put_str_at(row, 6, fname, C_DIR_FG, C_BG);
        } else {
            screen_put_char_at(row, 2, (char)DOT, VGA_DARK_GREY, C_BG);
            screen_put_str_at(row, 4, fname, C_FILE_FG, C_BG);
        }
        row++;

        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
    }

    /* ── Section: QUICK ACTIONS ── (if space) */
    if (row + 3 <= CHAT_BOT) {
        row++;
        screen_put_char_at(row, 1, (char)BLOCK_FULL, VGA_GREEN, C_BG);
        screen_put_str_at(row, 3, "ACTIONS", C_SECT_FG, C_BG);
        row++;
        draw_hline_single(row, 1, SIDEBAR_W-1, VGA_DARK_GREY);
        row++;
        screen_put_char_at(row, 2, (char)ARROW_RIGHT, VGA_GREEN, C_BG);
        screen_put_str_at(row, 4, "ask <q>", VGA_LIGHT_GREY, C_BG);
        row++;
        if (row <= CHAT_BOT) {
            screen_put_char_at(row, 2, (char)ARROW_RIGHT, VGA_CYAN, C_BG);
            screen_put_str_at(row, 4, "help", VGA_LIGHT_GREY, C_BG);
        }
    }
}

/* ── Chat area ────────────────────────────────────────────── */

static void add_chat(const char *text, int role) {
    if (chat_count >= MAX_CHAT) {
        for (int i = 0; i < MAX_CHAT - 1; i++)
            chat[i] = chat[i + 1];
        chat_count = MAX_CHAT - 1;
    }
    strncpy(chat[chat_count].text, text, MAX_MSG_LEN - 1);
    chat[chat_count].text[MAX_MSG_LEN - 1] = '\0';
    chat[chat_count].role = role;
    chat_count++;

    if (chat_count > CHAT_LINES)
        chat_scroll = chat_count - CHAT_LINES;
}

static void draw_chat(void) {
    /* Clear chat area */
    screen_fill_rect(CHAT_TOP, CHAT_LEFT, CHAT_BOT, W-2, ' ', VGA_WHITE, C_CHAT_BG);

    int draw_row = CHAT_TOP;
    int start = chat_scroll;

    for (int i = start; i < chat_count && draw_row <= CHAT_BOT; i++) {
        uint8_t badge_fg, text_fg;
        const char *badge;

        if (chat[i].role == 1) {        /* user */
            badge = "[YOU]";
            badge_fg = C_USER_BADGE;
            text_fg = VGA_WHITE;
        } else if (chat[i].role == 2) { /* AI */
            badge = "[ AI]";
            badge_fg = C_AI_BADGE;
            text_fg = VGA_LIGHT_GREY;
        } else {                        /* system */
            badge = "[SYS]";
            badge_fg = VGA_DARK_GREY;
            text_fg = VGA_DARK_GREY;
        }

        /* Draw role badge */
        screen_put_str_at(draw_row, CHAT_LEFT, badge, badge_fg, C_CHAT_BG);
        screen_put_char_at(draw_row, CHAT_LEFT + 5, ' ', text_fg, C_CHAT_BG);

        /* Print message text with word wrapping */
        int col = CHAT_LEFT + 6;
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

/* ── Input bar ────────────────────────────────────────────── */

static void draw_input(void) {
    screen_fill_row(INPUT_ROW, 1, W-2, ' ', C_INPUT_FG, C_INPUT_BG);

    /* Prompt: colored bracket + arrow */
    screen_put_char_at(INPUT_ROW, 1, (char)BLOCK_FULL, VGA_CYAN, C_BG);
    screen_put_char_at(INPUT_ROW, 2, ' ', C_INPUT_FG, C_BG);
    screen_put_char_at(INPUT_ROW, 3, (char)ARROW_RIGHT, C_PROMPT_FG, C_BG);
    screen_put_char_at(INPUT_ROW, 4, ' ', C_INPUT_FG, C_BG);

    /* Input text */
    screen_put_str_at(INPUT_ROW, 5, input_buf, C_INPUT_FG, C_INPUT_BG);
    screen_set_cursor(INPUT_ROW, 5 + input_pos);
}

/* ── Status bar ───────────────────────────────────────────── */

static void draw_status(void) {
    screen_fill_row(STATUS_ROW, 0, W-1, ' ', C_STATUS_FG, C_STATUS_BG);

    /* Brand segment */
    screen_put_char_at(STATUS_ROW, 1, (char)SPADE, VGA_LIGHT_CYAN, C_STATUS_BG);
    screen_put_str_at(STATUS_ROW, 3, "SwanOS", VGA_WHITE, C_STATUS_BG);
    screen_put_char_at(STATUS_ROW, 10, (char)BOX_V, VGA_DARK_GREY, C_STATUS_BG);

    /* Model segment */
    screen_put_str_at(STATUS_ROW, 12, "Groq LLM", C_STATUS_FG, C_STATUS_BG);
    screen_put_char_at(STATUS_ROW, 21, (char)BOX_V, VGA_DARK_GREY, C_STATUS_BG);

    /* Serial bridge segment */
    screen_put_str_at(STATUS_ROW, 23, "Serial", C_STATUS_FG, C_STATUS_BG);
    screen_put_char_at(STATUS_ROW, 30, (char)BULLET, C_ONLINE, C_STATUS_BG);
    screen_put_char_at(STATUS_ROW, 32, (char)BOX_V, VGA_DARK_GREY, C_STATUS_BG);

    /* Architecture */
    screen_put_str_at(STATUS_ROW, 34, "x86", C_STATUS_FG, C_STATUS_BG);

    /* Uptime on right */
    uint32_t secs = timer_get_seconds();
    uint32_t mins = secs / 60;
    uint32_t hrs = mins / 60;
    secs %= 60; mins %= 60;
    char uptime[20]; char tmp[8];
    strcpy(uptime, "Up:");
    itoa(hrs, tmp, 10); strcat(uptime, tmp); strcat(uptime, "h");
    itoa(mins, tmp, 10); strcat(uptime, tmp); strcat(uptime, "m");
    itoa(secs, tmp, 10); strcat(uptime, tmp); strcat(uptime, "s");
    int ulen = strlen(uptime);
    screen_put_str_at(STATUS_ROW, W - ulen - 2, uptime, VGA_WHITE, C_STATUS_BG);
}

/* ── Hints bar ────────────────────────────────────────────── */

static void draw_hint_badge(int col, const char *key, const char *desc) {
    /* [key] desc format with colored key */
    screen_put_char_at(HINTS_ROW, col, '[', C_DIM, C_BG);
    int klen = strlen(key);
    screen_put_str_at(HINTS_ROW, col + 1, key, C_HINT_KEY, C_BG);
    screen_put_char_at(HINTS_ROW, col + 1 + klen, ']', C_DIM, C_BG);
    int dlen = strlen(desc);
    if (dlen > 0)
        screen_put_str_at(HINTS_ROW, col + 2 + klen, desc, C_HINT_DESC, C_BG);
}

static void draw_hints(void) {
    /* Clear and redraw with badge-style shortcuts */
    draw_hint_badge(2,  "ask",  " ");
    draw_hint_badge(8,  "help", " ");
    draw_hint_badge(15, "ls",   " ");
    draw_hint_badge(20, "clear"," ");
    draw_hint_badge(28, "calc", " ");
    draw_hint_badge(35, "cli",  " ");
    draw_hint_badge(41, "status"," ");
    draw_hint_badge(50, "shutdown", "");
}

/* ── Full screen redraw ───────────────────────────────────── */

static void draw_full(void) {
    screen_fill_rect(0, 0, H-1, W-1, ' ', VGA_WHITE, C_BG);
    draw_title();
    draw_borders();
    draw_sidebar();
    draw_chat();
    draw_input();
    draw_status();
    draw_hints();
    screen_show_cursor();
    screen_set_cursor(INPUT_ROW, 5 + input_pos);
}

/* ── Command processor (GUI mode) ──────────────────────── */

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
        return -3;
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
        add_chat(arg, 1);
        draw_chat(); draw_input();
        screen_set_cursor(INPUT_ROW, 5);

        /* Show thinking indicator with animation */
        add_chat("Thinking...", 0);
        draw_chat();

        char response[2048];
        llm_query(arg, response, sizeof(response));

        chat_count--;  /* remove thinking msg */
        add_chat(response, 2);
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
        (void)r;
        add_chat(content, 0);
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

/* ── GUI main loop ────────────────────────────────────────── */

void gui_run(void) {
    chat_count = 0;
    chat_scroll = 0;
    input_pos = 0;
    input_buf[0] = '\0';

    /* Clear screen */
    screen_fill_rect(0, 0, H-1, W-1, ' ', VGA_WHITE, VGA_BLACK);

    /* Welcome animation */
    add_chat("Welcome to SwanOS!", 0);
    add_chat("Type 'help' or 'ask <question>' to begin.", 0);
    draw_full();

    uint32_t last_status_tick = 0;

    while (1) {
        char c = keyboard_getchar();

        if (c == '\n') {
            char *cmd = trim(input_buf);
            if (cmd[0] != '\0') {
                int result = gui_process_cmd(cmd);

                draw_sidebar();
                draw_chat();

                if (result == -1) {
                    /* Shutdown animation */
                    screen_fill_rect(0, 0, H-1, W-1, ' ', VGA_WHITE, VGA_BLACK);
                    screen_put_str_at(10, 28, "Shutting down...", VGA_YELLOW, VGA_BLACK);

                    /* Fade-out effect with block chars */
                    for (int r = 0; r < H; r++) {
                        for (int c2 = 0; c2 < W; c2++)
                            screen_put_char_at(r, c2, (char)BLOCK_LIGHT, VGA_DARK_GREY, VGA_BLACK);
                        screen_delay(40);
                    }
                    screen_fill_rect(0, 0, H-1, W-1, ' ', VGA_WHITE, VGA_BLACK);
                    screen_put_str_at(12, 32, "Goodbye.", VGA_DARK_GREY, VGA_BLACK);
                    screen_delay(500);
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
                    return; /* CLI mode */
                }
                if (result == -4) {
                    return; /* re-login */
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

        /* Periodic status/sidebar update (~5 seconds) */
        uint32_t ticks = timer_get_ticks();
        if (ticks - last_status_tick > 500) {
            draw_status();
            draw_sidebar();
            screen_set_cursor(INPUT_ROW, 5 + input_pos);
            last_status_tick = ticks;
        }
    }
}
