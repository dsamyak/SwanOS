---
description: How to build SwanOS ISO and run it
---

# Build SwanOS

## Prerequisites

You need WSL (Windows Subsystem for Linux) with these packages:

```bash
sudo apt install nasm gcc g++ make xorriso grub-pc-bin grub-common qemu-system-x86
```

## Build Steps

// turbo-all

1. Clean previous build artifacts:
```bash
wsl -e bash -c "cd /mnt/c/Users/VICTUS/Desktop/Swanos && make clean"
```

2. Build the ISO:
```bash
wsl -e bash -c "cd /mnt/c/Users/VICTUS/Desktop/Swanos && make 2>&1"
```

3. Verify the ISO was created:
```bash
wsl -e bash -c "ls -la /mnt/c/Users/VICTUS/Desktop/Swanos/swanos.iso"
```

## Run in QEMU

4. Launch QEMU to test:
```bash
wsl -e bash -c "cd /mnt/c/Users/VICTUS/Desktop/Swanos && qemu-system-i386 -cdrom swanos.iso -serial stdio -m 128M -device e1000 -show-cursor off"
```

## Run with AI Bridge

5. In a separate terminal, start the bridge:
```bash
wsl -e bash -c "cd /mnt/c/Users/VICTUS/Desktop/Swanos && python3 llm_bridge.py"
```

6. Then run QEMU with serial piped:
```bash
wsl -e bash -c "cd /mnt/c/Users/VICTUS/Desktop/Swanos && qemu-system-i386 -cdrom swanos.iso -serial stdio -m 128M -device e1000"
```

## Expected Output

- Build step should end with `✓ SwanOS ISO built: swanos.iso`
- ISO file should be ~5 MB
- QEMU should show SwanOS login screen

## Troubleshooting

- If `grub-mkrescue` fails, ensure `grub-pc-bin` is installed
- If QEMU shows no display, try adding `-vga std` flag
- For AI features, run `llm_bridge.py` on the host alongside QEMU
