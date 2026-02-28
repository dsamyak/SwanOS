# ============================================================
# SwanOS — Makefile
# Builds the bare-metal x86 kernel and creates a bootable ISO.
#
# Requirements (install via WSL or Linux):
#   sudo apt install nasm gcc make xorriso grub-pc-bin grub-common
#
# Usage:
#   make          → builds swanos.iso
#   make clean    → removes build artifacts
#   make run      → builds and runs in QEMU (if installed)
# ============================================================

# Compiler settings
CC      = gcc
CFLAGS  = -m32 -ffreestanding -fno-stack-protector -fno-pie -nostdlib \
          -Wall -Wextra -Isrc -O2
LDFLAGS = -m elf_i386 -T linker.ld -nostdlib
ASM     = nasm
ASMFLAGS = -f elf32

# Source files
C_SRCS   = $(wildcard src/*.c)
ASM_SRCS = $(wildcard src/*.asm)

# Object files
C_OBJS   = $(C_SRCS:.c=.o)
ASM_OBJS = $(ASM_SRCS:.asm=.o)
OBJS     = $(ASM_OBJS) $(C_OBJS)

# Output
KERNEL = swanos.bin
ISO    = swanos.iso

# ── Build targets ──────────────────────────────────────────

.PHONY: all clean run iso

all: $(ISO)

# Link kernel binary
$(KERNEL): $(OBJS)
	ld $(LDFLAGS) -o $@ $^

# Compile C sources
src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Assemble ASM sources
src/%.o: src/%.asm
	$(ASM) $(ASMFLAGS) $< -o $@

# Create bootable ISO with GRUB
$(ISO): $(KERNEL) grub.cfg
	mkdir -p iso/boot/grub
	cp $(KERNEL) iso/boot/swanos.bin
	cp grub.cfg iso/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) iso/
	rm -rf iso/
	@echo ""
	@echo "  ✓ SwanOS ISO built: $(ISO)"
	@echo "  Boot it in VirtualBox or QEMU!"
	@echo ""

# Run in QEMU (for quick testing)
run: $(ISO)
	qemu-system-i386 -cdrom $(ISO) -serial stdio -m 128M

# Clean
clean:
	rm -f src/*.o $(KERNEL) $(ISO)
	rm -rf iso/
