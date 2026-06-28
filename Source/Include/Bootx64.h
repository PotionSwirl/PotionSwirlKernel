/* Create a BootInfo struct which we can seamlessly add to and pass to the kernel
   after exiting UEFI boot services. */

#pragma once

#include <stdint.h>
#include <efi.h>
#include "PSF2Font.h"

typedef struct __attribute__((packed)) {
    EFI_GRAPHICS_OUTPUT_PROTOCOL* GraphicsOutputProtocol;
    uint32_t IOAPICAddress;
    uint64_t ECAMBaseAddress;
    PSF2Font_FileHeader* FontBuffer;
} BootInfo_t;
