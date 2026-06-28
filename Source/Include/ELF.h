#pragma once

#include <stdint.h>

// Indicates that an ELF program segment is loadable in a program header
#define ELF_SEGMENT_IS_LOADABLE 1

typedef struct {
    unsigned char ELF_Identification[16];
    uint16_t ELF_Type;
    uint16_t ELF_Machine;
    uint32_t ELF_Version;
    uint64_t ELF_ProgramEntry;
    uint64_t ELF_ProgramHeadersOffset;
    uint64_t ELF_SectionHeadersOffset;
    uint32_t ELF_Flags;
    uint16_t ELF_HeaderSize;
    uint16_t ELF_ProgramHeaderEntrySize;
    uint16_t ELF_ProgramHeaderNumber;
    uint16_t ELF_SectionHeaderEntrySize;
    uint16_t ELF_SectionHeaderNumber;
    uint16_t ELF_SectionHeaderStringTableIndex;
} elf64_elf_header_t;



// ======================== 📦 STRUCT =============================
/* _________________________________________________________________
   | TYPE DEFINITION STRUCT: elf64_program_header_t                 |
   |________________________________________________________________|
   | DESCRIPTION  | Contains ELF program information.               |       
   |______________|_________________________________________________| */

typedef struct {
    uint32_t Program_HeaderType;
    uint32_t Program_HeaderFlags;
    uint64_t Program_HeaderOffset;
    uint64_t Program_HeaderVirtualAddress;
    uint64_t Program_HeaderPhysicalAddress;
    uint64_t Program_HeaderFileSize;
    uint64_t Program_HeaderMemorySize;
    uint64_t Program_HeaderAlign;
} elf64_program_header_t;
