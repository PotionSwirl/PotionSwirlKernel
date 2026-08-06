#include <stdint.h>
#include "Include/FAT32.h"
#include "Include/Paging.h"
#include "Include/Libraries.h"

/* Whichever storage device we end up using, it's responsible
   for giving our filesystem the drive read function specific
   to its hardware so that we can HOPEFULLY support multiple types. */

void (*FAT32_DriveRead)(uint64_t StartingLBA, uint16_t NumberOfBlocks, void* PageAlignedBuffer);

FAT32_BPB_t* BPB;
static uint32_t DataStartSector;
static uint32_t FATStartSector;


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

    uint64_t BlocksPerPage = PAGING_PAGE_SIZE / BytesPerBlock;


    // Read as many blocks at a time as we can fit into one 4 KiB page, then look through the entries one by one in that page
    
    for (uint64_t i = 0; i < PartitionEntriesBlocks; i += BlocksPerPage) {

        FAT32_DriveRead(GPTHeader->PartitionEntriesLBA + i, BlocksPerPage, GPTPartitionEntries);

        for (uint64_t j = 0; j < BytesPerBlock / GPTHeader->SizeOfPartitionEntry; j++) {

            // Skip if it's all 0s

            if (memcmp(GPTPartitionEntries[j].PartitionTypeGUID, NullGUID, 16) == 0) continue;

            if (memcmp(GPTPartitionEntries[j].PartitionTypeGUID, EFISystemPartitionGUID, 16) == 0) {

                // If we've found the FAT32 partition, read in the BPB from its StartingLBA

                BPB = Paging_AllocatePage();
                memset(BPB, 0, PAGING_PAGE_SIZE);

                FAT32_DriveRead(GPTPartitionEntries[j].StartingLBA, 1, BPB);

                // The FAT begins after the reserved sectors
                // Calculate DataStart, which we'll need for our cluster calculations
                // Also, the reserved sectors start from the beginning of the FAT32 partition, not entire drive

                FATStartSector = GPTPartitionEntries[j].StartingLBA + BPB->ReservedSectors;
                DataStartSector = FATStartSector + BPB->NumberOfFATS * BPB->SectorsPerFAT32;
                
                // We don't need these anymore

                Paging_FreePage(GPTPartitionEntries);
                Paging_FreePage(GPTHeader);
                
                return;

            }

        }

    }

}


// As sectors 0 and 1 are reserved in the FAT32 partition, clusters count starting from 2
// Hence, our DataStartSector, which occurs after the reserved sectors as calculated above

uint32_t FAT32_ClusterToSector(uint32_t Cluster) {
    return DataStartSector + (Cluster - 2) * BPB->SectorsPerCluster;
}


uint32_t FAT32_GetNextCluster(uint32_t Cluster) {

    // Find the cluster entry (a uint32_t) of the given cluster in the FAT
    // Its entry will help us get the NEXT cluster
    // Its then a separate job to read data from the sectors of whatever cluster we're working with

    uint32_t CurrentClusterEntrySector = FATStartSector + (Cluster * FAT32_CLUSTER_ENTRY_SIZE / BPB->BytesPerSector);
    uint32_t CurrentClusterEntryOffset = Cluster * FAT32_CLUSTER_ENTRY_SIZE % BPB->BytesPerSector;

    // Read the cluster entry (separate from the cluster file data)
    // A cluster entry is well within a sector as there are multiple cluster entries in one sector

    void* Buffer = Paging_AllocatePage();
    FAT32_DriveRead(CurrentClusterEntrySector, 1, Buffer);

    // Get the contents of the cluster entry where they actually are
    // Cast the buffer to a uint8_t* so that adding to it increments it in bytes

    uint32_t* CurrentClusterEntryAddress = (uint32_t*) (((uint8_t*) Buffer) + CurrentClusterEntryOffset);
    Paging_FreePage(Buffer);

    // Only the lower 28 bits of the entry contain the actual number of the next cluster

    return *CurrentClusterEntryAddress & 0x0FFFFFFF;

}


FAT32_DirectoryEntry_t* FAT32_GetDirectoryEntry(const char* FilePath) {

    // We don't want to mess with the user's string!
    // Also - COPY ONE CHARACTER AHEAD - IT'S THE NULL STRING TERMINATOR!!

    char FilePathCopy[strlen(FilePath)];
    memcpy(FilePathCopy, FilePath, strlen(FilePath) + 1);

    // If the path has slashes, tokenize it into its separate folder names

    char* PathTokens[50];

    // The first path token begins at the beginning of the FilePath string

    PathTokens[0] = (char*) FilePathCopy;

    int NumberOfPathTokens = 1;

    for (int i = 0; i < strlen(FilePathCopy); i++) {
        if (FilePathCopy[i] == '/' || FilePathCopy[i] == '\\') {
            FilePathCopy[i] = '\0';
            PathTokens[NumberOfPathTokens++] = &FilePathCopy[i + 1];
        }
    }

    // Memcpying from before left a null at the end of the last token - we will need nulls for comparing later

    // Search through the directories represented by the tokens to find the final file entry
    // Or if the file name is the only token, simply fetch the entry from the root directory

    FAT32_BPB_t* BPB = FAT32_GetBPB();
    
    uint32_t CurrentDirectoryFirstCluster = BPB->RootCluster;

    FAT32_DirectoryEntry_t* DirectoryEntry;

    for (int i = 0; i < NumberOfPathTokens; i++) {
        DirectoryEntry = FAT32_FindInDirectory(CurrentDirectoryFirstCluster, PathTokens[i]);
        CurrentDirectoryFirstCluster = (DirectoryEntry->FirstClusterHigh << 16) | DirectoryEntry->FirstClusterLow;
    }

    // This will automatically be NULL if the entry couldn't be found

    return DirectoryEntry;

}


// Can be used to return the directory entry of either a file or folder (name must be given in 8.3 format)

FAT32_DirectoryEntry_t* FAT32_FindInDirectory(uint32_t DirectoryFirstCluster, const char* RegularFormattedName) {

    // RegularFormattedName = with dots and varied casing

    /* If the file or folder name is 11 characters or under, format it to capital 8.3 standard and will
       be compared with the entry */
    // If it's longer than 11 characters, it's a long file/folder name and will be compared as it is

    char FAT32FormattedName[11];

    // If it's not a long name:

    if ((!(Libraries_StringContains(RegularFormattedName, '.')) && strlen(RegularFormattedName) <= 11)
        || ((Libraries_StringContains(RegularFormattedName, '.')) && strlen(RegularFormattedName) <= 12))

    {

        // Format according to whether it's a file or folder name

        if (Libraries_StringContains(RegularFormattedName, '.')) {
            FAT32_FormatFileName(RegularFormattedName, FAT32FormattedName);
        } else {
            FAT32_FormatFolderName(RegularFormattedName, FAT32FormattedName);
        }

    }

    // So therefore, if it's a long name, we'll ignore the FAT32FormattedName and just use RegularFormattedName when comparing

    // Now, scan the directory entries

    // Have a buffer to construct a long name if the file has LFN entries - otherwise, this goes unused

    char ConstructedLongName[255];     // Max long name length allowed by FAT32

    // Work with one cluster of directory entries at a time

    uint32_t EntriesPerCluster = BPB->SectorsPerCluster * BPB->BytesPerSector / sizeof(FAT32_DirectoryEntry_t);
    uint32_t CurrentCluster = DirectoryFirstCluster;

    FAT32_DirectoryEntry_t* DirectoryEntries = Paging_AllocatePage();
    memset(DirectoryEntries, 0, PAGING_PAGE_SIZE);

    while (CurrentCluster < FAT32_EOF) {

        uint32_t StartingSector = FAT32_ClusterToSector(CurrentCluster);
        FAT32_DriveRead(StartingSector, BPB->SectorsPerCluster, DirectoryEntries);

        // If we hit an SFN entry and this boolean is set, it means we hit the end of an LFN chain
        // We can safely generate a new boolean for each cluster an LFN chain always sits in one cluster

        bool LFNChainInProgress = false;
        uint32_t NumberOfEntriesInLFNChainToProcess;

        for (uint32_t i = 0; i < EntriesPerCluster; i++) {

            FAT32_DirectoryEntry_t* DirectoryEntrySFNFormat = &DirectoryEntries[i];
            FAT32_LFNDirectoryEntry_t* DirectoryEntryLFNFormat = (FAT32_LFNDirectoryEntry_t*) &DirectoryEntries[i];

            if (DirectoryEntrySFNFormat->FileName[0] == FAT32_DIRECTORY_ENTRIES_IN_CLUSTER_FREE) break;

            // This will only run on a special LFN (long file name) file

            /* We piece together the long file name from the LFN entries, and if it matches 
               RegularFormattedName, we don't need to check the shortened SFN name and can
               simply start reading clusters from the SFN entry. */

            if (DirectoryEntrySFNFormat->Attribute == FAT32_LFN_ATTRIBUTE) {

                // If bit 0x40 is set in the SequenceNum, this is the start of a new LFN chain (last entry)
                // Mask out the flag to determine how many LFN entries this chain has

                if (DirectoryEntryLFNFormat->SequenceNum & 0x40) {
                    LFNChainInProgress = true;
                    NumberOfEntriesInLFNChainToProcess = DirectoryEntryLFNFormat->SequenceNum & 0x1F;
                }

                // If we're on an LFN entry, an LFN chain is most definitely in progress
                // Copy the name characters from this entry into ConstructedLongName
                
                /* As the first LFN in the chain (with that bit set) contains the last characters
                   of the name. The LFN entries are backwards, but name parts 1, 2 and 3 are
                   in ORDER. Huh. 
                   
                   Ok so let's go name part 1, 2 and 3 in order then, but start
                   at the last 13-character chunk of the buffer as each LFN entry can hold 13
                   characters. 
                   
                   We also need to remember that we copy every SECOND byte because
                   the long names that the drive stores are long strings (Unicode). We want to
                   get this to ASCII.
                   
                   Let's also decrement NumberofEntriesInLFNChainToProcess each time to help us
                   decide which 13-character chunk we're writing to. */

                // Decrement first as we're going to the BEGINNING of a 13-character chunk.

                NumberOfEntriesInLFNChainToProcess--;

                uint32_t CurrentBufferIndex = NumberOfEntriesInLFNChainToProcess * FAT32_CHARACTERS_HELD_BY_LFN_ENTRY;

                // Go through each name part - 0x00 = end of name

                // NamePart1

                for (int i = 0; i < 5; i++) {
                    if (DirectoryEntryLFNFormat->NamePart1[i] == 0) break;
                    ConstructedLongName[CurrentBufferIndex++] = (char) DirectoryEntryLFNFormat->NamePart1[i];
                }

                // NamePart2

                for (int i = 0; i < 6; i++) {
                    if (DirectoryEntryLFNFormat->NamePart2[i] == 0) break;
                    ConstructedLongName[CurrentBufferIndex++] = (char) DirectoryEntryLFNFormat->NamePart2[i];
                }

                // NamePart3

                for (int i = 0; i < 2; i++) {
                    if (DirectoryEntryLFNFormat->NamePart3[i] == 0) break;
                    ConstructedLongName[CurrentBufferIndex++] = (char) DirectoryEntryLFNFormat->NamePart3[i];
                }

            }

            // If it's a regular SFN file/folder with a short name OR the SFN entry of a successfully name-matched LFN chain: return the entry

            if (DirectoryEntrySFNFormat->Attribute == FAT32_SFN_ATTRIBUTE  || DirectoryEntrySFNFormat->Attribute == FAT32_DIRECTORY_ATTRIBUTE) {

                /* If there was an LFN chain before this SFN, compare the ConstructedLongName to
                   RegularFormattedName. Also, turn the LFNChainInProgress boolean off as, if this
                   SFN follows an LFN chain, that LFN chain has now come to an end. */

                if (LFNChainInProgress) {

                    if (memcmp(ConstructedLongName, RegularFormattedName, strlen(RegularFormattedName)) == 0) {
                        return DirectoryEntrySFNFormat;
                    }

                    LFNChainInProgress = false;

                }

                // If this SFN wasn't the end of an LFN chain, does it match the FAT32FormattedName if that was built?

                if (memcmp(DirectoryEntrySFNFormat->FileName, FAT32FormattedName, 11) == 0) {
                    return DirectoryEntrySFNFormat;
                }

            }

            // If we've found the file or folder, return its directory entry

            /* The entire page containing this specific entry is kept switched on
                - it is up to the function user to do what they want with it. */

        }

        CurrentCluster = FAT32_GetNextCluster(CurrentCluster);

    }

    // Otherwise, if it couldn't be found, don't maintain the page and return NULL

    Paging_FreePage(DirectoryEntries);

    return NULL;

}


void FAT32_FormatFileName(const char* FileName, char* Destination) {

    // Format the FileName into the 8.3 FAT32 format
    // Leave a space at the end as strangely the match only works with that...

    char FormattedFileName[11];
    memset(FormattedFileName, ' ', 11);

    // To convert a char to uppercase, subtract 32

    for (int i = 0; i < strlen(FileName) - 3 && i < 8; i++) {
        if (FileName[i] >= 'a' && FileName[i] <= 'z') {
            FormattedFileName[i] = FileName[i] - 32;
        } else if (FileName[i] != '.') {
            FormattedFileName[i] = FileName[i];
        }
    }

    for (int i = 0; i < 3; i++) {
        if (FileName[strlen(FileName) - i - 1] >= 'a' && FileName[strlen(FileName) - i - 1] <= 'z') {
            FormattedFileName[11 - i - 1] = FileName[strlen(FileName) - i - 1] - 32;
        } else if (FileName[strlen(FileName) - i - 1] != '.') {
            FormattedFileName[11 - i - 1] = FileName[strlen(FileName) - i - 1];
        }
    }

    memcpy(Destination, FormattedFileName, 11);

}


// This simply capitalises the entire name, then pads it with spaces to 11 characters

void FAT32_FormatFolderName(const char* FolderName, char* Destination) {

    memset(Destination, ' ', 11);

    for (int i = 0; i < strlen(FolderName); i++) {
        if (FolderName[i] >= 'a' && FolderName[i] <= 'z') {
            Destination[i] = FolderName[i] - 32;
        } else {
            Destination[i] = FolderName[i];
        }
    }
    
}


FAT32_BPB_t* FAT32_GetBPB() {
    return BPB;
}
