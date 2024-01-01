// vga.c
#include "vga.h"

#ifdef UEFI_BUILD
#include "../boot/efi/efi.h"
#else
#include "port.h"
#endif

#ifndef VGA_DRIVER_C
#define VGA_DRIVER_C

// VGA text mode base address (Legacy only)
#define VGA_BUFFER ((volatile vga_cell_t *)0xB8000)

// Screen dimensions
int g_vga_width = LEGACY_WIDTH;
int g_vga_height = LEGACY_HEIGHT;

// Cursor position
static int vga_cursor_row = 0;
static int vga_cursor_col = 0;

void vga_init(void)
{
    vga_cursor_row = 0;
    vga_cursor_col = 0;
    
#ifdef UEFI_BUILD
    // UEFI handles cursor disabling via protocol if needed
    if (g_SystemTable && g_SystemTable->ConOut) {
        g_SystemTable->ConOut->EnableCursor(g_SystemTable->ConOut, 0); // Hide cursor
        
        // Update dimensions from UEFI Console Mode
        UINTN cols, rows;
        // QueryMode(This, ModeNumber, &Columns, &Rows)
        if (g_SystemTable->ConOut->Mode) {
             g_SystemTable->ConOut->QueryMode(g_SystemTable->ConOut, g_SystemTable->ConOut->Mode->Mode, &cols, &rows);
             g_vga_width = (int)cols;
             g_vga_height = (int)rows;
        }
    }
#else
    // Default BIOS
    g_vga_width = LEGACY_WIDTH;
    g_vga_height = LEGACY_HEIGHT;

    // disable cursor via ports
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x20);
#endif
}

void vga_clear_screen(char attr)
{
#ifdef UEFI_BUILD
    if (g_SystemTable && g_SystemTable->ConOut) {
        g_SystemTable->ConOut->SetAttribute(g_SystemTable->ConOut, attr);
        g_SystemTable->ConOut->ClearScreen(g_SystemTable->ConOut);
    }
#else
    volatile vga_cell_t *vga = VGA_BUFFER;
    for (int i = 0; i < LEGACY_WIDTH * LEGACY_HEIGHT; i++)
    {
        vga[i].c = ' ';
        vga[i].attr = attr;
    }
#endif
    vga_cursor_row = 0;
    vga_cursor_col = 0;
}

void vga_put_char(char c, char attr, int row, int col)
{
    if (row < 0 || row >= g_vga_height || col < 0 || col >= g_vga_width)
        return; // Out of bounds

#ifdef UEFI_BUILD
    if (g_SystemTable && g_SystemTable->ConOut) {
        g_SystemTable->ConOut->SetCursorPosition(g_SystemTable->ConOut, col, row);
        g_SystemTable->ConOut->SetAttribute(g_SystemTable->ConOut, attr);
        
        // Map CP437 box drawing chars to Unicode
        short str[2] = {0, 0};
        switch ((unsigned char)c) {
            case 0xC9: str[0] = 0x2554; break; // ╔
            case 0xBB: str[0] = 0x2557; break; // ╗
            case 0xC8: str[0] = 0x255A; break; // ╚
            case 0xBC: str[0] = 0x255D; break; // ╝
            case 0xCD: str[0] = 0x2550; break; // ═
            case 0xBA: str[0] = 0x2551; break; // ║
            default:   str[0] = (unsigned char)c; break;
        }
        
        g_SystemTable->ConOut->OutputString(g_SystemTable->ConOut, str);
    }
#else
    volatile vga_cell_t *vga = VGA_BUFFER + (row * LEGACY_WIDTH + col);
    vga->c = c;
    vga->attr = attr;
#endif

    // Update cursor position
    vga_cursor_col = col + 1;
    if (vga_cursor_col >= g_vga_width)
    {
        vga_cursor_col = 0;
        vga_cursor_row = (vga_cursor_row + 1) % g_vga_height;
    }
}

void vga_put_string(const char *str, char attr)
{
    while (*str)
    {
        vga_put_char(*str++, attr, vga_cursor_row, vga_cursor_col);
    }
}

void putc_at(int row, int col, char ch, char attr)
{
    vga_put_char(ch, attr, row, col);
}

void draw_box(int row, int col, int w, int h, char attr)
{
    // corners
    putc_at(row, col, 0xC9, attr);                 // ╔
    putc_at(row, col + w - 1, 0xBB, attr);         // ╗
    putc_at(row + h - 1, col, 0xC8, attr);         // ╚
    putc_at(row + h - 1, col + w - 1, 0xBC, attr); // ╝

    // top and bottom edges
    for (int x = col + 1; x < col + w - 1; x++)
    {
        putc_at(row, x, 0xCD, attr);         // ═
        putc_at(row + h - 1, x, 0xCD, attr); // ═
    }

    // sides
    for (int y = row + 1; y < row + h - 1; y++)
    {
        putc_at(y, col, 0xBA, attr);         // ║
        putc_at(y, col + w - 1, 0xBA, attr); // ║
    }
}

void draw_menu(struct menu menu_opt)
{
    char attr = 0x0F; // white on black
    
    // Simple centering: Assume menu width ~40 chars for visual balance
    int menu_width = 40;
    int center_col = (g_vga_width - menu_width) / 2;
    if (center_col < 0) center_col = 0;

    // menu title
    const char *title = menu_opt.title;
    
    // Draw separator line
    for (int i = 0; i < menu_width; i++)
    {
        putc_at(6, center_col + i, 0xCD, attr);
    }

    // Draw Title (centered within the menu width)
    int title_len = 0;
    while(title[title_len]) title_len++;
    int title_start = center_col + (menu_width - title_len) / 2;
    
    for (int i = 0; title[i]; i++)
    {
        putc_at(5, title_start + i, title[i], attr);
    }

    // menu options
    for (int o = 0; o < menu_opt.length; o++)
    {
        const char *s = menu_opt.entries[o].name;
        for (int i = 0; s[i]; i++)
        {
            char color = (o == menu_opt.selected) ? 0x7F : attr;
            putc_at(8 + o, center_col + i, s[i], color);
        }
    }
}

#endif // VGA_DRIVER_C