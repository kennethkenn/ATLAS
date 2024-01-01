#include <stdint.h>

struct boot_info {
    uint64_t fb_base;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
};

// Memory test patterns
#define PATTERN_1 0xAAAAAAAAAAAAAAAAUL
#define PATTERN_2 0x5555555555555555UL
#define PATTERN_3 0xDEADBEEFCAFEBABEUL
#define PATTERN_4 0x0123456789ABCDEFUL

// Colors
#define COLOR_BG      0x00001020
#define COLOR_TITLE   0x00003060
#define COLOR_GREEN   0x0000FF00
#define COLOR_RED     0x00FF0000
#define COLOR_YELLOW  0x00FFFF00
#define COLOR_CYAN    0x0000FFFF
#define COLOR_WHITE   0x00FFFFFF

// Draw a filled rectangle
static void draw_rect(uint32_t *fb, uint32_t pitch, int x, int y, int w, int h, uint32_t color) {
    for (int dy = 0; dy < h; dy++) {
        for (int dx = 0; dx < w; dx++) {
            fb[(y + dy) * pitch + (x + dx)] = color;
        }
    }
}

// Test a memory region with a pattern
static int test_pattern(volatile uint64_t *addr, uint64_t pattern) {
    *addr = pattern;
    return (*addr == pattern) ? 1 : 0;
}

// Test a region with all 4 patterns
static int test_region(volatile uint64_t *addr) {
    int passed = 0;
    if (test_pattern(addr, PATTERN_1)) passed++;
    if (test_pattern(addr, PATTERN_2)) passed++;
    if (test_pattern(addr, PATTERN_3)) passed++;
    if (test_pattern(addr, PATTERN_4)) passed++;
    return passed;
}

// Draw a simple digit (0-4) - 5x7 bitmap
static void draw_digit(uint32_t *fb, uint32_t pitch, int x, int y, int digit, uint32_t color) {
    // Simple 5x7 font for digits 0-4
    const uint8_t font[5][7] = {
        // 0
        {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},
        // 1
        {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},
        // 2
        {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F},
        // 3
        {0x0E, 0x11, 0x01, 0x0E, 0x01, 0x11, 0x0E},
        // 4
        {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}
    };
    
    if (digit < 0 || digit > 4) return;
    
    for (int row = 0; row < 7; row++) {
        for (int col = 0; col < 5; col++) {
            if (font[digit][row] & (1 << (4 - col))) {
                fb[(y + row * 2) * pitch + (x + col * 2)] = color;
                fb[(y + row * 2) * pitch + (x + col * 2 + 1)] = color;
                fb[(y + row * 2 + 1) * pitch + (x + col * 2)] = color;
                fb[(y + row * 2 + 1) * pitch + (x + col * 2 + 1)] = color;
            }
        }
    }
}

__attribute__((noreturn))
void _start(uint64_t fb_base_arg) {
    // Read boot info
    struct boot_info *info = (struct boot_info*)0x1F0000;
    
    volatile uint32_t *fb = (volatile uint32_t*)info->fb_base;
    uint32_t width = info->width;
    uint32_t height = info->height;
    uint32_t pitch = info->pitch;
    
    // Fallback
    if (width == 0 || width > 10000 || height == 0 || height > 10000 || pitch == 0) {
        fb = (volatile uint32_t*)fb_base_arg;
        width = 1280;
        height = 800;
        pitch = 1280;
    }
    
    // Clear screen
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            fb[y * pitch + x] = COLOR_BG;
        }
    }
    
    // Title bar
    draw_rect((uint32_t*)fb, pitch, 0, 0, width, 50, COLOR_TITLE);
    
    // Test memory regions
    int test_y = 80;
    int bar_height = 60;
    int spacing = 20;
    int total_passed = 0;
    
    // Test addresses
    uint64_t test_addrs[] = {0x200000, 0x400000, 0x800000, 0x1000000};
    
    for (int i = 0; i < 4; i++) {
        volatile uint64_t *mem = (volatile uint64_t *)test_addrs[i];
        int passed = test_region(mem);
        
        // Draw bar background
        draw_rect((uint32_t*)fb, pitch, 50, test_y, 400, bar_height, 0x00002040);
        
        // Draw progress bar (green = pass, red = fail)
        int bar_width = (400 * passed) / 4;
        uint32_t bar_color = (passed == 4) ? COLOR_GREEN : COLOR_RED;
        draw_rect((uint32_t*)fb, pitch, 50, test_y, bar_width, bar_height, bar_color);
        
        // Draw score as digit
        draw_digit((uint32_t*)fb, pitch, 470, test_y + 20, passed, COLOR_WHITE);
        
        if (passed == 4) total_passed++;
        test_y += bar_height + spacing;
    }
    
    // Summary box
    int summary_x = width - 300;
    int summary_y = 80;
    draw_rect((uint32_t*)fb, pitch, summary_x, summary_y, 250, 250, 0x00003050);
    
    // Draw border
    draw_rect((uint32_t*)fb, pitch, summary_x, summary_y, 250, 3, COLOR_CYAN);
    draw_rect((uint32_t*)fb, pitch, summary_x, summary_y + 247, 250, 3, COLOR_CYAN);
    draw_rect((uint32_t*)fb, pitch, summary_x, summary_y, 3, 250, COLOR_CYAN);
    draw_rect((uint32_t*)fb, pitch, summary_x + 247, summary_y, 3, 250, COLOR_CYAN);
    
    // Draw summary indicators (circles made of rectangles)
    for (int i = 0; i < 4; i++) {
        int cx = summary_x + 60 + (i % 2) * 100;
        int cy = summary_y + 60 + (i / 2) * 100;
        uint32_t circle_color = (i < total_passed) ? COLOR_GREEN : COLOR_RED;
        
        // Draw a filled circle approximation
        for (int r = 0; r < 25; r++) {
            for (int angle = 0; angle < 360; angle += 10) {
                int dx = (r * 866) / 1000;  // cos approximation
                int dy = (r * 500) / 1000;  // sin approximation
                if (angle > 90 && angle < 270) dx = -dx;
                if (angle > 180) dy = -dy;
                
                int px = cx + dx;
                int py = cy + dy;
                if (px >= 0 && px < (int)width && py >= 0 && py < (int)height) {
                    fb[py * pitch + px] = circle_color;
                }
            }
        }
    }
    
    // Display pass count
    int digit_x = summary_x + 100;
    int digit_y = summary_y + 200;
    draw_digit((uint32_t*)fb, pitch, digit_x, digit_y, total_passed, 
               (total_passed == 4) ? COLOR_GREEN : COLOR_YELLOW);
    
    // Animate - pulsing border on summary box
    int pulse = 0;
    int pulse_dir = 1;
    
    while(1) {
        // Pulse border color
        uint32_t pulse_color = COLOR_CYAN;
        if (pulse > 50) {
            pulse_color = COLOR_WHITE;
        }
        
        draw_rect((uint32_t*)fb, pitch, summary_x, summary_y, 250, 3, pulse_color);
        draw_rect((uint32_t*)fb, pitch, summary_x, summary_y + 247, 250, 3, pulse_color);
        draw_rect((uint32_t*)fb, pitch, summary_x, summary_y, 3, 250, pulse_color);
        draw_rect((uint32_t*)fb, pitch, summary_x + 247, summary_y, 3, 250, pulse_color);
        
        pulse += pulse_dir;
        if (pulse >= 100) pulse_dir = -1;
        if (pulse <= 0) pulse_dir = 1;
        
        // Delay
        for (volatile int i = 0; i < 1000000; i++);
    }
    
    __builtin_unreachable();
}