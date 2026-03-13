section .text
global gdt_flush
global tss_flush

gdt_flush:
    mov eax, [esp+4]
    lgdt [eax]

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:.flush
.flush:
    ret

tss_flush:
    mov ax, 0x2B    ; 5th descriptor is index 5 * 8 = 0x28, RPL 3 = 0x2B
    ltr ax
    ret
