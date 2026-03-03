/* ============================================================
 * SwanOS — Command Shell
 * Parses and executes user commands.
 * Features: command history (up/down arrows), hexdump, date,
 *           mem, snake, cp, mv, append, and more.
 * ============================================================ */

#include "shell.h"
#include "screen.h"
#include "keyboard.h"
#include "string.h"
#include "timer.h"
#include "fs.h"
#include "user.h"
#include "llm.h"
#include "memory.h"
#include "rtc.h"
#include "game.h"
#include "ports.h"

#define CMD_BUF 256
#define OUT_BUF 4096

static char cmd_buf[CMD_BUF];
static char out_buf[OUT_BUF];

/* ── Command history ───────────────────────────────────────── */
#define HIST_SIZE 16
static char history[HIST_SIZE][CMD_BUF];
static int  hist_count = 0;
static int  hist_pos   = 0; /* browsing position */

static void hist_add(const char *cmd) {
    if (cmd[0] == '\0') return;
    /* Don't add duplicates of the last entry */
    if (hist_count > 0 && strcmp(history[(hist_count - 1) % HIST_SIZE], cmd) == 0)
        return;
    strcpy(history[hist_count % HIST_SIZE], cmd);
    hist_count++;
}

/* ── Simple calculator ─────────────────────────────────────── */
static int calc_eval(const char *expr) {
    /* Left-to-right integer calculator: +, -, *, / */
    int result = 0, num = 0, has_num = 0;
    char pending_op = '+';

    while (1) {
        if (isdigit(*expr)) {
            num = num * 10 + (*expr - '0');
            has_num = 1;
        } else {
            /* Apply pending operator */
            if (has_num) {
                switch (pending_op) {
                    case '+': result += num; break;
                    case '-': result -= num; break;
                    case '*': result *= num; break;
                    case '/': if (num != 0) result /= num; break;
                }
            }
            if (*expr == '\0') break;
            if (*expr == '+' || *expr == '-' || *expr == '*' || *expr == '/') {
                pending_op = *expr;
                num = 0;
                has_num = 0;
            }
            /* skip spaces and other chars */
        }
        expr++;
    }
    return result;
}

/* ── Hexdump ───────────────────────────────────────────────── */
static void cmd_hexdump(const char *filename) {
    int r = fs_read(filename, out_buf, OUT_BUF);
    if (r < 0) {
        screen_set_color(VGA_RED, VGA_BLACK);
        screen_print("  File not found: ");
        screen_print(filename);
        screen_print("\n");
        screen_set_color(VGA_WHITE, VGA_BLACK);
        return;
    }

    int len = strlen(out_buf);
    char hex[4];
    screen_set_color(VGA_CYAN, VGA_BLACK);
    screen_print("  Offset   Hex                                       ASCII\n");
    screen_set_color(VGA_DARK_GREY, VGA_BLACK);
    screen_print("  ──────── ──────────────────────────────────────────── ────────────────\n");
    screen_set_color(VGA_WHITE, VGA_BLACK);

    for (int off = 0; off < len; off += 16) {
        /* Offset */
        screen_set_color(VGA_DARK_GREY, VGA_BLACK);
        screen_print("  ");
        itoa(off, hex, 16);
        /* Pad offset to 8 chars */
        int hl = strlen(hex);
        for (int p = 0; p < 8 - hl; p++) screen_putchar('0');
        screen_print(hex);
        screen_print("  ");

        /* Hex bytes */
        screen_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
        for (int i = 0; i < 16; i++) {
            if (off + i < len) {
                uint8_t b = (uint8_t)out_buf[off + i];
                char h[3];
                h[0] = "0123456789abcdef"[b >> 4];
                h[1] = "0123456789abcdef"[b & 0xF];
                h[2] = '\0';
                screen_print(h);
                screen_putchar(' ');
            } else {
                screen_print("   ");
            }
        }

        /* ASCII */
        screen_set_color(VGA_GREEN, VGA_BLACK);
        screen_putchar(' ');
        for (int i = 0; i < 16 && off + i < len; i++) {
            char c = out_buf[off + i];
            if (c >= ' ' && c <= '~')
                screen_putchar(c);
            else
                screen_putchar('.');
        }
        screen_putchar('\n');
    }
    screen_set_color(VGA_DARK_GREY, VGA_BLACK);
    screen_print("  ");
    char nb[16]; itoa(len, nb, 10);
    screen_print(nb);
    screen_print(" bytes\n");
    screen_set_color(VGA_WHITE, VGA_BLACK);
}

/* ── Print prompt ──────────────────────────────────────────── */
static void print_prompt(void) {
    screen_set_color(VGA_GREEN, VGA_BLACK);
    screen_print("  ");
    screen_print(user_current());
    screen_set_color(VGA_CYAN, VGA_BLACK);
    screen_print(" > ");
    screen_set_color(VGA_WHITE, VGA_BLACK);
}

/* ── Read line with history ────────────────────────────────── */
static int read_line_with_history(char *buf, int max_len) {
    int pos = 0;
    buf[0] = '\0';
    hist_pos = hist_count;

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
                buf[pos] = '\0';
                screen_backspace();
            }
        }
        else if ((uint8_t)c == KEY_UP) {
            /* Recall previous command */
            if (hist_pos > 0 && hist_pos > hist_count - HIST_SIZE) {
                hist_pos--;
                /* Clear current line on screen */
                while (pos > 0) { screen_backspace(); pos--; }
                strcpy(buf, history[hist_pos % HIST_SIZE]);
                pos = strlen(buf);
                screen_print(buf);
            }
        }
        else if ((uint8_t)c == KEY_DOWN) {
            /* Recall next command */
            if (hist_pos < hist_count - 1) {
                hist_pos++;
                while (pos > 0) { screen_backspace(); pos--; }
                strcpy(buf, history[hist_pos % HIST_SIZE]);
                pos = strlen(buf);
                screen_print(buf);
            } else if (hist_pos == hist_count - 1) {
                hist_pos = hist_count;
                while (pos > 0) { screen_backspace(); pos--; }
                buf[0] = '\0';
            }
        }
        else if (c >= ' ') { /* printable */
            buf[pos++] = c;
            buf[pos] = '\0';
            screen_putchar(c);
        }
    }

    buf[pos] = '\0';
    screen_putchar('\n');
    return pos;
}

/* ── Help ──────────────────────────────────────────────────── */
static void cmd_help(void) {
    screen_set_color(VGA_CYAN, VGA_BLACK);
    screen_print("\n  SwanOS Commands\n");
    screen_print("  ─────────────────────────────\n");
    screen_set_color(VGA_YELLOW, VGA_BLACK);
    screen_print("  AI\n");
    screen_set_color(VGA_WHITE, VGA_BLACK);
    screen_print("    ask <question>   Ask the AI\n");
    screen_set_color(VGA_YELLOW, VGA_BLACK);
    screen_print("  Files\n");
    screen_set_color(VGA_WHITE, VGA_BLACK);
    screen_print("    ls [path]        List files\n");
    screen_print("    cat <file>       Read file\n");
    screen_print("    write <f> <txt>  Write file\n");
    screen_print("    append <f> <txt> Append to file\n");
    screen_print("    cp <src> <dst>   Copy file\n");
    screen_print("    mv <file> <name> Rename file\n");
    screen_print("    mkdir <name>     Create dir\n");
    screen_print("    rm <file>        Delete file\n");
    screen_print("    hexdump <file>   Hex viewer\n");
    screen_set_color(VGA_YELLOW, VGA_BLACK);
    screen_print("  Utils\n");
    screen_set_color(VGA_WHITE, VGA_BLACK);
    screen_print("    calc <expr>      Calculator\n");
    screen_print("    echo <text>      Print text\n");
    screen_print("    clear            Clear screen\n");
    screen_print("    snake            Snake game!\n");
    screen_set_color(VGA_YELLOW, VGA_BLACK);
    screen_print("  System\n");
    screen_set_color(VGA_WHITE, VGA_BLACK);
    screen_print("    help             This help\n");
    screen_print("    whoami           User info\n");
    screen_print("    status           System info\n");
    screen_print("    date             Date & time\n");
    screen_print("    mem              Memory usage\n");
    screen_print("    time             Uptime\n");
    screen_print("    history          Command history\n");
    screen_print("    login            Switch user\n");
    screen_print("    reboot           Reboot\n");
    screen_print("    shutdown         Power off\n\n");
    screen_set_color(VGA_DARK_GREY, VGA_BLACK);
    screen_print("  Tip: Use UP/DOWN arrows for command history\n\n");
    screen_set_color(VGA_WHITE, VGA_BLACK);
}

/* ── Execute command ───────────────────────────────────────── */
static int execute_command(char *input) {
    char *cmd = trim(input);
    if (cmd[0] == '\0') return 0;

    /* Split into command + arg */
    char *arg = cmd;
    while (*arg && !isspace(*arg)) arg++;
    if (*arg) { *arg = '\0'; arg++; }
    arg = trim(arg);

    /* ── Power ── */
    if (strcmp(cmd, "shutdown") == 0 || strcmp(cmd, "exit") == 0) {
        screen_set_color(VGA_YELLOW, VGA_BLACK);
        screen_print("\n  Shutting down SwanOS...\n");
        screen_print("  Goodbye.\n\n");
        screen_set_color(VGA_WHITE, VGA_BLACK);
        return -1; /* signal halt */
    }

    if (strcmp(cmd, "reboot") == 0) {
        screen_set_color(VGA_CYAN, VGA_BLACK);
        screen_print("\n  Rebooting...\n");
        screen_set_color(VGA_WHITE, VGA_BLACK);
        return -2; /* signal reboot */
    }

    /* ── Help ── */
    if (strcmp(cmd, "help") == 0) { cmd_help(); return 0; }

    /* ── Clear ── */
    if (strcmp(cmd, "clear") == 0) { screen_clear(); return 0; }

    /* ── Ask AI ── */
    if (strcmp(cmd, "ask") == 0) {
        if (arg[0] == '\0') {
            screen_set_color(VGA_RED, VGA_BLACK);
            screen_print("  Usage: ask <question>\n");
            screen_set_color(VGA_WHITE, VGA_BLACK);
            return 0;
        }
        char response[2048];
        llm_query(arg, response, sizeof(response));
        screen_set_color(VGA_CYAN, VGA_BLACK);
        screen_print("\n  SwanOS AI > ");
        screen_set_color(VGA_WHITE, VGA_BLACK);
        screen_print(response);
        screen_print("\n");
        return 0;
    }

    /* ── Files ── */
    if (strcmp(cmd, "ls") == 0) {
        fs_list(arg[0] ? arg : "/", out_buf, OUT_BUF);
        screen_print(out_buf);
        return 0;
    }

    if (strcmp(cmd, "cat") == 0) {
        if (arg[0] == '\0') {
            screen_set_color(VGA_RED, VGA_BLACK);
            screen_print("  Usage: cat <filename>\n");
            screen_set_color(VGA_WHITE, VGA_BLACK);
            return 0;
        }
        int r = fs_read(arg, out_buf, OUT_BUF);
        if (r < 0) {
            screen_set_color(VGA_RED, VGA_BLACK);
        }
        screen_print("  ");
        screen_print(out_buf);
        screen_print("\n");
        screen_set_color(VGA_WHITE, VGA_BLACK);
        return 0;
    }

    if (strcmp(cmd, "write") == 0) {
        if (arg[0] == '\0') {
            screen_set_color(VGA_RED, VGA_BLACK);
            screen_print("  Usage: write <filename> <content>\n");
            screen_set_color(VGA_WHITE, VGA_BLACK);
            return 0;
        }
        /* Split arg into filename + content */
        char *content = arg;
        while (*content && !isspace(*content)) content++;
        if (*content) { *content = '\0'; content++; }
        content = trim(content);

        if (content[0] == '\0') {
            screen_set_color(VGA_RED, VGA_BLACK);
            screen_print("  Usage: write <filename> <content>\n");
            screen_set_color(VGA_WHITE, VGA_BLACK);
            return 0;
        }

        if (fs_write(arg, content) == 0) {
            screen_set_color(VGA_GREEN, VGA_BLACK);
            screen_print("  Written to ");
            screen_print(arg);
            screen_print("\n");
        } else {
            screen_set_color(VGA_RED, VGA_BLACK);
            screen_print("  Failed to write.\n");
        }
        screen_set_color(VGA_WHITE, VGA_BLACK);
        return 0;
    }

    if (strcmp(cmd, "append") == 0) {
        if (arg[0] == '\0') {
            screen_set_color(VGA_RED, VGA_BLACK);
            screen_print("  Usage: append <filename> <text>\n");
            screen_set_color(VGA_WHITE, VGA_BLACK);
            return 0;
        }
        char *content = arg;
        while (*content && !isspace(*content)) content++;
        if (*content) { *content = '\0'; content++; }
        content = trim(content);
        if (content[0] == '\0') {
            screen_set_color(VGA_RED, VGA_BLACK);
            screen_print("  Usage: append <filename> <text>\n");
            screen_set_color(VGA_WHITE, VGA_BLACK);
            return 0;
        }
        if (fs_append(arg, content) == 0) {
            screen_set_color(VGA_GREEN, VGA_BLACK);
            screen_print("  Appended to ");
            screen_print(arg);
            screen_print("\n");
        } else {
            screen_set_color(VGA_RED, VGA_BLACK);
            screen_print("  Failed to append.\n");
        }
        screen_set_color(VGA_WHITE, VGA_BLACK);
        return 0;
    }

    if (strcmp(cmd, "cp") == 0) {
        if (arg[0] == '\0') {
            screen_set_color(VGA_RED, VGA_BLACK);
            screen_print("  Usage: cp <source> <destination>\n");
            screen_set_color(VGA_WHITE, VGA_BLACK);
            return 0;
        }
        char *dst = arg;
        while (*dst && !isspace(*dst)) dst++;
        if (*dst) { *dst = '\0'; dst++; }
        dst = trim(dst);
        if (dst[0] == '\0') {
            screen_set_color(VGA_RED, VGA_BLACK);
            screen_print("  Usage: cp <source> <destination>\n");
            screen_set_color(VGA_WHITE, VGA_BLACK);
            return 0;
        }
        if (fs_copy(arg, dst) == 0) {
            screen_set_color(VGA_GREEN, VGA_BLACK);
            screen_print("  Copied ");
            screen_print(arg);
            screen_print(" -> ");
            screen_print(dst);
            screen_print("\n");
        } else {
            screen_set_color(VGA_RED, VGA_BLACK);
            screen_print("  Copy failed.\n");
        }
        screen_set_color(VGA_WHITE, VGA_BLACK);
        return 0;
    }

    if (strcmp(cmd, "mv") == 0) {
        if (arg[0] == '\0') {
            screen_set_color(VGA_RED, VGA_BLACK);
            screen_print("  Usage: mv <file> <newname>\n");
            screen_set_color(VGA_WHITE, VGA_BLACK);
            return 0;
        }
        char *newname = arg;
        while (*newname && !isspace(*newname)) newname++;
        if (*newname) { *newname = '\0'; newname++; }
        newname = trim(newname);
        if (newname[0] == '\0') {
            screen_set_color(VGA_RED, VGA_BLACK);
            screen_print("  Usage: mv <file> <newname>\n");
            screen_set_color(VGA_WHITE, VGA_BLACK);
            return 0;
        }
        if (fs_rename(arg, newname) == 0) {
            screen_set_color(VGA_GREEN, VGA_BLACK);
            screen_print("  Renamed to ");
            screen_print(newname);
            screen_print("\n");
        } else {
            screen_set_color(VGA_RED, VGA_BLACK);
            screen_print("  Rename failed.\n");
        }
        screen_set_color(VGA_WHITE, VGA_BLACK);
        return 0;
    }

    if (strcmp(cmd, "mkdir") == 0) {
        if (arg[0] == '\0') {
            screen_set_color(VGA_RED, VGA_BLACK);
            screen_print("  Usage: mkdir <dirname>\n");
            screen_set_color(VGA_WHITE, VGA_BLACK);
            return 0;
        }
        if (fs_mkdir(arg) == 0) {
            screen_set_color(VGA_GREEN, VGA_BLACK);
            screen_print("  Created directory: ");
            screen_print(arg);
            screen_print("\n");
        } else {
            screen_set_color(VGA_RED, VGA_BLACK);
            screen_print("  Failed (exists or parent not found).\n");
        }
        screen_set_color(VGA_WHITE, VGA_BLACK);
        return 0;
    }

    if (strcmp(cmd, "rm") == 0) {
        if (arg[0] == '\0') {
            screen_set_color(VGA_RED, VGA_BLACK);
            screen_print("  Usage: rm <filename>\n");
            screen_set_color(VGA_WHITE, VGA_BLACK);
            return 0;
        }
        int r = fs_delete(arg);
        if (r == 0) {
            screen_set_color(VGA_GREEN, VGA_BLACK);
            screen_print("  Deleted: ");
            screen_print(arg);
            screen_print("\n");
        } else if (r == -2) {
            screen_set_color(VGA_RED, VGA_BLACK);
            screen_print("  Directory not empty.\n");
        } else {
            screen_set_color(VGA_RED, VGA_BLACK);
            screen_print("  Not found.\n");
        }
        screen_set_color(VGA_WHITE, VGA_BLACK);
        return 0;
    }

    if (strcmp(cmd, "hexdump") == 0) {
        if (arg[0] == '\0') {
            screen_set_color(VGA_RED, VGA_BLACK);
            screen_print("  Usage: hexdump <filename>\n");
            screen_set_color(VGA_WHITE, VGA_BLACK);
            return 0;
        }
        cmd_hexdump(arg);
        return 0;
    }

    /* ── Calc ── */
    if (strcmp(cmd, "calc") == 0) {
        if (arg[0] == '\0') {
            screen_set_color(VGA_RED, VGA_BLACK);
            screen_print("  Usage: calc <expression>\n");
            screen_set_color(VGA_WHITE, VGA_BLACK);
            return 0;
        }
        int result = calc_eval(arg);
        char num_buf[32];
        itoa(result, num_buf, 10);
        screen_set_color(VGA_GREEN, VGA_BLACK);
        screen_print("  = ");
        screen_set_color(VGA_CYAN, VGA_BLACK);
        screen_print(num_buf);
        screen_print("\n");
        screen_set_color(VGA_WHITE, VGA_BLACK);
        return 0;
    }

    /* ── Echo ── */
    if (strcmp(cmd, "echo") == 0) {
        screen_print("  ");
        screen_print(arg);
        screen_print("\n");
        return 0;
    }

    /* ── Snake ── */
    if (strcmp(cmd, "snake") == 0) {
        game_snake();
        return 0;
    }

    /* ── Whoami ── */
    if (strcmp(cmd, "whoami") == 0) {
        screen_set_color(VGA_CYAN, VGA_BLACK);
        screen_print("  User: ");
        screen_set_color(VGA_GREEN, VGA_BLACK);
        screen_print(user_current());
        screen_print("\n");
        screen_set_color(VGA_CYAN, VGA_BLACK);
        screen_print("  OS:   SwanOS v2.0 (bare-metal)\n");
        screen_set_color(VGA_WHITE, VGA_BLACK);
        return 0;
    }

    /* ── Date ── */
    if (strcmp(cmd, "date") == 0) {
        rtc_time_t t;
        rtc_read(&t);
        char date_buf[12], time_buf[10], day_buf[4];
        rtc_format_date(&t, date_buf);
        rtc_format_time(&t, time_buf);
        rtc_format_weekday(&t, day_buf);

        screen_set_color(VGA_CYAN, VGA_BLACK);
        screen_print("  ");
        screen_print(day_buf);
        screen_print("  ");
        screen_set_color(VGA_WHITE, VGA_BLACK);
        screen_print(date_buf);
        screen_print("  ");
        screen_set_color(VGA_GREEN, VGA_BLACK);
        screen_print(time_buf);
        screen_print("\n");
        screen_set_color(VGA_WHITE, VGA_BLACK);
        return 0;
    }

    /* ── Mem ── */
    if (strcmp(cmd, "mem") == 0) {
        char buf[32];
        screen_set_color(VGA_CYAN, VGA_BLACK);
        screen_print("  Memory Usage\n");
        screen_print("  ─────────────────────────────\n");
        screen_set_color(VGA_WHITE, VGA_BLACK);
        screen_print("  Total : ");
        itoa(mem_total() / 1024, buf, 10);
        screen_print(buf);
        screen_print(" KB\n");
        screen_print("  Used  : ");
        itoa(mem_used() / 1024, buf, 10);
        screen_print(buf);
        screen_print(" KB\n");
        screen_print("  Free  : ");
        screen_set_color(VGA_GREEN, VGA_BLACK);
        itoa(mem_free() / 1024, buf, 10);
        screen_print(buf);
        screen_print(" KB\n");

        /* Bar graph */
        int pct = (int)((mem_used() * 100) / mem_total());
        screen_set_color(VGA_WHITE, VGA_BLACK);
        screen_print("  Usage : [");
        int bars = pct / 5;
        screen_set_color(pct > 80 ? VGA_RED : pct > 50 ? VGA_YELLOW : VGA_GREEN, VGA_BLACK);
        for (int i = 0; i < 20; i++) {
            screen_putchar(i < bars ? (char)BLOCK_FULL : (char)BLOCK_LIGHT);
        }
        screen_set_color(VGA_WHITE, VGA_BLACK);
        screen_print("] ");
        itoa(pct, buf, 10);
        screen_print(buf);
        screen_print("%\n");
        return 0;
    }

    /* ── Status ── */
    if (strcmp(cmd, "status") == 0) {
        uint32_t secs = timer_get_seconds();
        uint32_t mins = secs / 60;
        uint32_t hrs = mins / 60;
        secs %= 60; mins %= 60;

        rtc_time_t t;
        rtc_read(&t);
        char date_buf[12], time_buf[10];
        rtc_format_date(&t, date_buf);
        rtc_format_time(&t, time_buf);

        char buf[16];
        screen_set_color(VGA_CYAN, VGA_BLACK);
        screen_print("  SwanOS v2.0 — System Status\n");
        screen_print("  ─────────────────────────────\n");
        screen_set_color(VGA_WHITE, VGA_BLACK);
        screen_print("  User   : "); screen_print(user_current()); screen_print("\n");
        screen_print("  Arch   : x86 (i686)\n");
        screen_print("  Uptime : ");
        itoa(hrs, buf, 10); screen_print(buf); screen_print("h ");
        itoa(mins, buf, 10); screen_print(buf); screen_print("m ");
        itoa(secs, buf, 10); screen_print(buf); screen_print("s\n");
        screen_print("  Date   : ");
        screen_print(date_buf); screen_print("  "); screen_print(time_buf); screen_print("\n");
        screen_print("  Memory : ");
        itoa(mem_used() / 1024, buf, 10); screen_print(buf); screen_print(" / ");
        itoa(mem_total() / 1024, buf, 10); screen_print(buf); screen_print(" KB\n");
        screen_print("  LLM    : Groq (via serial bridge)\n");
        screen_set_color(VGA_GREEN, VGA_BLACK);
        screen_print("  Status : ONLINE\n");
        screen_set_color(VGA_WHITE, VGA_BLACK);
        return 0;
    }

    /* ── Time ── */
    if (strcmp(cmd, "time") == 0) {
        uint32_t secs = timer_get_seconds();
        uint32_t mins = secs / 60;
        uint32_t hrs = mins / 60;
        secs %= 60; mins %= 60;
        char buf[16];

        screen_print("  Uptime: ");
        itoa(hrs, buf, 10); screen_print(buf); screen_print("h ");
        itoa(mins, buf, 10); screen_print(buf); screen_print("m ");
        itoa(secs, buf, 10); screen_print(buf); screen_print("s\n");
        return 0;
    }

    /* ── History ── */
    if (strcmp(cmd, "history") == 0) {
        screen_set_color(VGA_CYAN, VGA_BLACK);
        screen_print("  Command History\n");
        screen_set_color(VGA_DARK_GREY, VGA_BLACK);
        screen_print("  ─────────────────────────────\n");
        int start = hist_count > HIST_SIZE ? hist_count - HIST_SIZE : 0;
        for (int i = start; i < hist_count; i++) {
            char nb[8];
            itoa(i + 1, nb, 10);
            screen_set_color(VGA_DARK_GREY, VGA_BLACK);
            screen_print("  ");
            /* Pad number */
            if (i + 1 < 10) screen_putchar(' ');
            screen_print(nb);
            screen_print("  ");
            screen_set_color(VGA_WHITE, VGA_BLACK);
            screen_print(history[i % HIST_SIZE]);
            screen_print("\n");
        }
        screen_set_color(VGA_WHITE, VGA_BLACK);
        return 0;
    }

    /* ── Login ── */
    if (strcmp(cmd, "login") == 0) {
        return -3; /* signal re-login */
    }

    /* ── Unknown ── */
    screen_set_color(VGA_RED, VGA_BLACK);
    screen_print("  Unknown command: ");
    screen_print(cmd);
    screen_print("\n  Type 'help' for available commands.\n");
    screen_set_color(VGA_WHITE, VGA_BLACK);
    return 0;
}

/* ── Main shell loop ───────────────────────────────────────── */
void shell_run(void) {
    screen_set_color(VGA_CYAN, VGA_BLACK);
    screen_print("  Type ");
    screen_set_color(VGA_WHITE, VGA_BLACK);
    screen_print("help");
    screen_set_color(VGA_CYAN, VGA_BLACK);
    screen_print(" for commands, ");
    screen_set_color(VGA_WHITE, VGA_BLACK);
    screen_print("ask <question>");
    screen_set_color(VGA_CYAN, VGA_BLACK);
    screen_print(" to talk to AI.\n");
    screen_set_color(VGA_DARK_GREY, VGA_BLACK);
    screen_print("  Use UP/DOWN arrows for command history.\n\n");
    screen_set_color(VGA_WHITE, VGA_BLACK);

    while (1) {
        print_prompt();
        read_line_with_history(cmd_buf, CMD_BUF);

        /* Add to history before executing */
        char *trimmed = trim(cmd_buf);
        if (trimmed[0] != '\0') {
            hist_add(trimmed);
        }

        int result = execute_command(cmd_buf);

        if (result == -1) {
            /* Shutdown */
            __asm__ volatile ("cli; hlt");
            while (1);
        }
        if (result == -2) {
            /* Reboot via triple fault */
            uint8_t good = 0x02;
            while (good & 0x02) good = inb(0x64);
            outb(0x64, 0xFE); /* reset CPU */
            __asm__ volatile ("cli; hlt");
        }
        if (result == -3) {
            /* Re-login */
            return;
        }
    }
}
