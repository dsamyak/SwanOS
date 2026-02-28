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
    mov esp, stack_top      ; set up stack
    push ebx                ; push multiboot info pointer
    push eax                ; push multiboot magic
    call kernel_main        ; jump to C kernel
    cli                     ; disable interrupts
.hang:
    hlt
    jmp .hang               ; halt forever
