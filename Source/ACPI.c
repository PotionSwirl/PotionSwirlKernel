#include <stdint.h>
#include <efi.h>
#include "Include/ACPI.h"
#include "Include/Libraries.h"


static ACPI_RSDP2_t* RSDP;
static ACPI_MADT_t* MADT;
static uint32_t IOAPICAddress;
static uint64_t ECAMBaseAddress;
static uint64_t ECAMSize;


void ACPI_Initialise(ACPI_RSDP2_t* RSDPArgument) {

    RSDP = RSDPArgument;

    // We want to find the Multiple APIC Description Table (MADT).
    // Parse XSDT to find "APIC" entry, which = the MADT
    ACPI_XSDT_t* XSDT = (ACPI_XSDT_t*) RSDP->XSDTAddress;

    // Get number of entries in XSDT - our struct doesn't specify it.
    // Each XSDT entry is a uint64_t pointer.
    uint32_t XSDTEntryCount = (XSDT->Header.Length - sizeof(ACPI_SDTHeader_t)) / sizeof(uint64_t);

    // Finally, search for the MADT and MCFG - that's all we're here for!
    // MCFG = Memory Mapped Configuration Space
    ACPI_MCFG_t* MCFG;

    for (uint32_t i = 0; i < XSDTEntryCount; i++) {

        ACPI_SDTHeader_t* SDTHeader = (ACPI_SDTHeader_t*) XSDT->Entries[i];
    
        // Stop searching if we've found them.

        if (memcmp(SDTHeader->Signature, "APIC", 4) == 0) {
            MADT = (ACPI_MADT_t*) SDTHeader;
        }

        if (memcmp(SDTHeader->Signature, "MCFG", 4) == 0) {
            MCFG = (ACPI_MCFG_t*) SDTHeader;
            ACPI_MCFG_Entry_t* Entry = &(MCFG->Entries[0]);
            // Only care about first entry in MCFG.
            ECAMBaseAddress = Entry->BaseAddress;
            ECAMSize = (Entry->EndBus - Entry->StartBus + 1) * 0x100000;
        }

    }

    // Verify that we actually found it.

    if (!MADT) {
        printf("Failed to find MADT for APIC!\n");
        while(true);
    }

    if (!MCFG) {
        printf("Failed to find MCFG!\n");
        while(true);
    }

    // Scan MADT entries to find the IOAPIC.

    uint8_t* MADTEntryCursor = MADT->Entries;
    uint8_t* MADTEntriesEnd = (uint8_t*) MADT + MADT->Header.Length;

    while (MADTEntryCursor < MADTEntriesEnd) {

        uint8_t MADTEntryType = MADTEntryCursor[0];
        uint8_t MADTEntryLength = MADTEntryCursor[1];

        // Type 1 = IOAPIC
        if (MADTEntryType == 1) {
            ACPI_IOAPIC_Entry_t* IOAPIC = (ACPI_IOAPIC_Entry_t*) MADTEntryCursor;
            IOAPICAddress = IOAPIC->IOAPICAddress;
        }

        MADTEntryCursor += MADTEntryLength;

    }

}


ACPI_MADT_t* ACPI_GetMADT() {
    return MADT;
}


uint32_t ACPI_GetIOAPICAddress() {
    return IOAPICAddress;
}


uint64_t ACPI_GetECAMBaseAddress() {
    return ECAMBaseAddress;
}


uint64_t ACPI_GetECAMSize() {
    return ECAMSize;
}
