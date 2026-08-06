#pragma once

#include <stdint.h>

// IDT entry
typedef struct __attribute__((packed)) {
    uint16_t OffsetLow;
    uint16_t Selector;
    uint8_t  IST;    // Interrupt stack table
    uint8_t  TypeAttribute;
    uint16_t OffsetMid;
    uint32_t OffsetHigh;
    uint32_t Zero;
} IDT_Entry_t;

// IDT descriptor
typedef struct __attribute__((packed)) {
    uint16_t Limit;
    uint64_t Base;
} IDT_Descriptor_t;

void IDT_Initialise();
void IDT_SetGateIOAPIC(int Vector, void* HandlerAddress);
void IDT_SetGate(int Vector, void* HandlerAddress);

void IDT_Vector14ExceptionHandler();
void IDT_Vector33InterruptHandler();

// Our ISRs from ISR.asm
extern void ISR_14();
extern void ISR_33();
