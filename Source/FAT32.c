#include <stdint.h>
#include "Include/FAT32.h"
#include "Include/Paging.h"
#include "Include/Libraries.h"

/* Whichever storage device we end up using, it's responsible
   for giving our filesystem the drive read function specific
   to its hardware so that we can support both NVMe and SATA. */

void (*FAT32_DriveRead)(uint64_t StartingLBA, uint16_t NumberOfBlocks, void* PageAlignedBuffer);

FAT32_BPB_t* BPB;


void FAT32_Initialise(void (*FAT32_DriveReadArgument), uint64_t BytesPerBlock) {

    // Fetch the hardware-specific drive read abstraction

    FAT32_DriveRead = FAT32_DriveReadArgument;

    // The GPT header is located in LBA 1 in the storage device
    // Parse it to find the FAT32 partition

    GPTHeader_t* GPTHeader = Paging_AllocatePage();
   
    FAT32_DriveRead(1, 1, GPTHeader);

    // The bytes of the partition entry array will always be divisble into blocks

    uint64_t PartitionEntriesSize = GPTHeader->SizeOfPartitionEntry * GPTHeader->NumberOfPartitionEntries;
    uint64_t PartitionEntriesBlocks = PartitionEntriesSize / BytesPerBlock;

    // Let's work with one page of entries so we don't need to dynamically allocate anything
    // A partition entry will never overlap from one block into the next

    GPTPartitionEntry_t* GPTPartitionEntries = Paging_AllocatePage();
    memset(GPTPartitionEntries, 0, PAGING_PAGE_SIZE);

    // Look for the partition with an EFI System Partition GUID, which = FAT32

   uint8_t EFISystemPartitionGUID[16] = {0x28, 0x73, 0x2A, 0xC1, 0x1F, 0xF8, 0xD2, 0x11, 
                                         0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B};


    uint8_t NullGUID[16] = {0};


    // Read as many blocks at a time as we can fit into one 4 KiB page, then look through the entries one by one in that page
    
    for (uint64_t i = 0; i < PartitionEntriesBlocks; i += PAGING_PAGE_SIZE / BytesPerBlock) {

        FAT32_DriveRead(GPTHeader->PartitionEntriesLBA + i, PAGING_PAGE_SIZE / BytesPerBlock, GPTPartitionEntries);

        for (uint64_t j = 0; j < BytesPerBlock / GPTHeader->SizeOfPartitionEntry; j++) {

            // Skip if it's all 0s

            if (memcmp(GPTPartitionEntries[j].PartitionTypeGUID, NullGUID, 16) == 0) continue;

            if (memcmp(GPTPartitionEntries[j].PartitionTypeGUID, EFISystemPartitionGUID, 16) == 0) {

                // If we've found the FAT32 partition, read in the BPB from its StartingLBA

                BPB = Paging_AllocatePage();
                memset(BPB, 0, PAGING_PAGE_SIZE);

                FAT32_DriveRead(GPTPartitionEntries[j].StartingLBA, 1, BPB);
                
                break;

            }

        }

    }

    // We don't need these anymore

    Paging_FreePage(GPTPartitionEntries);
    Paging_FreePage(GPTHeader);

}



