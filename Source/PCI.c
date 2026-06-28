#include <stdint.h>
#include "Include/PCI.h"
#include "Include/Libraries.h"

void PCI_InitialiseDevices(uint64_t ECAMBaseAddress) {
    // Scan PCI for NVMe SSD

    for (int Bus = 0; Bus < 256; Bus++) {
        for (int Device = 0; Device < 32; Device++) {
            for (int Function = 0; Function < 8; Function++) {
                
                uint32_t VendorRegister = PCI_Read32Bit(ECAMBaseAddress, Bus, Device, Function, 0x00);
                if ((uint16_t) (VendorRegister & 0xFFFF) == 0xFFFF) continue;

                uint32_t ClassRegister = PCI_Read32Bit(ECAMBaseAddress, Bus, Device, Function, 0x08);
                uint8_t ClassCode = (ClassRegister >> 24) & 0xFF;
                uint8_t SubClass = (ClassRegister >> 16) & 0xFF;
                uint8_t ProgIF = (ClassRegister >> 8) & 0xFF;

                if (ClassCode == CLASS_CODE_FOR_MASS_STORAGE_CONTROLLER && SubClass == SUBCLASS_FOR_NVME) {
                    printf("Found NVMe controller at %d:%d:%d, Vendor: 0x%x, Class: 0x%x:0x%x\n", Bus, Device, Function, VendorRegister & 0xFFFF, ClassCode, SubClass);
                    break;
                }
            }
        }
    }
}

uint32_t PCI_Read32Bit(uint64_t ECAMBaseAddress, int Bus, int Device, int Function, int Offset) {
    uint64_t Raw = PCI_GET_ADDRESS(ECAMBaseAddress, Bus, Device, Function, Offset);
    volatile uint32_t* Address = (volatile uint32_t*) (uintptr_t) Raw;
    return *Address;
}
