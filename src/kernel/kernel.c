#include "vga.h"
#include "mem.h"
#include "fat32.h"
#include "disk.h"
#include "keyboard.h"

#define MAX_OPTIONS 16

struct menu atlas_opts;

// Helper to compare strings without library functions
static int kstrcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

// Helper to check if a line starts with a string
static int kstarts_with(const char *pref, const char *str)
{
    while (*pref)
    {
        if (*pref++ != *str++)
            return 0;
    }
    return 1;
}

void kmain(char *config_addr, struct fat32_bpb *bpb)
{
    vga_init();
    kheap_init();
    fat32_init(bpb);

    vga_clear_screen(VGA_DEFAULT_ATTR);
    draw_box(0, 0, g_vga_width, g_vga_height, VGA_DEFAULT_ATTR);

    char *title = "The Atlas Bootloader";
    struct menu_entry *entries = kmalloc(sizeof(struct menu_entry) * MAX_OPTIONS);
    if (!entries) {
        // Heap exhausted - use minimal fallback
        vga_put_string("Error: Out of memory!", VGA_DEFAULT_ATTR);
        for (;;);
    }
    int entry_count = 0;

    if (config_addr != 0)
    {
        char *line = config_addr;
        while (*line)
        {
            // Skip leading whitespace/newlines
            while (*line && (*line == ' ' || *line == '\n' || *line == '\r' || *line == '\t'))
                line++;
            
            if (!*line) break;

            char *line_start = line;
            while (*line && *line != '\n' && *line != '\r')
                line++;
            
            char save = *line;
            *line = '\0';

            // Clean trailing spaces in the line
            char *end = line - 1;
            while (end > line_start && *end == ' ') {
                *end = '\0';
                end--;
            }

            if (kstarts_with("[menu]", line_start))
            {
                // Title parsing logic
            }
            else if (kstarts_with("[entry]", line_start))
            {
                if (entry_count < MAX_OPTIONS)
                {
                    entries[entry_count].name = "Unknown Entry";
                    entries[entry_count].kernel_path = "";
                    entry_count++;
                }
            }
            else if (kstarts_with("name=", line_start))
            {
                if (entry_count > 0)
                {
                    char *val = line_start + 5;
                    int len = 0;
                    while(val[len]) len++;
                    char *name = kmalloc(len + 1);
                    for (int i = 0; i < len; i++) name[i] = val[i];
                    name[len] = '\0';
                    entries[entry_count - 1].name = name;
                }
            }
            else if (kstarts_with("title=", line_start))
            {
                char *val = line_start + 6;
                int len = 0;
                while(val[len]) len++;
                title = kmalloc(len + 1);
                for (int i = 0; i < len; i++) title[i] = val[i];
                title[len] = '\0';
            }
            else
            {
                int is_kernel = 0;
                char *val = 0;

                // Priority Check: Arch-specific key
#ifdef UEFI_BUILD
                if (kstarts_with("kernel_x64=", line_start)) {
                    is_kernel = 1;
                    val = line_start + 11;
                }
#else
                if (kstarts_with("kernel_x86=", line_start)) {
                    is_kernel = 1;
                    val = line_start + 11;
                }
#endif

                // Fallback: Generic key
                if (!is_kernel && kstarts_with("kernel=", line_start)) {
                    is_kernel = 1;
                    val = line_start + 7;
                }

                if (is_kernel && entry_count > 0)
                {
                    int len = 0;
                    while(val[len]) len++;
                    char *path = kmalloc(len + 1);
                    for (int i = 0; i < len; i++) path[i] = val[i];
                    path[len] = '\0';
                    entries[entry_count - 1].kernel_path = path;
                }
            }

            *line = save;
            if (*line) line++;
        }
    }

    if (entry_count > 0)
    {
        // Filter out entries with no valid kernel path for this architecture
        int valid_count = 0;
        for (int i = 0; i < entry_count; i++) {
            if (entries[i].kernel_path && entries[i].kernel_path[0] != '\0') {
                // Keep this entry
                if (valid_count != i) {
                    entries[valid_count] = entries[i];
                }
                valid_count++;
            }
        }
        entry_count = valid_count;
    }

    if (entry_count == 0)
    {
        entries[0].name = "No valid entries for this mode";
        entries[0].kernel_path = "";
        entry_count = 1;
    }

    atlas_opts.entries = entries;
    atlas_opts.length = entry_count;
    atlas_opts.selected = 0;
    atlas_opts.title = title;

    draw_menu(atlas_opts);

    draw_menu(atlas_opts);

#ifdef UEFI_BUILD
    // UEFI Polling Loop
    while (1) {
        int scancode = uefi_get_scancode();
        if (scancode != 0) {
            keyboard_handler_c((uint8_t)scancode);
        }
        // Optional: Stall to prevent 100% CPU usage if desired, but not strictly necessary for bootloader
        // g_SystemTable->BootServices->Stall(50000); // 50ms
    }
#else
    // Legacy BIOS Interrupt Wait
    __asm__ __volatile__ ("sti");

    for (;;)
        ;
#endif
}
