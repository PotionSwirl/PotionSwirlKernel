#pragma once

#include <stdint.h>

// NVMe register offsets
#define NVME_CAP_REGISTER_OFFSET           0x0000
#define NVME_CC_REGISTER_OFFSET            0x0014
#define NVME_CSTS_REGISTER_OFFSET          0x001C
#define NVME_AQA_REGISTER_OFFSET           0x0024
#define NVME_ASQ_REGISTER_OFFSET           0x0028
#define NVME_ACQ_REGISTER_OFFSET           0x0030
#define NVME_ASQ_DOORBELL_REGISTER_OFFSET  0x1000

// Command opcodes
#define NVME_IDENTIFY_COMMAND_OPCODE       0x06
#define NVME_CREATE_IOCQ_COMMAND_OPCODE    0x05
#define NVME_CREATE_IOSQ_COMMAND_OPCODE    0x01
#define NVME_READ_COMMAND_OPCODE           0x02

// Command-specific values
#define NVME_COMMAND_SPECIFIC_DWORD_FOR_IDENTIFY_CONTROLLER 1
#define NVME_COMMAND_SPECIFIC_DWORD_FOR_IDENTIFY_NAMESPACE  0

// Thanks AI for the structs :)

#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint64_t SLBA;      // Dword 10 & 11: Starting LBA
    uint16_t NLB;       // Dword 12: Number of Blocks
    uint16_t Control;   // Dword 12: Control flags
    uint8_t  DSM;       // Dword 13: Dataset Management
    uint8_t  Reserved;
    uint16_t DTYPE;     // Dword 13: Directive Type
    uint32_t ILBRT;     // Dword 14: Initial Logical Block Reference Tag
    uint16_t LBAT;      // Dword 15: Logical Block Application Tag
    uint16_t LBAM;      // Dword 15: Logical Block Application Tag Mask
} NVMe_ReadWriteFields_t;

typedef struct __attribute__((packed)) {
    uint8_t  CNS;       // Dword 10 (Byte 0): Controller or Namespace Structure
    uint8_t  Reserved0; // Dword 10 (Byte 1)
    uint16_t CNTID;     // Dword 10 (Bytes 2-3): Controller Identifier
    uint32_t CDW11;     // Dword 11: CSI (Command Set Identifier) / Reserved
    uint32_t CDW12;     // Dword 12
    uint32_t CDW13;     // Dword 13
    uint32_t CDW14;     // Dword 14
    uint32_t CDW15;     // Dword 15
} NVMe_IdentifyFields_t;

typedef struct __attribute__((packed)) {
    // Common NVMe Header (First 16 bytes)
    uint8_t  Opcode;
    uint8_t  Flags;
    uint16_t CommandID;
    uint32_t NamespaceID;
    uint64_t Reserved;
    // Data Pointer Area (16 bytes)
    uint64_t MetadataPointer;
    uint64_t PRP1;
    uint64_t PRP2;
    // Command Specific Area (24 bytes / 6 Dwords)
    union {
        uint32_t Dwords[6];
        NVMe_ReadWriteFields_t ReadWriteFields;
        NVMe_IdentifyFields_t IdentifyFields;
    } CommandSpecificFields;
} NVMe_Command_t;


typedef struct __attribute__((packed)) {
    uint32_t DW0;
    uint32_t Reserved;
    uint16_t SubmissionQueueHead;
    uint16_t SubmissionQueueID;
    uint16_t CommandID;
    uint16_t Status;
} NVMe_CompletionEntry_t;

typedef struct __attribute__((packed)) {
    uint16_t VID;
    uint16_t SSVID;
    char     SerialNumber[20];
    char     ModelNumber[40];
    char     FirmwareRevision[8];
    uint8_t  RAB;
    uint8_t  IEEE[3];
    uint8_t  CMIC;
    uint8_t  MDTS;
    uint16_t CNTLID;
    uint32_t VER;
    uint32_t RTD3R;
    uint32_t RTD3E;
    uint32_t OAES;
    uint32_t CTRATT;
    uint16_t RRLS;
    uint8_t  Reserved1[9];
    uint8_t  CNTRLTYPE;
    uint8_t  FGUID[16];
    uint16_t CRDT1;
    uint16_t CRDT2;
    uint16_t CRDT3;
    uint8_t  Reserved2[114];
    uint16_t OACS;
    uint8_t  ACL;
    uint8_t  AERL;
    uint8_t  FRMW;
    uint8_t  LPA;
    uint8_t  ELPE;
    uint8_t  NPSS;
    uint8_t  AVSCC;
    uint8_t  APSTA;
    uint16_t WCTEMP;
    uint16_t CCTEMP;
    uint16_t MTFA;
    uint32_t HMPRE;
    uint32_t HMMIN;
    uint8_t  TNVMCAP[16];
    uint8_t  UNVMCAP[16];
    uint32_t RPMBS;
    uint16_t EDSTT;
    uint8_t  DSTO;
    uint8_t  FWUG;
    uint16_t KAS;
    uint16_t HCTMA;
    uint16_t MNTMT;
    uint16_t MXTMT;
    uint32_t SANICAP;
    uint32_t HMMINNDA;
    uint16_t HMMIND;
    uint8_t  Reserved3[170];
    uint32_t ONCS;
    uint16_t FUSES;
    uint8_t  VWC;
    uint16_t AWUN;
    uint16_t AWUPF;
    uint8_t  NVSCC;
    uint8_t  NWPC;
    uint16_t ACWU;
    uint8_t  Reserved4[2];
    uint32_t SGLS;
    uint32_t MNAN;
    uint8_t  Reserved5[164];
    uint8_t  Reserved6[1344];
    uint8_t  PowerState[1024];
    uint8_t  VendorSpecific[1024];
} NVMe_IdentifyController_t;

typedef struct __attribute__((packed)) {
    uint16_t MS;             // Metadata Size: Number of metadata bytes per logical block (LBA).
    uint8_t  LBADS;          // LBA Data Size: Sector size expressed as a power of two (e.g., 9=512B, 12=4096B).
    uint8_t  RP;             // Relative Performance: Bits 0-1 show speed layout (00b=Best, 01b=Better, 10b=Good, 11b=Degraded).
    uint8_t  Reserved[12];   // Reserved: Padding bytes reserved for future extensions in the NVMe specification.
} NVMe_IdentifyNamespace_LBAF_t;

/* Bits 0-3 of FLBAS (Formatted LBA Size) tell us which index to access
   in the LBAF entries to find information about the active LBA format,
   e.g. sector size. */

typedef struct {
    uint64_t NSZE;
    uint64_t NCAP;
    uint64_t NUSE;
    uint8_t  NSFEAT;
    uint8_t  NLBAF;
    uint8_t  FLBAS;
    uint8_t  MC;
    uint8_t  DPC;
    uint8_t  DPS;
    uint8_t  NMIC;
    uint8_t  Reserved1[37];
    uint8_t  NGUID[16];
    uint8_t  EUI64[8];
    uint8_t Reserved[36];
    NVMe_IdentifyNamespace_LBAF_t LBAF[64];
    uint8_t  Reserved3[2944];
} NVMe_IdentifyNamespace_t;

void NVMe_Initialise(volatile uint8_t* NVMeBAR0Argument);
void NVMe_ReadDrive(uint64_t StartingLBA, uint16_t NumberOfSectors, void* Buffer);
void NVMe_RingASQDoorbell();
void NVMe_RingIOSQDoorbell();
NVMe_Command_t* NVMe_CreateAdminCommand();
NVMe_Command_t* NVMe_CreateIOCommand();
void NVMe_DisableController();
void NVMe_InitialiseAdminQueues();
void NVMe_ReEnableController();
void NVMe_IdentifyController();
void NVMe_IdentifyNamespace();
void NVMe_InitialiseIOQueues();
void NVMe_CreateIOSQ();
void NVMe_CreateIOCQ();
uint32_t NVMe_Read32Bit(uint64_t Offset);
void NVMe_Write32Bit(uint64_t Offset, uint32_t Value);
uint64_t NVMe_Read64Bit(uint64_t Offset);
void NVMe_Write64Bit(uint64_t Offset, uint64_t Value);
