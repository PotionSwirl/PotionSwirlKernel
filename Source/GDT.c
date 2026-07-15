#include <stdint.h>
#include "Include/GDT.h"

uint64_t GDT[] = {

    // Null descriptor
    0x0000000000000000,

    // Kernel code segment
    0x00AF9A000000FFFF,

    // Kernel data segment
    0x00AF92000000FFFF
};

GDT_Descriptor_t GDT_Descriptor;

void GDT_Initialise() {
    GDT_Descriptor.Limit = sizeof(GDT) - 1;
    GDT_Descriptor.Base = (uint64_t) &GDT;
    
    asm volatile (
        "LGDT %0\n\t"
        "MOV AX, 0x10\n\t"
        "MOV DS, AX\n\t"
        "MOV ES, AX\n\t"
        "MOV FS, AX\n\t"
        "MOV GS, AX\n\t"
        "MOV SS, AX\n\t"
        "PUSH 0x08\n\t"
        "LEA RAX, [RIP + 1f]\n\t"
        "PUSH RAX\n\t"
        "LRETQ\n"
        "1:\n\t"
        :: "m"(GDT_Descriptor)
        : "rax", "memory" // C clobber list constraints must remain lowercase
    );
}
