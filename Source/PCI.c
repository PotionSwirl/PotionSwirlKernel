#include <stdint.h>
#include "Include/PCI.h"
#include "Include/Libraries.h"

static volatile uint8_t* NVMe;

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
                // uint8_t ProgIF = (ClassRegister >> 8) & 0xFF;

                if (ClassCode == CLASS_CODE_FOR_MASS_STORAGE_CONTROLLER && SubClass == SUBCLASS_FOR_NVME) {
                    uint32_t Bar0Low = PCI_Read32Bit(ECAMBaseAddress, Bus, Device, Function, 0x10);
                    uint32_t Bar0High = PCI_Read32Bit(ECAMBaseAddress, Bus, Device, Function, 0x14);
                    // map this????
                    uint64_t NVMeBase = ((uint64_t) Bar0High << 32) | (Bar0Low & ~0xFULL);  // Not 'full' lol, but 0xF as an unsigned long long integer
                    NVMe = (volatile uint8_t*) NVMeBase;
                    // CAP at offset 0 from NVMe base
                    uint64_t CAP = *(volatile uint64_t*) (NVMe + 0x00);
                    uint32_t CSTS = *(volatile uint32_t*) (NVMe + 0x1C);
                    uint32_t VS = *(volatile uint32_t*) (NVMe + 0x08);
                    printf("----- PCI -----\n");
                    printf("NVMe CAP: 0x%x\n", CAP);
                    printf("NVMe Controller Status: 0x%x\n", CSTS);
                    printf("NVme Version: 0x%x\n", VS);
                    printf("---------------------------------------\n");
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
