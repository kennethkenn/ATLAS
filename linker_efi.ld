OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(efi_main)

SECTIONS {
    . = 0;
    ImageBase = .;
    
    .text : {
        *(.text .text.*)
    }
    
    .reloc : {
        *(.reloc)
    }
    
    .data : {
        *(.data .data.*)
        *(.rodata .rodata.*)
        *(.sdata)
    }
    
    .dynamic : {
        *(.dynamic)
    }
    
    .dynsym : {
        *(.dynsym)
    }
    
    .rel : {
        *(.rel .rel.*)
    }
    
    .rela : {
        *(.rela .rela.*)
    }
    
    .bss : {
        *(.bss .bss.*)
        *(COMMON)
    }
}
