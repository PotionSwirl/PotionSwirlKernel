#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <efi.h>
#include "Bootx64.h"

#define PAGING_PAGE_SIZE 0x1000
#define PAGING_NO_PAGE_AVAILABLE (NULL)

// Official flags
#define PAGING_PAGE_PRESENT                    (1ULL)
#define PAGING_PAGE_WRITABLE                   (1ULL << 1)
#define PAGING_STRUCTURE_ENTRY_STRIP_FLAGS     (0x000FFFFFFFFFF000ULL)

#define PAGING_GET_NEXT_EFI_MEMORY_DESCRIPTOR(Descriptor, Size) \
    ((EFI_MEMORY_DESCRIPTOR*) (((uint8_t*) Descriptor) + Size))

void Paging_Initialise(BootInfo_t BootInfo);
static void LoadCR3();
static void MapMemory(void* VirtualAddress, void* PhysicalAddress, uint64_t Flags, uint64_t NumberOfPages);
static void MapPage(void* VirtualAddress, void* PhysicalAddress, uint64_t Flags);
static uint64_t* GetNextPagingStructure(uint64_t* CurrentPagingStructure, uint64_t Index);
static void* AllocatePage();
static void FreePage(void* Address);
static void MemoryBitmapInitialise(MemoryInfo_t MemoryInfo);
static void MemoryBitmapSet(uint64_t Page);
static void MemoryBitmapClear(uint64_t Page);
static bool MemoryBitmapTestPage(uint64_t Page);
static uint64_t PageAlignRoundUp(uint64_t Value);
static uint64_t BytesToPages(uint64_t Bytes);
