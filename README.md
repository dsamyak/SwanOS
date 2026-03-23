# SwanOS v3.0 — Bare-Metal AI Operating System

A real x86 operating system written in C and Assembly that boots directly on hardware.
No Linux, no Windows underneath. Features a full graphical desktop environment,
AI capability via serial bridge to Groq API, and a rich CLI shell.

## Quick Start

### 1. Install Build Tools (WSL/Linux)
```bash
sudo apt install nasm gcc g++ make xorriso grub-pc-bin grub-common qemu-system-x86
```

### 2. Build
```bash
make
```
This produces `swanos.iso` — a bootable CD image.

### 3. Run in QEMU (quick test)
```bash
# Run QEMU with serial on stdio
make run

# The AI bridge runs on the same terminal (serial = stdio)
```

### 4. Run in QEMU with AI bridge
```bash
# Terminal 1: Start bridge
python llm_bridge.py &

# Terminal 2: Run QEMU piping serial to bridge
qemu-system-i386 -cdrom swanos.iso -serial stdio -m 128M -device e1000
```

### 5. Run in VirtualBox

1. **Create VM**: Type = Other, Version = Other/Unknown
   - RAM: 128 MB (minimum)
   - No hard disk needed (boots from ISO)

2. **Attach ISO**: Settings → Storage → Add CD → select `swanos.iso`

3. **Enable Serial Port**: Settings → Serial Ports → Port 1
   - Enable: ✓
   - Port Mode: Host Pipe
   - Path: `\\.\pipe\swanos_serial` (Windows) or `/tmp/swanos_serial` (Linux)
   - Uncheck "Connect to existing pipe"

4. **Start VM** → SwanOS boots!

5. **Start AI Bridge** (on host):
   ```bash
   # Windows
   python llm_bridge.py --pipe \\.\pipe\swanos_serial

   # Linux
   python llm_bridge.py --pipe /tmp/swanos_serial
   ```

### 6. Install to Real Hardware (USB Boot)

1. **Write ISO to USB** (Linux/WSL):
   ```bash
   # Find your USB device (e.g. /dev/sdb)
   lsblk

   # Write the ISO — CAUTION: this erases the USB drive!
   sudo dd if=swanos.iso of=/dev/sdX bs=4M status=progress && sync
   ```

2. **Write ISO to USB** (Windows — use Rufus):
   - Download [Rufus](https://rufus.ie)
   - Select `swanos.iso`, target USB drive
   - Partition scheme: MBR, Target: BIOS
   - Click Start

3. **Boot from USB**:
   - Enter BIOS/UEFI setup (F2/F12/DEL at boot)
   - Set boot priority to USB
   - Save & reboot → SwanOS loads!

> **Note:** AI features require a serial connection to host running `llm_bridge.py`.
> On real hardware, use a USB-to-serial adapter connected to COM1.

## Architecture

```
Power On → BIOS → GRUB → SwanOS Kernel (C/ASM)
                              ├── Display
                              │     ├── VGA Text Mode (80×25)
                              │     └── VESA/VBE Graphics (1024×768×32bpp)
                              ├── Input
                              │     ├── PS/2 Keyboard (IRQ1)
                              │     └── PS/2 Mouse (IRQ12)
                              ├── Hardware
                              │     ├── PIT Timer (IRQ0, 100 Hz)
                              │     ├── RTC (Real-Time Clock)
                              │     ├── COM1 Serial (115200 baud)
                              │     └── E1000 Network (virtio)
                              ├── Core
                              │     ├── GDT + TSS (Ring 0/3)
                              │     ├── IDT (Interrupts + Exceptions)
                              │     ├── Paging (Virtual Memory)
                              │     ├── Process Manager (Ring 3 tasks)
                              │     ├── Syscall Interface
                              │     └── Memory Allocator (4 MB heap)
                              ├── Services
                              │     ├── In-Memory Filesystem
                              │     ├── User Login System
                              │     ├── Network Stack (ARP/IP)
                              │     └── AI (serial → llm_bridge.py → Groq)
                              └── User Interface
                                    ├── CLI Shell (styled, history)
                                    ├── Desktop Environment (Neon Aurora)
                                    │     ├── Floating Dock + System Tray
                                    │     ├── Kickoff Menu
                                    │     ├── Window Manager
                                    │     └── Context Menu
                                    └── Apps
                                          ├── AI Chat
                                          ├── Terminal
                                          ├── File Manager
                                          ├── Notes (with save)
                                          ├── Calculator
                                          ├── System Monitor
                                          ├── Software Store
                                          ├── Web Browser
                                          ├── Network Info
                                          ├── About / Snake Game
                                          └── Quick Settings
```

## Shell Commands

| Command | Description |
|---------|-------------|
| **AI** | |
| `ask <question>` | Ask the AI assistant |
| `setkey <key>` | Set Groq API key |
| `aikey` | Check API key status |
| **Files** | |
| `cd <dir>` | Change directory |
| `ls [path]` | List files |
| `cat <file>` | Read file |
| `write <file> <text>` | Create/write file |
| `append <file> <text>` | Append to file |
| `cp <src> <dst>` | Copy file |
| `mv <file> <name>` | Rename file |
| `mkdir <name>` | Create directory |
| `rm <file>` | Delete file/dir |
| `hexdump <file>` | Hex viewer |
| **Utilities** | |
| `calc <expr>` | Calculator (+−×÷) |
| `echo <text>` | Print text |
| `clear` | Clear screen |
| `snake` | Snake game! |
| **System** | |
| `help` | Command reference |
| `whoami` | User & system info |
| `status` | Full system status |
| `date` | Date & time |
| `mem` | Memory usage |
| `time` | Uptime |
| `history` | Command history |
| `gui` | Switch to GUI mode |
| `login` | Switch user |
| `reboot` | Reboot |
| `shutdown` | Power off |

## GUI Desktop

SwanOS v3.0 features a full graphical desktop environment with:
- **Neon Aurora theme** — frosted glass panels, neon glows, gradient wallpaper
- **Floating dock** — launcher, task buttons, system tray with sparklines
- **Window manager** — draggable, focusable windows with titlebars
- **10 built-in apps** — AI Chat, Terminal, Files, Notes, Calculator, System Monitor, Store, Browser, Network, About
- **Kickoff menu** — app launcher with user avatar
- **Context menu** — right-click for quick actions
- **Quick settings** — volume, brightness, Wi-Fi, Bluetooth, DND toggles

## Files

```
src/
├── boot.asm          # Multiboot entry point
├── gdt_asm.asm       # GDT/TSS assembly
├── idt_asm.asm       # ISR/IRQ assembly stubs
├── paging_asm.asm    # Paging enable assembly
├── kernel.c          # Kernel main + mode switching
├── gdt.c/h           # Global Descriptor Table + TSS
├── idt.c/h           # Interrupt Descriptor Table
├── paging.c/h        # Virtual memory / paging
├── process.c/h       # Process manager (Ring 0/3)
├── syscall.c/h       # System call interface
├── memory.c/h        # Memory allocator (4 MB heap)
├── screen.c/h        # VGA text mode driver
├── vga_gfx.c/h       # VESA/VBE graphics driver (1024×768)
├── keyboard.c/h      # PS/2 keyboard driver
├── mouse.c/h         # PS/2 mouse driver
├── timer.c/h         # PIT timer driver (100 Hz)
├── rtc.c/h           # Real-time clock driver
├── serial.c/h        # COM1 serial driver
├── network.c/h       # E1000 NIC + network stack
├── fs.c/h            # In-memory filesystem
├── user.c/h          # User login manager
├── llm.c/h           # LLM client (serial protocol)
├── kernel_ai.c/h     # Kernel-level AI helpers
├── shell.c/h         # CLI command shell
├── desktop.c/h       # Desktop environment (GUI)
├── game.c/h          # Snake game engine
├── ui_theme.h        # Neon Aurora theme system
├── string.c/h        # String utilities
├── ports.h           # x86 I/O port helpers
├── multiboot.h       # Multiboot header definitions
├── cxxabi.cpp         # C++ ABI stubs
├── vga_font.c/h      # VGA bitmap font data

Makefile              # Build system
linker.ld             # Linker script
grub.cfg              # GRUB boot menu
llm_bridge.py         # Host-side AI bridge (Groq)
```

## License

SwanOS is a personal/educational project.
