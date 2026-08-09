#pragma once

#include <stdint.h>

// Class codes
#define PCI_CLASS_CODE_FOR_MASS_STORAGE_CONTROLLER  0x01
#define PCI_CLASS_CODE_FOR_MULTIMEDIA               0x04

// Subclasses
#define PCI_SUBCLASS_FOR_NVME                       0x08
#define PCI_SUBCLASS_FOR_INTEL_HDA                  0x03

// PCI Configuration Space Register offsets
#define PCI_COMMAND_REGISTER_OFFSET                 0x04
#define PCI_STATUS_REGISTER_OFFSET                  0x06
#define PCI_CAPABILITIES_POINTER_REGISTER_OFFSET    0x34

// PCI Capability codes
#define PCI_MSI_CAPABILITY                          0x05
#define PCI_MSIX_CAPABILITY                         0x11

#define PCI_GET_ADDRESS(ECAMBaseAddress, Bus, Device, Function, Offset) \
    (ECAMBaseAddress + \
    ((Bus & 0xFF)  << 20) + \
    ((Device & 0x1F)  << 15) + \
    ((Function & 0x07)  << 12) + \
    (Offset & 0xFFF))

typedef struct {
    uint16_t Bus;
    uint8_t Device;
    uint8_t Function;
} PCI_BusDeviceFunction_t;

void PCI_InitialiseDevices(uint64_t ECAMBaseAddressArgument);
uint64_t PCI_GetECAMBaseAddress();
uint32_t PCI_SetupRead32Bit(uint16_t Bus, uint8_t Device, uint8_t Function, uint64_t Offset);
void PCI_SetupWrite32Bit(uint16_t Bus, uint8_t Device, uint8_t Function, uint64_t Offset, uint32_t Value);
uint16_t PCI_SetupRead16Bit(uint16_t Bus, uint8_t Device, uint8_t Function, uint64_t Offset);
void PCI_SetupWrite16Bit(uint16_t Bus, uint8_t Device, uint8_t Function, uint64_t Offset, uint16_t Value);
uint8_t PCI_SetupRead8Bit(uint16_t Bus, uint8_t Device, uint8_t Function, uint64_t Offset);
uint32_t PCI_Read8Bit(volatile uint8_t* Base, uint64_t Offset);
void PCI_Write8Bit(volatile uint8_t* Base, uint64_t Offset, uint8_t Value);
uint32_t PCI_Read16Bit(volatile uint8_t* Base, uint64_t Offset);
void PCI_Write16Bit(volatile uint8_t* Base, uint64_t Offset, uint16_t Value);
uint32_t PCI_Read32Bit(volatile uint8_t* Base, uint64_t Offset);
void PCI_Write32Bit(volatile uint8_t* Base, uint64_t Offset, uint32_t Value);
uint64_t PCI_Read64Bit(volatile uint8_t* Base, uint64_t Offset);
void PCI_Write64Bit(volatile uint8_t* Base, uint64_t Offset, uint64_t Value);
