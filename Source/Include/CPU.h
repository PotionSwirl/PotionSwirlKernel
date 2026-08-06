#pragma once

#include <stdint.h>
#include <stdbool.h>

#define CPU_IA32_EFER_MSR                                       0xC0000080
#define CPU_EXTENDED_PROCESSOR_SIGNATURE_AND_FEATURE_BITS_EAX   0x80000001
#define CPU_ITSC_SUPPORT_EAX                                    0x80000007
#define CPU_TSC_AND_NCC                                         0x15
#define CPU_SELECT_EAX                                          1
#define CPU_SELECT_EBX                                          2
#define CPU_SELECT_ECX                                          3
#define CPU_SELECT_EDX                                          4

uint64_t CPU_ReadMSR(uint32_t MSR);
void CPU_WriteMSR(uint32_t MSR, uint64_t Value);
uint32_t CPU_GetCPUID(uint32_t EAXInput, uint8_t RegisterToReturn);
void CPU_SleepInSeconds(uint64_t Seconds);
void CPU_SleepInMicroseconds(uint64_t Microseconds);
void CPU_SleepInitialise();
