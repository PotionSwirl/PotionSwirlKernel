#pragma once

#include <stdint.h>
#include <stdbool.h>

bool CPU_CPUSupportsAPIC();

uint64_t CPU_ReadMSR(uint32_t MSR);
void CPU_WriteMSR(uint32_t MSR, uint64_t Value);
