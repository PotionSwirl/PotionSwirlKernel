/* Create a BootInfo struct which we can seamlessly add to and pass to the kernel
   after exiting UEFI boot services. */

#pragma once

#include <stdint.h>
#include <efi.h>
#include "PSF2Font.h"
#include "ACPI.h"


typedef struct __attribute__((packed)) {
    EFI_MEMORY_DESCRIPTOR* MemoryMap;
    uint64_t MemoryMapSize;
    uint64_t MemoryMapKey;
    uint64_t DescriptorSize;
    uint32_t DescriptorVersion;
} MemoryInfo_t;


typedef struct __attribute__((packed)) {
    EFI_GRAPHICS_OUTPUT_PROTOCOL* GraphicsOutputProtocol;
    ACPI_RSDP2_t* RSDP;
    MemoryInfo_t MemoryInfo;
    PSF2Font_FileHeader* FontBuffer;
} BootInfo_t;
