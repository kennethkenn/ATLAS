#include <stdint.h>

struct boot_info {
    uint64_t fb_base;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
};

__attribute__((noreturn))
void _start(uint64_t fb_base_arg) {
    // Read boot info
    struct boot_info *info = (struct boot_info*)0x1F0000;
    
    volatile uint32_t *fb = (volatile uint32_t*)info->fb_base;
    uint32_t width = info->width;
    uint32_t height = info->height;
    uint32_t pitch = info->pitch;
    
    // Fallback if boot info looks invalid
    if (width == 0 || width > 10000 || height == 0 || height > 10000 || pitch == 0) {
        fb = (volatile uint32_t*)fb_base_arg;
        width = 1280;
        height = 720;
        pitch = 1280;
    }
    
    // Clear screen to dark blue
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            fb[y * pitch + x] = 0x00001A3A;
        }
    }
    
    // Draw a colorful grid of rectangles that fits any resolution
    uint32_t colors[] = {
        0x00FF0000, // Red
        0x0000FF00, // Green
        0x000000FF, // Blue
        0x00FFFF00, // Yellow
        0x00FF00FF, // Magenta
        0x0000FFFF, // Cyan
        0x00FFFFFF, // White
        0x00FFA500  // Orange
    };
    
    int rect_size = (width < 800) ? 80 : 150;
    int spacing = 20;
    int color_idx = 0;
    
    for (uint32_t y = spacing; y < height - rect_size; y += rect_size + spacing) {
        for (uint32_t x = spacing; x < width - rect_size; x += rect_size + spacing) {
            uint32_t color = colors[color_idx % 8];
            
            // Draw filled rectangle
            for (uint32_t dy = 0; dy < (uint32_t)rect_size; dy++) {
                for (uint32_t dx = 0; dx < (uint32_t)rect_size; dx++) {
                    if (y + dy < height && x + dx < width) {
                        fb[(y + dy) * pitch + (x + dx)] = color;
                    }
                }
            }
            
            color_idx++;
        }
    }
    
    // Animate: bouncing square at bottom
    int x = 0;
    int square_size = 50;
    int y_pos = (height > square_size) ? height - square_size - 10 : 0;
    int direction = 1;
    
    while(1) {
        // Clear previous position
        for (int dy = 0; dy < square_size; dy++) {
            for (int dx = 0; dx < square_size; dx++) {
                if ((uint32_t)(y_pos + dy) < height && (uint32_t)(x + dx) < width) {
                    fb[(y_pos + dy) * pitch + (x + dx)] = 0x00001A3A;
                }
            }
        }
        
        // Move
        x += direction * 10;
        if (x >= (int)width - square_size) direction = -1;
        if (x <= 0) direction = 1;
        
        // Draw at new position
        for (int dy = 0; dy < square_size; dy++) {
            for (int dx = 0; dx < square_size; dx++) {
                if ((uint32_t)(y_pos + dy) < height && (uint32_t)(x + dx) < width) {
                    fb[(y_pos + dy) * pitch + (x + dx)] = 0x00FFFFFF;
                }
            }
        }
        
        // Delay
        for (volatile int i = 0; i < 2000000; i++);
    }
    
    __builtin_unreachable();
}