#include <stdint.h>

#pragma once

static inline uint8_t inb(uint16_t Port) {
    uint8_t Result;
    asm volatile ("inb %1, %0" : "=a"(Result) : "Nd"(Port));
    return Result;
}

static inline void outb(uint16_t Port, uint8_t Value) {
    asm volatile ("outb %0, %1" : : "a"(Value), "Nd"(Port));
};

static inline uint16_t inw(uint16_t Port) {
    uint16_t Result;
    asm volatile ("inw %1, %0" : "=a"(Result) : "Nd"(Port));
    return Result;
};