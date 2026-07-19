#include <stdint.h>
#include "Include/NVMe.h"
#include "Include/Libraries.h"
#include "Include/Paging.h"
#include "Include/FAT32.h"


static volatile uint8_t* NVMeBAR0;

// Admin = accessing disk hardware itself, e.g. to get info about disk
// Admin Submission Queue (ASQ) = array of commands that WE write
// Admin Completion Queue (ACQ) = array of results written by the CONTROLLER
// ASQHead + ACQTail = hardware's problem... :)
static NVMe_Command_t* ASQ;
static NVMe_CompletionEntry_t* ACQ;
static uint16_t ASQTail = 0;
static uint16_t ACQHead = 0;

// I/O = reading + writing
static NVMe_Command_t* IOSQ;
static NVMe_CompletionEntry_t* IOCQ;
static uint16_t IOSQTail = 0;
static uint16_t IOCQHead = 0;

// MQES = Maximum Queue Entries Supported
// DSTRD = Used to calculate doorbell stride - space between doorbell registers
// IOSQ doorbell register offset = calculated after we create the IOSQ
static uint16_t MQES;
static uint8_t DSTRD;
static uint32_t IOSQDoorbellRegisterOffset;

/* After writing an CQ entry, NVMe also writes a phase bit and flips
   when the CQ wraps around. The host (us) must maintain our own phase
   bit and also flip it upon wrap-around, and if our bit matches with
   NVMe's bit, it means we're reading new entries, not old ones that
   we're simply seeing again because we wrapped around. */

// In an ACQ entry, the phase bit is bit 0 of the status field

static uint8_t ACQPhaseBit = 1;
static uint8_t IOCQPhaseBit = 1;

// Generate unique IDs for every command (these can't be 0!)
static uint64_t AdminCommandID = 1;
static uint64_t IOCommandID = 1;


void NVMe_Initialise(volatile uint8_t* NVMeBAR0Argument) {

    // Initialise globals

    NVMeBAR0 = NVMeBAR0Argument;
    uint64_t CAP = NVMe_Read64Bit(NVME_CAP_REGISTER_OFFSET);
    MQES = CAP & 0xFFFF;  // Lower 16 bits of CAP
    DSTRD = (CAP >> 32) & 0xF;

    // Set up the controller

    NVMe_DisableController();
    NVMe_InitialiseAdminQueues();
    NVMe_ReEnableController();

    // We must set up I/O queues to read and write, USING the admin queues

    NVMe_InitialiseIOQueues();

    // Run the Identify Namespace command to get bytes per sector

    NVMe_IdentifyNamespace_t* IdentifyNamespace = Paging_AllocatePage();
    memset(IdentifyNamespace, 0, PAGING_PAGE_SIZE);

    NVMe_Command_t* IdentifyNamespaceCommand = NVMe_CreateAdminCommand();
    
    IdentifyNamespaceCommand->Opcode = NVME_IDENTIFY_COMMAND_OPCODE;
    IdentifyNamespaceCommand->NamespaceID = 1;
    IdentifyNamespaceCommand->PRP1 = (uint64_t) IdentifyNamespace;
    IdentifyNamespaceCommand->CommandSpecificFields.IdentifyFields.CNS = NVME_COMMAND_SPECIFIC_DWORD_FOR_IDENTIFY_NAMESPACE;

    NVMe_RingASQDoorbell();

    // The bytes per sector is stored as a power of 2 in the Identify Namespace structure
    // The index in the LBAF of where to get it is bits 0-3 of FLBAS

    uint64_t BytesPerBlock = 1 << IdentifyNamespace->LBAF[IdentifyNamespace->FLBAS & 0xF].LBADS;

    // We don't need it anymore

    Paging_FreePage(IdentifyNamespace);

    /* Pass the hardware-specific drive reading abstraction to the FAT32 driver,
       as well as the bytes per sector. */

    FAT32_Initialise(NVMe_ReadDrive, BytesPerBlock);

}


void NVMe_ReadDrive(uint64_t StartingLBA, uint16_t NumberOfBlocks, void* PageAlignedBuffer) {

    NVMe_Command_t* ReadCommand = NVMe_CreateIOCommand();
    
    ReadCommand->Opcode = NVME_READ_COMMAND_OPCODE;
    ReadCommand->NamespaceID = 1;
    ReadCommand->PRP1 = (uint64_t) PageAlignedBuffer;
    ReadCommand->CommandSpecificFields.ReadWriteFields.SLBA = StartingLBA;
    ReadCommand->CommandSpecificFields.ReadWriteFields.NLB = NumberOfBlocks - 1;

    NVMe_RingIOSQDoorbell();

};


void NVMe_RingASQDoorbell() {

    // Ring the submission doorbell

    NVMe_Write32Bit(NVME_ASQ_DOORBELL_REGISTER_OFFSET, ASQTail);

    // Wait until our phase bit matches with NVME's phase bit
    
    /* Use a pointer to point directly to the new entry in memory rather
       than make a copy using a non-pointer struct type. This is to ensure
       we track changes made by NVMe real-time. */

    NVMe_CompletionEntry_t* CompletionEntry = &ACQ[ACQHead];
    do {} while ((CompletionEntry->Status & 1) != ACQPhaseBit);
    ACQHead = (ACQHead + 1) % 64;

    // If we wrap around, flip the phase bit
    if (ACQHead == 0) ACQPhaseBit ^= 1;

}


void NVMe_RingIOSQDoorbell() {
    NVMe_Write32Bit(IOSQDoorbellRegisterOffset, IOSQTail);
    NVMe_CompletionEntry_t* CompletionEntry = &IOCQ[IOCQHead];
    do {} while ((CompletionEntry->Status & 1) != IOCQPhaseBit);
    IOCQHead = (IOCQHead + 1) % 64;
    if (IOCQHead == 0) IOCQPhaseBit ^= 1;
}


NVMe_Command_t* NVMe_CreateAdminCommand() {

    // Create a new command and clear any old entry that might be there! We had quite the trouble here...
    NVMe_Command_t* Command = &ASQ[ASQTail];
    memset(Command, 0, sizeof(NVMe_Command_t));

    /* Set the Command ID to the position in the queue (NOT starting from 0 as the ID is also 0
       in a fresh new command when everything is cleared to 0 and we would get confused). */
    Command->CommandID = AdminCommandID++;
    ASQTail = (ASQTail + 1) % 64;

    return Command;

}


NVMe_Command_t* NVMe_CreateIOCommand() {
    NVMe_Command_t* Command = &IOSQ[IOSQTail];
    memset(Command, 0, sizeof(NVMe_Command_t));
    Command->CommandID = IOCommandID++;
    IOSQTail = (IOSQTail + 1) % 64;
    return Command;
}


void NVMe_DisableController() {

    // NVMe mandates that we disable the controller before programming queues

    // 1. Read CC.EN
    uint32_t CC = NVMe_Read32Bit(NVME_CC_REGISTER_OFFSET);

    // 2. Clear bit CC.EN, then write CC back
    CC &= ~1;
    NVMe_Write32Bit(NVME_CC_REGISTER_OFFSET, CC);

    // 3. Wait until CSTS.RDY becomes 0
    uint32_t CSTS;
    
    do {
        CSTS = NVMe_Read32Bit(NVME_CSTS_REGISTER_OFFSET);
    } while (CSTS & 1);

    // The controller is now disabled!

}


void NVMe_InitialiseAdminQueues() {

    // Set up the admin queues

    ASQ = Paging_AllocatePage();
    memset(ASQ, 0, PAGING_PAGE_SIZE);

    ACQ = Paging_AllocatePage();
    memset(ACQ, 0, PAGING_PAGE_SIZE);

    // Set up AQA - Admin Queue Attributes

    uint32_t AQA = 0;

    // Specify the ASQS and ACQS (ASQ size and ACQ size of supported entries)
    // One queue in the ASQ and ACQ will support 64 entries, but we must write 64 - 1 = 3

    AQA |= (64 - 1);         // ASQS
    AQA |= (64 - 1) << 16;   // ACQS

    // Write the new data to the relevant registers

    NVMe_Write32Bit(NVME_AQA_REGISTER_OFFSET, AQA);
    NVMe_Write64Bit(NVME_ASQ_REGISTER_OFFSET, (uint64_t) ASQ);
    NVMe_Write64Bit(NVME_ACQ_REGISTER_OFFSET, (uint64_t) ACQ);

}


void NVMe_ReEnableController() {

    // Now re-enable the controller!

    /* Set CC.EN = 1, as well as CC.IOSQES (I/O Submission Queue Entry Size)
       and CC.IOCQES (I/O Completion Queue Entry Size) */
    // Then wait for CSTS.RDY to = 1

    uint32_t CC = 0;

    CC |= 1;          // EN = 1
    CC |= (6 << 16);  // IOSQES = 64 bytes (2^6 shifted to bits 16-19)
    CC |= (4 << 20);  // IOCQES = 16 bytes (2^4 shifted to bits 20-23)

    NVMe_Write32Bit(NVME_CC_REGISTER_OFFSET, CC);

    uint32_t CSTS;

    do {
        CSTS = NVMe_Read32Bit(NVME_CSTS_REGISTER_OFFSET);
    } while (!(CSTS & 1));

}


void NVMe_InitialiseIOQueues() {

    // Must always create IOCQ first so that we can link the IOSQ to an existing IOCQ!

    NVMe_CreateIOCQ();
    NVMe_CreateIOSQ();

    // The doorbell register for our queue must be calculated using DRSTD
    // Offset = 0x1000 + (2 x QueueID x StrideBytes)
    // StrideBytes = 4 << DSTRD

    uint32_t QueueID = 1;
    uint32_t StrideBytes = 4 << DSTRD;
    IOSQDoorbellRegisterOffset = 0x1000 + (2 * QueueID * StrideBytes);

}


void NVMe_CreateIOCQ() {

    IOCQ = Paging_AllocatePage();
    memset(IOCQ, 0, PAGING_PAGE_SIZE);

    // Execute admin commands to create I/O queues
    // We write details to the Admin Queue this time rather than the registers

    NVMe_Command_t* CreateIOCQCommand = NVMe_CreateAdminCommand();

    CreateIOCQCommand->Opcode = NVME_CREATE_IOCQ_COMMAND_OPCODE;

    // Where our IOCQ is
    CreateIOCQCommand->PRP1 = (uint64_t) IOCQ;
    
    // Dword 10 = Queue ID (bits 15:0) and queue size (bits 31:16)
    // Queue ID = 1, entry size = 64 (zero-based, so enter 64 - 1)
    CreateIOCQCommand->CommandSpecificFields.Dwords[0] = ((64 - 1) << 16) | 1;

    // Dword 11 = Interrupts and phase
    // Bit 0 = 1 (physically contiguous memory layout)
    // Bit 1 = 0 (no interrupts so set to 0)
    CreateIOCQCommand->CommandSpecificFields.Dwords[1] = 1;

    NVMe_RingASQDoorbell();

}


void NVMe_CreateIOSQ() {

    IOSQ = Paging_AllocatePage();
    memset(IOSQ, 0, PAGING_PAGE_SIZE);

    NVMe_Command_t* CreateIOSQCommand = NVMe_CreateAdminCommand();

    CreateIOSQCommand->Opcode = NVME_CREATE_IOSQ_COMMAND_OPCODE;

    // Where our IOSQ is
    CreateIOSQCommand->PRP1 = (uint64_t) IOSQ;

    // Link to IOCQ by using same queue ID
    // Dword 10: Queue ID = 1, Queue size = 64 (zero-based so 64 - 1 = 63)
    CreateIOSQCommand->CommandSpecificFields.Dwords[0] = ((64 - 1) << 16) | 1;

    // Dword 11: Queue ID = 1 (bits 31:16), Physically contiguous = 1 (bit 0)
    CreateIOSQCommand->CommandSpecificFields.Dwords[1] = (1 << 16) | 1;

    NVMe_RingASQDoorbell();

}


uint32_t NVMe_Read32Bit(uint64_t Offset) {
    return *(volatile uint32_t*) (NVMeBAR0 + Offset);
}


void NVMe_Write32Bit(uint64_t Offset, uint32_t Value) {
    *(volatile uint32_t*) (NVMeBAR0 + Offset) = Value;
}


uint64_t NVMe_Read64Bit(uint64_t Offset) {
    return *(volatile uint64_t*) (NVMeBAR0 + Offset);
}


void NVMe_Write64Bit(uint64_t Offset, uint64_t Value) {
    *(volatile uint64_t*) (NVMeBAR0 + Offset) = Value;
}
