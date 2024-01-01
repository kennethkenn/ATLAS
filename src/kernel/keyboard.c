#include "keyboard.h"
#include "vga.h"
#include "fat32.h"
#include "port.h"

extern struct menu atlas_opts;

#ifdef UEFI_BUILD
#include "../boot/efi/efi.h"

// Map UEFI Input to Legacy Scancodes
// Up: 0x48, Down: 0x50, Enter: 0x1C
int uefi_get_scancode() {
    if (!g_SystemTable || !g_SystemTable->ConIn) return 0;
    
    EFI_INPUT_KEY key;
    EFI_STATUS status = g_SystemTable->ConIn->ReadKeyStroke(g_SystemTable->ConIn, &key);
    
    if (status == 0) { // EFI_SUCCESS
        // Prioritize ScanCode for arrows
        if (key.ScanCode == 0x01) return 0x48; // Up Arrow
        if (key.ScanCode == 0x02) return 0x50; // Down Arrow
        
        // Use UnicodeChar for Enter
        if (key.UnicodeChar == 0x0D) return 0x1C; // Enter
    }
    return 0;
}
#endif

void keyboard_handler_c(uint8_t scancode)
{
    if (atlas_opts.entries == 0) return;

    if (scancode == 0x48)
    { // up arrow
        if (atlas_opts.selected > 0)
            atlas_opts.selected--;
        draw_menu(atlas_opts);
    }
    else if (scancode == 0x50)
    { // down arrow
        if (atlas_opts.selected < atlas_opts.length - 1)
            atlas_opts.selected++;
        draw_menu(atlas_opts);
    }
    else if (scancode == 0x1C)
    { // enter
        vga_clear_screen(0x1F); // Blue screen
        vga_put_string("Loading kernel: ", 0x1F);
        vga_put_string(atlas_opts.entries[atlas_opts.selected].kernel_path, 0x1F);
        vga_put_string("...", 0x1F);

        void *load_addr = (void *)0x100000;
        int success = 0;
        uint64_t fb_base = 0xB8000; // Default to VGA text mode address for BIOS

#ifdef UEFI_BUILD
        // UEFI: Try to allocate at 2MB (0x200000) - a standard load address
        if (g_SystemTable && g_SystemTable->BootServices) {
            UINTN pAddr = 0x200000;
            // Allocate enough pages for kernel (256 pages = 1MB)
            EFI_STATUS status = g_SystemTable->BootServices->AllocatePages(
                AllocateAddress, 
                1,  // EfiLoaderCode
                256,
                &pAddr
            );
            
            if (status == 0) {
                load_addr = (void*)pAddr;
                success = (fat32_read_file(atlas_opts.entries[atlas_opts.selected].kernel_path, (void*)pAddr) == 0);
            } else {
                vga_put_string("\nUEFI Error: 0x200000 occupied. Cannot load kernel.", 0x1F);
                return;
            }
        }
#else
        // BIOS: Memory map assumed available at 0x100000
        success = (fat32_read_file(atlas_opts.entries[atlas_opts.selected].kernel_path, load_addr) == 0);
#endif

        if (!success)
        {
            vga_put_string("\nError: Could not load file!", 0x1F);
            return;
        }

        vga_put_string("\nExecuting...", 0x1F);
        
#ifdef UEFI_BUILD
        // Get Framebuffer Info via GOP
        EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
        EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
        
        EFI_STATUS gop_status = g_SystemTable->BootServices->LocateProtocol(&gop_guid, NULL, (void**)&gop);
        if (gop_status == 0 && gop && gop->Mode) {
            fb_base = gop->Mode->FrameBufferBase;
            
            // Store GOP info at a known location (0x1F0000) for kernel
            struct {
                uint64_t fb_base;
                uint32_t width;
                uint32_t height;
                uint32_t pitch;  // Pixels per scanline
            } *boot_info = (void*)0x1F0000;
            
            boot_info->fb_base = gop->Mode->FrameBufferBase;
            boot_info->width = gop->Mode->Info->HorizontalResolution;
            boot_info->height = gop->Mode->Info->VerticalResolution;
            boot_info->pitch = gop->Mode->Info->PixelsPerScanLine;
            
            // Debug: verify values before printing
            uint32_t actual_width = gop->Mode->Info->HorizontalResolution;
            uint32_t actual_height = gop->Mode->Info->VerticalResolution;
            
            vga_put_string("\n[UEFI] Framebuffer at 0x", 0x1F);
            
            // Print framebuffer address
            char hex[] = "0123456789ABCDEF";
            for(int k = 60; k >= 0; k -= 4) {
                if (k < 28 && ((fb_base >> k) == 0)) continue;
                char c[2] = { hex[(fb_base >> k) & 0xF], 0 };
                vga_put_string(c, 0x1F);
            }
            
            // Print resolution
            vga_put_string("\n[UEFI] Resolution: ", 0x1F);
            
            // Print width
            uint32_t w = boot_info->width;
            char w_buf[16];
            int w_len = 0;
            
            // Convert to string
            if (w == 0) {
                w_buf[w_len++] = '0';
            } else {
                uint32_t temp = w;
                int start = 0;
                while (temp > 0) {
                    w_buf[w_len++] = '0' + (temp % 10);
                    temp /= 10;
                }
                // Reverse
                for (int i = 0; i < w_len / 2; i++) {
                    char t = w_buf[i];
                    w_buf[i] = w_buf[w_len - 1 - i];
                    w_buf[w_len - 1 - i] = t;
                }
            }
            w_buf[w_len] = 0;
            vga_put_string(w_buf, 0x1F);
            
            vga_put_string(" x ", 0x1F);
            
            // Print height
            uint32_t h = boot_info->height;
            char h_buf[16];
            int h_len = 0;
            
            if (h == 0) {
                h_buf[h_len++] = '0';
            } else {
                uint32_t temp = h;
                while (temp > 0) {
                    h_buf[h_len++] = '0' + (temp % 10);
                    temp /= 10;
                }
                // Reverse
                for (int i = 0; i < h_len / 2; i++) {
                    char t = h_buf[i];
                    h_buf[i] = h_buf[h_len - 1 - i];
                    h_buf[h_len - 1 - i] = t;
                }
            }
            h_buf[h_len] = 0;
            vga_put_string(h_buf, 0x1F);
            
            vga_put_string(" (pitch: ", 0x1F);
            
            // Print pitch
            uint32_t p = boot_info->pitch;
            char p_buf[16];
            int p_len = 0;
            
            if (p == 0) {
                p_buf[p_len++] = '0';
            } else {
                uint32_t temp = p;
                while (temp > 0) {
                    p_buf[p_len++] = '0' + (temp % 10);
                    temp /= 10;
                }
                // Reverse
                for (int i = 0; i < p_len / 2; i++) {
                    char t = p_buf[i];
                    p_buf[i] = p_buf[p_len - 1 - i];
                    p_buf[p_len - 1 - i] = t;
                }
            }
            p_buf[p_len] = 0;
            vga_put_string(p_buf, 0x1F);
            vga_put_string(")", 0x1F);
        } else {
            vga_put_string("\n[UEFI] Warning: GOP not available, passing VGA address", 0x1F);
            fb_base = 0xB8000;
        }
        
        vga_put_string("\n[UEFI] Jumping to kernel (Boot Services still active)...", 0x1F);
        
        // Debug: Show where we're jumping
        vga_put_string("\n[UEFI] Entry point: 0x", 0x1F);
        char hex[] = "0123456789ABCDEF";
        uint64_t addr = (uint64_t)load_addr;
        for(int k = 60; k >= 0; k -= 4) {
            if (k < 28 && ((addr >> k) == 0)) continue;
            char c[2] = { hex[(addr >> k) & 0xF], 0 };
            vga_put_string(c, 0x1F);
        }
        
        // Debug: Show first 16 bytes at entry point
        vga_put_string("\n[UEFI] First bytes: ", 0x1F);
        unsigned char *bytes = (unsigned char*)load_addr;
        for (int i = 0; i < 16; i++) {
            char b[3] = { hex[bytes[i] >> 4], hex[bytes[i] & 0xF], ' ' };
            vga_put_string(b, 0x1F);
        }
        
        // Delay to see message
        for (volatile int i = 0; i < 50000000; i++);
        
        // Disable interrupts before jumping
        __asm__ volatile("cli");
        
        vga_put_string("\n[DEBUG] About to jump...", 0x1F);
        for (volatile int i = 0; i < 50000000; i++);  // Long delay
        
        // Jump to kernel with framebuffer address (using existing stack)
        void (*kernel_entry)(uint64_t) = (void (*)(uint64_t))load_addr;
        
        // Test: Can we call a simple function pointer?
        // Try calling with inline asm instead
        __asm__ volatile(
            "mov %0, %%rdi\n"        // First arg in RDI
            "call *%1\n"             // Call kernel
            "int3\n"                 // Breakpoint if we return
            :
            : "r"(fb_base), "r"((uint64_t)kernel_entry)
            : "rdi", "memory"
        );
        
        vga_put_string("\n[DEBUG] Returned from jump!", 0x1F);
        
        // If we get here, kernel returned (shouldn't happen)
        __asm__ volatile("sti"); // Re-enable for error display
        vga_put_string("\n[UEFI] Error: Kernel returned!", 0x1F);
        while(1) __asm__("hlt");
#else
        // BIOS: Disable interrupts and jump to kernel
        __asm__ volatile("cli");
        
        void (*kernel_entry)(uint64_t) = (void (*)(uint64_t))load_addr;
        kernel_entry(fb_base);
        
        // If kernel returns
        __asm__ volatile("sti");
        vga_put_string("\nError: Kernel returned!", 0x1F);
        while(1) __asm__("hlt");
#endif
    }
}