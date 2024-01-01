// UEFI Entry Point for Atlas Bootloader
#include "efi.h"
#include "fat32.h"
#include "mem.h"

// Global System Table
EFI_SYSTEM_TABLE *g_SystemTable = NULL;
EFI_HANDLE g_ImageHandle = NULL;

// Protocol GUIDs
EFI_GUID EFI_LOADED_IMAGE_PROTOCOL_GUID = { 0x5B1B31A1, 0x9562, 0x11D2, { 0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B } };
EFI_GUID EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID = { 0x0964E5B22, 0x6459, 0x11D2, { 0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B } };
EFI_GUID EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID = { 0x9042a9de, 0x23dc, 0x4a38, { 0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a } };

// External Kernel Entry Point
void kmain(char *config_addr, struct fat32_bpb *bpb);

// UEFI Entry Point
EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    // Save global pointers
    g_SystemTable = SystemTable;
    g_ImageHandle = ImageHandle;

    // Clear screen
    SystemTable->ConOut->ClearScreen(SystemTable->ConOut);
    SystemTable->ConOut->SetAttribute(SystemTable->ConOut, 0x07); // Light Gray on Black (Standard)
    
    // Allocate buffer for config
    // kmalloc in UEFI mode calls AllocatePool, so safe to use immediately
    char *config_buf = (char *)kmalloc(4096); 
    
    // Load Configuration
    if (config_buf) {
        // Zero out buffer
        for(int i=0; i<4096; i++) config_buf[i] = 0;
        
        if (fat32_read_file("ATLAS.CFG", config_buf) != 0) {
            SystemTable->ConOut->OutputString(SystemTable->ConOut, (short*)L"Warning: ATLAS.CFG not found.\r\n");
            // kmain handles null/empty config gracefully (default entries)
        }
    } else {
        SystemTable->ConOut->OutputString(SystemTable->ConOut, (short*)L"Error: Failed to allocate config buffer.\r\n");
    }

    // Call shared kernel main
    // Pass config buffer and NULL for BPB (as BPB is BIOS specific)
    kmain(config_buf, NULL);

    // Should not return, but if kmain exits:
    while (1) {
        __asm__ __volatile__("pause");
    }
    
    return EFI_SUCCESS;
}
