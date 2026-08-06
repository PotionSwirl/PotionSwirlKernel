/********** INTEL HIGH DEFINITION AUDIO **********/
// Remember to test multiple types of WAVs!


#include <stdint.h>
#include <stdbool.h>
#include "Include/IHDA.h"
#include "Include/PCI.h"
#include "Include/Libraries.h"
#include "Include/CPU.h"
#include "Include/Paging.h"
#include "Include/FAT32.h"


static volatile uint8_t* IHDABAR0;
static uint8_t SelectedCAd;
static uint16_t CORBEntries;

// Our command ring buffers
static uint32_t* CORB;  // Commands (verbs)
static uint64_t* RIRB;  // Responses

// Streaming audio to output
static uint16_t SDnFMT;
static uintptr_t OutputStreamDescriptorsStartingAddress;
static void* OutputStreamBuffer = NULL;

// Allocate some contiguous pages to work in when playing a stream
// Must be an even number of pages to split in half for the two BDL entries
static uint8_t OutputStreamBufferPages = 4;

// Buffer Descriptor List (BDL)
static IHDA_BDL_t OutputStream1BDL;

// Our primary nodes
uint8_t AFGNID;
uint8_t SpeakerPinNID;
uint8_t DACNID;


void IHDA_Initialise(volatile uint8_t* IHDABAR0Argument) {
    IHDABAR0 = IHDABAR0Argument;
    IHDA_TakeControllerOutOfReset();
    IHDA_FindCodec();
    IHDA_InitialiseCORB();
    IHDA_InitialiseRIRB();
    IHDA_InitialiseOutputStream();
    IHDA_CreateSpeakerPinAndDACAudioPath();
    IHDA_InitialiseSpeakerPinAndDAC();
}


void IHDA_TakeControllerOutOfReset() {

    // Set GCTL.CRST (bit 0) to 1, and wait for it to read back as 1

    uint32_t GCTL = PCI_Read32Bit(IHDABAR0, IHDA_GCTL_REGISTER_OFFSET);
    GCTL |= 1;
    PCI_Write32Bit(IHDABAR0, IHDA_GCTL_REGISTER_OFFSET, GCTL);

    do {
        GCTL = PCI_Read32Bit(IHDABAR0, IHDA_GCTL_REGISTER_OFFSET);
    } while (!(GCTL & 1));

    // Wait at least 521 us after reset clear for the codecs to register

    CPU_SleepInMicroseconds(521);

}


void IHDA_FindCodec() {

    uint16_t STATESTS = PCI_Read16Bit(IHDABAR0, IHDA_STATESTS_REGISTER_OFFSET);

    // If a bit number is set, a codec is present at that codec address
    // We must check bits 0-14, the SDIWAKE bits
    
    for (int i = 0; i <= 14; i++) {
        if (STATESTS & (1 << i)) {
            SelectedCAd = i;
            break;
        }
    }

}


void IHDA_InitialiseCORB() {

    // The CORB is the Command Outbound Ring Buffer and is used to send commands to the controller in a queue

    // Make sure the CORB DMA engine is stopped by clearing CORBCTL.CORBRUN (bit 1) if it's running
    // This is already 0 after reset so we don't need to worry about that

    // Allocate a buffer for the CORB - this must be 128-byte aligned (compatible with page alignment)
    // The max size is 256 x 4-byte entries = 1024 bytes, which fits well within one 4KB page

    CORB = Paging_AllocatePage();
    memset(CORB, 0, PAGING_PAGE_SIZE);

    // If GCAP.64OK (bit 0) is set, 64-bit addressing is supported by the controller
    // If it isn't, our address must be below 4GB to fit inside a uint32_t

    uint16_t GCAP = PCI_Read16Bit(IHDABAR0, IHDA_GCAP_REGISTER_OFFSET);

    if (GCAP & 1) {

        // If 64-bit addressing is supported, the upper 32 bits of our CORB address can exist
        // Write those to CORBUBASE
        uint32_t CORBUBASE = (uint32_t) (((uintptr_t) CORB) >> 32);
        PCI_Write32Bit(IHDABAR0, IHDA_CORBUBASE_REGISTER_OFFSET, CORBUBASE);

    } else if (((uintptr_t) CORB) >= 0x100000000) {

        printf("The IHDA controller doesn't support 64-bit addressing, and CORB is placed at a 64-bit address!\n");
        printf("Please place CORB below the 4GB boundary. Thanks :)\n");
        printf("Halting...\n");
        while(true);

    }

    // Write the lower 32 bits of our CORB base address to CORBLBASE
    // 6:0 is hardwired to 0 to enforce 128-byte alignment

    uint32_t CORBLBASE = (uint32_t) (uintptr_t) CORB;
    PCI_Write32Bit(IHDABAR0, IHDA_CORBLBASE_REGISTER_OFFSET, CORBLBASE);

    // Intel says that we should set the CORB size to 256 entries
    // This is unless the size supported in the CORBSIZE.CORBSZCAP is smaller
    
    uint8_t CORBSIZE = PCI_Read8Bit(IHDABAR0, IHDA_CORBSIZE_REGISTER_OFFSET);

    // 7:4 = CORBSZCAP, bit 7 being reserved

    uint8_t CORBSZCAP = (CORBSIZE >> 4) & 0x7;

    // If 0b100 is part of the bits, i.e. bit 2 is set, then 256 entries are supported
    // 0b10 -> 16 entries, 0b001 -> 2 entries

    // Set the size using the CORBSIZE field of 1:0
    // 0b00 = 2 entries, 0b01 = 16 entries and 0b10 = 256 entries
    
    if (CORBSZCAP & (1 << 2)) {
        CORBEntries = 256;
        CORBSIZE |= 0x2;
    } else if (CORBSZCAP & (1 << 1)) {
        CORBEntries = 16;
        CORBSIZE |= 0x1;
    } else {
        CORBEntries = 2;
        CORBSIZE &= ~0x3;
    }

    PCI_Write8Bit(IHDABAR0, IHDA_CORBSIZE_REGISTER_OFFSET, CORBSIZE);

    // Reset the CORB Read Pointer to 0
    
    // First, set CORBRP.CORBRPRST (bit 15) and wait till it reads back as 1

    uint16_t CORBRP = PCI_Read16Bit(IHDABAR0, IHDA_CORBRP_REGISTER_OFFSET);
    CORBRP |= (1 << 15);
    PCI_Write16Bit(IHDABAR0, IHDA_CORBRP_REGISTER_OFFSET, CORBRP);

    do {
        CORBRP = PCI_Read16Bit(IHDABAR0, IHDA_CORBRP_REGISTER_OFFSET);
    } while (!(CORBRP & (1 << 15)));

    // Then clear it, and wait till it reads back as 0

    CORBRP &= ~(1 << 15);
    PCI_Write16Bit(IHDABAR0, IHDA_CORBRP_REGISTER_OFFSET, CORBRP);

    do {
        CORBRP = PCI_Read16Bit(IHDABAR0, IHDA_CORBRP_REGISTER_OFFSET);
    } while (CORBRP & (1 << 15));

    // Reset the CORB Write Pointer to 0 by writing 0x00 to CORBWP.RW (7:0)
    // We can leave this too as RW is 0 after reset

    // Finally, turn the CORB on
    // Set CORBCTL.CORBRUN (bit 1) and wait till it reads back as 1

    uint8_t CORBCTL = PCI_Read8Bit(IHDABAR0, IHDA_CORBCTL_REGISTER_OFFSET);
    CORBCTL |= (1 << 1);
    PCI_Write8Bit(IHDABAR0, IHDA_CORBCTL_REGISTER_OFFSET, CORBCTL);

    do {
        CORBCTL = PCI_Read8Bit(IHDABAR0, IHDA_CORBCTL_REGISTER_OFFSET);
    } while (!(CORBCTL & (1 << 1)));

}


void IHDA_InitialiseRIRB() {

    // The RIRB is the Response Input Ring Buffer

    // Initialising it is a similar process to the CORB, except for a few steps
    /* The order here is slightly different because we're strictly
       sticking to the flowcharts in the specification for both CORB 
       and RIRB initialisation, which order things differently */

    // The RIRB needs to be stopped
    // RIRBCTL.RIRBDMAEN is already 0 after reset so we'll leave this

    // Allocate a buffer for the RIRB - this must be 128-byte aligned as well
    // The max size is 256 x 8-byte entries = 2048 bytes, which fits in the same 4KB page as the CORB

    // If 256 entries in CORB, RIRB is 128-byte-aligned if we put it right after CORB
    // If 16 or 2 entries, RIRB must be placed 128 bytes after CORB to 128-byte-align

    if (CORBEntries == 256) {
        RIRB = (uint64_t*) (((uintptr_t) CORB) + 1024);
    } else {
        RIRB = (uint64_t*) (((uintptr_t) CORB) + 128);
    }

    // Write the RIRB address to the RIRB base registers

    uint16_t GCAP = PCI_Read16Bit(IHDABAR0, IHDA_GCAP_REGISTER_OFFSET);

    if (GCAP & 1) {
        uint32_t RIRBUBASE = (uint32_t) (((uintptr_t) RIRB) >> 32);
        PCI_Write32Bit(IHDABAR0, IHDA_RIRBUBASE_REGISTER_OFFSET, RIRBUBASE);
    }

    uint32_t RIRBLBASE = (uint32_t) (uintptr_t) RIRB;
    PCI_Write32Bit(IHDABAR0, IHDA_RIRBLBASE_REGISTER_OFFSET, RIRBLBASE);

    // Determine RIRB size
    
    uint8_t RIRBSIZE = PCI_Read8Bit(IHDABAR0, IHDA_RIRBSIZE_REGISTER_OFFSET);

    uint8_t RIRBSZCAP = (RIRBSIZE >> 4) & 0x7;
    
    if (RIRBSZCAP & (1 << 2)) {
        RIRBSIZE |= 0x2;
    } else if (RIRBSZCAP & (1 << 1)) {
        RIRBSIZE |= 0x1;
    } else {
        RIRBSIZE &= ~0x3;
    }

    PCI_Write8Bit(IHDABAR0, IHDA_RIRBSIZE_REGISTER_OFFSET, RIRBSIZE);

    // We don't have to reset the RIRB Write Pointer as it becomes 0 after reset
    // And the RIRB Read Pointer is not maintained by hardware

    // We don't need to update interrupt status for responses
    // After reset, it's 0 by default
    // We'll just poll for a response, so we'll leave RIRBCTL.RINTCTL clear

    // Turn the RIRB DMA engine on by setting and reading back RIRBCTL.RIRBDMAEN (bit 1)
    // I know the specification doesn't say to do this for RIRB, but you must!

    uint8_t RIRBCTL = PCI_Read8Bit(IHDABAR0, IHDA_RIRBCTL_REGISTER_OFFSET);
    RIRBCTL |= (1 << 1);
    PCI_Write8Bit(IHDABAR0, IHDA_RIRBCTL_REGISTER_OFFSET, RIRBCTL);

    do {
        RIRBCTL = PCI_Read8Bit(IHDABAR0, IHDA_RIRBCTL_REGISTER_OFFSET);
    } while (!(RIRBCTL & (1 << 1)));

}


// We'll only be using Output Stream 1 to play all the system's audio

void IHDA_InitialiseOutputStream() {

    // Each stream gets its own Stream Descriptor
    // A Stream Descriptor is a set of registers controlling the stream
    // The Input Stream Descriptor registers start at 0x80 after BAR0
    // Each Stream Descriptor is 0x20 bytes
    // Output Stream Descriptor 0 starts right after the Input Stream Descriptor

    // Get the number of input streams supported
    // GCAP.ISS is [11:8]

    uint16_t GCAP = PCI_Read16Bit(IHDABAR0, IHDA_GCAP_REGISTER_OFFSET);
    uint8_t ISS = (GCAP >> 8) & 0xF;

    OutputStreamDescriptorsStartingAddress = ((uintptr_t) IHDABAR0) + 0x80 + ISS * 0x20;

    // Loop through all pages to find consecutive free pages

    for (uint64_t i = 0; i < Paging_GetTotalPages(); i++) {

        // Check if a page is free
        // E.g. page 5

        bool ConsecutiveFreePagesFound = true;

        if (!(Paging_MemoryBitmapTestPage(i))) {

            // If it's free, check whether the relevant number of pages after the first are free too
            // E.g. pages 6, 7 and 8 if we want 4 contiguous pages

            for (uint64_t j = i + 1; j <= i + OutputStreamBufferPages - 1; j++) {

                if (Paging_MemoryBitmapTestPage(j)) {
                    ConsecutiveFreePagesFound = false;
                    break;
                }
                
            }

        } else {

            ConsecutiveFreePagesFound = false;

        }

        // If we've found consecutive pages, reserve them all
        // Then, get the address at the beginning of this new contiguous block of memory

        if (ConsecutiveFreePagesFound) {
            
            for (uint64_t j = i; j <= i + OutputStreamBufferPages - 1; j++) {
                Paging_MemoryBitmapSet(j);
            }

            OutputStreamBuffer = (void*) (i * PAGING_PAGE_SIZE);

            break;

        }

    }

    if (OutputStreamBuffer == NULL) {
        printf("IHDA_InitialiseOutputStream: Couldn't find enough contiguous memory to play sound.\n");
        return;
    }

    // To configure the controller with our audio, we need to do a few things
    
    // First, set up the BDL for stream 1
    // Point the first BDL entry to the first half of the stream buffer, and the second to the second half
    // We'll also set the IOC (Interrupt On Completion) bits
    // -> When the first half finishes, Intel will set bit 3 in SDnSTS
    /* Then, we can fill the first half with new audio data while the second half plays, then fill the second half
       while the first plays, and so on */
    // This is called ping-pong buffering :) - remember, the BDL must have at least 2 valid entries!

    uint8_t OutputStreamBufferHalfSize = OutputStreamBufferPages / 2 * PAGING_PAGE_SIZE;

    OutputStream1BDL.BDLE0.Address = (uintptr_t) OutputStreamBuffer;
    OutputStream1BDL.BDLE0.Length = OutputStreamBufferHalfSize;
    OutputStream1BDL.BDLE0.IOCAndReserved = 1;

    OutputStream1BDL.BDLE1.Address = ((uintptr_t) OutputStreamBuffer) + OutputStreamBufferHalfSize;
    OutputStream1BDL.BDLE1.Length = OutputStreamBufferHalfSize;
    OutputStream1BDL.BDLE1.IOCAndReserved = 1;

    // Now let's configure Output Stream Descriptor 1 with a base profile of 48 kHz 16-bit stereo audio
    // This profile is supported by virtually every codec
    // We'll resample any audio data that presents with other attributes

    // Before writing to the stream registers, we must reset the stream
    // To reset, set SDnCTL.SRST (bit 0), read it back, then clear it and read it back again

    uint32_t SDnCTL = IHDA_OutputStreamDescriptorRead(IHDA_24_BIT_REGISTER, 1, IHDA_SDNCTL_REGISTER_OFFSET);
    SDnCTL |= 1;
    IHDA_OutputStreamDescriptorWrite(IHDA_24_BIT_REGISTER, 1, IHDA_SDNCTL_REGISTER_OFFSET, SDnCTL);

    do {
        SDnCTL = IHDA_OutputStreamDescriptorRead(IHDA_24_BIT_REGISTER, 1, IHDA_SDNCTL_REGISTER_OFFSET);
    } while (!(SDnCTL & 1));

    SDnCTL &= ~1;
    IHDA_OutputStreamDescriptorWrite(IHDA_24_BIT_REGISTER, 1, IHDA_SDNCTL_REGISTER_OFFSET, SDnCTL);

    do {
        SDnCTL = IHDA_OutputStreamDescriptorRead(IHDA_24_BIT_REGISTER, 1, IHDA_SDNCTL_REGISTER_OFFSET);
    } while (SDnCTL & 1);

    // Enable an interrupt to be triggered by IOC by setting SDnCTL.IOCE (bit 2)

    SDnCTL |= (1 << 2);
    IHDA_OutputStreamDescriptorWrite(IHDA_24_BIT_REGISTER, 1, IHDA_SDNCTL_REGISTER_OFFSET, SDnCTL);

    // Assign the Stream Descriptor its stream ID of 1 in SDnCTL.STRM (23:20)

    SDnCTL |= (1 << 20);
    IHDA_OutputStreamDescriptorWrite(IHDA_24_BIT_REGISTER, 1, IHDA_SDNCTL_REGISTER_OFFSET, SDnCTL);

    // Now we configure the stream format (SDnFMT)
    // For 48 kHz 16-bit stereo audio, we can hardcode SDnFMT
    
    // Bit 14 = Sample Base Rate (BASE) -> 0 for 48 kHz
    // Bits 13:11 = Sample Base Rate Multiple (MULT) -> 000 for 48 kHz
    // Bits 10:8 = Sample Base Rate Divisor (DIV) -> 000 for 48 kHz
    // Bit 7 = Reserved
    // Bits 6:4 = Bits per Sample (BITS) -> 001 for 16 bits
    // Bits 3:0 = Number of Channels (CHAN) -> 0001 for 2 channels (stereo)
    
    SDnFMT = 0x0011;

    IHDA_OutputStreamDescriptorWrite(IHDA_16_BIT_REGISTER, 1, IHDA_SDNFMT_REGISTER_OFFSET, SDnFMT);

    // Next, write the lower 32 bits of the address of our BDL to SDnBDPL
    // And if the controller supports 64-bit addressing, write the upper 32 bits to SDnBDPU
    // Bits 6:0 in SDnBDPL are hardwired to 0 to enforce 128-byte alignment

    uint32_t SDnBDPL = (uint32_t) (uintptr_t) &OutputStream1BDL;
    IHDA_OutputStreamDescriptorWrite(IHDA_32_BIT_REGISTER, 1, IHDA_SDNBDPL_REGISTER_OFFSET, SDnBDPL);

    if (GCAP & 1) {

        uint32_t SDnBDPU = (uint32_t) (((uintptr_t) &OutputStream1BDL) >> 32);
        IHDA_OutputStreamDescriptorWrite(IHDA_32_BIT_REGISTER, 1, IHDA_SDNBDPU_REGISTER_OFFSET, SDnBDPU);

    } else if (((uintptr_t) &OutputStream1BDL) >= 0x100000000) {

        printf("IHDA_InitialiseOutputStream: The IHDA controller doesn't support 64-bit addressing, and the BDL for Output Stream %d is placed at a 64-bit address!\n", 1);
        printf("IHDA_InitialiseOutputStream: Please place it below the 4GB boundary. Thanks :)\n");
        while(true);

    }

    // Now, write the length of the cyclic buffer (all BDLE buffer sizes combined) to SDnCBL
    // That's just a fixed number of pages that we already set up for out stream buffer -> that IS our cyclic buffer!

    uint32_t SDnCBL = OutputStreamBufferPages * PAGING_PAGE_SIZE;
    IHDA_OutputStreamDescriptorWrite(IHDA_32_BIT_REGISTER, 1, IHDA_SDNCBL_REGISTER_OFFSET, SDnCBL);

    // Finally, write the last valid index of the BDL to SDnLVI
    // For a 2-entry-minimum BDL, this is at least 1

    uint16_t SDnLVI = 1;
    IHDA_OutputStreamDescriptorWrite(IHDA_16_BIT_REGISTER, 1, IHDA_SDNLVI_REGISTER_OFFSET, SDnLVI);

    // The stream's characteristics are now configured!

}


void IHDA_CreateSpeakerPinAndDACAudioPath() {

    // We need to find the pin widget connected to the speaker
    // Then, we need to select a DAC from its connection list
    // Both must support our base profile of 48 kHz 16-bit stereo audio

    // Query the Subordinate Node Count of NID 0x0
    // This will give us the number of function groups in the codec, and the starting node in that group
    // Bits 23:16 = Starting Node Number, 7:0 = Total Number of Nodes
    // Intel says that the NIDs in a group of nodes are always sequential

    // Only the lower 32 bits of the 64-bit response in the RIRB form the actual response

    uint32_t Response = IHDA_Submit12BitIDVerb(SelectedCAd, 0, IHDA_GET_PARAMETER_VERB_ID, IHDA_SUBORDINATE_NODE_COUNT_PARAMETER_ID);
    uint8_t StartingNodeNumber = Response >> 16;
    uint8_t TotalNumberOfNodes = Response & 0xFF;

    // The nodes under the root node are function groups
    // Go through them and find the NID of the Audio Function Group (AFG)
    
    for (uint8_t i = StartingNodeNumber; i <= StartingNodeNumber + TotalNumberOfNodes - 1; i++) {
       
        Response = IHDA_Submit12BitIDVerb(SelectedCAd, i, IHDA_GET_PARAMETER_VERB_ID, IHDA_FUNCTION_GROUP_TYPE_PARAMETER_ID);
        
        // Node type is bits 7:0

        if ((Response & 0xFF) == IHDA_AUDIO_FUNCTION_GROUP_TYPE) {
            AFGNID = i;
            break;
        }

    }

    // Enumerate the AFG and find the speaker pin by reading its capabilities and Configuration Default

    Response = IHDA_Submit12BitIDVerb(SelectedCAd, AFGNID, IHDA_GET_PARAMETER_VERB_ID, IHDA_SUBORDINATE_NODE_COUNT_PARAMETER_ID);
    StartingNodeNumber = Response >> 16;
    TotalNumberOfNodes = Response & 0xFF;

    for (uint8_t i = StartingNodeNumber; i <= StartingNodeNumber + TotalNumberOfNodes - 1; i++) {

        // Check if the node is a pin
        // Widget type is bits 23:20

        uint32_t CurrentPinAudioWidgetCapabilities = IHDA_Submit12BitIDVerb(SelectedCAd, i, IHDA_GET_PARAMETER_VERB_ID, IHDA_AUDIO_WIDGET_CAPABILITIES_PARAMETER_ID);

        if (((CurrentPinAudioWidgetCapabilities >> 20) & 0xF) == IHDA_PIN_TYPE) {

            // If it's a pin, check its:
            // -> Pin Capabilities to see if it's output capable
            // -> Configuration Default to see if it's fixed and internal, and connect to speakers
            // -> Audio Widget Capabilities to see if it supports stereo audio (part of our base profile)

            uint32_t PinCapabilities = IHDA_Submit12BitIDVerb(SelectedCAd, i, IHDA_GET_PARAMETER_VERB_ID, IHDA_PIN_CAPABILITIES_PARAMETER_ID);
            uint32_t ConfigurationDefault = IHDA_Submit12BitIDVerb(SelectedCAd, i, IHDA_GET_CONFIGURATION_DEFAULT_VERB_ID, 0);

            bool PinCapabilitiesConditionsMet = PinCapabilities & (1 << 4);
            bool ConfigurationDefaultConditionsMet = ((ConfigurationDefault >> 30) & 0x3) == 0x2 && ((ConfigurationDefault >> 20) & 0xF) == 0x1;
            bool AudioWidgetCapabilitiesConditionsMet = CurrentPinAudioWidgetCapabilities & 1;

            if (PinCapabilitiesConditionsMet && ConfigurationDefaultConditionsMet && AudioWidgetCapabilitiesConditionsMet) {

                // Now that we've found a speaker pin, find a DAC that supports our base profile
                // Search the connection list of the pin
                // If we find the DAC, great - select that as the pin's connection
                // If we find a mixer or selector instead, search THEIR connection lists
                // Do this until we traverse the connection lists of more mixers or selectors (if need be) and find a valid DAC

                // Check the length of the pin's connection list

                // Begin the search for a DAC, starting the chain with the pin

                uint8_t NodeToSearch = i;

                bool DeadEnd = false;

                // We'll return from the function once we find it (or break out if we hit a dead end)

                while (!DeadEnd) {

                    bool Continue = false;

                    // Construct the connection list
                    // We must request a zero-based index
                    // If it's in short form, we request every 4th index and get back 4 x 8-bit NID entries
                    // If it's in long form, we request every 2nd index and get back 2 x 16-bit NID entries

                    Response = IHDA_Submit12BitIDVerb(SelectedCAd, NodeToSearch, IHDA_GET_PARAMETER_VERB_ID, IHDA_CONNECTION_LIST_LENGTH_PARAMETER_ID);

                    uint8_t ConnectionListLength = Response & 0x7F;
                    bool LongForm = Response & (1 << 7);
                    
                    // If there are an uneven number of entries (there is a remainder), round up the number of 32-bit chunks needed to store them

                    uint8_t EntriesPerRequest = LongForm ? 2 : 4;
                    uint8_t RequestsRequired = ConnectionListLength / EntriesPerRequest + ((ConnectionListLength % EntriesPerRequest) ? 1 : 0);
                    uint32_t InitialConnectionList[RequestsRequired];

                    for (uint8_t j = 0; j < RequestsRequired; j++) {
                        InitialConnectionList[j] = IHDA_Submit12BitIDVerb(SelectedCAd, NodeToSearch, IHDA_GET_CONNECTION_LIST_ENTRY_VERB_ID, j * EntriesPerRequest);
                    }

                    // Go through the entries the first time
                    // We want to extend the ConnectionList array size to fit NIDs between two NIDs that form a range
                    // So here, we're calculating the size of the array that includes the range NIDs
                    // Look for the range bit set in an entry (bit 7 for short form, 15 for long form)
                    // If the range bit is set, the NID forms a range of NIDs with the previous entry

                    uint8_t LastNID = 0;
                    uint8_t TemporaryConnectionListLength = ConnectionListLength;

                    for (uint8_t j = 0; j < TemporaryConnectionListLength; j++) {

                        uint16_t Entry = (uint16_t) (LongForm ? ((uint16_t*) InitialConnectionList)[j] : ((uint8_t*) InitialConnectionList)[j]);
                        uint8_t NID = Entry & 0x7F;

                        if (Entry & (1 << (LongForm ? 15 : 7))) {
                            for (uint8_t k = LastNID + 1; k <= NID - 1; k++) ConnectionListLength++;
                        }

                        LastNID = NID;

                    }

                    // Allocate a new connection list with the new size, and copy over the last one

                    uint8_t Final32BitSize = ConnectionListLength / EntriesPerRequest + ((ConnectionListLength % EntriesPerRequest) ? 1 : 0);
                    uint32_t ConnectionList[Final32BitSize];
                    memcpy(ConnectionList, InitialConnectionList, sizeof(InitialConnectionList));

                    // Go through the entries a second time
                    // Actually add the range NIDs to the newly-sized array

                    LastNID = 0;

                    for (uint8_t j = 0; j < TemporaryConnectionListLength; j++) {

                        uint16_t Entry = (uint16_t) (LongForm ? ((uint16_t*) InitialConnectionList)[j] : ((uint8_t*) InitialConnectionList)[j]);
                        uint8_t NID = Entry & 0x7F;

                        if (Entry & (1 << (LongForm ? 15 : 7))) {
                            for (uint8_t k = LastNID + 1; k <= NID - 1; k++) {
                                if (LongForm) {
                                    ((uint16_t*) ConnectionList)[TemporaryConnectionListLength++] = k;
                                } else {
                                    ((uint8_t*) ConnectionList)[TemporaryConnectionListLength++] = k;
                                }
                            }
                        }

                        LastNID = NID;

                    }
                    
                    // Go through the entries a third time
                    // Now, actually look through the final connection list for a DAC, mixer or selector

                    for (uint8_t j = 0; j < ConnectionListLength; j++) {
                        
                        uint8_t NID = (uint8_t) (LongForm ? (((uint16_t*) ConnectionList)[j] & 0x7F) : (((uint8_t*) ConnectionList)[j] & 0x7F));

                        uint32_t NIDAudioWidgetCapabilities = IHDA_Submit12BitIDVerb(SelectedCAd, NID, IHDA_GET_PARAMETER_VERB_ID, IHDA_AUDIO_WIDGET_CAPABILITIES_PARAMETER_ID);
                        uint8_t NodeType = (NIDAudioWidgetCapabilities >> 20) & 0xF;
                        
                        bool DACFound = false;

                        switch (NodeType) {

                            case IHDA_DAC_TYPE:

                                // If it's a DAC, check its:
                                // -> Supported PCM, Size Rates to see if it supports 16-bit 48 kHz audio
                                // -> Supported Stream Formats to see if it supports PCM audio
                                // -> Audio Widget Capabilities to see if it supports stereo audio

                                uint32_t SupportedPCMAndSizeRates = IHDA_Submit12BitIDVerb(SelectedCAd, NID, IHDA_GET_PARAMETER_VERB_ID, IHDA_SUPPORTED_PCM_SIZE_AND_RATES_PARAMETER_ID);
                                uint32_t SupportedStreamFormats = IHDA_Submit12BitIDVerb(SelectedCAd, NID, IHDA_GET_PARAMETER_VERB_ID, IHDA_SUPPORTED_STREAM_FORMATS_PARAMETER_ID);

                                bool SupportedPCMAndSizeRatesConditionsMet = (SupportedPCMAndSizeRates & (1 << 17)) && (SupportedPCMAndSizeRates & (1 << 6));
                                bool SupportedStreamFormatsConditionsMet = SupportedStreamFormats & 1;
                                bool AudioWidgetCapabilitiesConditionsMet = Response & 1;

                                if (SupportedPCMAndSizeRatesConditionsMet && SupportedStreamFormatsConditionsMet && AudioWidgetCapabilitiesConditionsMet) {

                                    // If we've reached here, we've finally found a DAC to play our sound!
                                    // Hooray!

                                    SpeakerPinNID = i;
                                    DACNID = NID;

                                    DACFound = true;

                                    goto ConnectCurrentNodeToLastOne;

                                }

                                break;   // Break from this switch statement


                            case IHDA_MIXER_TYPE:

                                /* A mixer has multiple simultaneous inputs that are read from its connection list.
                                   To unmute it, we must unmute only the index of the connection list item we want
                                   input from. Since this mixer widget has been read from ANOTHER widget's connection
                                   list, we have to wait till we start searching ITS OWN connection list before unmuting.*/

                                // For a mixer:
                                // -> Check its Audio Widget Capabilities to see if it supports stereo audio

                                if (NIDAudioWidgetCapabilities & 1) {
                                    goto ConnectCurrentNodeToLastOne;
                                }

                                break;  // Break from this switch statement

                            case IHDA_SELECTOR_TYPE:

                                // For a selector:
                                // -> Check its Audio Widget Capabilities to see if it supports stereo audio
                                // -> If it does, unmute it

                                if (NIDAudioWidgetCapabilities & 1) {
                                    IHDA_UnmuteNode(NID, 0);
                                    goto ConnectCurrentNodeToLastOne;
                                }

                                break;  // Break from this switch statement

                        }

                        // After the widget found in the path has been configured, we need to connect it to the one before it
                        // This only happens if a pin, mixer or selector was actually found

                        ConnectCurrentNodeToLastOne:

                            // If there's only one connection in the list, it's hardwired and we don't need to select it
                            // Also, a mixer doesn't use connection select as it has multiple simultaneous outputs

                            uint8_t NodeToSearchType = (IHDA_Submit12BitIDVerb(SelectedCAd, NodeToSearch, IHDA_GET_PARAMETER_VERB_ID, IHDA_AUDIO_WIDGET_CAPABILITIES_PARAMETER_ID) >> 20) & 0xF;

                            if (ConnectionListLength > 1 && NodeToSearchType != IHDA_MIXER_TYPE) {
                                IHDA_Submit12BitIDVerb(SelectedCAd, NodeToSearch, IHDA_SET_CONNECTION_SELECT_CONTROL_VERB_ID, j);
                            }

                            if (DACFound) return;  // A valid DAC has been found and configured - let's get out of this mess!

                            /* If the connection list is a mixer's, we must unmute the mixer at the index of the valid node
                               that was found in its list. That's how it's unmuted */

                            if (NodeToSearchType == IHDA_MIXER_TYPE) {
                                IHDA_UnmuteNode(NodeToSearch, j);
                            }

                            // A valid mixer/selector has been found and connected to the one behind it
                            // Now search the connection list of THIS mixer/selector node on the way to finding a valid DAC

                            Continue = true;
                            NodeToSearch = NID;
                            break;  // Break from the for loop that is searching through the current connection list

                    }

                    /* If Continue if false, it means a valid node wasn't found to continue the audio path.
                       In that case, we've reached a dead end and should search the next pin for a path.
                       This will break us out of the while loop. */
                       
                    if (!Continue) DeadEnd = true;

                }

            }

        }

    }

    printf("IHDA_InitialiseSpeakerPinAndDAC: No valid speaker pin and DAC path could be found.\n");
    printf("IHDA_InitialiseSpeakerPinAndDAC: Halting...\n");
    while(true);

}


void IHDA_InitialiseSpeakerPinAndDAC() {

    // Collect some more information about the successfully found speaker pin

    uint32_t PinCapabilities = IHDA_Submit12BitIDVerb(SelectedCAd, SpeakerPinNID, IHDA_GET_PARAMETER_VERB_ID, IHDA_PIN_CAPABILITIES_PARAMETER_ID);

    bool SpeakerPinEAPDCapable = PinCapabilities & (1 << 16);
    bool SpeakerPinBTLCapable = PinCapabilities & (1 << 6);
    
    // For the pin:
    // -> Set power state to D0 using the Power State Control
    // -> Enable output using the Pin Widget Control
    // -> Enable EAPD if it it's EAPD-capable and BTL if BTL-capable
    // -> Unmute the input and output amps if they exist, and set both left and right amps to 0dB gain
    //    using the Amplifier Gain/Mute control

    IHDA_Submit12BitIDVerb(SelectedCAd, SpeakerPinNID, IHDA_SET_POWER_STATE_VERB_ID, 0x0);

    IHDA_Submit12BitIDVerb(SelectedCAd, SpeakerPinNID, IHDA_SET_PIN_WIDGET_CONTROL_VERB_ID, 1 << 6);
    
    if (SpeakerPinEAPDCapable || SpeakerPinBTLCapable) {
        uint8_t Payload = 0;
        if (SpeakerPinEAPDCapable) Payload |= (1 << 1);
        if (SpeakerPinBTLCapable) Payload |= 1;
        IHDA_Submit12BitIDVerb(SelectedCAd, SpeakerPinNID, IHDA_SET_EAPD_BTL_ENABLE_VERB_ID, Payload);
    }

    IHDA_UnmuteNode(SpeakerPinNID, 0);

    // The pin is ready!

    // Now, for the DAC:
    // -> Set its power state to D0 using the Power State Control
    // -> Set its Converter Format using the same SDnFMT value programmed into Output Stream Descriptor 1
    // -> Set its Converter Stream, Channel to match our Output Stream Descriptor 1
    // -> Set its Converter Channel Count to match stereo (our base profile)
    // -> Unmute the input and output amps if they exist, and set both left and right amps to 0dB gain
    //    using the Amplifier Gain/Mute control

    IHDA_Submit12BitIDVerb(SelectedCAd, DACNID, IHDA_SET_POWER_STATE_VERB_ID, 0x0);

    IHDA_Submit4BitIDVerb(SelectedCAd, DACNID, IHDA_SET_CONVERTER_FORMAT_VERB_ID, SDnFMT);

    // We're using stream 1 (bits 7:4) and channel 0 is the lowest of 0 and 1 for stereo (bits 3:0)

    IHDA_Submit12BitIDVerb(SelectedCAd, DACNID, IHDA_SET_CONVERTER_STREAM_CHANNEL_VERB_ID, 0x10);

    // Converter Channel Count is 0-indexed, so passing in a value of 1 = 2 channels (stereo)

    IHDA_Submit12BitIDVerb(SelectedCAd, DACNID, IHDA_SET_CONVERTER_CHANNEL_COUNT_VERB_ID, 0x1);

    IHDA_UnmuteNode(DACNID, 0);

}


void IHDA_UnmuteNode(uint8_t NID, uint8_t IndexToUnmute) {

    // Check whether the pin itself has information about its amps, or the AFG does

    uint32_t AudioWidgetCapabilities = IHDA_Submit12BitIDVerb(SelectedCAd, NID, IHDA_GET_PARAMETER_VERB_ID, IHDA_AUDIO_WIDGET_CAPABILITIES_PARAMETER_ID);

    if (!(AudioWidgetCapabilities & (1 << 3))) {
        AudioWidgetCapabilities = IHDA_Submit12BitIDVerb(SelectedCAd, AFGNID, IHDA_GET_PARAMETER_VERB_ID, IHDA_AUDIO_WIDGET_CAPABILITIES_PARAMETER_ID);
    }

    bool InAmpPresent = AudioWidgetCapabilities & (1 << 1);
    bool OutAmpPresent = AudioWidgetCapabilities & (1 << 2);

    if (InAmpPresent) {
        uint32_t AmplifierCapabilities = IHDA_Submit12BitIDVerb(SelectedCAd, NID, IHDA_GET_PARAMETER_VERB_ID, IHDA_INPUT_AMPLIFIER_CAPABILITIES_PARAMETER_ID);
        uint8_t InAmp0dbOffset = AmplifierCapabilities & 0x7F;
        uint16_t Payload = 0x7000 | (IndexToUnmute << 8) | InAmp0dbOffset;
        IHDA_Submit4BitIDVerb(SelectedCAd, NID, IHDA_SET_AMPLIFIER_GAIN_MUTE_VERB_ID, Payload);
    }

    if (OutAmpPresent) {
        uint32_t AmplifierCapabilities = IHDA_Submit12BitIDVerb(SelectedCAd, NID, IHDA_GET_PARAMETER_VERB_ID, IHDA_OUTPUT_AMPLIFIER_CAPABILITIES_PARAMETER_ID);
        uint8_t OutAmp0dbOffset = AmplifierCapabilities & 0x7F;
        uint16_t Payload = 0xB000 | (IndexToUnmute << 8) | OutAmp0dbOffset;
        IHDA_Submit4BitIDVerb(SelectedCAd, NID, IHDA_SET_AMPLIFIER_GAIN_MUTE_VERB_ID, Payload);
    }

}


uint32_t IHDA_OutputStreamDescriptorRead(uint8_t RegisterSizeID, uint8_t StreamID, uint32_t Offset) {

    uint64_t FinalOffset = OutputStreamDescriptorsStartingAddress + StreamID * 0x20 + Offset;
    uint32_t FinalValue;

    switch (RegisterSizeID) {
        case IHDA_8_BIT_REGISTER:
            FinalValue = *(volatile uint8_t*) FinalOffset;
            break;
        case IHDA_16_BIT_REGISTER:
            FinalValue = *(volatile uint16_t*) FinalOffset;
            break;
        case IHDA_24_BIT_REGISTER:
            // Get only the lower 24 bits as the next 8 bits go over into another register
            FinalValue = (*(volatile uint32_t*) FinalOffset) & 0xFFFFFF;
            break;
        case IHDA_32_BIT_REGISTER:
            FinalValue = *(volatile uint32_t*) FinalOffset;
            break;
    }

    return FinalValue;

}


void IHDA_OutputStreamDescriptorWrite(uint8_t RegisterSizeID, uint8_t StreamID, uint64_t Offset, uint32_t Value) {

    uint64_t FinalOffset = OutputStreamDescriptorsStartingAddress + StreamID * 0x20 + Offset;

    switch (RegisterSizeID) {
        case IHDA_8_BIT_REGISTER:
            *(volatile uint8_t*) FinalOffset = (uint8_t) Value;
            break;
        case IHDA_16_BIT_REGISTER:
            *(volatile uint16_t*) FinalOffset = (uint16_t) Value;
            break;
        case IHDA_24_BIT_REGISTER:
            // First delete the lower 24 bits, then inject the new value into those bits
            *(volatile uint32_t*) FinalOffset &= ~0xFFFFFF;
            *(volatile uint32_t*) FinalOffset |= (uint32_t) Value;
            break;
        case IHDA_32_BIT_REGISTER:
            *(volatile uint32_t*) FinalOffset = Value;
            break;
    }

}


uint32_t IHDA_Submit4BitIDVerb(uint8_t CAd, uint8_t NID, uint8_t VerbID, uint16_t Payload) {
    uint32_t Verb = (CAd << 28) | (NID << 20) | (VerbID << 16) | Payload;
    return IHDA_SubmitVerbStub(Verb);
}


uint32_t IHDA_Submit12BitIDVerb(uint8_t CAd, uint8_t NID, uint16_t VerbID, uint8_t Payload) {
    uint32_t Verb = (CAd << 28) | (NID << 20) | (VerbID << 8) | Payload;
    return IHDA_SubmitVerbStub(Verb);
}


uint32_t IHDA_SubmitVerbStub(uint32_t Verb) {
    
    // Write the verb into the CORB at the index: CORB Write Pointer + 1
    // The write pointer is CORBWP[7:0]

    uint16_t CORBWP = PCI_Read16Bit(IHDABAR0, IHDA_CORBWP_REGISTER_OFFSET);

    uint8_t NewCORBWP = ((CORBWP & 0xFF) + 1) % CORBEntries;
    CORB[NewCORBWP] = Verb;

    // Update CORBWP with the new index -> ringing the controller's doorbell to fetch the verb

    CORBWP = (CORBWP & ~0xFF) | NewCORBWP;
    PCI_Write16Bit(IHDABAR0, IHDA_CORBWP_REGISTER_OFFSET, CORBWP);

    // Intel to wait for the CORB Read Pointer to equal to the new CORB Write Pointer
    /* This indicates the controller fetched the command, but not that the response has
       actually been written to the RIRB - for that, we'll have to wait for the RIRB
       WRITE pointer to equal to the CORB read or write pointer */
    // The write pointer is RIRBWP[7:0]

    CORBWP = PCI_Read16Bit(IHDABAR0, IHDA_CORBWP_REGISTER_OFFSET);  // New CORBWP
    uint16_t RIRBWP;

    do {
        RIRBWP = PCI_Read16Bit(IHDABAR0, IHDA_RIRBWP_REGISTER_OFFSET);
    } while ((RIRBWP & 0xFF) != (CORBWP & 0xFF));

    // The response has now been written to RIRB - return it

    return RIRB[RIRBWP & 0xFF];

}


void IHDA_PlayWAV(const char* FilePath) {

    // Read only the first sector of the WAV file to get the header up to BitsPerSample
    // We will use this information to set up our stream using the controller
    // The minimum size of one sector today is 512 bytes, which our desired header info is well within

    FAT32_BPB_t* BPB = FAT32_GetBPB();
    FAT32_DirectoryEntry_t* DirectoryEntry = FAT32_GetDirectoryEntry(FilePath);

    if (!DirectoryEntry) {
        printf("IHDA_PlayWAV: Couldn't find %s.\n");
        return;
    }

    uint32_t FirstCluster = (DirectoryEntry->FirstClusterHigh << 16) | DirectoryEntry->FirstClusterLow;
    uint32_t StartingSector = FAT32_ClusterToSector(FirstCluster);

    // We'll overwrite the header later when we fill the buffer with actual audio samples

    IHDA_WAVHeader_t* WAVHeader = OutputStreamBuffer;

    FAT32_DriveRead(StartingSector, 1, WAVHeader);

    // Now WAVHeader is full of information about the WAV file!

    // Will continue here Inshallah :)

    // Free our buffer pages

    for (uint8_t i = 0; i < OutputStreamBufferPages; i++) {
        void* PageAddress = (void*) (((uintptr_t) OutputStreamBuffer) + i * PAGING_PAGE_SIZE);
        Paging_FreePage(PageAddress);
    }

}
