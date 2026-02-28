# SwanOS — Bare-Metal AI Operating System

A real x86 operating system written in C and Assembly that boots directly on hardware.
No Linux, no Windows underneath. Includes AI capability via serial bridge to Groq API.

## Quick Start

### 1. Install Build Tools (WSL/Linux)
```bash
sudo apt install nasm gcc make xorriso grub-pc-bin grub-common qemu-system-x86
```

### 2. Build
```bash
make
```
This produces `swanos.iso` — a bootable CD image.

### 3. Run in QEMU (quick test)
```bash
# Terminal 1: Run QEMU with serial on stdio
make run

# The bridge runs on the same terminal (serial = stdio)
```

### 4. Run in QEMU with AI bridge
```bash
# Terminal 1: Start bridge
python bridge.py &

# Terminal 2: Run QEMU piping serial to bridge
qemu-system-i386 -cdrom swanos.iso -serial stdio -m 128M
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
   python bridge.py --pipe \\.\pipe\swanos_serial

   # Linux
   python bridge.py --pipe /tmp/swanos_serial
   ```

## Architecture

```
Power On → BIOS → GRUB → SwanOS Kernel (C/ASM)
                              ├── VGA Display (80×25 text mode)
                              ├── PS/2 Keyboard (IRQ1)
                              ├── PIT Timer (IRQ0, 100 Hz)
                              ├── COM1 Serial (115200 baud)
                              ├── Memory Allocator (4 MB heap)
                              ├── In-Memory Filesystem
                              ├── User Login System
                              └── Command Shell
                                    └── AI (via serial → bridge.py → Groq API)
```

## Shell Commands

| Command | Description |
|---------|-------------|
| `ask <question>` | Ask the AI |
| `ls [path]` | List files |
| `cat <file>` | Read file |
| `write <file> <text>` | Create/write file |
| `mkdir <name>` | Create directory |
| `rm <file>` | Delete file |
| `calc <expr>` | Calculator |
| `echo <text>` | Print text |
| `clear` | Clear screen |
| `help` | Help |
| `whoami` | User info |
| `status` | System status |
| `time` | Uptime |
| `login` | Switch user |
| `reboot` | Reboot |
| `shutdown` | Power off |

## Files

```
src/
├── boot.asm       # Multiboot entry point
├── idt_asm.asm    # ISR/IRQ assembly stubs
├── kernel.c       # Kernel main
├── screen.c/h     # VGA text mode driver
├── keyboard.c/h   # PS/2 keyboard driver
├── timer.c/h      # PIT timer driver
├── serial.c/h     # COM1 serial driver
├── idt.c/h        # Interrupt Descriptor Table
├── memory.c/h     # Memory allocator
├── string.c/h     # String utilities
├── ports.h        # x86 I/O ports
├── fs.c/h         # In-memory filesystem
├── user.c/h       # User login manager
├── llm.c/h        # LLM client (serial)
└── shell.c/h      # Command shell

Makefile           # Build system
linker.ld          # Linker script
grub.cfg           # GRUB boot menu
bridge.py          # Host-side AI bridge
```
