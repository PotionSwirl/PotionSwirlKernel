#include "Include/IDT.h"
#include "Include/Libraries.h"
#include "Include/APIC.h"
#include "Include/IO.h"
#include "Include/Paging.h"

// These header files contain our interrupt handlers, scattered across different sources.
#include "Include/Keyboard.h"


IDT_Entry_t IDT[256];
IDT_Descriptor_t IDT_Descriptor;


void IDT_Initialise() {

    // Disable legacy PIC
    APIC_DisablePIC();

    // Fill IDT with interrupt entries
    IDT_SetGate(14, ISR_14);            // Page fault
    IDT_SetGateIOAPIC(33, ISR_33);      // Keyboard

    // Load IDT using descriptor
    IDT_Descriptor.Limit = sizeof(IDT_Entry_t) * 256 - 1;
    IDT_Descriptor.Base = (uint64_t) &IDT;

    asm volatile (
        "LIDT %0\n"
        "STI"
        :: "m"(IDT_Descriptor)
    );

}


void IDT_SetGateIOAPIC(int Vector, void* HandlerAddress) {

    IDT_SetGate(Vector, HandlerAddress);

    // Remap interrupt to use IOAPIC instead of PIC.
    // Bits 0-7 of IOAPIC entry = vector number, bit 16 = masked/unmasked.
    // In this case, our interrupt is unmasked, or NOT ignored, so we set bit 16 to 0;

    uint32_t IOAPICEntryLow = Vector;
    uint32_t IOAPICEntryHigh = 0;

    // Offset in IOAPIC = 0x10 + IRQ number x 2
    uint8_t IOAPICEntryOffset = 0x10 + (Vector - 32) * 2;

    APIC_IOAPICWrite(IOAPICEntryOffset, IOAPICEntryLow);
    APIC_IOAPICWrite(IOAPICEntryOffset + 0x1, IOAPICEntryHigh);

}


void IDT_SetGate(int Vector, void* HandlerAddress) {
    // Add IDT entry.
    IDT[Vector].OffsetLow = (uint64_t) HandlerAddress & 0xFFFF;
    IDT[Vector].Selector = 0x08;
    IDT[Vector].IST = 0;
    IDT[Vector].TypeAttribute = 0x8E;  // Interrupt gate
    IDT[Vector].OffsetMid = ((uint64_t) HandlerAddress >> 16) & 0xFFFF;
    IDT[Vector].OffsetHigh = (uint64_t) HandlerAddress >> 32;
    IDT[Vector].Zero = 0;
}


// Page fault

void IDT_Vector14ExceptionHandler(uint64_t ErrorCode) {

    uint64_t CR2;
    
    asm volatile(
        "MOV %0, CR2"
        : "=r"(CR2)
    );

    Paging_Vector14ExceptionHandlePassover(CR2, ErrorCode);

}


// Keyboard

void IDT_Vector33InterruptHandler() {
    uint8_t Scancode = inb(0x60);
    Keyboard_Vector33InterruptHandlePassover(Scancode);
    APIC_SendEOI();
}
