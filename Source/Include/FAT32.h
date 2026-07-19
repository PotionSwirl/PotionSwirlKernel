#include <stdint.h>

// GPT header in LBA 1 of any storage device that's GPT-formatted
typedef struct __attribute__((packed)) {
    char Signature[8];
    uint32_t Revision;
    uint32_t HeaderSize;
    uint32_t HeaderCRC32;
    uint32_t Reserved;
    uint64_t CurrentLBA;
    uint64_t BackupLBA;
    uint64_t FirstUsableLBA;
    uint64_t LastUsableLBA;
    char DiskGUID[16];
    uint64_t PartitionEntriesLBA;
    uint32_t NumberOfPartitionEntries;
    uint32_t SizeOfPartitionEntry;
    uint32_t PartitionEntriesCRC32;
} GPTHeader_t;

typedef struct __attribute__((packed)) {
    char PartitionTypeGUID[16];       // Offset 0: Unique type GUID (Look for Basic Data)
    char UniquePartitionGUID[16];     // Offset 16: Unique ID for this specific partition
    uint64_t StartingLBA;             // Offset 32: The first sector of this partition
    uint64_t EndingLBA;               // Offset 40: The last sector (inclusive)
    uint64_t Attributes;              // Offset 48: Attribute flags (e.g. bootable)
    char PartitionName[36];           // Offset 56: UTF-16LE partition name
} GPTPartitionEntry_t;

typedef struct __attribute__((packed)) {
    uint8_t  BootJumpInstruction[3];
    uint8_t  OEMName[8];
    // Standard BPB
    uint16_t BytesPerSector;
    uint8_t  SectorsPerCluster;
    uint16_t ReservedSectors;
    uint8_t  NumberOfFATS;
    uint16_t RootEntryCount;
    uint16_t TotalSectors16;
    uint8_t  MediaDescriptor;
    uint16_t FatSize16;
    uint16_t SectorsPerTrack;
    uint16_t NumberOfHeads;
    uint32_t HiddenSectors;
    uint32_t TotalSectors32;
    // FAT32 Extended Boot Record (EBR)
    uint32_t SectorsPerFat32;
    uint16_t ExtendedFlags;
    uint16_t FileSystemVersion;
    uint32_t RootCluster;
    uint16_t FSInfoSector;
    uint16_t BackupBootSector;
    uint8_t  Reserved1[12];
    uint8_t  DriveNumber;
    uint8_t  Reserved2;
    uint8_t  BootSignature;
    uint32_t VolumeId;
    uint8_t  VolumeLabel[11];
    uint8_t  FileSystemType[8];
    // Bootable partition remainder
    uint8_t  BootCode[420];
    uint16_t BootSectorSignature; // 0xAA55
} FAT32_BPB_t;

extern void (*FAT32_DriveRead)(uint64_t StartingLBA, uint16_t NumberOfBlocks, void* PageAlignedBuffer);
void FAT32_Initialise(void (*FAT32_DriveReadArgument), uint64_t BytesPerBlock);
