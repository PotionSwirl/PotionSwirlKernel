#include <stdint.h>
#include "Include/PCI.h"
#include "Include/Libraries.h"
#include "Include/Paging.h"
#include "Include/NVMe.h"
#include "Include/IHDA.h"


// Remember to accommodate VMD!


static volatile uint8_t* NVMeBAR0;
static volatile uint8_t* IHDABAR0;


void PCI_InitialiseDevices(uint64_t ECAMBaseAddress) {

    // Scan the PCI for the devices we'll be using
    // These devices are only initialised if they exist

    for (int Bus = 0; Bus < 256; Bus++) {

        for (int Device = 0; Device < 32; Device++) {

            for (int Function = 0; Function < 8; Function++) {

                uint32_t VendorRegister = PCI_SetupRead32Bit(ECAMBaseAddress, Bus, Device, Function, 0x00);
                if ((uint16_t) (VendorRegister & 0xFFFF) == 0xFFFF) continue;
                uint32_t ClassRegister = PCI_SetupRead32Bit(ECAMBaseAddress, Bus, Device, Function, 0x08);
                uint8_t ClassCode = (ClassRegister >> 24) & 0xFF;
                uint8_t SubClass = (ClassRegister >> 16) & 0xFF;

                // Intel HD Audio

                if (ClassCode == PCI_CLASS_CODE_FOR_MULTIMEDIA && SubClass == PCI_SUBCLASS_FOR_INTEL_HDA) {

                    // Configure the PCI configuration space of IHDA to respond to memory requests
                    // Set bits 1 (Memory Space) and 2 (Bus) of the PCI Command Register to 1
                    printf("Found Intel HD Audio!\n");
                    uint32_t PCICommandAndStatusRegister = PCI_SetupRead32Bit(ECAMBaseAddress, Bus, Device, Function, PCI_COMMAND_REGISTER_OFFSET);
                    PCICommandAndStatusRegister |= (1 << 1) | (1 << 2);
                    PCI_SetupWrite32Bit(ECAMBaseAddress, Bus, Device, Function, PCI_COMMAND_REGISTER_OFFSET, PCICommandAndStatusRegister);
                    uint32_t BAR0Low = PCI_SetupRead32Bit(ECAMBaseAddress, Bus, Device, Function, 0x10);
                    uint32_t BAR0High = PCI_SetupRead32Bit(ECAMBaseAddress, Bus, Device, Function, 0x14);
                    IHDABAR0 = (volatile uint8_t*) ((((uint64_t) BAR0High) << 32) | (BAR0Low & ~0xFULL));
                    void* PhysicalAddress = (void*) IHDABAR0;
                    Paging_MapMemory(PhysicalAddress, PhysicalAddress, PAGING_PAGE_PRESENT | PAGING_PAGE_WRITABLE | PAGING_PAGE_WRITE_THROUGH | PAGING_PAGE_CACHE_DISABLE | PAGING_PAGE_NO_EXECUTE, 4);
                    IHDA_Initialise(IHDABAR0);

                }

                // NVMe drive

                if (ClassCode == PCI_CLASS_CODE_FOR_MASS_STORAGE_CONTROLLER && SubClass == PCI_SUBCLASS_FOR_NVME) {
                    printf("Found NVMe SSD!\n");
                    uint32_t BAR0Low = PCI_SetupRead32Bit(ECAMBaseAddress, Bus, Device, Function, 0x10);
                    uint32_t BAR0High = PCI_SetupRead32Bit(ECAMBaseAddress, Bus, Device, Function, 0x14);
                    NVMeBAR0 = (volatile uint8_t*) (((uint64_t) BAR0High << 32) | (BAR0Low & ~0xFULL));
                    void* PhysicalAddress = (void*) NVMeBAR0;
                    Paging_MapMemory(PhysicalAddress, PhysicalAddress, PAGING_PAGE_PRESENT | PAGING_PAGE_WRITABLE | PAGING_PAGE_WRITE_THROUGH | PAGING_PAGE_CACHE_DISABLE | PAGING_PAGE_NO_EXECUTE, 2);
                    NVMe_Initialise(NVMeBAR0);
                }

            }

        }

    }

}


uint32_t PCI_SetupRead32Bit(uint64_t ECAMBaseAddress, int Bus, int Device, int Function, int Offset) {
    uint64_t Raw = PCI_GET_ADDRESS(ECAMBaseAddress, Bus, Device, Function, Offset);
    volatile uint32_t* Address = (volatile uint32_t*) (uintptr_t) Raw;
    return *Address;
}


void PCI_SetupWrite32Bit(uint64_t ECAMBaseAddress, int Bus, int Device, int Function, int Offset, uint32_t Value) {
    uint64_t Raw = PCI_GET_ADDRESS(ECAMBaseAddress, Bus, Device, Function, Offset);
    volatile uint32_t* Address = (volatile uint32_t*) (uintptr_t) Raw;
    *Address = Value;
}


uint32_t PCI_Read8Bit(volatile uint8_t* BAR0, uint64_t Offset) {
    return *(volatile uint8_t*) (BAR0 + Offset);
}


void PCI_Write8Bit(volatile uint8_t* BAR0, uint64_t Offset, uint8_t Value) {
    *(volatile uint8_t*) (BAR0 + Offset) = Value;
}


uint32_t PCI_Read16Bit(volatile uint8_t* BAR0, uint64_t Offset) {
    return *(volatile uint16_t*) (BAR0 + Offset);
}


void PCI_Write16Bit(volatile uint8_t* BAR0, uint64_t Offset, uint16_t Value) {
    *(volatile uint16_t*) (BAR0 + Offset) = Value;
}


uint32_t PCI_Read32Bit(volatile uint8_t* BAR0, uint64_t Offset) {
    return *(volatile uint32_t*) (BAR0 + Offset);
}


void PCI_Write32Bit(volatile uint8_t* BAR0, uint64_t Offset, uint32_t Value) {
    *(volatile uint32_t*) (BAR0 + Offset) = Value;
}


uint64_t PCI_Read64Bit(volatile uint8_t* BAR0, uint64_t Offset) {
    return *(volatile uint64_t*) (BAR0 + Offset);
}


void PCI_Write64Bit(volatile uint8_t* BAR0, uint64_t Offset, uint64_t Value) {
    *(volatile uint64_t*) (BAR0 + Offset) = Value;
}
