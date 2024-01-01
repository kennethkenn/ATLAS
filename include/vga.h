// vga.h
#ifndef VGA_DRIVER_H
#define VGA_DRIVER_H

// Screen dimensions (Dynamic)
extern int g_vga_width;
extern int g_vga_height;

// Legacy constants for BIOS buffer
#define LEGACY_WIDTH 80
#define LEGACY_HEIGHT 25

// VGA attributes (default: light gray on black)
#define VGA_DEFAULT_ATTR 0x07

typedef struct
{
    volatile char c;
    volatile char attr;
} __attribute__((packed)) vga_cell_t;

struct menu_entry
{
    char *name;
    char *kernel_path;
};

struct menu
{
    short selected;
    short length;
    struct menu_entry *entries;
    char *title;
};

void vga_clear_screen(char attr);
void vga_put_char(char c, char attr, int row, int col);
void vga_put_string(const char *str, char attr);
void vga_init(void);
void draw_menu(struct menu menu_opt);
void draw_box(int row, int col, int w, int h, char attr);

#endif // VGA_DRIVER_H