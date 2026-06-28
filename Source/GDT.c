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
        "lgdt %0\n"
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "mov %%ax, %%ss\n"
        "pushq $0x08\n"
        "leaq 1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "lretq\n"
        "1:\n"
        :: "m"(GDT_Descriptor)
        : "rax", "memory"
    );
}
