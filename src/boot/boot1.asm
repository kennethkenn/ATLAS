; ------------------------------------------------------------
; Bootloader Stage 1 (16-bit real mode)
; Loads Stage 2 (from reserved sectors) and jumps
; ------------------------------------------------------------

bits 16
org 0x7C00

jmp short start
nop

; --- FAT32 BIOS Parameter Block (BPB) ---
oem_name            db "ATLAS   "      ; 8 bytes
bytes_per_sector    dw 512
sectors_per_cluster db 1
reserved_sectors    dw 32             ; Stage 2 + kernel will live here
fat_count           db 2
root_entry_count    dw 0              ; 0 for FAT32
total_sectors       dw 0              ; use 32-bit version
media_type          db 0xF8
sectors_per_fat_16  dw 0
sectors_per_track   dw 32
head_count          dw 64
hidden_sectors      dd 0
total_sectors_32    dd 0x20000        ; 64MB placeholder

; Extended Boot Record (FAT32)
sectors_per_fat_32  dd 0x400
ext_flags           dw 0
fs_version          dw 0
root_cluster        dd 2
fs_info_sector      dw 1
backup_boot_sector  dw 6
reserved            times 12 db 0
drive_number        db 0x80
reserved1           db 0
ext_signature       db 0x29
volume_id           dd 0x12345678
volume_label        db "ATLAS BOOT "  ; 11 bytes
fs_type             db "FAT32   "      ; 8 bytes

start:
    ; Setup segment registers
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    ; Save boot drive
    mov [BootDrive], dl

    ; Print "Stage 1"
    mov si, msg1
    call print_string

    ; --- Load Stage 2 ---
    ; Use LBA (Extended Read) to avoid CHS geometry issues
    ; Load Stage 2 starting at sector 1 (LBA 1 is the 2nd sector)
    mov si, dap
    mov dl, [BootDrive]
    mov ah, 0x42
    int 0x13
    jc disk_error

    ; Jump to Stage 2
    jmp 0x0000:0x7E00

align 4
dap:
    db 0x10                ; size of DAP
    db 0                   ; reserved
    dw 127                  ; count (read more than enough, Stage 2 will fit)
    dw 0x7E00              ; offset
    dw 0x0000              ; segment
    dq 8                   ; LBA (Sector 9, where Stage 2 starts)

; --- Error Handler ---
disk_error:
    mov si, msg_err
    call print_string
.hang: jmp .hang

; --- Subroutines ---
print_string:
    mov ah, 0x0E
.next_char:
    lodsb
    or al, al
    jz .done
    int 0x10
    jmp .next_char
.done:
    ret

; --- Data ---
msg1     db "Stage 1 (FAT32)", 0x0D, 0x0A, 0
msg_err  db "Disk error!", 0
BootDrive db 0

; --- Boot signature ---
times 510-($-$$) db 0
dw 0xAA55
