; ============================================================
; SwanOS — Multiboot Entry Point
; Sets up the stack and calls kernel_main() in C
; ============================================================

; Multiboot header constants
MBOOT_MAGIC     equ 0x1BADB002
MBOOT_FLAGS     equ 0x00000003    ; align + meminfo
MBOOT_CHECKSUM  equ -(MBOOT_MAGIC + MBOOT_FLAGS)

section .multiboot
align 4
    dd MBOOT_MAGIC
    dd MBOOT_FLAGS
    dd MBOOT_CHECKSUM

; ── Stack ──────────────────────────────────────────────────
section .bss
align 16
stack_bottom:
    resb 16384              ; 16 KB kernel stack
stack_top:

; ── Entry ──────────────────────────────────────────────────
section .text
global _start
extern kernel_main

_start:
    ; Load our GDT
    lgdt [gdt_descriptor]

    ; Force reload of CS
    jmp 0x08:.reload_cs

.reload_cs:
    ; Reload other segment registers
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov esp, stack_top      ; set up stack
    push ebx                ; push multiboot info pointer
    push eax                ; push multiboot magic
    call kernel_main        ; jump to C kernel
    cli                     ; disable interrupts
.hang:
    hlt
    jmp .hang               ; halt forever

; ── Global Descriptor Table ────────────────────────────────
align 8
gdt_start:
    ; Null descriptor
    dq 0

    ; Code segment (0x08)
    dw 0xFFFF       ; Limit
    dw 0x0000       ; Base
    db 0x00         ; Base
    db 10011010b    ; Access byte (Present, Ring 0, Executable)
    db 11001111b    ; Flags (32-bit, 4KB granularity)
    db 0x00         ; Base

    ; Data segment (0x10)
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b    ; Access byte (Present, Ring 0, Readable/Writable)
    db 11001111b
    db 0x00
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start
