#include <stdint.h>
#include <stdbool.h>

bool CPU_CPUSupportsAPIC() {

    uint32_t EAX, EBX, ECX, EDX;
    asm volatile (
        "cpuid"
        : "=a"(EAX), "=b"(EBX), "=c"(ECX), "=d"(EDX)
        : "a"(1)
    );

    // Bit 9 of edx set = CPU supports APIC!
    return EDX & (1 << 9);

}

uint64_t CPU_ReadMSR(uint32_t MSR) {

    uint32_t ValueLow, ValueHigh;

    asm volatile (
        "rdmsr"
        : "=a"(ValueLow), "=d"(ValueHigh)
        : "c"(MSR)
    );

    return ((uint64_t) ValueHigh << 32) | ValueLow;

}

void CPU_WriteMSR(uint32_t MSR, uint64_t Value) {

    // Converting a 64-bit integer to a 32-bit one omits the top half of it.
    uint32_t ValueLow = (uint32_t) Value;
    uint32_t ValueHigh = (uint32_t) (Value >> 32);

    asm volatile (
        "wrmsr"
        :: "c"(MSR), "a"(ValueLow), "d"(ValueHigh)
    );
    
}
