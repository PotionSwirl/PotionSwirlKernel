#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <efi.h>
#include "Bootx64.h"

#define PAGING_PAGE_SIZE                0x1000
#define PAGING_NO_PAGE_AVAILABLE        (NULL)

// Official flags
#define PAGING_PAGE_PRESENT             (1ULL)
#define PAGING_PAGE_WRITABLE            (1ULL << 1)
#define PAGING_PAGE_USER                (1ULL << 2)
#define PAGING_PAGE_WRITE_THROUGH       (1ULL << 3)
#define PAGING_PAGE_CACHE_DISABLE       (1ULL << 4)
#define PAGING_PAGE_NO_EXECUTE          (1ULL << 63)

#define PAGING_STRUCTURE_ENTRY_STRIP_FLAGS     (0x000FFFFFFFFFF000ULL)

#define PAGING_GET_NEXT_EFI_MEMORY_DESCRIPTOR(Descriptor, Size) \
    ((EFI_MEMORY_DESCRIPTOR*) (((uint8_t*) Descriptor) + Size))

void Paging_Initialise(BootInfo_t* BootInfo);
void LoadCR3();
void Paging_EnableNoExecuteFlag();
void Paging_MapMemory(void* VirtualAddress, void* PhysicalAddress, uint64_t Flags, uint64_t NumberOfPages);
void Paging_MapPage(void* VirtualAddress, void* PhysicalAddress, uint64_t Flags);
uint64_t* GetNextPagingStructure(uint64_t* CurrentPagingStructure, uint64_t Index);
void* Paging_AllocatePage();
void Paging_FreePage(void* Address);
void MemoryBitmapInitialise(MemoryInfo_t* MemoryInfo);
void Paging_MemoryBitmapSet(uint64_t Page);
void Paging_MemoryBitmapClear(uint64_t Page);
bool Paging_MemoryBitmapTestPage(uint64_t Page);
uint64_t PageAlignRoundUp(uint64_t Value);
uint64_t Paging_PageAlignRoundDown(uint64_t Value);
uint64_t BytesToPages(uint64_t Bytes);
uint64_t Paging_GetTotalPages();
void Paging_Vector14ExceptionHandlePassover(uint64_t PageFaultAddress, uint64_t ErrorCode);
