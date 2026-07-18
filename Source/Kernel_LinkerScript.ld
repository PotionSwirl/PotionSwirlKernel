ENTRY(kmain)

SECTIONS {

    . = 0x100000;

    .text : {
        *(.text*)
        text_end = .;
    }

    .rodata : {
        *(.rodata*)
        rodata_end = .;
    }

    .data : {
        *(.data*)
        data_end = .;
    }

    .bss : {
        *(.bss*)
        bss_end = .;
    }

    kernel_end = .;

}
