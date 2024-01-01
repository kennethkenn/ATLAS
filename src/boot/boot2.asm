bits 16
global _start
_start:
    cld
    ; Setup segments explicitly
    xor ax, ax
    mov ds, ax
    mov es, ax

    ; Print Stage 2 message (real mode)
    mov si, msg2
    call print_string

    mov [boot_drive], dl

    call enable_a20

    ; --- Read Root Directory ---
    ; LBA calculation: reserved_sectors (32) + (fat_count (2) * sectors_per_fat (0x400))
    ; = 32 + (2 * 1024) = 32 + 2048 = 2080
    ; This matches the BPB parameters defined in boot1.asm
    mov eax, 2080           ; LBA of Root Dir
    mov bx, 0x1000          ; temporary buffer
    mov es, bx
    xor bx, bx
    mov di, 1
    call read_sectors_lba

    ; Checkpoint D
    mov al, 'D'
    call print_char

    ; Debug: Show first 11 chars of Root Dir
    mov si, 0
    mov cx, 11
.show_root:
    mov al, [es:si]
    call print_char
    inc si
    loop .show_root

    ; Search for "ATLAS   CFG" in the root directory
    mov si, 0
.search_loop:
    push si
    mov di, si              ; DI = entry start in ES
    mov si, config_filename ; SI = target name in DS
    mov cx, 11
    repe cmpsb
    pop si
    je .found_config
    add si, 32              ; next directory entry
    cmp si, 512             ; end of sector
    jl .search_loop
    
    ; If not found, just use hardcoded defaults (handled in kernel)
    mov dword [config_addr], 0
    jmp .enter_protected_mode

.found_config:
    ; Found it! Get the starting cluster
    ; Offset 20 (high word) and 26 (low word)
    mov ax, [es:si + 20]
    shl eax, 16
    mov ax, [es:si + 26]
    mov [config_cluster], eax

    ; Convert cluster to LBA:
    ; LBA = (cluster - 2) * sectors_per_cluster + DataAreaStart
    ; LBA = (cluster - 2) * 1 + 2080 (where 2080 = reserved + FAT area)
    sub eax, 2
    add eax, 2080
    
    ; Load the config file to 0x2000:0000 (0x20000)
    mov bx, 0x2000
    mov es, bx
    xor bx, bx
    mov di, 1               ; assume config is < 512 bytes for now
    call read_sectors_lba
    
    mov dword [config_addr], 0x20000

.enter_protected_mode:
    cli
    lgdt [gdtr]             ; load GDTR (pointer)
    ; enable protected mode
    mov eax, cr0
    or  eax, 1
    mov cr0, eax
    jmp 0x08:protected_mode_entry ; far jump to flush prefetch and load CS

; ----------------- real mode helpers -----------------
print_char:
    pusha
    mov ah, 0x0E
    int 0x10
    popa
    ret

enable_a20:
    pusha
    in  al, 0x92
    or  al, 2
    out 0x92, al
    popa
    ret

print_string:
    pusha
    mov ah, 0x0E
.ps_next:
    lodsb
    or al, al
    jz .ps_done
    int 0x10
    jmp .ps_next
.ps_done:
    popa
    ret

; Read sectors using LBA
; EAX = LBA, ES:BX = buffer, DI = count
read_sectors_lba:
    pushad
    mov [dap_lba], eax
    mov [dap_buffer_off], bx
    mov [dap_buffer_seg], es
    mov [dap_count], di
    mov si, dap
    mov dl, [boot_drive]
    mov ah, 0x42
    int 0x13
    jc .error
    popad
    ret
.error:
    mov si, msg_disk_err
    call print_string
    hlt

; ----------------- Data -----------------
msg2 db "Stage 2 (LBA/FAT32 Support)", 0x0D, 0x0A, 0
msg_disk_err db "LBA Read Error!", 0
boot_drive db 0
config_filename db "ATLAS   CFG"
config_cluster dd 0
config_addr dd 0

; Disk Address Packet for INT 13h AH=42h
align 4
dap:
    db 0x10                ; size of DAP
    db 0                   ; reserved
dap_count:
    dw 0                   ; number of sectors to read
dap_buffer_off:
    dw 0                   ; offset
dap_buffer_seg:
    dw 0                   ; segment
dap_lba:
    dq 0                   ; LBA

; ----------------- 32-bit protected mode -----------------
bits 32
extern kmain
protected_mode_entry:
    ; set up flat data segments and stack
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000        ; set stack

    ; Checkpoint OK (PM)
    mov dword [0xB8000], 0x2F212F21 ; "!!" in green

    ; remap PICs (properly)
    call remap_pic_proper

    ; install IDT entries and load IDT
    call install_idt

    ; Pass arguments to kmain: kmain(config_addr, bpb_ptr)
    ; BPB starts at 0x7C0B (Stage 1 starts at 0x7C00, skip 3 bytes JMP and 8 bytes OEM)
    push 0x7C0B             ; bpb_ptr
    push dword [config_addr] ; config_addr
    call kmain
    add esp, 8

    ; test: print in protected mode by writing to VGA text buffer
    mov esi, msg3
    mov edi, 0x000B8000
    call print_pm

.hang_pm:
    hlt
    jmp .hang_pm

; ----------------- protected-mode helpers -----------------
print_pm:
.pm_loop:
    lodsb
    test al, al
    jz .pm_done
    mov byte [edi], al
    mov byte [edi+1], 0x07
    add edi, 2
    jmp .pm_loop
.pm_done:
    ret

; ----------------- GDT (null, code, data) -----------------
; 3 descriptors: null, code (0x08), data (0x10)
gdt:
    dq 0x0000000000000000

    ; code descriptor: base=0, limit=4GiB, access=0x9A, flags=0xCF
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 0x9A
    db 0xCF
    db 0x00

    ; data descriptor: base=0, limit=4GiB, access=0x92, flags=0xCF
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 0x92
    db 0xCF
    db 0x00
gdt_end:

gdtr:                       ; GDTR pointer (limit then base)
    dw gdt_end - gdt - 1
    dd gdt

; ----------------- IDT table (zeroed) -----------------
; 256 entries * 8 bytes = 2048 bytes
idt_start:
    times 256 dq 0
idt_end:

idt_descriptor:
    dw idt_end - idt_start - 1
    dd idt_start

; ----------------- runtime: set one IDT entry -----------------
; inputs:   EAX = handler address, ECX = vector index
; writes an 8-byte gate at (idt_start + ECX*8)
set_idt_entry:
    push edi
    push esi
    push ebx

    mov edi, idt_start
    mov ebx, ecx
    shl ebx, 3              ; ebx = index * 8
    add edi, ebx            ; edi -> entry address

    mov ebx, eax            ; save handler addr in EBX
    mov ax, bx              ; low 16
    mov [edi], ax           ; offset low
    mov word [edi+2], 0x08  ; selector (code)
    mov byte [edi+4], 0     ; zero
    mov byte [edi+5], 0x8E  ; present, ring0, 32-bit interrupt gate
    
    mov eax, ebx
    shr eax, 16
    mov [edi+6], ax         ; offset high (cleaner version)

    pop ebx
    pop esi
    pop edi
    ret

; ----------------- install IDT entries and load IDT -----------------
install_idt:
    ; create entries for IRQ0 (vector 0x20), IRQ1 (0x21), and an exception (0x0E)
    mov ecx, 0x20
    mov eax, isr_timer
    call set_idt_entry

    mov ecx, 0x21
    mov eax, isr_keyboard
    call set_idt_entry

    mov ecx, 0x0E
    mov eax, isr_default
    call set_idt_entry

    lidt [idt_descriptor]
    ret

; ----------------- PIC remap (proper, preserves masks) ----------
%define PIC1_CMD 0x20
%define PIC1_DATA 0x21
%define PIC2_CMD 0xA0
%define PIC2_DATA 0xA1
%define ICW1_INIT 0x11
%define ICW4_8086 0x01

extern keyboard_handler_c

remap_pic_proper:
    cli
    ; save masks
    in al, PIC1_DATA
    mov [saved_mask1], al
    in al, PIC2_DATA
    mov [saved_mask2], al

    ; start init
    mov al, ICW1_INIT
    out PIC1_CMD, al
    out PIC2_CMD, al

    ; set offsets
    mov al, 0x20
    out PIC1_DATA, al
    mov al, 0x28
    out PIC2_DATA, al

    ; wiring
    mov al, 0x04
    out PIC1_DATA, al
    mov al, 0x02
    out PIC2_DATA, al

    ; ICW4
    mov al, ICW4_8086
    out PIC1_DATA, al
    out PIC2_DATA, al

    ; restore saved masks
    mov al, [saved_mask1]
    out PIC1_DATA, al
    mov al, [saved_mask2]
    out PIC2_DATA, al

    ret

saved_mask1: db 0
saved_mask2: db 0

; ----------------- IRQ handlers (32-bit stubs) -----------------
; Use pushad/popad in protected mode and iretd to return
isr_timer:
    pushad
    ; (increment tick count here if you have one)
    ; send EOI to master PIC
    mov al, 0x20
    out 0x20, al
    popad
    iretd

isr_keyboard:
    pushad
    in al, 0x60             ; read scancode

    push eax                ; argument for C handler
    call keyboard_handler_c
    add esp, 4

    ; EOI to master PIC
    mov al, 0x20
    out 0x20, al
    popad
    iretd


isr_default:
    pushad
    ; default stub for exceptions; you can log or halt here
    popad
    iretd


msg3 db "Protected Mode!", 0