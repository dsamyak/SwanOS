; ============================================================
; SwanOS — ISR / IRQ Assembly Stubs
; Each stub pushes an interrupt number and jumps to a common
; handler that calls the C function isr_handler / irq_handler.
; ============================================================

section .text

; ── External C handlers ────────────────────────────────────
extern isr_handler
extern irq_handler

; ── Common ISR stub ────────────────────────────────────────
isr_common_stub:
    pusha               ; push all general-purpose registers
    mov ax, ds
    push eax            ; save data segment

    mov ax, 0x10        ; kernel data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    call isr_handler

    pop eax             ; restore data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    popa                ; restore registers
    add esp, 8          ; clean up error code + ISR number
    sti
    iret

; ── Common IRQ stub ────────────────────────────────────────
irq_common_stub:
    pusha
    mov ax, ds
    push eax

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    call irq_handler

    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    popa
    add esp, 8
    sti
    iret

; ── ISR stubs (exceptions 0-31) ───────────────────────────
; Some push a dummy error code (0) for uniformity

%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    cli
    push dword 0        ; dummy error code
    push dword %1       ; interrupt number
    jmp isr_common_stub
%endmacro

%macro ISR_ERRCODE 1
global isr%1
isr%1:
    cli
    push dword %1       ; interrupt number (error code already pushed by CPU)
    jmp isr_common_stub
%endmacro

ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE   8
ISR_NOERRCODE 9
ISR_ERRCODE   10
ISR_ERRCODE   11
ISR_ERRCODE   12
ISR_ERRCODE   13
ISR_ERRCODE   14
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_NOERRCODE 17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_NOERRCODE 30
ISR_NOERRCODE 31

; ── IRQ stubs (IRQ 0-15 → INT 32-47) ─────────────────────
%macro IRQ 2
global irq%1
irq%1:
    cli
    push dword 0        ; dummy error code
    push dword %2       ; interrupt number
    jmp irq_common_stub
%endmacro

IRQ  0, 32    ; Timer
IRQ  1, 33    ; Keyboard
IRQ  2, 34
IRQ  3, 35    ; COM2
IRQ  4, 36    ; COM1
IRQ  5, 37
IRQ  6, 38
IRQ  7, 39
IRQ  8, 40
IRQ  9, 41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47
