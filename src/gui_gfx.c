/* ============================================================
 * SwanOS — VGA Mode 13h Graphical Desktop
 * 320x200, 256 colors, double-buffered pixel rendering.
 *
 *  Layout (320x200):
 *  y 0-12:    Title bar
 *  y 13:      Separator line
 *  y 14-172:  Sidebar(0-79) | Chat area(82-319)
 *  y 173:     Separator line
 *  y 174-185: Input bar
 *  y 186:     Separator line
 *  y 187-199: Status bar
 * ============================================================ */

#include "gui_gfx.h"
#include "vga_gfx.h"
#include "keyboard.h"
#include "string.h"
#include "timer.h"
#include "fs.h"
#include "user.h"
#include "llm.h"
#include "memory.h"
#include "rtc.h"
#include "game.h"
#include "screen.h"
#include "ports.h"

/* ── Layout constants ─────────────────────────────────────── */
#define SCRW         320
#define SCRH         200
#define TITLE_H      12
#define SEP_Y1       (TITLE_H)          /* 12 */
#define SIDEBAR_W    80
#define SIDEBAR_X    0
#define CHAT_X       82
#define CHAT_W       (SCRW - CHAT_X - 1) /* 237 */
#define MAIN_Y       (SEP_Y1 + 1)       /* 13 */
#define MAIN_BOTTOM  172
#define MAIN_H       (MAIN_BOTTOM - MAIN_Y) /* 159 */
#define SEP_Y2       173
#define INPUT_Y      174
#define INPUT_H      12
#define SEP_Y3       186
#define STATUS_Y     187
#define STATUS_H     13
#define CHAR_W       7
#define CHAR_H       9  /* 8px font + 1px spacing */

/* Max chars per line in chat area */
#define CHAT_CHARS   ((CHAT_W - 4) / CHAR_W)  /* ~33 */

/* ── Palette indices ──────────────────────────────────────── */
/* Using the modern palette from vga_gfx.c:
   0 = black, 1-2 = near-black bg, 3-5 = greys, 6 = near-white, 7 = white
   10-19 = cyan gradient, 20-29 = blue, 30-39 = green, 40-49 = gold
   50-59 = purple, 80-89 = subtle bg gradient */

#define P_BG         1    /* dark bg */
#define P_BG2        2    /* slightly lighter bg */
#define P_BG3        80   /* panel bg */
#define P_BORDER     3    /* border color */
#define P_BORDER2    4    /* lighter border */
#define P_DIM        3    /* dim text */
#define P_TEXT       6    /* normal text */
#define P_BRIGHT     7    /* bright text */
#define P_CYAN       15   /* bright cyan */
#define P_CYAN_DIM   12   /* dim cyan */
#define P_GREEN      35   /* bright green */
#define P_GREEN_DIM  32   /* dim green */
#define P_GOLD       45   /* bright gold */
#define P_GOLD_DIM   42   /* dim gold */
#define P_BLUE       25   /* blue */
#define P_PURPLE     55   /* purple */
#define P_RED        4    /* using VGA red — we'll add a custom one */
#define P_TITLE_BG   22   /* dark blue for title bar */

/* ── Chat buffer ──────────────────────────────────────────── */
#define MAX_CHAT      64
#define MAX_MSG_LEN   200

typedef struct {
    char text[MAX_MSG_LEN];
    int  role; /* 0=system, 1=user, 2=ai */
} gchat_msg_t;

static gchat_msg_t gchat[MAX_CHAT];
static int gchat_count = 0;
static int gchat_scroll = 0;

/* ── Input buffer ─────────────────────────────────────────── */
#define GINPUT_MAX 120
static char ginput_buf[GINPUT_MAX];
static int  ginput_pos = 0;

/* ── Command history ──────────────────────────────────────── */
#define GHIST_SIZE 16
static char ghist[GHIST_SIZE][GINPUT_MAX];
static int  ghist_count = 0;
static int  ghist_pos   = 0;

static void ghist_add(const char *cmd) {
    if (cmd[0] == '\0') return;
    if (ghist_count > 0 && strcmp(ghist[(ghist_count - 1) % GHIST_SIZE], cmd) == 0)
        return;
    strcpy(ghist[ghist_count % GHIST_SIZE], cmd);
    ghist_count++;
}

/* ── Additional palette setup ─────────────────────────────── */
static void setup_gui_palette(void) {
    /* 90: red for errors */
    vga_set_palette(90, 55, 12, 12);
    /* 91: brighter red */
    vga_set_palette(91, 63, 18, 18);
    /* 92: orange */
    vga_set_palette(92, 63, 40, 10);
    /* P_TITLE_BG: deeper blue */
    vga_set_palette(P_TITLE_BG, 3, 6, 18);
    /* 95: sidebar header bg */
    vga_set_palette(95, 2, 4, 12);
    /* 96: input bg */
    vga_set_palette(96, 2, 3, 8);
}

/* ── Drawing helpers ──────────────────────────────────────── */

/* Draw str truncated to max_chars */
static void bb_str_trunc(int x, int y, const char *s, int max_chars, uint8_t fg, uint8_t bg) {
    int i = 0;
    while (s[i] && i < max_chars) {
        vga_bb_draw_char(x + i * CHAR_W, y, s[i], fg, bg);
        i++;
    }
}

/* Count lines needed for a wrapped message */
static int msg_lines(const char *text) {
    if (!text[0]) return 1;
    int lines = 1, col = 0;
    const char *p = text;
    while (*p) {
        if (*p == '\n' || col >= CHAT_CHARS) {
            lines++;
            col = 0;
            if (*p == '\n') p++;
            continue;
        }
        col++;
        p++;
    }
    return lines;
}

/* ── Title bar ────────────────────────────────────────────── */
static void draw_gfx_title(void) {
    /* Title bar background */
    vga_bb_fill_rect(0, 0, SCRW, TITLE_H, P_TITLE_BG);

    /* Swan brand */
    vga_bb_draw_string(3, 2, "SWANOS", P_CYAN, P_TITLE_BG);
    vga_bb_draw_string(3 + 6 * CHAR_W + 2, 2, "v2.0", P_DIM, P_TITLE_BG);

    /* AI badge */
    vga_bb_draw_string(80, 2, "[AI]", P_CYAN_DIM, P_TITLE_BG);

    /* User badge (right side) */
    const char *usr = user_current();
    int ulen = strlen(usr);
    int ux = SCRW - (ulen + 9) * CHAR_W;
    vga_bb_draw_string(ux, 2, usr, P_BRIGHT, P_TITLE_BG);

    /* Online indicator */
    int ox = SCRW - 7 * CHAR_W;
    /* Green dot */
    for (int dy = 0; dy < 4; dy++)
        for (int dx = 0; dx < 4; dx++)
            vga_bb_putpixel(ox + dx, 4 + dy, P_GREEN);
    vga_bb_draw_string(ox + 6, 2, "ON", P_GREEN, P_TITLE_BG);

    /* RTC clock */
    rtc_time_t rtc;
    rtc_read(&rtc);
    char clk[10];
    rtc_format_time(&rtc, clk);
    vga_bb_draw_string(SCRW - 6 * CHAR_W - 2, 2, clk, P_TEXT, P_TITLE_BG);
}

/* ── Separator lines ──────────────────────────────────────── */
static void draw_gfx_separators(void) {
    vga_bb_draw_hline(0, SEP_Y1, SCRW, P_BORDER);
    vga_bb_draw_hline(0, SEP_Y2, SCRW, P_BORDER);
    vga_bb_draw_hline(0, SEP_Y3, SCRW, P_BORDER);
    /* Sidebar right border */
    vga_bb_draw_vline(SIDEBAR_W, MAIN_Y, MAIN_BOTTOM - MAIN_Y, P_BORDER);
}

/* ── Sidebar ──────────────────────────────────────────────── */
static void draw_gfx_sidebar(void) {
    /* Clear sidebar */
    vga_bb_fill_rect(0, MAIN_Y, SIDEBAR_W, MAIN_BOTTOM - MAIN_Y, P_BG);

    int y = MAIN_Y + 2;

    /* SYSTEM header */
    vga_bb_fill_rect(1, y, SIDEBAR_W - 2, CHAR_H, 95);
    vga_bb_draw_string(3, y, "SYSTEM", P_GOLD, 95);
    y += CHAR_H + 2;

    /* AI model */
    vga_bb_draw_string(4, y, "AI:", P_DIM, P_BG);
    vga_bb_draw_string(4 + 3 * CHAR_W, y, "Groq LLM", P_TEXT, P_BG);
    y += CHAR_H;

    /* Uptime */
    uint32_t secs = timer_get_seconds();
    uint32_t mins = secs / 60;
    uint32_t hrs  = mins / 60;
    secs %= 60; mins %= 60;
    char upbuf[20]; char tmp[8];
    upbuf[0] = '\0';
    itoa(hrs, tmp, 10); strcat(upbuf, tmp); strcat(upbuf, "h ");
    itoa(mins, tmp, 10); strcat(upbuf, tmp); strcat(upbuf, "m ");
    itoa(secs % 60, tmp, 10); strcat(upbuf, tmp); strcat(upbuf, "s");
    vga_bb_draw_string(4, y, "Up:", P_DIM, P_BG);
    vga_bb_draw_string(4 + 3 * CHAR_W, y, upbuf, P_TEXT, P_BG);
    y += CHAR_H;

    /* Status */
    vga_bb_draw_string(4, y, "Net:", P_DIM, P_BG);
    /* Green dot */
    for (int dy2 = 0; dy2 < 3; dy2++)
        for (int dx2 = 0; dx2 < 3; dx2++)
            vga_bb_putpixel(4 + 4 * CHAR_W + dx2, y + 3 + dy2, P_GREEN);
    vga_bb_draw_string(4 + 5 * CHAR_W, y, "Online", P_GREEN, P_BG);
    y += CHAR_H;

    /* Msgs count */
    char mbuf[8]; itoa(gchat_count, mbuf, 10);
    vga_bb_draw_string(4, y, "Msg:", P_DIM, P_BG);
    vga_bb_draw_string(4 + 4 * CHAR_W, y, mbuf, P_TEXT, P_BG);
    y += CHAR_H + 4;

    /* FILES header */
    vga_bb_fill_rect(1, y, SIDEBAR_W - 2, CHAR_H, 95);
    vga_bb_draw_string(3, y, "FILES", P_GOLD, 95);
    y += CHAR_H + 2;

    /* File listing */
    char listing[512];
    fs_list("/", listing, sizeof(listing));
    char *p = listing;
    while (*p && y + CHAR_H <= MAIN_BOTTOM) {
        while (*p == ' ') p++;
        if (*p == '\0' || *p == '\n') { if (*p) p++; continue; }

        int is_dir = (strncmp(p, "[DIR]", 5) == 0);
        char *ns = strchr(p, ']');
        if (ns) ns += 2; else ns = p;

        char fname[12]; int fi = 0;
        while (*ns && *ns != '\n' && fi < 10) {
            fname[fi++] = *ns++;
        }
        fname[fi] = '\0';

        if (is_dir) {
            vga_bb_draw_string(6, y, "+", P_GOLD, P_BG);
            vga_bb_draw_string(6 + CHAR_W + 2, y, fname, P_GOLD_DIM, P_BG);
        } else {
            vga_bb_draw_string(6, y, "-", P_DIM, P_BG);
            vga_bb_draw_string(6 + CHAR_W + 2, y, fname, P_CYAN_DIM, P_BG);
        }
        y += CHAR_H;

        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
    }

    /* ACTIONS section if space */
    if (y + CHAR_H * 4 <= MAIN_BOTTOM) {
        y += 3;
        vga_bb_fill_rect(1, y, SIDEBAR_W - 2, CHAR_H, 95);
        vga_bb_draw_string(3, y, "ACTIONS", P_GOLD, 95);
        y += CHAR_H + 2;
        vga_bb_draw_string(6, y, "> ask <q>", P_GREEN_DIM, P_BG);
        y += CHAR_H;
        vga_bb_draw_string(6, y, "> help", P_CYAN_DIM, P_BG);
        y += CHAR_H;
        vga_bb_draw_string(6, y, "> clear", P_DIM, P_BG);
    }
}

/* ── Chat area ────────────────────────────────────────────── */
static void gchat_add(const char *text, int role) {
    if (gchat_count >= MAX_CHAT) {
        for (int i = 0; i < MAX_CHAT - 1; i++)
            gchat[i] = gchat[i + 1];
        gchat_count = MAX_CHAT - 1;
    }
    strncpy(gchat[gchat_count].text, text, MAX_MSG_LEN - 1);
    gchat[gchat_count].text[MAX_MSG_LEN - 1] = '\0';
    gchat[gchat_count].role = role;
    gchat_count++;

    /* Auto-scroll: calculate total visual lines */
    int total_vis = 0;
    int max_vis_lines = (MAIN_BOTTOM - MAIN_Y - 2) / CHAR_H;
    for (int i = 0; i < gchat_count; i++)
        total_vis += msg_lines(gchat[i].text);
    if (total_vis > max_vis_lines) {
        /* Find scroll position so last messages are visible */
        int accum = 0;
        gchat_scroll = 0;
        for (int i = gchat_count - 1; i >= 0; i--) {
            accum += msg_lines(gchat[i].text);
            if (accum > max_vis_lines) { gchat_scroll = i + 1; break; }
        }
    }
}

static void draw_gfx_chat(void) {
    /* Clear chat area */
    vga_bb_fill_rect(CHAT_X, MAIN_Y, CHAT_W, MAIN_BOTTOM - MAIN_Y, P_BG);

    int y = MAIN_Y + 2;
    int max_lines = (MAIN_BOTTOM - MAIN_Y - 2) / CHAR_H;
    int lines_drawn = 0;

    for (int i = gchat_scroll; i < gchat_count && lines_drawn < max_lines; i++) {
        uint8_t badge_fg, text_fg;
        const char *badge;

        if (gchat[i].role == 1) {         /* user */
            badge = "[YOU]";
            badge_fg = P_GREEN;
            text_fg = P_BRIGHT;
        } else if (gchat[i].role == 2) {  /* AI */
            badge = "[ AI]";
            badge_fg = P_CYAN;
            text_fg = P_TEXT;
        } else {                          /* system */
            badge = "[SYS]";
            badge_fg = P_DIM;
            text_fg = P_DIM;
        }

        /* Draw badge */
        vga_bb_draw_string(CHAT_X + 2, y, badge, badge_fg, P_BG);

        /* Draw message text with wrapping */
        int x = CHAT_X + 2 + 5 * CHAR_W + 2;
        const char *p = gchat[i].text;
        int col = 0;
        int avail = CHAT_CHARS - 6; /* chars after badge */

        while (*p && lines_drawn < max_lines) {
            if (*p == '\n' || col >= avail) {
                lines_drawn++;
                y += CHAR_H;
                if (y + CHAR_H > MAIN_BOTTOM) break;
                x = CHAT_X + 2 + 6 * CHAR_W; /* indented continuation */
                col = 0;
                avail = CHAT_CHARS - 7;
                if (*p == '\n') { p++; continue; }
            }
            vga_bb_draw_char(x, y, *p, text_fg, P_BG);
            x += CHAR_W;
            col++;
            p++;
        }
        lines_drawn++;
        y += CHAR_H;
    }
}

/* ── Input bar ────────────────────────────────────────────── */
static void draw_gfx_input(void) {
    /* Background */
    vga_bb_fill_rect(0, INPUT_Y, SCRW, INPUT_H, 96);

    /* Prompt: == user > _ */
    int x = 3;
    vga_bb_draw_string(x, INPUT_Y + 2, "==", P_DIM, 96);
    x += 2 * CHAR_W + 2;

    const char *usr = user_current();
    vga_bb_draw_string(x, INPUT_Y + 2, usr, P_GREEN, 96);
    x += strlen(usr) * CHAR_W + 2;

    vga_bb_draw_string(x, INPUT_Y + 2, ">", P_CYAN, 96);
    x += CHAR_W + 2;

    /* Input text */
    int prompt_x = x;
    int max_vis = (SCRW - x - 4) / CHAR_W;
    int start = 0;
    if (ginput_pos > max_vis) start = ginput_pos - max_vis;
    for (int i = start; i < ginput_pos && (i - start) < max_vis; i++) {
        vga_bb_draw_char(x + (i - start) * CHAR_W, INPUT_Y + 2,
                         ginput_buf[i], P_BRIGHT, 96);
    }

    /* Cursor (blinking rectangle) */
    int cx = prompt_x + (ginput_pos - start) * CHAR_W;
    if (cx < SCRW - 4) {
        uint8_t ccolor = (timer_get_ticks() / 30) % 2 ? P_CYAN : 96;
        vga_bb_fill_rect(cx, INPUT_Y + 2, CHAR_W - 1, 8, ccolor);
    }
}

/* ── Status bar ───────────────────────────────────────────── */
static void draw_gfx_status(void) {
    vga_bb_fill_rect(0, STATUS_Y, SCRW, STATUS_H, P_TITLE_BG);

    int x = 3;
    /* Brand */
    vga_bb_draw_string(x, STATUS_Y + 3, "SwanOS", P_BRIGHT, P_TITLE_BG);
    x += 6 * CHAR_W + 3;
    vga_bb_draw_string(x, STATUS_Y + 3, "|", P_DIM, P_TITLE_BG);
    x += CHAR_W + 2;

    /* Model */
    vga_bb_draw_string(x, STATUS_Y + 3, "Groq", P_TEXT, P_TITLE_BG);
    x += 4 * CHAR_W + 3;
    vga_bb_draw_string(x, STATUS_Y + 3, "|", P_DIM, P_TITLE_BG);
    x += CHAR_W + 2;

    /* Serial */
    vga_bb_draw_string(x, STATUS_Y + 3, "Serial", P_TEXT, P_TITLE_BG);
    x += 6 * CHAR_W + 2;
    for (int dy = 0; dy < 3; dy++)
        for (int dx = 0; dx < 3; dx++)
            vga_bb_putpixel(x + dx, STATUS_Y + 5 + dy, P_GREEN);
    x += 6;
    vga_bb_draw_string(x, STATUS_Y + 3, "|", P_DIM, P_TITLE_BG);
    x += CHAR_W + 2;

    /* Architecture */
    vga_bb_draw_string(x, STATUS_Y + 3, "x86", P_TEXT, P_TITLE_BG);

    /* Memory on right */
    char membuf[16]; char tmp[8];
    strcpy(membuf, "Mem:");
    itoa(mem_used() / 1024, tmp, 10); strcat(membuf, tmp);
    strcat(membuf, "K");
    int memx = SCRW - (strlen(membuf) + 1) * CHAR_W;
    vga_bb_draw_string(memx, STATUS_Y + 3, membuf, P_TEXT, P_TITLE_BG);
}

/* ── Full redraw ──────────────────────────────────────────── */
static void draw_gfx_full(void) {
    /* Dark background */
    vga_bb_fill_rect(0, 0, SCRW, SCRH, P_BG);

    draw_gfx_title();
    draw_gfx_separators();
    draw_gfx_sidebar();
    draw_gfx_chat();
    draw_gfx_input();
    draw_gfx_status();

    vga_flip();
}

/* ── Command processor ────────────────────────────────────── */
static int gfx_process_cmd(char *cmd) {
    char *arg = cmd;
    while (*arg && *arg != ' ') arg++;
    if (*arg) { *arg = '\0'; arg++; }
    while (*arg == ' ') arg++;

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
        gchat_count = 0;
        gchat_scroll = 0;
        gchat_add("SwanOS v2.0 - Graphical Mode", 0);
        return 0;
    }
    if (strcmp(cmd, "help") == 0) {
        gchat_add("=== COMMANDS ===", 0);
        gchat_add("ask <q>  - Ask AI", 0);
        gchat_add("ls       - List files", 0);
        gchat_add("cat <f>  - Read file", 0);
        gchat_add("write <f> <text>", 0);
        gchat_add("calc <expr>", 0);
        gchat_add("status   - System info", 0);
        gchat_add("mem      - Memory usage", 0);
        gchat_add("date     - Show date/time", 0);
        gchat_add("clear    - Clear chat", 0);
        gchat_add("cli      - CLI mode", 0);
        gchat_add("snake    - Play Snake!", 0);
        gchat_add("shutdown - Power off", 0);
        return 0;
    }
    if (strcmp(cmd, "ask") == 0) {
        if (arg[0] == '\0') {
            gchat_add("Usage: ask <question>", 0);
            return 0;
        }
        gchat_add(arg, 1);
        /* Show thinking */
        gchat_add("Thinking...", 0);
        draw_gfx_chat();
        vga_flip();

        char response[2048];
        llm_query(arg, response, sizeof(response));

        gchat_count--; /* remove thinking msg */
        gchat_add(response, 2);
        return 0;
    }
    if (strcmp(cmd, "status") == 0) {
        uint32_t secs = timer_get_seconds();
        uint32_t mins = secs / 60;
        uint32_t hrs  = mins / 60;
        secs %= 60; mins %= 60;
        char buf[80]; char tmp[8];
        strcpy(buf, "Uptime: ");
        itoa(hrs, tmp, 10); strcat(buf, tmp); strcat(buf, "h ");
        itoa(mins, tmp, 10); strcat(buf, tmp); strcat(buf, "m ");
        itoa(secs, tmp, 10); strcat(buf, tmp); strcat(buf, "s");
        gchat_add(buf, 0);
        strcpy(buf, "User: "); strcat(buf, user_current());
        gchat_add(buf, 0);
        gchat_add("Model: Groq LLM | x86", 0);
        return 0;
    }
    if (strcmp(cmd, "ls") == 0) {
        char listing[512];
        fs_list(arg[0] ? arg : "/", listing, sizeof(listing));
        /* Split into lines */
        char *p = listing;
        while (*p) {
            char line[64]; int li = 0;
            while (*p && *p != '\n' && li < 60) line[li++] = *p++;
            line[li] = '\0';
            if (li > 0) gchat_add(line, 0);
            if (*p == '\n') p++;
        }
        return 0;
    }
    if (strcmp(cmd, "cat") == 0) {
        if (arg[0] == '\0') { gchat_add("Usage: cat <file>", 0); return 0; }
        char content[512];
        int r = fs_read(arg, content, sizeof(content));
        (void)r;
        gchat_add(content, 0);
        return 0;
    }
    if (strcmp(cmd, "write") == 0) {
        char *ct = arg;
        while (*ct && *ct != ' ') ct++;
        if (*ct) { *ct = '\0'; ct++; }
        while (*ct == ' ') ct++;
        if (arg[0] == '\0' || ct[0] == '\0') {
            gchat_add("Usage: write <f> <text>", 0);
            return 0;
        }
        if (fs_write(arg, ct) == 0) {
            char msg[64]; strcpy(msg, "Written: "); strcat(msg, arg);
            gchat_add(msg, 0);
        } else {
            gchat_add("Write failed.", 0);
        }
        return 0;
    }
    if (strcmp(cmd, "mkdir") == 0) {
        if (arg[0] == '\0') { gchat_add("Usage: mkdir <name>", 0); return 0; }
        if (fs_mkdir(arg) == 0) {
            char msg[64]; strcpy(msg, "Created: "); strcat(msg, arg);
            gchat_add(msg, 0);
        } else gchat_add("Failed (exists?).", 0);
        return 0;
    }
    if (strcmp(cmd, "rm") == 0) {
        if (arg[0] == '\0') { gchat_add("Usage: rm <file>", 0); return 0; }
        int r = fs_delete(arg);
        if (r == 0) { char msg[64]; strcpy(msg, "Deleted: "); strcat(msg, arg); gchat_add(msg, 0); }
        else if (r == -2) gchat_add("Dir not empty.", 0);
        else gchat_add("Not found.", 0);
        return 0;
    }
    if (strcmp(cmd, "calc") == 0) {
        if (arg[0] == '\0') { gchat_add("Usage: calc <expr>", 0); return 0; }
        int result = 0, num = 0, sign = 1; char op = '+'; int has = 0;
        const char *e = arg;
        while (*e) {
            if (*e >= '0' && *e <= '9') { num = num * 10 + (*e - '0'); has = 1; }
            else if (*e == '+' || *e == '-' || *e == '*' || *e == '/') {
                if (has) {
                    if (op == '+') result += sign * num;
                    else if (op == '-') result -= num;
                    else if (op == '*') result *= num;
                    else if (op == '/' && num) result /= num;
                }
                op = *e;
                if (op == '+' || op == '-') { sign = (op == '-') ? -1 : 1; op = '+'; }
                num = 0; has = 0;
            }
            e++;
        }
        if (has) {
            if (op == '+') result += sign * num;
            else if (op == '-') result -= num;
            else if (op == '*') result *= num;
            else if (op == '/' && num) result /= num;
        }
        char msg[32]; char nb[16];
        strcpy(msg, "= "); itoa(result, nb, 10); strcat(msg, nb);
        gchat_add(msg, 0);
        return 0;
    }
    if (strcmp(cmd, "echo") == 0) {
        gchat_add(arg, 0);
        return 0;
    }
    if (strcmp(cmd, "whoami") == 0) {
        char msg[32]; strcpy(msg, "User: "); strcat(msg, user_current());
        gchat_add(msg, 0);
        return 0;
    }
    if (strcmp(cmd, "date") == 0) {
        rtc_time_t t;
        rtc_read(&t);
        char dbuf[12], tbuf[10], wbuf[4];
        rtc_format_date(&t, dbuf);
        rtc_format_time(&t, tbuf);
        rtc_format_weekday(&t, wbuf);
        char msg[40];
        strcpy(msg, wbuf); strcat(msg, " ");
        strcat(msg, dbuf); strcat(msg, " ");
        strcat(msg, tbuf);
        gchat_add(msg, 0);
        return 0;
    }
    if (strcmp(cmd, "mem") == 0) {
        char msg[64]; char nb[16];
        strcpy(msg, "Used: ");
        itoa(mem_used() / 1024, nb, 10); strcat(msg, nb);
        strcat(msg, "K / ");
        itoa(mem_total() / 1024, nb, 10); strcat(msg, nb);
        strcat(msg, "K");
        gchat_add(msg, 0);
        strcpy(msg, "Free: ");
        itoa(mem_free() / 1024, nb, 10); strcat(msg, nb);
        strcat(msg, "K");
        gchat_add(msg, 0);
        return 0;
    }
    if (strcmp(cmd, "snake") == 0) {
        game_snake();
        /* After snake, re-init graphics mode */
        vga_gfx_init();
        setup_gui_palette();
        return 0;
    }
    if (strcmp(cmd, "cp") == 0) {
        char *dst = arg;
        while (*dst && *dst != ' ') dst++;
        if (*dst) { *dst = '\0'; dst++; }
        while (*dst == ' ') dst++;
        if (arg[0] == '\0' || dst[0] == '\0') {
            gchat_add("Usage: cp <src> <dst>", 0); return 0;
        }
        if (fs_copy(arg, dst) == 0) {
            char msg[64]; strcpy(msg, "Copied: "); strcat(msg, arg);
            strcat(msg, " -> "); strcat(msg, dst);
            gchat_add(msg, 0);
        } else gchat_add("Copy failed.", 0);
        return 0;
    }
    if (strcmp(cmd, "mv") == 0) {
        char *nn = arg;
        while (*nn && *nn != ' ') nn++;
        if (*nn) { *nn = '\0'; nn++; }
        while (*nn == ' ') nn++;
        if (arg[0] == '\0' || nn[0] == '\0') {
            gchat_add("Usage: mv <f> <new>", 0); return 0;
        }
        if (fs_rename(arg, nn) == 0) {
            char msg[64]; strcpy(msg, "Renamed: "); strcat(msg, nn);
            gchat_add(msg, 0);
        } else gchat_add("Rename failed.", 0);
        return 0;
    }
    if (strcmp(cmd, "append") == 0) {
        char *ct = arg;
        while (*ct && *ct != ' ') ct++;
        if (*ct) { *ct = '\0'; ct++; }
        while (*ct == ' ') ct++;
        if (arg[0] == '\0' || ct[0] == '\0') {
            gchat_add("Usage: append <f> <text>", 0); return 0;
        }
        if (fs_append(arg, ct) == 0) {
            char msg[64]; strcpy(msg, "Appended: "); strcat(msg, arg);
            gchat_add(msg, 0);
        } else gchat_add("Append failed.", 0);
        return 0;
    }
    if (strcmp(cmd, "history") == 0) {
        int start = ghist_count > GHIST_SIZE ? ghist_count - GHIST_SIZE : 0;
        for (int i = start; i < ghist_count; i++) {
            char msg[GINPUT_MAX + 8]; char nb[8];
            itoa(i + 1, nb, 10);
            strcpy(msg, nb); strcat(msg, ": ");
            strcat(msg, ghist[i % GHIST_SIZE]);
            gchat_add(msg, 0);
        }
        return 0;
    }

    /* Unknown command */
    char msg[64]; strcpy(msg, "Unknown: "); strcat(msg, cmd);
    gchat_add(msg, 0);
    gchat_add("Type 'help' for commands.", 0);
    return 0;
}

/* ── GUI Main Loop ────────────────────────────────────────── */
void gui_gfx_run(void) {
    /* Initialize graphics mode */
    screen_set_serial_mirror(0);
    vga_gfx_init();
    setup_gui_palette();

    /* Reset state */
    gchat_count = 0;
    gchat_scroll = 0;
    ginput_pos = 0;
    ginput_buf[0] = '\0';
    ghist_pos = ghist_count;

    /* Welcome */
    gchat_add("Welcome to SwanOS!", 0);
    gchat_add("Graphical Desktop v2.0", 0);
    gchat_add("Type 'help' for commands.", 0);

    /* Fade in */
    draw_gfx_full();
    vga_fade_from_black(8);

    uint32_t last_refresh = 0;

    while (1) {
        /* Check for keyboard input */
        if (keyboard_has_key()) {
            char c = keyboard_getchar();

            if (c == '\n') {
                /* Process command */
                ginput_buf[ginput_pos] = '\0';
                char *cmd = ginput_buf;
                while (*cmd == ' ') cmd++;

                if (cmd[0] != '\0') {
                    ghist_add(cmd);
                    int result = gfx_process_cmd(cmd);

                    if (result == -1) {
                        /* Shutdown */
                        vga_fade_to_black(8);
                        vga_gfx_exit();
                        screen_init();
                        screen_set_serial_mirror(1);
                        screen_clear();
                        screen_set_color(VGA_DARK_GREY, VGA_BLACK);
                        screen_print("\n\n   Goodbye.\n");
                        screen_delay(500);
                        __asm__ volatile("cli; hlt");
                        while (1);
                    }
                    if (result == -2) {
                        /* Reboot */
                        uint8_t good = 0x02;
                        while (good & 0x02) good = inb(0x64);
                        outb(0x64, 0xFE);
                        __asm__ volatile("cli; hlt");
                    }
                    if (result == -3 || result == -4) {
                        /* Switch to CLI / re-login */
                        vga_fade_to_black(8);
                        vga_gfx_exit();
                        screen_init();
                        screen_set_serial_mirror(1);
                        return;
                    }
                }

                ginput_pos = 0;
                ginput_buf[0] = '\0';
                ghist_pos = ghist_count;
                draw_gfx_full();
            }
            else if (c == '\b') {
                if (ginput_pos > 0) {
                    ginput_pos--;
                    ginput_buf[ginput_pos] = '\0';
                    draw_gfx_input();
                    vga_flip();
                }
            }
            else if ((uint8_t)c == KEY_UP) {
                if (ghist_pos > 0 && ghist_pos > ghist_count - GHIST_SIZE) {
                    ghist_pos--;
                    strcpy(ginput_buf, ghist[ghist_pos % GHIST_SIZE]);
                    ginput_pos = strlen(ginput_buf);
                    draw_gfx_input();
                    vga_flip();
                }
            }
            else if ((uint8_t)c == KEY_DOWN) {
                if (ghist_pos < ghist_count - 1) {
                    ghist_pos++;
                    strcpy(ginput_buf, ghist[ghist_pos % GHIST_SIZE]);
                    ginput_pos = strlen(ginput_buf);
                } else {
                    ghist_pos = ghist_count;
                    ginput_buf[0] = '\0';
                    ginput_pos = 0;
                }
                draw_gfx_input();
                vga_flip();
            }
            else if (c >= ' ' && ginput_pos < GINPUT_MAX - 1) {
                ginput_buf[ginput_pos++] = c;
                ginput_buf[ginput_pos] = '\0';
                draw_gfx_input();
                vga_flip();
            }
        }

        /* Periodic refresh (~every 3 seconds for clock/uptime) */
        uint32_t ticks = timer_get_ticks();
        if (ticks - last_refresh > 300) {
            draw_gfx_title();
            draw_gfx_sidebar();
            draw_gfx_status();
            draw_gfx_input();  /* refresh cursor blink */
            vga_flip();
            last_refresh = ticks;
        }

        __asm__ volatile("hlt"); /* sleep until next interrupt */
    }
}
