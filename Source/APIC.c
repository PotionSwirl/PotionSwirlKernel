#include <stdint.h>
#include "Include/APIC.h"
#include "Include/CPU.h"
#include "Include/IO.h"
#include "Include/Graphics.h"
#include "Include/Libraries.h"


static volatile uint32_t* LAPIC;     // Local APIC
static volatile uint32_t* IOAPIC;


void APIC_Initialise(volatile uint32_t IOAPICAddressArgument) {

    uint64_t APICBase = CPU_ReadMSR((uint32_t) APIC_IA32_BASE_MSR);
    APICBase |= APIC_IA32_BASE_MSR_ENABLE_VALUE;
    CPU_WriteMSR(APIC_IA32_BASE_MSR, APICBase);

    LAPIC = (volatile uint32_t*) APIC_GetBase();
    IOAPIC = (volatile uint32_t*) (uintptr_t) IOAPICAddressArgument;

}


void APIC_DisablePIC() {
    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);
}


uint64_t APIC_GetBase() {
    return CPU_ReadMSR((uint32_t) (APIC_IA32_BASE_MSR)) & 0xFFFFF000;
}


void APIC_SendEOI() {
    APIC_LAPICWrite(LAPIC_EOI_REGISTER, 0);
}


uint32_t APIC_IOAPICRead(uint8_t Register) {
    IOAPIC[0] = Register;  // IOREGSEL
    return IOAPIC[4];      // IOWIN
}


void APIC_IOAPICWrite(uint8_t Register, uint32_t Value) {
    IOAPIC[0] = Register;
    IOAPIC[4] = Value;
}


uint32_t APIC_LAPICRead(uint32_t Register) {
    return LAPIC[Register / 4];
}


void APIC_LAPICWrite(uint32_t Register, uint32_t Value) {
    LAPIC[Register / 4] = Value;
}
