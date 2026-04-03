/* ============================================================
 * SwanOS — Command Shell
 * Visually rich CLI with styled prompt, banners, and
 * box-drawn output formatting.
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
#include "process.h"
#include "audit.h"

#define CMD_BUF 256
#define OUT_BUF 4096

static char cmd_buf[CMD_BUF];
static char out_buf[OUT_BUF];
static char cwd[128] = "/";

/* ── Path Resolution ───────────────────────────────────────── */
static void resolve_path(const char *input, char *output) {
    if (input[0] == '/') {
        strncpy(output, input, 127);
        output[127] = '\0';
        return;
    }
    
    char temp[256];
    strcpy(temp, cwd);
    
    if (strcmp(input, "..") == 0) {
        if (strcmp(temp, "/") != 0) {
            int len = strlen(temp);
            while (len > 0 && temp[len - 1] != '/') {
                len--;
            }
            if (len > 1) {
                temp[len - 1] = '\0';
            } else {
                temp[1] = '\0';
            }
        }
    } else if (strcmp(input, ".") != 0 && input[0] != '\0') {
        if (strcmp(temp, "/") != 0) {
            strcat(temp, "/");
        }
        strcat(temp, input);
    }
    
    strncpy(output, temp, 127);
    output[127] = '\0';
}

/* ── Command history ───────────────────────────────────────── */
#define HIST_SIZE 16
static char history[HIST_SIZE][CMD_BUF];
static int  hist_count = 0;
static int  hist_pos   = 0;

static void hist_add(const char *cmd) {
    if (cmd[0] == '\0') return;
    if (hist_count > 0 && strcmp(history[(hist_count - 1) % HIST_SIZE], cmd) == 0)
        return;
    strcpy(history[hist_count % HIST_SIZE], cmd);
    hist_count++;
}

/* ── Simple calculator ─────────────────────────────────────── */
static int calc_eval(const char *expr) {
    int result = 0, num = 0, has_num = 0;
    char pending_op = '+';

    while (1) {
        if (isdigit(*expr)) {
            num = num * 10 + (*expr - '0');
            has_num = 1;
        } else {
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
        }
        expr++;
    }
    return result;
}

/* ── Hexdump ───────────────────────────────────────────────── */
static void cmd_hexdump(const char *filename) {
    char abs_path[128];
    resolve_path(filename, abs_path);

    int r = fs_read(abs_path, out_buf, OUT_BUF);
    if (r < 0) {
        screen_set_color(VGA_RED, VGA_BLACK);
        screen_print("  ");
        screen_putchar((char)254);
        screen_print(" File not found: ");
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
    for (int i = 0; i < 60; i++) screen_putchar((char)196);
    screen_print("\n");
    screen_set_color(VGA_WHITE, VGA_BLACK);

    for (int off = 0; off < len; off += 16) {
        screen_set_color(VGA_DARK_GREY, VGA_BLACK);
        screen_print("  ");
        itoa(off, hex, 16);
        int hl = strlen(hex);
        for (int p = 0; p < 8 - hl; p++) screen_putchar('0');
        screen_print(hex);
        screen_print("  ");

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

/* ── Styled prompt ─────────────────────────────────────────── */
static void print_prompt(void) {
    screen_set_color(VGA_DARK_GREY, VGA_BLACK);
    screen_putchar((char)204);  /* ╠ */
    screen_putchar((char)205);  /* ═ */
    screen_putchar((char)205);  /* ═ */
    screen_set_color(VGA_CYAN, VGA_BLACK);
    screen_putchar((char)254);  /* ■ */
    screen_putchar(' ');
    screen_set_color(VGA_GREEN, VGA_BLACK);
    screen_print(user_current());
    screen_set_color(VGA_DARK_GREY, VGA_BLACK);
    screen_print("@");
    screen_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    screen_print(cwd);
    screen_set_color(VGA_DARK_GREY, VGA_BLACK);
    screen_print(" ");
    screen_putchar((char)16);   /* ► */
    screen_set_color(VGA_WHITE, VGA_BLACK);
    screen_print(" ");
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
            if (hist_pos > 0 && hist_pos > hist_count - HIST_SIZE) {
                hist_pos--;
                while (pos > 0) { screen_backspace(); pos--; }
                strcpy(buf, history[hist_pos % HIST_SIZE]);
                pos = strlen(buf);
                screen_print(buf);
            }
        }
        else if ((uint8_t)c == KEY_DOWN) {
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
        else if (c >= ' ') {
            buf[pos++] = c;
            buf[pos] = '\0';
            screen_putchar(c);
        }
    }

    buf[pos] = '\0';
    screen_putchar('\n');
    return pos;
}

/* ── Help command with styled categories ───────────────────── */
static void print_help_entry(const char *cmd, const char *desc) {
    screen_set_color(VGA_DARK_GREY, VGA_BLACK);
    screen_print("    ");
    screen_putchar((char)250);  /* · */
    screen_print(" ");
    screen_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    screen_print(cmd);
    /* Pad to fixed width */
    int clen = strlen(cmd);
    for (int i = clen; i < 16; i++) screen_putchar(' ');
    screen_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    screen_print(desc);
    screen_print("\n");
}

static void print_help_section(const char *title, uint8_t color) {
    screen_print("\n");
    screen_set_color(VGA_DARK_GREY, VGA_BLACK);
    screen_print("   ");
    screen_putchar((char)219);  /* █ */
    screen_set_color(color, VGA_BLACK);
    screen_print(" ");
    screen_print(title);
    screen_print("\n");
    screen_set_color(VGA_DARK_GREY, VGA_BLACK);
    screen_print("   ");
    for (int i = 0; i < 50; i++) screen_putchar((char)196);
    screen_print("\n");
}

static void cmd_help(void) {
    /* Header */
    screen_set_color(VGA_DARK_GREY, VGA_BLACK);
    screen_print("\n   ");
    screen_putchar((char)201);
    for (int i = 0; i < 48; i++) screen_putchar((char)205);
    screen_putchar((char)187);
    screen_print("\n   ");
    screen_putchar((char)186);
    screen_set_color(VGA_CYAN, VGA_BLACK);
    screen_print("  ");
    screen_putchar((char)6);
    screen_print(" SwanOS v3.0");
    screen_set_color(VGA_DARK_GREY, VGA_BLACK);
    screen_print("  ");
    screen_putchar((char)250);
    screen_print("  ");
    screen_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    screen_print("Command Reference       ");
    screen_set_color(VGA_DARK_GREY, VGA_BLACK);
    screen_putchar((char)186);
    screen_print("\n   ");
    screen_putchar((char)200);
    for (int i = 0; i < 48; i++) screen_putchar((char)205);
    screen_putchar((char)188);
    screen_print("\n");

    print_help_section("AI", VGA_LIGHT_CYAN);
    print_help_entry("ask <question>", "Ask the AI assistant");
    print_help_entry("setkey <key>", "Set Groq API key");
    print_help_entry("aikey", "Check API key status");

    print_help_section("FILES", VGA_YELLOW);
    print_help_entry("cd <dir>", "Change directory");
    print_help_entry("ls [path]", "List files");
    print_help_entry("cat <file>", "Read file");
    print_help_entry("write <f> <txt>", "Write file");
    print_help_entry("append <f> <txt>", "Append to file");
    print_help_entry("cp <src> <dst>", "Copy file");
    print_help_entry("mv <file> <name>", "Rename file");
    print_help_entry("mkdir <name>", "Create directory");
    print_help_entry("rm <file>", "Delete file/dir");
    print_help_entry("hexdump <file>", "Hex viewer");
    print_help_entry("exec <file>", "Run dynamic app");
    print_help_entry("mkapp <file>", "Create test app");

    print_help_section("UTILITIES", VGA_GREEN);
    print_help_entry("calc <expr>", "Calculator");
    print_help_entry("echo <text>", "Print text");
    print_help_entry("clear", "Clear screen");
    print_help_entry("snake", "Snake game!");

    print_help_section("SYSTEM", VGA_LIGHT_MAGENTA);
    print_help_entry("help", "This reference");
    print_help_entry("whoami", "Current user");
    print_help_entry("profile", "User profile & stats");
    print_help_entry("audit", "View audit log");
    print_help_entry("status", "System info");
    print_help_entry("date", "Date & time");
    print_help_entry("mem", "Memory usage");
    print_help_entry("time", "Uptime");
    print_help_entry("history", "Command history");
    print_help_entry("gui", "Switch to GUI mode");
    print_help_entry("login", "Switch user");
    print_help_entry("reboot", "Reboot");
    print_help_entry("shutdown", "Power off");

    screen_print("\n");
    screen_set_color(VGA_DARK_GREY, VGA_BLACK);
    screen_print("   Tip: ");
    screen_putchar((char)24);  /* ↑ */
    screen_putchar((char)25);  /* ↓ */
    screen_print(" arrows for command history\n\n");
    screen_set_color(VGA_WHITE, VGA_BLACK);
}

static void ring3_crash_dummy(void) {
    /* This process is spawned in Ring 3 User Space. 
       Executing CLI without IOPL=3 will cause a General Protection Fault. */
    __asm__ volatile ("cli");
    while(1);
}

/* ── Execute command ───────────────────────────────────────── */
static int execute_command(char *input) {
    char *cmd = trim(input);
    if (cmd[0] == '\0') return 0;

    char *arg = cmd;
    while (*arg && !isspace(*arg)) arg++;
    if (*arg) { *arg = '\0'; arg++; }
    arg = trim(arg);

    /* ── Power ── */
    if (strcmp(cmd, "shutdown") == 0 || strcmp(cmd, "exit") == 0) {
        screen_print("\n");
        screen_set_color(VGA_DARK_GREY, VGA_BLACK);
        screen_print("   ");
        for (int i = 0; i < 40; i++) screen_putchar((char)196);
        screen_print("\n");
        screen_set_color(VGA_YELLOW, VGA_BLACK);
        screen_print("   ");
        screen_putchar((char)254);
        screen_print(" Shutting down SwanOS...\n");
        screen_set_color(VGA_DARK_GREY, VGA_BLACK);
        screen_print("   Goodbye.\n\n");
        screen_set_color(VGA_WHITE, VGA_BLACK);
        return -1;
    }

    if (strcmp(cmd, "reboot") == 0) {
        screen_set_color(VGA_CYAN, VGA_BLACK);
        screen_print("\n   ");
        screen_putchar((char)254);
        screen_print(" Rebooting...\n");
        screen_set_color(VGA_WHITE, VGA_BLACK);
        return -2;
    }

    /* ── Help ── */
    if (strcmp(cmd, "help") == 0) { cmd_help(); return 0; }

    /* ── Clear ── */
    if (strcmp(cmd, "clear") == 0) {
        screen_clear();
        /* Show subtle brand watermark after clear */
        screen_set_color(VGA_DARK_GREY, VGA_BLACK);
        screen_print("   ");
        screen_putchar((char)6);
        screen_print(" SwanOS v3.0\n\n");
        screen_set_color(VGA_WHITE, VGA_BLACK);
        return 0;
    }

    /* ── GUI ── */
    if (strcmp(cmd, "gui") == 0) {
        return -4; /* switch to GUI */
    }

    /* ── Ask AI ── */
    if (strcmp(cmd, "ask") == 0) {
        if (arg[0] == '\0') {
            screen_set_color(VGA_RED, VGA_BLACK);
            screen_print("   ");
            screen_putchar((char)254);
            screen_print(" Usage: ask <question>\n");
            screen_set_color(VGA_WHITE, VGA_BLACK);
            return 0;
        }
        /* Show thinking indicator */
        screen_set_color(VGA_DARK_GREY, VGA_BLACK);
        screen_print("   ");
        screen_putchar((char)250);
        screen_print(" Thinking...\n");
        screen_set_color(VGA_WHITE, VGA_BLACK);

        char response[2048];
        llm_query(arg, response, sizeof(response));

        screen_set_color(VGA_CYAN, VGA_BLACK);
        screen_print("\n   ");
        screen_putchar((char)6);
        screen_print(" AI ");
        screen_set_color(VGA_DARK_GREY, VGA_BLACK);
        screen_putchar((char)16);
        screen_print(" ");
        screen_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        screen_print(response);
        screen_print("\n\n");
        screen_set_color(VGA_WHITE, VGA_BLACK);
        return 0;
    }

    /* ── Set API Key ── */
    if (strcmp(cmd, "setkey") == 0) {
        if (arg[0] == '\0') {
            screen_set_color(VGA_RED, VGA_BLACK);
            screen_print("   ");
            screen_putchar((char)254);
            screen_print(" Usage: setkey <GROQ_API_KEY>\n");
            screen_set_color(VGA_WHITE, VGA_BLACK);
            return 0;
        }
        llm_set_api_key(arg);
        screen_set_color(VGA_GREEN, VGA_BLACK);
        screen_print("   ");
        screen_putchar((char)254);
        screen_print(" API key saved.\n");
        screen_set_color(VGA_WHITE, VGA_BLACK);
        return 0;
    }

    /* ── Check API Key ── */
    if (strcmp(cmd, "aikey") == 0) {
        screen_set_color(VGA_DARK_GREY, VGA_BLACK);
        screen_print("   ");
        screen_putchar((char)250);
        screen_print(" ");
        if (llm_ready()) {
            screen_set_color(VGA_GREEN, VGA_BLACK);
            screen_putchar((char)254);
            screen_print(" API key is configured.\n");
        } else {
            screen_set_color(VGA_YELLOW, VGA_BLACK);
            screen_putchar((char)254);
            screen_print(" No API key set. Use 'setkey <KEY>'.\n");
        }
        screen_set_color(VGA_WHITE, VGA_BLACK);
        return 0;
    }

    /* ── Files ── */
    if (strcmp(cmd, "cd") == 0) {
        if (arg[0] == '\0') {
            /* cd home if no arg */
            strcpy(cwd, "/home/");
            strcat(cwd, user_current());
            return 0;
        }
        char abs_path[128];
        resolve_path(arg, abs_path);
        if (fs_exists(abs_path)) {
            /* Simple check: if fs_list succeeds, it's a dir */
            int r = fs_list(abs_path, out_buf, 10);
            if (r >= 0) {
                strcpy(cwd, abs_path);
            } else {
                screen_set_color(VGA_RED, VGA_BLACK);
                screen_print("   ");
                screen_putchar((char)254);
                screen_print(" Not a directory.\n");
                screen_set_color(VGA_WHITE, VGA_BLACK);
            }
        } else {
            screen_set_color(VGA_RED, VGA_BLACK);
            screen_print("   ");
            screen_putchar((char)254);
            screen_print(" Directory not found.\n");
            screen_set_color(VGA_WHITE, VGA_BLACK);
        }
        return 0;
    }

    if (strcmp(cmd, "ls") == 0) {
        char abs_path[128];
        resolve_path(arg[0] ? arg : ".", abs_path);
        fs_list(abs_path, out_buf, OUT_BUF);
        screen_print(out_buf);
        return 0;
    }

    if (strcmp(cmd, "cat") == 0) {
        if (arg[0] == '\0') {
            screen_set_color(VGA_RED, VGA_BLACK);
            screen_print("   ");
            screen_putchar((char)254);
            screen_print(" Usage: cat <filename>\n");
            screen_set_color(VGA_WHITE, VGA_BLACK);
            return 0;
        }
        char abs_path[128];
        resolve_path(arg, abs_path);
        int r = fs_read(abs_path, out_buf, OUT_BUF);
        if (r < 0) screen_set_color(VGA_RED, VGA_BLACK);
        screen_print("  ");
        screen_print(out_buf);
        screen_print("\n");
        screen_set_color(VGA_WHITE, VGA_BLACK);
        return 0;
    }

    if (strcmp(cmd, "write") == 0) {
        if (arg[0] == '\0') {
            screen_set_color(VGA_RED, VGA_BLACK);
            screen_print("   ");
            screen_putchar((char)254);
            screen_print(" Usage: write <filename> <content>\n");
            screen_set_color(VGA_WHITE, VGA_BLACK);
            return 0;
        }
        char *content = arg;
        while (*content && !isspace(*content)) content++;
        if (*content) { *content = '\0'; content++; }
        content = trim(content);

        if (content[0] == '\0') {
            screen_set_color(VGA_RED, VGA_BLACK);
            screen_print("   ");
            screen_putchar((char)254);
            screen_print(" Usage: write <filename> <content>\n");
            screen_set_color(VGA_WHITE, VGA_BLACK);
            return 0;
        }

        char abs_path[128];
        resolve_path(arg, abs_path);

        if (fs_write(abs_path, content) == 0) {
            screen_set_color(VGA_GREEN, VGA_BLACK);
            screen_print("   ");
            screen_putchar((char)254);
            screen_print(" Written: ");
            screen_print(arg);
            screen_print("\n");
            audit_log(AUDIT_FILE_WRITE, arg);
        } else {
            screen_set_color(VGA_RED, VGA_BLACK);
            screen_print("   ");
            screen_putchar((char)254);
            screen_print(" Write failed.\n");
        }
        screen_set_color(VGA_WHITE, VGA_BLACK);
        return 0;
    }

    if (strcmp(cmd, "append") == 0) {
        if (arg[0] == '\0') {
            screen_set_color(VGA_RED, VGA_BLACK);
            screen_print("   ");
            screen_putchar((char)254);
            screen_print(" Usage: append <filename> <text>\n");
            screen_set_color(VGA_WHITE, VGA_BLACK);
            return 0;
        }
        char *content = arg;
        while (*content && !isspace(*content)) content++;
        if (*content) { *content = '\0'; content++; }
        content = trim(content);
        if (content[0] == '\0') {
            screen_set_color(VGA_RED, VGA_BLACK);
            screen_print("   ");
            screen_putchar((char)254);
            screen_print(" Usage: append <filename> <text>\n");
            screen_set_color(VGA_WHITE, VGA_BLACK);
            return 0;
        }
        char abs_path[128];
        resolve_path(arg, abs_path);
        
        if (fs_append(abs_path, content) == 0) {
            screen_set_color(VGA_GREEN, VGA_BLACK);
            screen_print("   ");
            screen_putchar((char)254);
            screen_print(" Appended to ");
            screen_print(arg);
            screen_print("\n");
        } else {
            screen_set_color(VGA_RED, VGA_BLACK);
            screen_print("   ");
            screen_putchar((char)254);
            screen_print(" Append failed.\n");
        }
        screen_set_color(VGA_WHITE, VGA_BLACK);
        return 0;
    }

    if (strcmp(cmd, "cp") == 0) {
        if (arg[0] == '\0') {
            screen_set_color(VGA_RED, VGA_BLACK);
            screen_print("   ");
            screen_putchar((char)254);
            screen_print(" Usage: cp <source> <destination>\n");
            screen_set_color(VGA_WHITE, VGA_BLACK);
            return 0;
        }
        char *dst = arg;
        while (*dst && !isspace(*dst)) dst++;
        if (*dst) { *dst = '\0'; dst++; }
        dst = trim(dst);
        if (dst[0] == '\0') {
            screen_set_color(VGA_RED, VGA_BLACK);
            screen_print("   ");
            screen_putchar((char)254);
            screen_print(" Usage: cp <source> <destination>\n");
            screen_set_color(VGA_WHITE, VGA_BLACK);
            return 0;
        }
        char abs_src[128], abs_dst[128];
        resolve_path(arg, abs_src);
        resolve_path(dst, abs_dst);

        if (fs_copy(abs_src, abs_dst) == 0) {
            screen_set_color(VGA_GREEN, VGA_BLACK);
            screen_print("   ");
            screen_putchar((char)254);
            screen_print(" Copied ");
            screen_print(arg);
            screen_print(" -> ");
            screen_print(dst);
            screen_print("\n");
        } else {
            screen_set_color(VGA_RED, VGA_BLACK);
            screen_print("   ");
            screen_putchar((char)254);
            screen_print(" Copy failed.\n");
        }
        screen_set_color(VGA_WHITE, VGA_BLACK);
        return 0;
    }

    if (strcmp(cmd, "mv") == 0) {
        if (arg[0] == '\0') {
            screen_set_color(VGA_RED, VGA_BLACK);
            screen_print("   ");
            screen_putchar((char)254);
            screen_print(" Usage: mv <file> <newname>\n");
            screen_set_color(VGA_WHITE, VGA_BLACK);
            return 0;
        }
        char *newname = arg;
        while (*newname && !isspace(*newname)) newname++;
        if (*newname) { *newname = '\0'; newname++; }
        newname = trim(newname);
        if (newname[0] == '\0') {
            screen_set_color(VGA_RED, VGA_BLACK);
            screen_print("   ");
            screen_putchar((char)254);
            screen_print(" Usage: mv <file> <newname>\n");
            screen_set_color(VGA_WHITE, VGA_BLACK);
            return 0;
        }
        
        char abs_path[128];
        resolve_path(arg, abs_path);
        
        /* Note: mv only renames inside current directory for now, 
           because fs.c rename doesn't support changing parents yet.
           We pass abs_path as source, and base newname as destination. */
        if (fs_rename(abs_path, newname) == 0) {
            screen_set_color(VGA_GREEN, VGA_BLACK);
            screen_print("   ");
            screen_putchar((char)254);
            screen_print(" Renamed to ");
            screen_print(newname);
            screen_print("\n");
        } else {
            screen_set_color(VGA_RED, VGA_BLACK);
            screen_print("   ");
            screen_putchar((char)254);
            screen_print(" Rename failed.\n");
        }
        screen_set_color(VGA_WHITE, VGA_BLACK);
        return 0;
    }

    if (strcmp(cmd, "mkdir") == 0) {
        if (arg[0] == '\0') {
            screen_set_color(VGA_RED, VGA_BLACK);
            screen_print("   ");
            screen_putchar((char)254);
            screen_print(" Usage: mkdir <dirname>\n");
            screen_set_color(VGA_WHITE, VGA_BLACK);
            return 0;
        }
        char abs_path[128];
        resolve_path(arg, abs_path);

        if (fs_mkdir(abs_path) == 0) {
            screen_set_color(VGA_GREEN, VGA_BLACK);
            screen_print("   ");
            screen_putchar((char)254);
            screen_print(" Created: ");
            screen_print(arg);
            screen_print("\n");
            audit_log(AUDIT_FILE_CREATE, arg);
        } else {
            screen_set_color(VGA_RED, VGA_BLACK);
            screen_print("   ");
            screen_putchar((char)254);
            screen_print(" Failed (exists?).\n");
        }
        screen_set_color(VGA_WHITE, VGA_BLACK);
        return 0;
    }

    if (strcmp(cmd, "rm") == 0) {
        if (arg[0] == '\0') {
            screen_set_color(VGA_RED, VGA_BLACK);
            screen_print("   ");
            screen_putchar((char)254);
            screen_print(" Usage: rm <filename>\n");
            screen_set_color(VGA_WHITE, VGA_BLACK);
            return 0;
        }
        char abs_path[128];
        resolve_path(arg, abs_path);

        int r = fs_delete(abs_path);
        if (r == 0) {
            screen_set_color(VGA_GREEN, VGA_BLACK);
            screen_print("   ");
            screen_putchar((char)254);
            screen_print(" Deleted: ");
            screen_print(arg);
            screen_print("\n");
            audit_log(AUDIT_FILE_DELETE, arg);
        } else if (r == -2) {
            screen_set_color(VGA_RED, VGA_BLACK);
            screen_print("   ");
            screen_putchar((char)254);
            screen_print(" Directory not empty.\n");
        } else {
            screen_set_color(VGA_RED, VGA_BLACK);
            screen_print("   ");
            screen_putchar((char)254);
            screen_print(" Not found.\n");
        }
        screen_set_color(VGA_WHITE, VGA_BLACK);
        return 0;
    }

    if (strcmp(cmd, "hexdump") == 0) {
        if (arg[0] == '\0') {
            screen_set_color(VGA_RED, VGA_BLACK);
            screen_print("   ");
            screen_putchar((char)254);
            screen_print(" Usage: hexdump <filename>\n");
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
            screen_print("   ");
            screen_putchar((char)254);
            screen_print(" Usage: calc <expression>\n");
            screen_set_color(VGA_WHITE, VGA_BLACK);
            return 0;
        }
        int result = calc_eval(arg);
        char num_buf[32];
        itoa(result, num_buf, 10);
        screen_set_color(VGA_GREEN, VGA_BLACK);
        screen_print("   = ");
        screen_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
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
        screen_set_color(VGA_DARK_GREY, VGA_BLACK);
        screen_print("   ");
        screen_putchar((char)218);
        for (int i = 0; i < 36; i++) screen_putchar((char)196);
        screen_putchar((char)191);
        screen_print("\n   ");
        screen_putchar((char)179);
        screen_set_color(VGA_CYAN, VGA_BLACK);
        screen_print(" User  : ");
        screen_set_color(VGA_GREEN, VGA_BLACK);
        screen_print(user_current());
        int ulen = strlen(user_current());
        for (int i = ulen; i < 25; i++) screen_putchar(' ');
        screen_set_color(VGA_DARK_GREY, VGA_BLACK);
        screen_putchar((char)179);
        screen_print("\n   ");
        screen_putchar((char)179);
        screen_set_color(VGA_CYAN, VGA_BLACK);
        screen_print(" OS    : ");
        screen_set_color(VGA_WHITE, VGA_BLACK);
        screen_print("SwanOS v3.0 (bare-metal)");
        screen_set_color(VGA_DARK_GREY, VGA_BLACK);
        screen_putchar((char)179);
        screen_print("\n   ");
        screen_putchar((char)179);
        screen_set_color(VGA_CYAN, VGA_BLACK);
        screen_print(" Arch  : ");
        screen_set_color(VGA_WHITE, VGA_BLACK);
        screen_print("x86 (i686)              ");
        screen_set_color(VGA_DARK_GREY, VGA_BLACK);
        screen_putchar((char)179);
        screen_print("\n   ");
        screen_putchar((char)192);
        for (int i = 0; i < 36; i++) screen_putchar((char)196);
        screen_putchar((char)217);
        screen_print("\n");
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

        screen_set_color(VGA_DARK_GREY, VGA_BLACK);
        screen_print("   ");
        screen_putchar((char)250);
        screen_print(" ");
        screen_set_color(VGA_CYAN, VGA_BLACK);
        screen_print(day_buf);
        screen_set_color(VGA_DARK_GREY, VGA_BLACK);
        screen_print("  ");
        screen_putchar((char)250);
        screen_print("  ");
        screen_set_color(VGA_WHITE, VGA_BLACK);
        screen_print(date_buf);
        screen_set_color(VGA_DARK_GREY, VGA_BLACK);
        screen_print("  ");
        screen_putchar((char)250);
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
        /* Header */
        screen_set_color(VGA_DARK_GREY, VGA_BLACK);
        screen_print("\n   ");
        screen_putchar((char)218);
        for (int i = 0; i < 40; i++) screen_putchar((char)196);
        screen_putchar((char)191);
        screen_print("\n   ");
        screen_putchar((char)179);
        screen_set_color(VGA_CYAN, VGA_BLACK);
        screen_print("  ");
        screen_putchar((char)254);
        screen_print(" Memory Usage");
        screen_set_color(VGA_DARK_GREY, VGA_BLACK);
        screen_print("                         ");
        screen_putchar((char)179);
        screen_print("\n   ");
        screen_putchar((char)195);
        for (int i = 0; i < 40; i++) screen_putchar((char)196);
        screen_putchar((char)180);

        /* Total */
        screen_print("\n   ");
        screen_putchar((char)179);
        screen_set_color(VGA_WHITE, VGA_BLACK);
        screen_print("  Total : ");
        itoa(mem_total() / 1024, buf, 10);
        screen_print(buf);
        screen_print(" KB");
        int pad = 28 - strlen(buf) - 3;
        for (int i = 0; i < pad; i++) screen_putchar(' ');
        screen_set_color(VGA_DARK_GREY, VGA_BLACK);
        screen_putchar((char)179);

        /* Used */
        screen_print("\n   ");
        screen_putchar((char)179);
        screen_set_color(VGA_WHITE, VGA_BLACK);
        screen_print("  Used  : ");
        itoa(mem_used() / 1024, buf, 10);
        screen_print(buf);
        screen_print(" KB");
        pad = 28 - strlen(buf) - 3;
        for (int i = 0; i < pad; i++) screen_putchar(' ');
        screen_set_color(VGA_DARK_GREY, VGA_BLACK);
        screen_putchar((char)179);

        /* Free */
        screen_print("\n   ");
        screen_putchar((char)179);
        screen_set_color(VGA_GREEN, VGA_BLACK);
        screen_print("  Free  : ");
        itoa(mem_free() / 1024, buf, 10);
        screen_print(buf);
        screen_print(" KB");
        pad = 28 - strlen(buf) - 3;
        for (int i = 0; i < pad; i++) screen_putchar(' ');
        screen_set_color(VGA_DARK_GREY, VGA_BLACK);
        screen_putchar((char)179);

        /* Bar graph */
        int pct = (int)((mem_used() * 100) / mem_total());
        screen_print("\n   ");
        screen_putchar((char)179);
        screen_set_color(VGA_WHITE, VGA_BLACK);
        screen_print("  [");
        int bars = pct / 5;
        screen_set_color(pct > 80 ? VGA_RED : pct > 50 ? VGA_YELLOW : VGA_GREEN, VGA_BLACK);
        for (int i = 0; i < 20; i++) {
            screen_putchar(i < bars ? (char)219 : (char)176);
        }
        screen_set_color(VGA_WHITE, VGA_BLACK);
        screen_print("] ");
        itoa(pct, buf, 10);
        screen_print(buf);
        screen_print("%");
        pad = 13 - strlen(buf);
        for (int i = 0; i < pad; i++) screen_putchar(' ');
        screen_set_color(VGA_DARK_GREY, VGA_BLACK);
        screen_putchar((char)179);

        /* Bottom border */
        screen_print("\n   ");
        screen_putchar((char)192);
        for (int i = 0; i < 40; i++) screen_putchar((char)196);
        screen_putchar((char)217);
        screen_print("\n");
        screen_set_color(VGA_WHITE, VGA_BLACK);
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

        /* Boxed status display */
        screen_set_color(VGA_DARK_GREY, VGA_BLACK);
        screen_print("\n   ");
        screen_putchar((char)201);
        for (int i = 0; i < 44; i++) screen_putchar((char)205);
        screen_putchar((char)187);

        screen_print("\n   ");
        screen_putchar((char)186);
        screen_set_color(VGA_CYAN, VGA_BLACK);
        screen_print("  ");
        screen_putchar((char)6);
        screen_print(" SwanOS v3.0 ");
        screen_set_color(VGA_DARK_GREY, VGA_BLACK);
        screen_putchar((char)250);
        screen_print("  ");
        screen_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        screen_print("System Status           ");
        screen_set_color(VGA_DARK_GREY, VGA_BLACK);
        screen_putchar((char)186);

        screen_print("\n   ");
        screen_putchar((char)204);
        for (int i = 0; i < 44; i++) screen_putchar((char)205);
        screen_putchar((char)185);

        /* User */
        screen_print("\n   ");
        screen_putchar((char)186);
        screen_set_color(VGA_WHITE, VGA_BLACK);
        screen_print("  User   : ");
        screen_set_color(VGA_GREEN, VGA_BLACK);
        screen_print(user_current());
        int ulen = strlen(user_current());
        for (int i = ulen; i < 31; i++) screen_putchar(' ');
        screen_set_color(VGA_DARK_GREY, VGA_BLACK);
        screen_putchar((char)186);

        /* Arch */
        screen_print("\n   ");
        screen_putchar((char)186);
        screen_set_color(VGA_WHITE, VGA_BLACK);
        screen_print("  Arch   : x86 (i686)                  ");
        screen_set_color(VGA_DARK_GREY, VGA_BLACK);
        screen_putchar((char)186);

        /* Uptime */
        screen_print("\n   ");
        screen_putchar((char)186);
        screen_set_color(VGA_WHITE, VGA_BLACK);
        screen_print("  Uptime : ");
        itoa(hrs, buf, 10); screen_print(buf); screen_print("h ");
        itoa(mins, buf, 10); screen_print(buf); screen_print("m ");
        itoa(secs, buf, 10); screen_print(buf); screen_print("s");
        /* rough padding */
        screen_print("                       ");
        screen_set_color(VGA_DARK_GREY, VGA_BLACK);
        screen_putchar((char)186);

        /* Date */
        screen_print("\n   ");
        screen_putchar((char)186);
        screen_set_color(VGA_WHITE, VGA_BLACK);
        screen_print("  Date   : ");
        screen_print(date_buf); screen_print("  "); screen_print(time_buf);
        screen_print("              ");
        screen_set_color(VGA_DARK_GREY, VGA_BLACK);
        screen_putchar((char)186);

        /* Memory */
        screen_print("\n   ");
        screen_putchar((char)186);
        screen_set_color(VGA_WHITE, VGA_BLACK);
        screen_print("  Memory : ");
        itoa(mem_used() / 1024, buf, 10); screen_print(buf); screen_print(" / ");
        itoa(mem_total() / 1024, buf, 10); screen_print(buf); screen_print(" KB");
        screen_print("                  ");
        screen_set_color(VGA_DARK_GREY, VGA_BLACK);
        screen_putchar((char)186);

        /* LLM */
        screen_print("\n   ");
        screen_putchar((char)186);
        screen_set_color(VGA_WHITE, VGA_BLACK);
        screen_print("  LLM    : Groq (via serial bridge)    ");
        screen_set_color(VGA_DARK_GREY, VGA_BLACK);
        screen_putchar((char)186);

        /* Status */
        screen_print("\n   ");
        screen_putchar((char)186);
        screen_set_color(VGA_WHITE, VGA_BLACK);
        screen_print("  Status : ");
        screen_set_color(VGA_GREEN, VGA_BLACK);
        screen_putchar((char)254);
        screen_print(" ONLINE                       ");
        screen_set_color(VGA_DARK_GREY, VGA_BLACK);
        screen_putchar((char)186);

        /* Bottom */
        screen_print("\n   ");
        screen_putchar((char)200);
        for (int i = 0; i < 44; i++) screen_putchar((char)205);
        screen_putchar((char)188);
        screen_print("\n");
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

        screen_set_color(VGA_DARK_GREY, VGA_BLACK);
        screen_print("   ");
        screen_putchar((char)250);
        screen_print(" ");
        screen_set_color(VGA_WHITE, VGA_BLACK);
        screen_print("Uptime: ");
        itoa(hrs, buf, 10); screen_print(buf); screen_print("h ");
        itoa(mins, buf, 10); screen_print(buf); screen_print("m ");
        itoa(secs, buf, 10); screen_print(buf); screen_print("s\n");
        return 0;
    }

    /* ── History ── */
    if (strcmp(cmd, "history") == 0) {
        screen_set_color(VGA_CYAN, VGA_BLACK);
        screen_print("\n   ");
        screen_putchar((char)254);
        screen_print(" Command History\n");
        screen_set_color(VGA_DARK_GREY, VGA_BLACK);
        screen_print("   ");
        for (int i = 0; i < 40; i++) screen_putchar((char)196);
        screen_print("\n");

        int start = hist_count > HIST_SIZE ? hist_count - HIST_SIZE : 0;
        for (int i = start; i < hist_count; i++) {
            char nb[8];
            itoa(i + 1, nb, 10);
            screen_set_color(VGA_DARK_GREY, VGA_BLACK);
            screen_print("   ");
            if (i + 1 < 10) screen_putchar(' ');
            screen_print(nb);
            screen_print("  ");
            screen_putchar((char)250);
            screen_print(" ");
            screen_set_color(VGA_WHITE, VGA_BLACK);
            screen_print(history[i % HIST_SIZE]);
            screen_print("\n");
        }
        screen_print("\n");
        screen_set_color(VGA_WHITE, VGA_BLACK);
        return 0;
    }

    /* ── Audit ── */
    if (strcmp(cmd, "audit") == 0) {
        screen_set_color(VGA_DARK_GREY, VGA_BLACK);
        screen_print("\n   ");
        screen_putchar((char)201);
        for (int i = 0; i < 54; i++) screen_putchar((char)205);
        screen_putchar((char)187);
        screen_print("\n   ");
        screen_putchar((char)186);
        screen_set_color(VGA_CYAN, VGA_BLACK);
        screen_print("  ");
        screen_putchar((char)254);
        screen_print(" Audit Log");
        screen_set_color(VGA_DARK_GREY, VGA_BLACK);
        screen_print("                                         ");
        screen_putchar((char)186);
        screen_print("\n   ");
        screen_putchar((char)200);
        for (int i = 0; i < 54; i++) screen_putchar((char)205);
        screen_putchar((char)188);
        screen_print("\n");

        int cnt = audit_get_count();
        if (cnt == 0) {
            screen_set_color(VGA_DARK_GREY, VGA_BLACK);
            screen_print("   No audit events recorded.\n");
        } else {
            int show = cnt > 10 ? 10 : cnt;
            int total_avail = cnt > AUDIT_MAX_ENTRIES ? AUDIT_MAX_ENTRIES : cnt;
            int start_idx = total_avail - show;
            for (int i = start_idx; i < total_avail; i++) {
                const audit_entry_t *e = audit_get_entry(i);
                if (!e || !e->used) continue;
                screen_set_color(VGA_DARK_GREY, VGA_BLACK);
                screen_print("   ");
                char tmp[8];
                screen_print("[");
                if (e->hour < 10) screen_putchar('0');
                itoa(e->hour, tmp, 10); screen_print(tmp);
                screen_putchar(':');
                if (e->minute < 10) screen_putchar('0');
                itoa(e->minute, tmp, 10); screen_print(tmp);
                screen_putchar(':');
                if (e->second < 10) screen_putchar('0');
                itoa(e->second, tmp, 10); screen_print(tmp);
                screen_print("] ");

                /* Color by event type */
                uint8_t ec = VGA_WHITE;
                switch (e->type) {
                    case AUDIT_LOGIN:       ec = VGA_GREEN; break;
                    case AUDIT_LOGOUT:      ec = VGA_YELLOW; break;
                    case AUDIT_FILE_CREATE: ec = VGA_LIGHT_CYAN; break;
                    case AUDIT_FILE_DELETE: ec = VGA_RED; break;
                    case AUDIT_APP_OPEN:    ec = VGA_CYAN; break;
                    case AUDIT_APP_CLOSE:   ec = VGA_DARK_GREY; break;
                    case AUDIT_COMMAND:     ec = VGA_WHITE; break;
                    case AUDIT_FILE_WRITE:  ec = VGA_LIGHT_GREEN; break;
                    case AUDIT_SYSTEM:      ec = VGA_LIGHT_MAGENTA; break;
                }
                screen_set_color(ec, VGA_BLACK);
                screen_print(audit_type_name(e->type));
                screen_set_color(VGA_DARK_GREY, VGA_BLACK);
                screen_print(" ");
                screen_set_color(VGA_GREEN, VGA_BLACK);
                screen_print(e->user);
                if (e->detail[0]) {
                    screen_set_color(VGA_DARK_GREY, VGA_BLACK);
                    screen_print(": ");
                    screen_set_color(VGA_LIGHT_GREY, VGA_BLACK);
                    screen_print(e->detail);
                }
                screen_print("\n");
            }
            screen_set_color(VGA_DARK_GREY, VGA_BLACK);
            screen_print("\n   Total events: ");
            char tcnt[8]; itoa(cnt, tcnt, 10);
            screen_print(tcnt);
            screen_print("\n");
        }
        screen_set_color(VGA_WHITE, VGA_BLACK);
        screen_print("\n");
        return 0;
    }

    /* ── Profile ── */
    if (strcmp(cmd, "profile") == 0) {
        user_profile_t *p = user_get_profile();
        screen_set_color(VGA_DARK_GREY, VGA_BLACK);
        screen_print("\n   ");
        screen_putchar((char)201);
        for (int i = 0; i < 44; i++) screen_putchar((char)205);
        screen_putchar((char)187);
        screen_print("\n   ");
        screen_putchar((char)186);
        screen_set_color(VGA_CYAN, VGA_BLACK);
        screen_print("  ");
        screen_putchar((char)4);
        screen_print(" User Profile");
        screen_set_color(VGA_DARK_GREY, VGA_BLACK);
        screen_print("                             ");
        screen_putchar((char)186);
        screen_print("\n   ");
        screen_putchar((char)204);
        for (int i = 0; i < 44; i++) screen_putchar((char)205);
        screen_putchar((char)185);

        /* Username */
        screen_print("\n   ");
        screen_putchar((char)186);
        screen_set_color(VGA_WHITE, VGA_BLACK);
        screen_print("  Name     : ");
        screen_set_color(VGA_GREEN, VGA_BLACK);
        screen_print(user_current());
        int upad = 29 - (int)strlen(user_current());
        for (int i = 0; i < upad; i++) screen_putchar(' ');
        screen_set_color(VGA_DARK_GREY, VGA_BLACK);
        screen_putchar((char)186);

        /* Login count */
        screen_print("\n   ");
        screen_putchar((char)186);
        screen_set_color(VGA_WHITE, VGA_BLACK);
        screen_print("  Logins   : ");
        screen_set_color(VGA_YELLOW, VGA_BLACK);
        char lc[8]; itoa(p->login_count, lc, 10);
        screen_print(lc);
        int lpad = 29 - (int)strlen(lc);
        for (int i = 0; i < lpad; i++) screen_putchar(' ');
        screen_set_color(VGA_DARK_GREY, VGA_BLACK);
        screen_putchar((char)186);

        /* Last login */
        screen_print("\n   ");
        screen_putchar((char)186);
        screen_set_color(VGA_WHITE, VGA_BLACK);
        screen_print("  Last     : ");
        if (p->last_day > 0) {
            char lt[24]; char tmp[8];
            lt[0] = '\0';
            itoa(p->last_day, tmp, 10); strcat(lt, tmp); strcat(lt, "/");
            itoa(p->last_month, tmp, 10); strcat(lt, tmp); strcat(lt, " ");
            if (p->last_hour < 10) strcat(lt, "0");
            itoa(p->last_hour, tmp, 10); strcat(lt, tmp); strcat(lt, ":");
            if (p->last_minute < 10) strcat(lt, "0");
            itoa(p->last_minute, tmp, 10); strcat(lt, tmp);
            screen_print(lt);
            int lpad2 = 29 - (int)strlen(lt);
            for (int i = 0; i < lpad2; i++) screen_putchar(' ');
        } else {
            screen_set_color(VGA_DARK_GREY, VGA_BLACK);
            screen_print("First login                  ");
        }
        screen_set_color(VGA_DARK_GREY, VGA_BLACK);
        screen_putchar((char)186);

        /* Session duration */
        screen_print("\n   ");
        screen_putchar((char)186);
        screen_set_color(VGA_WHITE, VGA_BLACK);
        screen_print("  Session  : ");
        {
            uint32_t ss = user_session_seconds();
            uint32_t sm = ss / 60; uint32_t sh = sm / 60;
            ss %= 60; sm %= 60;
            char sb[24]; char tmp[8];
            sb[0] = '\0';
            itoa(sh, tmp, 10); strcat(sb, tmp); strcat(sb, "h ");
            itoa(sm, tmp, 10); strcat(sb, tmp); strcat(sb, "m ");
            itoa(ss, tmp, 10); strcat(sb, tmp); strcat(sb, "s");
            screen_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
            screen_print(sb);
            int spad = 29 - (int)strlen(sb);
            for (int i = 0; i < spad; i++) screen_putchar(' ');
        }
        screen_set_color(VGA_DARK_GREY, VGA_BLACK);
        screen_putchar((char)186);

        /* Home directory */
        screen_print("\n   ");
        screen_putchar((char)186);
        screen_set_color(VGA_WHITE, VGA_BLACK);
        screen_print("  Home     : ");
        screen_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
        char hdir[32]; strcpy(hdir, "/home/"); strcat(hdir, user_current());
        screen_print(hdir);
        int hpad = 29 - (int)strlen(hdir);
        for (int i = 0; i < hpad; i++) screen_putchar(' ');
        screen_set_color(VGA_DARK_GREY, VGA_BLACK);
        screen_putchar((char)186);

        /* Bottom */
        screen_print("\n   ");
        screen_putchar((char)200);
        for (int i = 0; i < 44; i++) screen_putchar((char)205);
        screen_putchar((char)188);
        screen_print("\n\n");
        screen_set_color(VGA_WHITE, VGA_BLACK);
        return 0;
    }

    /* ── Login ── */
    if (strcmp(cmd, "login") == 0) {
        audit_log(AUDIT_LOGOUT, user_current());
        return -3;
    }

    /* ── Crash Test ── */
    if (strcmp(cmd, "crash_test") == 0) {
        screen_set_color(VGA_YELLOW, VGA_BLACK);
        screen_print("\n   ");
        screen_putchar((char)254);
        screen_print(" Spawning volatile Ring 3 User process...\n");
        screen_set_color(VGA_WHITE, VGA_BLACK);
        
        process_create(ring3_crash_dummy, 3);
        return 0;
    }

    /* ── Exec ── */
    if (strcmp(cmd, "exec") == 0) {
        if (arg[0] == '\0') {
            screen_set_color(VGA_RED, VGA_BLACK);
            screen_print("   ");
            screen_putchar((char)254);
            screen_print(" Usage: exec <filename>\n");
            screen_set_color(VGA_WHITE, VGA_BLACK);
            return 0;
        }
        char abs_path[128];
        resolve_path(arg, abs_path);
        
        int pid = process_exec(abs_path);
        if (pid < 0) {
            screen_set_color(VGA_RED, VGA_BLACK);
            screen_print("   ");
            screen_putchar((char)254);
            screen_print(" Failed to execute. Check file exists and memory.\n");
        } else {
            screen_set_color(VGA_GREEN, VGA_BLACK);
            screen_print("   ");
            screen_putchar((char)254);
            screen_print(" Executed process PID: ");
            char nbuf[16]; itoa(pid, nbuf, 10);
            screen_print(nbuf);
            screen_print("\n");
            audit_log(AUDIT_APP_OPEN, abs_path);
        }
        screen_set_color(VGA_WHITE, VGA_BLACK);
        return 0;
    }

    /* ── MkApp ── */
    if (strcmp(cmd, "mkapp") == 0) {
        if (arg[0] == '\0') {
            screen_set_color(VGA_RED, VGA_BLACK);
            screen_print("   ");
            screen_putchar((char)254);
            screen_print(" Usage: mkapp <filename>\n");
            screen_set_color(VGA_WHITE, VGA_BLACK);
            return 0;
        }
        char abs_path[128];
        resolve_path(arg, abs_path);
        
        /* Machine code for:
           while(1) { __asm__ volatile("yield (int 0x80)"); }
           Actually int 0x80 with eax=0 is yield.
           Code:
           b8 00 00 00 00  mov eax, 0
           cd 80           int 0x80
           eb f9           jmp -7
        */
        char app_code[] = {
            '\xB8', '\x00', '\x00', '\x00', '\x00', /* mov eax, 0 */
            '\xCD', '\x80',                         /* int 0x80 */
            '\xEB', '\xF7'                          /* jmp to mov (starts at IP-9, but size is 2, so offset is -9) */
        };
        app_code[8] = (char)0xF7; /* -9 in 2's complement */
        
        /* Using internal fs_write trick since it null terminates, we'll write via fs internals if needed. 
           Wait, fs_write accepts a null-terminated string and stops at 0. Since our code has 0x00, it will stop!
           Let's rewrite mkapp to directly poke the fs_append or handle binary write.
           Wait, there is a better infinite loop without 0x00:
           31 c0     xor eax, eax
           cd 80     int 0x80
           eb fa     jmp -6
        */
        char safe_app_code[] = {
            '\x31', '\xC0',         /* xor eax, eax */
            '\xCD', '\x80',         /* int 0x80 */
            '\xEB', '\xFA', '\0'    /* jmp -6 */
        };
        
        if (fs_write(abs_path, safe_app_code) == 0) {
            screen_set_color(VGA_GREEN, VGA_BLACK);
            screen_print("   ");
            screen_putchar((char)254);
            screen_print(" Successfully written test app code.\n");
        } else {
            screen_set_color(VGA_RED, VGA_BLACK);
            screen_print("   ");
            screen_putchar((char)254);
            screen_print(" Failed to write test app.\n");
        }
        screen_set_color(VGA_WHITE, VGA_BLACK);
        return 0;
    }

    /* ── Unknown ── */
    screen_set_color(VGA_RED, VGA_BLACK);
    screen_print("   ");
    screen_putchar((char)254);
    screen_print(" Unknown: ");
    screen_print(cmd);
    screen_print("\n");
    screen_set_color(VGA_DARK_GREY, VGA_BLACK);
    screen_print("   Type 'help' for commands.\n");
    screen_set_color(VGA_WHITE, VGA_BLACK);
    return 0;
}

/* ── Main shell loop ───────────────────────────────────────── */
int shell_run(void) {
    /* Welcome banner */
    screen_set_color(VGA_DARK_GREY, VGA_BLACK);
    screen_print("   ");
    for (int i = 0; i < 50; i++) screen_putchar((char)196);
    screen_print("\n");
    screen_set_color(VGA_CYAN, VGA_BLACK);
    screen_print("   ");
    screen_putchar((char)6);
    screen_print(" SwanOS CLI");
    /* Set current directory to home directory on entry */
    strcpy(cwd, "/home/");
    strcat(cwd, user_current());
    
    screen_set_color(VGA_DARK_GREY, VGA_BLACK);
    screen_print("  ");
    screen_putchar((char)250);
    screen_print("  ");
    screen_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    screen_print("Logged in as ");
    screen_set_color(VGA_GREEN, VGA_BLACK);
    screen_print(user_current());
    screen_print("\n");
    screen_set_color(VGA_DARK_GREY, VGA_BLACK);
    screen_print("   ");
    for (int i = 0; i < 50; i++) screen_putchar((char)196);
    screen_print("\n");
    screen_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    screen_print("   Type ");
    screen_set_color(VGA_WHITE, VGA_BLACK);
    screen_print("help");
    screen_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    screen_print(" for commands, ");
    screen_set_color(VGA_WHITE, VGA_BLACK);
    screen_print("ask <q>");
    screen_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    screen_print(" to talk to AI\n\n");
    screen_set_color(VGA_WHITE, VGA_BLACK);

    while (1) {
        print_prompt();
        read_line_with_history(cmd_buf, CMD_BUF);

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
            outb(0x64, 0xFE);
            __asm__ volatile ("cli; hlt");
        }
        if (result == -3) {
            /* Re-login */
            return -3;
        }
        if (result == -4) {
            /* Switch to GUI — handled by kernel_main */
            return -4;
        }
    }
}
