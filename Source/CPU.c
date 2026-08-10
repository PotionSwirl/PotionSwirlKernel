#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "Include/CPU.h"
#include "Include/Libraries.h"
#include "Include/Graphics.h"


static uint64_t TSCFrequency;


uint32_t CPU_GetCPUID(uint32_t EAXInput, CPU_SelectedRegister_t RegisterToReturn) {

    uint32_t EAX, EBX, ECX, EDX;

    asm volatile (
        "CPUID"
        : "=a"(EAX), "=b"(EBX), "=c"(ECX), "=d"(EDX)
        : "a"(EAXInput)
    );

    switch (RegisterToReturn) {
        case CPU_SELECT_EAX:
            return EAX;
        case CPU_SELECT_EBX:
            return EBX;
        case CPU_SELECT_ECX:
            return ECX;
        case CPU_SELECT_EDX:
            return EDX;
    }

    return 0;

}


uint64_t CPU_ReadMSR(uint32_t MSR) {

    uint32_t ValueLow, ValueHigh;

    asm volatile (
        "RDMSR"
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
        "WRMSR"
        :: "c"(MSR), "a"(ValueLow), "d"(ValueHigh)
    );
    
}


void CPU_SleepInitialise() {

    uint32_t EAX, EBX, ECX;

    asm volatile (
        "CPUID"
        : "=a"(EAX), "=b"(EBX), "=c"(ECX)
        : "a"(CPU_TSC_AND_NCC)
    );

    TSCFrequency = ECX * (EBX / EAX);

}


void CPU_SleepInSeconds(uint64_t Seconds) {
    CPU_SleepInMicroseconds(Seconds * 1000000);
}


void CPU_SleepInMicroseconds(uint64_t Microseconds) {

    uint32_t EAX, EDX;
    
    asm volatile (
        "RDTSC"
        : "=a"(EAX), "=d"(EDX)
    );

    uint64_t StartTSCValue = (((uint64_t) EDX) << 32) | EAX;
    uint64_t EndTSCValue = StartTSCValue + (TSCFrequency * (((double) Microseconds) / 1000000));
    uint64_t NewTSCValue;

    /* Don't care about TSC wrapping around to 0 as Intel guarantees it won't do that
       within 10 years :D */

    do {

        asm volatile (
            "RDTSC"
            : "=a"(EAX), "=d"(EDX)
        );

        NewTSCValue = (((uint64_t) EDX) << 32) | EAX;

    } while (NewTSCValue < EndTSCValue);

}
