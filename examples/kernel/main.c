// Simple Atlas Kernel Example
// Targeted for Atlas OS Standalone Execution

void _start()
{
    volatile char *vga = (volatile char *)0xB8000;
    const char *msg = "Atlas Kernel Loaded Successfully!";

    // Clear screen
    for (int i = 0; i < 80 * 25 * 2; i++)
        vga[i] = 0;

    // Print message
    for (int i = 0; msg[i]; i++)
    {
        vga[i * 2] = msg[i];
        vga[i * 2 + 1] = 0x0F;
    }

    while (1)
        ;
}
