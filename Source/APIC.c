#include <stdint.h>
#include "Include/APIC.h"
#include "Include/CPU.h"
#include "Include/IO.h"

static volatile uint32_t* LocalAPIC;
static volatile uint32_t* IOAPIC;

void APIC_Initialise(volatile uint32_t IOAPICAddressArgument) {

    uint64_t APICBase = CPU_ReadMSR((uint32_t) APIC_IA32_BASE_MSR);
    APICBase |= APIC_IA32_BASE_MSR_ENABLE_VALUE;
    CPU_WriteMSR(APIC_IA32_BASE_MSR, APICBase);

    LocalAPIC = (volatile uint32_t*) APIC_GetBase();
    IOAPIC = (volatile uint32_t*) IOAPICAddressArgument;

}

void APIC_DisablePIC() {
    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);
}

uint64_t APIC_GetBase() {
    return CPU_ReadMSR((uint32_t) (APIC_IA32_BASE_MSR)) & 0xFFFFF000;
}

void APIC_SendEOI() {
    LocalAPIC[APIC_REGISTER_EOI / 4] = 0;
}

uint32_t APIC_IOAPICRead(uint8_t Register) {
    IOAPIC[0] = Register;  // IOREGSEL
    return IOAPIC[4];      // IOWIN
}

void APIC_IOAPICWrite(uint8_t Register, uint32_t Value) {
    IOAPIC[0] = Register;
    IOAPIC[4] = Value;
}
