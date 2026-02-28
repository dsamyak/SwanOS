/* ============================================================
 * SwanOS — Command Shell
 * Parses and executes user commands.
 * ============================================================ */

#include "shell.h"
#include "screen.h"
#include "keyboard.h"
#include "string.h"
#include "timer.h"
#include "fs.h"
#include "user.h"
#include "llm.h"
#include "ports.h"

#define CMD_BUF 256
#define OUT_BUF 4096

static char cmd_buf[CMD_BUF];
static char out_buf[OUT_BUF];

/* ── Simple calculator ─────────────────────────────────────── */
static int calc_eval(const char *expr) {
    int result = 0, num = 0, sign = 1;
    char op = '+';
    int has_num = 0;

    while (*expr) {
        if (isdigit(*expr)) {
            num = num * 10 + (*expr - '0');
            has_num = 1;
        } else if (*expr == '+' || *expr == '-' || *expr == '*' || *expr == '/') {
            if (has_num) {
                if (op == '+') result += sign * num;
                else if (op == '-') result -= num;
                else if (op == '*') result *= num;
                else if (op == '/' && num != 0) result /= num;
            }
            op = *expr;
            if (op == '+' || op == '-') {
                sign = (op == '-') ? -1 : 1;
                op = '+';
            }
            num = 0;
            has_num = 0;
        }
        expr++;
    }
    /* Final number */
    if (has_num) {
        if (op == '+') result += sign * num;
        else if (op == '-') result -= num;
        else if (op == '*') result *= num;
        else if (op == '/' && num != 0) result /= num;
    }
    return result;
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
    screen_print("    mkdir <name>     Create dir\n");
    screen_print("    rm <file>        Delete file\n");
    screen_set_color(VGA_YELLOW, VGA_BLACK);
    screen_print("  Utils\n");
    screen_set_color(VGA_WHITE, VGA_BLACK);
    screen_print("    calc <expr>      Calculator\n");
    screen_print("    echo <text>      Print text\n");
    screen_print("    clear            Clear screen\n");
    screen_set_color(VGA_YELLOW, VGA_BLACK);
    screen_print("  System\n");
    screen_set_color(VGA_WHITE, VGA_BLACK);
    screen_print("    help             This help\n");
    screen_print("    whoami           User info\n");
    screen_print("    status           System info\n");
    screen_print("    time             Uptime\n");
    screen_print("    login            Switch user\n");
    screen_print("    reboot           Reboot\n");
    screen_print("    shutdown         Power off\n\n");
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

    /* ── Status ── */
    if (strcmp(cmd, "status") == 0) {
        uint32_t secs = timer_get_seconds();
        uint32_t mins = secs / 60;
        uint32_t hrs = mins / 60;
        secs %= 60; mins %= 60;

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
    screen_print(" to talk to AI.\n\n");
    screen_set_color(VGA_WHITE, VGA_BLACK);

    while (1) {
        print_prompt();
        keyboard_read_line(cmd_buf, CMD_BUF);

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
