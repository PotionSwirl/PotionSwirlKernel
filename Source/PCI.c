#include <stdint.h>
#include "Include/PCI.h"
#include "Include/Libraries.h"
#include "Include/Paging.h"
#include "Include/NVMe.h"


static volatile uint8_t* NVMeBAR0;


void PCI_InitialiseDevices(uint64_t ECAMBaseAddress) {

    // Scan the PCI for the devices we'll be using

    for (int Bus = 0; Bus < 256; Bus++) {

        for (int Device = 0; Device < 32; Device++) {

            for (int Function = 0; Function < 8; Function++) {

                uint32_t VendorRegister = PCI_Read32Bit(ECAMBaseAddress, Bus, Device, Function, 0x00);
                if ((uint16_t) (VendorRegister & 0xFFFF) == 0xFFFF) continue;
                uint32_t ClassRegister = PCI_Read32Bit(ECAMBaseAddress, Bus, Device, Function, 0x08);
                uint8_t ClassCode = (ClassRegister >> 24) & 0xFF;
                uint8_t SubClass = (ClassRegister >> 16) & 0xFF;

                if (ClassCode == CLASS_CODE_FOR_MASS_STORAGE_CONTROLLER && SubClass == SUBCLASS_FOR_NVME) {
                    uint32_t BAR0Low = PCI_Read32Bit(ECAMBaseAddress, Bus, Device, Function, 0x10);
                    uint32_t BAR0High = PCI_Read32Bit(ECAMBaseAddress, Bus, Device, Function, 0x14);
                    NVMeBAR0 = (volatile uint8_t*) (((uint64_t) BAR0High << 32) | (BAR0Low & ~0xFULL));
                    void* PhysicalAddress = (void*) NVMeBAR0;
                    Paging_MapMemory(PhysicalAddress, PhysicalAddress, PAGING_PAGE_PRESENT | PAGING_PAGE_WRITABLE | PAGING_PAGE_WRITE_THROUGH | PAGING_PAGE_CACHE_DISABLE | PAGING_PAGE_NO_EXECUTE, 2);
                    NVMe_Initialise(NVMeBAR0);
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
