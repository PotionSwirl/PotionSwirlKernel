#pragma once

#include <stdint.h>

#define APIC_IA32_BASE_MSR 0x1B
#define APIC_IA32_BASE_MSR_ENABLE_VALUE (1 << 11)

#define APIC_REGISTER_EOI 0xB0

void APIC_Initialise(volatile uint32_t IOAPICAddressArgument);
void APIC_Enable();
void APIC_DisablePIC();
uint64_t APIC_GetBase();
void APIC_SendEOI();
uint32_t APIC_IOAPICRead(uint8_t Register);
void APIC_IOAPICWrite(uint8_t Register, uint32_t Value);
