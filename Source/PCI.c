#include <stdint.h>
#include "Include/PCI.h"
#include "Include/Libraries.h"
#include "Include/Paging.h"
#include "Include/NVMe.h"
#include "Include/IHDA.h"


// Remember to accommodate VMD!


static uint64_t ECAMBaseAddress;

static volatile uint8_t* NVMeBAR0 = NULL;

static volatile uint8_t* IHDABAR0 = NULL;
static volatile PCI_BusDeviceFunction_t IHDABusDeviceFunction;


void PCI_InitialiseDevices(uint64_t ECAMBaseAddressArgument) {

    ECAMBaseAddress = ECAMBaseAddressArgument;

    // Scan the PCI for the devices we'll be using
    // These devices are only initialised if they exist

    for (uint16_t Bus = 0; Bus < 256; Bus++) {

        for (uint8_t Device = 0; Device < 32; Device++) {

            for (uint8_t Function = 0; Function < 8; Function++) {

                uint32_t VendorRegister = PCI_SetupRead32Bit(Bus, Device, Function, 0x00);
                if ((uint16_t) (VendorRegister & 0xFFFF) == 0xFFFF) continue;
                uint32_t ClassRegister = PCI_SetupRead32Bit(Bus, Device, Function, 0x08);
                uint8_t ClassCode = (ClassRegister >> 24) & 0xFF;
                uint8_t SubClass = (ClassRegister >> 16) & 0xFF;

                // Intel HD Audio

                if (ClassCode == PCI_CLASS_CODE_FOR_MULTIMEDIA && SubClass == PCI_SUBCLASS_FOR_INTEL_HDA) {
                    // Configure the PCI configuration space of IHDA to respond to memory requests
                    // Set bits 1 (Memory Space) and 2 (Bus) of the PCI Command Register to 1
                    printf("Found Intel HD Audio!\n");
                    uint32_t CommandAndStatusRegister = PCI_SetupRead32Bit(Bus, Device, Function, PCI_COMMAND_REGISTER_OFFSET);
                    CommandAndStatusRegister |= (1 << 1) | (1 << 2);
                    // Ensure bit 10 is set to disable legacy #INTx interrupts
                    CommandAndStatusRegister |= (1 << 10);
                    PCI_SetupWrite32Bit(Bus, Device, Function, PCI_COMMAND_REGISTER_OFFSET, CommandAndStatusRegister);
                    uint32_t BAR0Low = PCI_SetupRead32Bit(Bus, Device, Function, 0x10);
                    uint32_t BAR0High = PCI_SetupRead32Bit(Bus, Device, Function, 0x14);
                    IHDABAR0 = (volatile uint8_t*) ((((uint64_t) BAR0High) << 32) | (BAR0Low & ~0xFULL));
                    void* PhysicalAddress = (void*) IHDABAR0;
                    Paging_MapMemory(PhysicalAddress, PhysicalAddress, PAGING_PAGE_PRESENT | PAGING_PAGE_WRITABLE | PAGING_PAGE_WRITE_THROUGH | PAGING_PAGE_CACHE_DISABLE | PAGING_PAGE_NO_EXECUTE, 4);
                    IHDABusDeviceFunction.Bus = Bus; IHDABusDeviceFunction.Device = Device; IHDABusDeviceFunction.Function = Function;
                }

                // NVMe drive

                if (ClassCode == PCI_CLASS_CODE_FOR_MASS_STORAGE_CONTROLLER && SubClass == PCI_SUBCLASS_FOR_NVME) {
                    printf("Found NVMe SSD!\n");
                    uint32_t BAR0Low = PCI_SetupRead32Bit(Bus, Device, Function, 0x10);
                    uint32_t BAR0High = PCI_SetupRead32Bit(Bus, Device, Function, 0x14);
                    NVMeBAR0 = (volatile uint8_t*) (((uint64_t) BAR0High << 32) | (BAR0Low & ~0xFULL));
                    void* PhysicalAddress = (void*) NVMeBAR0;
                    Paging_MapMemory(PhysicalAddress, PhysicalAddress, PAGING_PAGE_PRESENT | PAGING_PAGE_WRITABLE | PAGING_PAGE_WRITE_THROUGH | PAGING_PAGE_CACHE_DISABLE | PAGING_PAGE_NO_EXECUTE, 2);
                }

            }

        }

    }

    // Initialise the devices in these order
    // Intel High Definition Audio relies on the disk and FAT32, so initialise those first!

    if (NVMeBAR0) {
        NVMe_Initialise(NVMeBAR0);
    }

    if (IHDABAR0) {
        IHDA_Initialise(IHDABAR0, (PCI_BusDeviceFunction_t*) &IHDABusDeviceFunction);
    }

}


uint64_t PCI_GetECAMBaseAddress() {
    return ECAMBaseAddress;
}


uint32_t PCI_SetupRead32Bit(uint16_t Bus, uint8_t Device, uint8_t Function, uint64_t Offset) {
    uint64_t Raw = PCI_GET_ADDRESS(ECAMBaseAddress, Bus, Device, Function, Offset);
    volatile uint32_t* Address = (volatile uint32_t*) (uintptr_t) Raw;
    return *Address;
}


void PCI_SetupWrite32Bit(uint16_t Bus, uint8_t Device, uint8_t Function, uint64_t Offset, uint32_t Value) {
    uint64_t Raw = PCI_GET_ADDRESS(ECAMBaseAddress, Bus, Device, Function, Offset);
    volatile uint32_t* Address = (volatile uint32_t*) (uintptr_t) Raw;
    *Address = Value;
}


uint16_t PCI_SetupRead16Bit(uint16_t Bus, uint8_t Device, uint8_t Function, uint64_t Offset) {
    uint64_t Raw = PCI_GET_ADDRESS(ECAMBaseAddress, Bus, Device, Function, Offset);
    volatile uint16_t* Address = (volatile uint16_t*) (uintptr_t) Raw;
    return *Address;
}


void PCI_SetupWrite16Bit(uint16_t Bus, uint8_t Device, uint8_t Function, uint64_t Offset, uint16_t Value) {
    uint64_t Raw = PCI_GET_ADDRESS(ECAMBaseAddress, Bus, Device, Function, Offset);
    volatile uint16_t* Address = (volatile uint16_t*) (uintptr_t) Raw;
    *Address = Value;
}


uint8_t PCI_SetupRead8Bit(uint16_t Bus, uint8_t Device, uint8_t Function, uint64_t Offset) {
    uint64_t Raw = PCI_GET_ADDRESS(ECAMBaseAddress, Bus, Device, Function, Offset);
    volatile uint8_t* Address = (volatile uint8_t*) (uintptr_t) Raw;
    return *Address;
}


uint32_t PCI_Read8Bit(volatile uint8_t* Base, uint64_t Offset) {
    return *(volatile uint8_t*) (Base + Offset);
}


void PCI_Write8Bit(volatile uint8_t* Base, uint64_t Offset, uint8_t Value) {
    *(volatile uint8_t*) (Base + Offset) = Value;
}


uint32_t PCI_Read16Bit(volatile uint8_t* Base, uint64_t Offset) {
    return *(volatile uint16_t*) (Base + Offset);
}


void PCI_Write16Bit(volatile uint8_t* Base, uint64_t Offset, uint16_t Value) {
    *(volatile uint16_t*) (Base + Offset) = Value;
}


uint32_t PCI_Read32Bit(volatile uint8_t* Base, uint64_t Offset) {
    return *(volatile uint32_t*) (Base + Offset);
}


void PCI_Write32Bit(volatile uint8_t* Base, uint64_t Offset, uint32_t Value) {
    *(volatile uint32_t*) (Base + Offset) = Value;
}


uint64_t PCI_Read64Bit(volatile uint8_t* Base, uint64_t Offset) {
    return *(volatile uint64_t*) (Base + Offset);
}


void PCI_Write64Bit(volatile uint8_t* Base, uint64_t Offset, uint64_t Value) {
    *(volatile uint64_t*) (Base + Offset) = Value;
}
