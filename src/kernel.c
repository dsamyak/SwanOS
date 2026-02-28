/* ============================================================
 * SwanOS — Kernel Main
 * Boot sequence → mode selection (GUI / CLI) → login → shell
 * ============================================================ */

#include <stdint.h>
#include "screen.h"
#include "idt.h"
#include "timer.h"
#include "keyboard.h"
#include "serial.h"
#include "memory.h"
#include "fs.h"
#include "user.h"
#include "shell.h"
#include "gui.h"

static void boot_banner(void) {
    screen_set_color(VGA_CYAN, VGA_BLACK);
    screen_print("\n");
    screen_print("   ███████╗██╗    ██╗ █████╗ ███╗   ██╗     ██████╗ ███████╗\n");
    screen_print("   ██╔════╝██║    ██║██╔══██╗████╗  ██║    ██╔═══██╗██╔════╝\n");
    screen_print("   ███████╗██║ █╗ ██║███████║██╔██╗ ██║    ██║   ██║███████╗\n");
    screen_print("   ╚════██║██║███╗██║██╔══██║██║╚██╗██║    ██║   ██║╚════██║\n");
    screen_print("   ███████║╚███╔███╔╝██║  ██║██║ ╚████║    ╚██████╔╝███████║\n");
    screen_print("   ╚══════╝ ╚══╝╚══╝ ╚═╝  ╚═╝╚═╝  ╚═══╝     ╚═════╝ ╚══════╝\n");
    screen_print("\n");

    screen_set_color(VGA_WHITE, VGA_BLACK);
    screen_print("   AI-Powered Operating System v2.0\n");
    screen_set_color(VGA_DARK_GREY, VGA_BLACK);
    screen_print("   ──────────────────────────────────────────────────────────\n\n");
    screen_set_color(VGA_WHITE, VGA_BLACK);
}

static void boot_status(const char *msg) {
    screen_set_color(VGA_GREEN, VGA_BLACK);
    screen_print("   [OK] ");
    screen_set_color(VGA_WHITE, VGA_BLACK);
    screen_print(msg);
    screen_print("\n");
}

static int select_mode(void) {
    screen_set_color(VGA_DARK_GREY, VGA_BLACK);
    screen_print("\n   ──────────────────────────────────────────────────────────\n\n");

    screen_set_color(VGA_CYAN, VGA_BLACK);
    screen_print("   Select interface:\n\n");

    screen_set_color(VGA_WHITE, VGA_BLACK);
    screen_print("     ");
    screen_set_color(VGA_CYAN, VGA_BLACK);
    screen_print("[1]");
    screen_set_color(VGA_WHITE, VGA_BLACK);
    screen_print("  GUI Mode   ");
    screen_set_color(VGA_DARK_GREY, VGA_BLACK);
    screen_print("— Visual interface with panels & sidebar\n");

    screen_set_color(VGA_WHITE, VGA_BLACK);
    screen_print("     ");
    screen_set_color(VGA_GREEN, VGA_BLACK);
    screen_print("[2]");
    screen_set_color(VGA_WHITE, VGA_BLACK);
    screen_print("  CLI Mode   ");
    screen_set_color(VGA_DARK_GREY, VGA_BLACK);
    screen_print("— Classic command-line interface\n\n");

    screen_set_color(VGA_CYAN, VGA_BLACK);
    screen_print("   Press 1 or 2: ");
    screen_set_color(VGA_WHITE, VGA_BLACK);

    while (1) {
        char c = keyboard_getchar();
        if (c == '1') {
            screen_print("GUI\n");
            return 1;
        }
        if (c == '2') {
            screen_print("CLI\n");
            return 2;
        }
    }
}

void kernel_main(uint32_t magic, uint32_t mboot_info) {
    (void)magic;
    (void)mboot_info;

    /* ── Initialize subsystems ── */
    screen_init();
    boot_banner();

    boot_status("VGA display initialized (80x25)");

    idt_init();
    boot_status("Interrupt Descriptor Table loaded");

    timer_init(100);
    boot_status("PIT timer initialized (100 Hz)");

    keyboard_init();
    boot_status("PS/2 keyboard driver loaded");

    serial_init();
    boot_status("COM1 serial port initialized (115200 baud)");

    memory_init();
    boot_status("Memory allocator ready (4 MB heap)");

    fs_init();
    fs_write("readme.txt",
        "Welcome to SwanOS!\n"
        "A bare-metal AI-powered operating system.\n"
        "Type 'help' for commands, 'ask <q>' to talk to AI.");
    fs_mkdir("documents");
    fs_mkdir("programs");
    boot_status("In-memory filesystem mounted");

    user_init();
    boot_status("User manager initialized");

    screen_set_color(VGA_GREEN, VGA_BLACK);
    screen_print("\n   All systems online.\n");
    screen_set_color(VGA_WHITE, VGA_BLACK);

    /* ── Mode selection + login loop ── */
    int mode = select_mode();

    while (1) {
        /* Login */
        if (mode == 1) {
            /* GUI login happens inside GUI; quick login first */
            screen_clear();
            boot_banner();
        }

        if (!user_login()) continue;

        screen_print("\n");

        if (mode == 1) {
            gui_run();
            /* GUI returns on 'cli' command or 'login' */
            mode = 2; /* switch to CLI on return */
            screen_clear();
            screen_set_color(VGA_CYAN, VGA_BLACK);
            screen_print("\n  Switched to CLI mode.\n");
            screen_print("  Type 'gui' to switch back.\n\n");
            screen_set_color(VGA_WHITE, VGA_BLACK);
        }

        if (mode == 2) {
            shell_run();
            /* shell returns on 'login' command → loop back */
            mode = select_mode(); /* ask again on re-login */
        }
    }
}
