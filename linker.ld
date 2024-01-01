OUTPUT_FORMAT("elf32-i386")
ENTRY(_start)

SECTIONS {
    . = 0x7e00;

    /* Code (executable, read-only) */
    .text : {
        *(.text)
    }

    /* Read-only data */
    .rodata : {
        *(.rodata)
    }

    /* Initialized data */
    .data : {
        *(.data)
    }

    /* Uninitialized data (BSS) */
    .bss : {
        *(.bss COMMON)
    }
}
