#pragma once

#include <stdint.h>

#define CLASS_CODE_FOR_MASS_STORAGE_CONTROLLER 0x01
#define SUBCLASS_FOR_NVME 0x08

#define PCI_GET_ADDRESS(ECAMBaseAddress, Bus, Device, Function, Offset) \
    (ECAMBaseAddress + \
    ((Bus & 0xFF)  << 20) + \
    ((Device & 0x1F)  << 15) + \
    ((Function & 0x07)  << 12) + \
    (Offset & 0xFFF))

void PCI_InitialiseDevices(uint64_t ECAMBaseAddress);
uint32_t PCI_Read32Bit(uint64_t ECAMBaseAddress, int Bus, int Device, int Function, int Offset);
