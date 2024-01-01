// Simple Memory Test Utility
// Targeted for Atlas OS Standalone Execution

void _start()
{
    volatile char *vga = (volatile char *)0xB8000;
    const char *msg_start = "Memtest starting...";
    const char *msg_pass = "Memtest passed!";
    const char *msg_fail = "Memtest FAILED!";

    // Clear screen first (simple way)
    for (int i = 0; i < 80 * 25 * 2; i++)
        vga[i] = 0;

    // Show starting message
    for (int i = 0; msg_start[i]; i++)
    {
        vga[i * 2] = msg_start[i];
        vga[i * 2 + 1] = 0x07;
    }

    // Test a 1MB range at 2MB mark
    unsigned int *ptr = (unsigned int *)0x200000;
    int count = 1024 * 1024 / 4; // 1MB / 4 bytes
    int failed = 0;

    // Pattern 1: 0xAAAAAAAA
    for (int i = 0; i < count; i++)
        ptr[i] = 0xAAAAAAAA;
    for (int i = 0; i < count; i++)
    {
        if (ptr[i] != 0xAAAAAAAA)
        {
            failed = 1;
            break;
        }
    }

    if (!failed)
    {
        // Pattern 2: 0x55555555
        for (int i = 0; i < count; i++)
            ptr[i] = 0x55555555;
        for (int i = 0; i < count; i++)
        {
            if (ptr[i] != 0x55555555)
            {
                failed = 1;
                break;
            }
        }
    }

    // Report Result
    const char *res = failed ? msg_fail : msg_pass;
    char color = failed ? 0x4F : 0x2F; // Red vs Green
    for (int i = 0; res[i]; i++)
    {
        vga[160 + i * 2] = res[i];
        vga[161 + i * 2] = color;
    }

    while (1)
        ;
}
