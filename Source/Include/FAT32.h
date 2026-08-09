#pragma once

#include <stdint.h>

#define FAT32_CLUSTER_ENTRY_SIZE                   4
#define FAT32_EOF                                  0x0FFFFFF8
#define FAT32_DIRECTORY_ENTRIES_IN_CLUSTER_FREE   '\0'
#define FAT32_LFN_ATTRIBUTE                        0x0F
#define FAT32_SFN_ATTRIBUTE                        0x20
#define FAT32_DIRECTORY_ATTRIBUTE                  0x10
#define FAT32_CHARACTERS_HELD_BY_LFN_ENTRY         13

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
} FAT32_GPTHeader_t;

typedef struct __attribute__((packed)) {
    char PartitionTypeGUID[16];       // Offset 0: Unique type GUID (Look for Basic Data)
    char UniquePartitionGUID[16];     // Offset 16: Unique ID for this specific partition
    uint64_t StartingLBA;             // Offset 32: The first sector of this partition
    uint64_t EndingLBA;               // Offset 40: The last sector (inclusive)
    uint64_t Attributes;              // Offset 48: Attribute flags (e.g. bootable)
    char PartitionName[36];           // Offset 56: UTF-16LE partition name
} FAT32_GPTPartitionEntry_t;

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
    uint16_t FATSize16;
    uint16_t SectorsPerTrack;
    uint16_t NumberOfHeads;
    uint32_t HiddenSectors;
    uint32_t TotalSectors32;
    // FAT32 Extended Boot Record (EBR)
    uint32_t SectorsPerFAT32;
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

typedef struct __attribute__((packed)) {
    char     FileName[11];      // 8 chars name, 3 chars extension
    uint8_t  Attribute;         // File attributes (Read Only, Hidden, System, etc.)
    uint8_t  NTReserved;        // Reserved for Use by Windows NT
    uint8_t  CreationTimeTenth; // Millisecond stamp at file creation
    uint16_t CreationTime;      // Time file was created
    uint16_t CreationDate;      // Date file was created
    uint16_t LastAccessDate;    // Last access date
    uint16_t FirstClusterHigh;  // High 16 bits of this entry's first cluster
    uint16_t WriteTime;         // Time of last write
    uint16_t WriteDate;         // Date of last write
    uint16_t FirstClusterLow;   // Low 16 bits of this entry's first cluster
    uint32_t FileSize;          // 32-bit DWORD holding file size in bytes
} FAT32_DirectoryEntry_t;

typedef struct __attribute__((packed)) {
    uint8_t  SequenceNum;       // Sequence number and flags (e.g., 0x41)
    uint16_t NamePart1[5];      // Characters 1-5 of the file name (UTF-16)
    uint8_t  Attributes;        // Always 0x0F (ATTR_LONG_NAME)
    uint8_t  Type;              // Long entry type, always 0x00
    uint8_t  Checksum;          // Checksum of the associated short file name
    uint16_t NamePart2[6];      // Characters 6-11 of the file name (UTF-16)
    uint16_t StartingCluster;   // Always 0x0000 for compatibility
    uint16_t NamePart3[2];      // Characters 12-13 of the file name (UTF-16)
} FAT32_LFNDirectoryEntry_t;

extern void (*FAT32_DriveRead)(uint64_t StartingLBA, uint16_t NumberOfBlocks, void* PageAlignedBuffer);
void FAT32_Initialise(void (*FAT32_DriveReadArgument), uint64_t BytesPerBlock);
uint32_t FAT32_ClusterToSector(uint32_t Cluster);
uint32_t FAT32_GetNextCluster(uint32_t Cluster);
FAT32_DirectoryEntry_t* FAT32_GetDirectoryEntry(const char* FilePath);
FAT32_DirectoryEntry_t* FAT32_FindInDirectory(uint32_t DirectoryFirstCluster, const char* RegularFormattedName);
void FAT32_FormatFileName(const char* FileName, char* Destination);
void FAT32_FormatFolderName(const char* FolderName, char* Destination);
FAT32_BPB_t* FAT32_GetBPB();
