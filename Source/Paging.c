#include <stdint.h>
#include <stdbool.h>
#include "Include/Paging.h"
#include "Include/Bootx64.h"
#include "Include/Libraries.h"
#include "Include/APIC.h"
#include "Include/Graphics.h"
#include "Include/CPU.h"


// The structures that keep track of our pages
static uint64_t* PML4;

// The size of our memory bitmap will change based on the available RAM,
static uint8_t* MemoryBitmap;
static uint64_t MemoryBitmapSize;
static uint64_t HighestMemoryAddress;
static uint64_t UsableMemory = 0;
static uint64_t TotalPages;
static uint64_t FreePages = 0;


// Everything that sets up paging when we start up

void Paging_Initialise(BootInfo_t* BootInfo) {

    // Set up the bitmap used to keep track of our pages
    MemoryBitmapInitialise(&BootInfo->MemoryInfo);

    // Set up PML4
    PML4 = Paging_AllocatePage();
    memset(PML4, 0, PAGING_PAGE_SIZE);

    // Identity-map the following memory:

    // Conventional memory

    void* PhysicalAddress;

    EFI_MEMORY_DESCRIPTOR* CurrentMemoryDescriptor = (EFI_MEMORY_DESCRIPTOR*) BootInfo->MemoryInfo.MemoryMap;
    EFI_MEMORY_DESCRIPTOR* LastMemoryDescriptor = (EFI_MEMORY_DESCRIPTOR*) (((uint8_t*) BootInfo->MemoryInfo.MemoryMap) + BootInfo->MemoryInfo.MemoryMapSize);

    while (CurrentMemoryDescriptor < LastMemoryDescriptor) {

        if  (CurrentMemoryDescriptor->Type == EfiConventionalMemory
            || CurrentMemoryDescriptor->Type == EfiLoaderCode
            || CurrentMemoryDescriptor->Type == EfiLoaderData
            || CurrentMemoryDescriptor->Type == EfiBootServicesCode
            || CurrentMemoryDescriptor->Type == EfiBootServicesData
            || CurrentMemoryDescriptor->Type == EfiACPIReclaimMemory) {

            PhysicalAddress = (void*) Paging_PageAlignRoundDown(CurrentMemoryDescriptor->PhysicalStart);
            Paging_MapMemory(PhysicalAddress, PhysicalAddress, PAGING_PAGE_PRESENT | PAGING_PAGE_WRITABLE, CurrentMemoryDescriptor->NumberOfPages);

        }

        CurrentMemoryDescriptor = PAGING_GET_NEXT_EFI_MEMORY_DESCRIPTOR(CurrentMemoryDescriptor, BootInfo->MemoryInfo.DescriptorSize);

    }

    // Framebuffer

    PhysicalAddress = (void*) Paging_PageAlignRoundDown(BootInfo->GraphicsOutputProtocol->Mode->FrameBufferBase);
    Paging_MapMemory(PhysicalAddress, PhysicalAddress, PAGING_PAGE_PRESENT | PAGING_PAGE_WRITABLE, BytesToPages(BootInfo->GraphicsOutputProtocol->Mode->FrameBufferSize));

    // ECAM

    PhysicalAddress = (void*) BootInfo->ECAMBaseAddress;
    Paging_MapMemory(PhysicalAddress, PhysicalAddress, PAGING_PAGE_PRESENT | PAGING_PAGE_WRITABLE | PAGING_PAGE_WRITE_THROUGH | PAGING_PAGE_CACHE_DISABLE | PAGING_PAGE_NO_EXECUTE, BytesToPages(BootInfo->ECAMSize));

    // APIC

    PhysicalAddress = (void*) (uint64_t) BootInfo->IOAPICAddress;
    Paging_MapPage(PhysicalAddress, PhysicalAddress, PAGING_PAGE_PRESENT | PAGING_PAGE_WRITABLE | PAGING_PAGE_WRITE_THROUGH | PAGING_PAGE_CACHE_DISABLE | PAGING_PAGE_NO_EXECUTE);

    PhysicalAddress = (void*) APIC_GetBase();
    Paging_MapPage(PhysicalAddress, PhysicalAddress, PAGING_PAGE_PRESENT | PAGING_PAGE_WRITABLE | PAGING_PAGE_WRITE_THROUGH | PAGING_PAGE_CACHE_DISABLE | PAGING_PAGE_NO_EXECUTE);

    // Enable no-execute flag

    Paging_EnableNoExecuteFlag();

    // And finally, switch to paging!

    LoadCR3();

}

// Switching to paging mode

void LoadCR3() {
    asm volatile(
        "MOV CR3, %0"
        :
        : "r"(PML4)
        : "memory"
    );
}


// Enabling the no-execute (NX) flag for paging

void Paging_EnableNoExecuteFlag() {

    // Use CPUID for extended features and check bit 20 of EDX - if set, NX is supported

    uint32_t EDX = CPU_GetCPUID(CPU_EXTENDED_PROCESSOR_SIGNATURE_AND_FEATURE_BITS_EAX, CPU_SELECT_EDX);
    
    if (!(EDX & (1 << 20))) {
        printf("This CPU doesn't support the NX flag for paging!\n");
        printf("Halting...\n");
        while(true);
    }

    // Read the IA32_EFER MSR, set bit 11, and write back

    uint64_t EFER = CPU_ReadMSR(CPU_IA32_EFER_MSR);
    EFER |= (1ULL << 11);
    CPU_WriteMSR(CPU_IA32_EFER_MSR, EFER);
    
}


// Page mapping

// Bits 0-11  = offset
//      12-20 = PT index
//      21-29 = PD index
//      30-38 = PDPT index
//      39-47 = PML4 index
//      48-63 = required extension

void Paging_MapMemory(void* VirtualAddress, void* PhysicalAddress, uint64_t Flags, uint64_t NumberOfPages) {

    for (uint64_t i = 0; i < NumberOfPages; i++) {
        Paging_MapPage(VirtualAddress, PhysicalAddress, Flags);
        VirtualAddress += PAGING_PAGE_SIZE;
        PhysicalAddress += PAGING_PAGE_SIZE;
    }

}


void Paging_MapPage(void* VirtualAddress, void* PhysicalAddress, uint64_t Flags) {

    uint64_t ConvertedVirtualAddress = (uint64_t) VirtualAddress;

    // Get indexes in each level of the hierarchy where our address will be mapped
    uint64_t PML4Index = (ConvertedVirtualAddress >> 39) & 0x1FF;
    uint64_t PDPTIndex = (ConvertedVirtualAddress >> 30) & 0x1FF;
    uint64_t PDIndex = (ConvertedVirtualAddress >> 21) & 0x1FF;
    uint64_t PTIndex = (ConvertedVirtualAddress >> 12) & 0x1FF;

    // Walk it down
    uint64_t* PDPT = GetNextPagingStructure(PML4, PML4Index);
    uint64_t* PD = GetNextPagingStructure(PDPT, PDPTIndex);
    uint64_t* PT = GetNextPagingStructure(PD, PDIndex);

    // Plug the address into the page table entry
    PT[PTIndex] = ((uint64_t) PhysicalAddress) | Flags | PAGING_PAGE_PRESENT;

}


uint64_t* GetNextPagingStructure(uint64_t* CurrentPagingStructure, uint64_t Index) {

    // Check if the entry is present

    if (CurrentPagingStructure[Index] & PAGING_PAGE_PRESENT) {
        // Strip the current flags to get the address
        return (uint64_t*) (CurrentPagingStructure[Index] & PAGING_STRUCTURE_ENTRY_STRIP_FLAGS);
    }

    // If it isn't, create a new entry

    uint64_t* NewStructure = Paging_AllocatePage();

    if (NewStructure == NULL) return NULL;

    memset(NewStructure, 0, PAGING_PAGE_SIZE);

    CurrentPagingStructure[Index] = ((uint64_t) NewStructure) | PAGING_PAGE_PRESENT | PAGING_PAGE_WRITABLE;

    return NewStructure;

}


// Page allocation - scan through bitmap for free page, calculate address and return

void* Paging_AllocatePage() {

    for (uint64_t i = 0; i < TotalPages; i++) {
        if (!(Paging_MemoryBitmapTestPage(i))) {
            Paging_MemoryBitmapSet(i);
            FreePages--;
            return ((void*) (i * PAGING_PAGE_SIZE));
        }
    }

    return PAGING_NO_PAGE_AVAILABLE;

}


void Paging_FreePage(void* Address) {

    uint64_t Page;
    
    // Account for page-unaligned addresses
    if (((uint64_t) Address) % PAGING_PAGE_SIZE != 0) {
        Page = (PageAlignRoundUp((uint64_t) Address) - PAGING_PAGE_SIZE) / PAGING_PAGE_SIZE;
    } else {
        Page = ((uint64_t) Address) / PAGING_PAGE_SIZE;
    }

    // Check if it really IS occupied just in case
    if (Paging_MemoryBitmapTestPage(Page)) {
        Paging_MemoryBitmapClear(Page);
        FreePages++;
    }

}


// Memory bitmap helpers

void MemoryBitmapInitialise(MemoryInfo_t* MemoryInfo) {

    // Find the size the bitmap should be, as well as highest memory address

    EFI_MEMORY_DESCRIPTOR* FirstMemoryDescriptor = (EFI_MEMORY_DESCRIPTOR*) MemoryInfo->MemoryMap;
    EFI_MEMORY_DESCRIPTOR* LastMemoryDescriptor = (EFI_MEMORY_DESCRIPTOR*) (((uint8_t*) MemoryInfo->MemoryMap) + MemoryInfo->MemoryMapSize);

    EFI_MEMORY_DESCRIPTOR* CurrentMemoryDescriptor = FirstMemoryDescriptor;

    while (CurrentMemoryDescriptor < LastMemoryDescriptor) {

        if (CurrentMemoryDescriptor->Type == EfiConventionalMemory) {
            uint64_t CurrentMemoryRegionEnd = CurrentMemoryDescriptor->PhysicalStart + CurrentMemoryDescriptor->NumberOfPages * PAGING_PAGE_SIZE;
            if (CurrentMemoryRegionEnd > HighestMemoryAddress) HighestMemoryAddress = CurrentMemoryRegionEnd;
            UsableMemory += CurrentMemoryDescriptor->NumberOfPages * PAGING_PAGE_SIZE;
        }

        CurrentMemoryDescriptor = PAGING_GET_NEXT_EFI_MEMORY_DESCRIPTOR(CurrentMemoryDescriptor, MemoryInfo->DescriptorSize);

    }

    HighestMemoryAddress = PageAlignRoundUp(HighestMemoryAddress);
    TotalPages = HighestMemoryAddress / PAGING_PAGE_SIZE;
    MemoryBitmapSize = PageAlignRoundUp((TotalPages + 7) / 8);

    // Find an appropriate place in RAM to place the bitmap

    CurrentMemoryDescriptor = FirstMemoryDescriptor;
    EFI_MEMORY_DESCRIPTOR* LargestRegion = NULL;

    while (CurrentMemoryDescriptor < LastMemoryDescriptor) {

        if (CurrentMemoryDescriptor->Type == EfiConventionalMemory
            && (!LargestRegion || CurrentMemoryDescriptor->NumberOfPages >= LargestRegion->NumberOfPages))
        {
            
            LargestRegion = CurrentMemoryDescriptor;

        }

        CurrentMemoryDescriptor = PAGING_GET_NEXT_EFI_MEMORY_DESCRIPTOR(CurrentMemoryDescriptor, MemoryInfo->DescriptorSize);

    }

    MemoryBitmap = (uint8_t*) PageAlignRoundUp(LargestRegion->PhysicalStart);
    
    // Reserve EVERY page in the bitmap

    memset(MemoryBitmap, 0b11111111, MemoryBitmapSize);

    // Free every conventional memory region

    CurrentMemoryDescriptor = FirstMemoryDescriptor;

    while (CurrentMemoryDescriptor < LastMemoryDescriptor) {

        if (CurrentMemoryDescriptor->Type == EfiConventionalMemory) {

            uint64_t StartingPage = CurrentMemoryDescriptor->PhysicalStart / PAGING_PAGE_SIZE;

            for (uint64_t i = 0; i < CurrentMemoryDescriptor->NumberOfPages; i++) {
                Paging_MemoryBitmapClear(StartingPage + i);
                FreePages++;
            }

        }

        CurrentMemoryDescriptor = PAGING_GET_NEXT_EFI_MEMORY_DESCRIPTOR(CurrentMemoryDescriptor, MemoryInfo->DescriptorSize);

    }

    /* Reserve page 0 so that we don't confuse a returned page address
       of 0x0 with NULL from Paging_AllocatePage(). */

    Paging_MemoryBitmapSet(0);

    // Reserve the bitmap pages

    uint64_t MemoryBitmapStartingPage = ((uint64_t) MemoryBitmap) / PAGING_PAGE_SIZE;
    
    for (uint64_t i = 0; i < BytesToPages(MemoryBitmapSize); i++) {
        Paging_MemoryBitmapSet(MemoryBitmapStartingPage + i);
        FreePages--;
    }

}


void Paging_MemoryBitmapSet(uint64_t Page) {
    MemoryBitmap[Page / 8] |= (1 << (Page % 8));
}


void Paging_MemoryBitmapClear(uint64_t Page) {
    MemoryBitmap[Page / 8] &= ~(1 << (Page % 8));
}


bool Paging_MemoryBitmapTestPage(uint64_t Page) {
    return ((MemoryBitmap[Page / 8] & (1 << (Page % 8))) != 0);
}


// Paging math helpers

uint64_t PageAlignRoundUp(uint64_t Value) {
    return (Value + 0xFFF) & ~0xFFF;
}


uint64_t Paging_PageAlignRoundDown(uint64_t Value) {
    return Value / PAGING_PAGE_SIZE * PAGING_PAGE_SIZE;
}


uint64_t BytesToPages(uint64_t Bytes) {
    return (Bytes + PAGING_PAGE_SIZE - 1) / PAGING_PAGE_SIZE;
}


uint64_t Paging_GetTotalPages() {
    return TotalPages;
}


// Page fault handler

void Paging_Vector14ExceptionHandlePassover(uint64_t PageFaultAddress, uint64_t ErrorCode) {
    // Graphics_ClearScreen();
    printf("EXCEPTION: PAGE FAULT\n");
    printf("---------------------------------------\n");
    printf("Page fault address: 0x%x\n", PageFaultAddress);
    printf("Error code: 0x%x\n", ErrorCode);
    while(true);
}
