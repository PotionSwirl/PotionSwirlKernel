#include <stdint.h>

typedef struct __attribute__((packed)) {
    char Signature[8];
    uint8_t Checksum;
    char OEMID[6];
    uint8_t Revision;
    uint32_t RSDTAddress;
    uint32_t Length;
    uint64_t XSDTAddress;
    uint8_t ExtendedChecksum;
    uint8_t Reserved[3];
} ACPI_RSDP2_t;

typedef struct __attribute__((packed)) {
    char Signature[4];
    uint32_t Length;
    uint8_t Revision;
    uint8_t Checksum;
    char OEMID[6];
    char OEMTableID[8];
    uint32_t OEMRevision;
    uint32_t CreatorID;
    uint32_t CreatorRevision;
} ACPI_SDTHeader_t;

typedef struct __attribute__((packed)) {
    ACPI_SDTHeader_t Header;
    uint64_t Entries[];
} ACPI_XSDT_t;

typedef struct __attribute__((packed)) {
    ACPI_SDTHeader_t Header;
    uint32_t AddressInLocalAPIC;
    uint32_t Flags;
    uint8_t Entries[];
} ACPI_MADT_t;

typedef struct __attribute__((packed)) {
    uint64_t BaseAddress;
    uint16_t PCISegment;
    uint8_t StartBus;
    uint8_t EndBus;
    uint32_t Reserved;
} ACPI_MCFG_Entry_t;

typedef struct __attribute__((packed)) {
    ACPI_SDTHeader_t Header;
    uint64_t Reserved;
    ACPI_MCFG_Entry_t Entries[];
} ACPI_MCFG_t;

typedef struct __attribute__((packed)) {
    uint8_t Type;        // 1 = IOAPIC
    uint8_t Length;
    uint8_t IOAPICID;
    uint8_t Reserved;
    uint32_t IOAPICAddress;
    uint32_t GlobalSystemInterruptBase;
} ACPI_IOAPIC_Entry_t;
