SECTIONS {
    . =  0xc0001500;
    .text :
    {
        build/main.o    
        *(.text)
    }
    .rodata ALIGN(4):{*(.rodata*)}
    .data ALIGN(4):{*(.data)}
    . = ALIGN(4);
    __bss_start = . ;
    .bss ALIGN(4):{*(.bss)*(COMMON)}
    . = ALIGN(4);
    __bss_end = . ; 
}
