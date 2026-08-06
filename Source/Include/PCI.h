#pragma once

#include <stdint.h>

// Class codes
#define PCI_CLASS_CODE_FOR_MASS_STORAGE_CONTROLLER  0x01
#define PCI_CLASS_CODE_FOR_MULTIMEDIA               0x04

// Subclasses
#define PCI_SUBCLASS_FOR_NVME        0x08
#define PCI_SUBCLASS_FOR_INTEL_HDA   0x03

// PCI commands
#define PCI_COMMAND_REGISTER_OFFSET  0x04

#define PCI_GET_ADDRESS(ECAMBaseAddress, Bus, Device, Function, Offset) \
    (ECAMBaseAddress + \
    ((Bus & 0xFF)  << 20) + \
    ((Device & 0x1F)  << 15) + \
    ((Function & 0x07)  << 12) + \
    (Offset & 0xFFF))

void PCI_InitialiseDevices(uint64_t ECAMBaseAddress);
uint32_t PCI_SetupRead32Bit(uint64_t ECAMBaseAddress, int Bus, int Device, int Function, int Offset);
void PCI_SetupWrite32Bit(uint64_t ECAMBaseAddress, int Bus, int Device, int Function, int Offset, uint32_t Value);
uint32_t PCI_Read8Bit(volatile uint8_t* BAR0, uint64_t Offset);
void PCI_Write8Bit(volatile uint8_t* BAR0, uint64_t Offset, uint8_t Value);
uint32_t PCI_Read16Bit(volatile uint8_t* BAR0, uint64_t Offset);
void PCI_Write16Bit(volatile uint8_t* BAR0, uint64_t Offset, uint16_t Value);
uint32_t PCI_Read32Bit(volatile uint8_t* BAR0, uint64_t Offset);
void PCI_Write32Bit(volatile uint8_t* BAR0, uint64_t Offset, uint32_t Value);
uint64_t PCI_Read64Bit(volatile uint8_t* BAR0, uint64_t Offset);
void PCI_Write64Bit(volatile uint8_t* BAR0, uint64_t Offset, uint64_t Value);
