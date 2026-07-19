#include <efi.h>
#include <efilib.h>
#include <stdbool.h>
#include "Include/ELF.h"
#include "Include/ACPI.h"
#include "Include/Bootx64.h"
#include "Include/PSF2Font.h"   // REMOVE LATER
#include "Include/PCI.h"


// For gnu-efi, function MUST be called 'efi_main'!
EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {

    // Set up libraries that allow us to use UEFI Boot Services like Print
    InitializeLib(ImageHandle, SystemTable);

    // Clear screen
    uefi_call_wrapper(ST->ConOut->ClearScreen, 1, ST->ConOut);

    // Status: stores failure of a UEFI operation if applicable.
    EFI_STATUS Status;

    /* -------------------------------------------------------------
       | 1. UEFI Loaded Image Protocol                             |
       ------------------------------------------------------------- */

    EFI_LOADED_IMAGE *LoadedImage;
    Status = uefi_call_wrapper(BS->OpenProtocol, 6, ImageHandle, &gEfiLoadedImageProtocolGuid, (void**) &LoadedImage, ImageHandle, NULL, EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
    
    if (EFI_ERROR(Status)) {
        Print(L"Loaded Image Protocol failed.\r\n");
        while(true);
    }

    /* -------------------------------------------------------------
       | 2. Get filesystem protocol.                               |
       ------------------------------------------------------------- */

    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    Status = uefi_call_wrapper(BS->OpenProtocol, 6, LoadedImage->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (void**) &FileSystem, ImageHandle, NULL, EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
    
    if (EFI_ERROR(Status)) {
        Print(L"Filesystem Protocol failed.\r\n");
        while(true);
    }

    /* -------------------------------------------------------------
       | 3. Open root directory of USB we're in.                   |
       ------------------------------------------------------------- */

    EFI_FILE *Root;
    Status = uefi_call_wrapper(FileSystem->OpenVolume, 2, FileSystem, &Root);
    
    if (EFI_ERROR(Status)) {
        Print(L"Failed to open root directory.\r\n");
        while(true);
    }

    /* -------------------------------------------------------------
       | 4. Open Kernel.elf.                                       |
       ------------------------------------------------------------- */

    EFI_FILE *KernelFile;
    Status = uefi_call_wrapper(Root->Open, 5, Root, &KernelFile, L"Kernel.elf", EFI_FILE_MODE_READ, 0);
    
    if (EFI_ERROR(Status)) {
        Print(L"Failed to open Kernel.elf.\r\n");
        while(true);
    }

    /* -------------------------------------------------------------
       | 5. Get size of Kernel.elf                                 |                                    
       ------------------------------------------------------------- */

    EFI_FILE_INFO *KernelFileInfo;
    UINTN KernelFileInfoSize = sizeof(EFI_FILE_INFO) + 200;
    Status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, KernelFileInfoSize, (void**) &KernelFileInfo);
    
    if (EFI_ERROR(Status)) {
        Print(L"Failed to allocate memory for Kernel.elf file info.\r\n");
        while(true);
    }

    EFI_GUID KernelFileInfoGUID = EFI_FILE_INFO_ID;
    Status = uefi_call_wrapper(KernelFile->GetInfo, 4, KernelFile, &KernelFileInfoGUID, &KernelFileInfoSize, KernelFileInfo);
    
    if (EFI_ERROR(Status)) {
        Print(L"Failed to get file info for Kernel.elf.\r\n");
        while(true);
    }

    /* -------------------------------------------------------------
       | 6. Allocate buffer for Kernel.elf                         |                                
       ------------------------------------------------------------- */

    void *KernelBuffer;
    Status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, KernelFileInfo->FileSize, &KernelBuffer);
    
    if (EFI_ERROR(Status)) {
        Print(L"Failed to allocate memory for Kernel.elf.\r\n");
        while(true);
    }

    /* -------------------------------------------------------------
       | 7. Read Kernel.elf into memory.                           |                              
       ------------------------------------------------------------- */

    UINTN KernelFileSize = KernelFileInfo->FileSize;
    Status = uefi_call_wrapper(KernelFile->Read, 3, KernelFile, &KernelFileSize, KernelBuffer);
    
    if (EFI_ERROR(Status)) {
        Print(L"Failed to read Kernel.elf into memory.\r\n");
        while(true);
    }

    /* -------------------------------------------------------------
       | 8. Parse Kernel.elf ELF header and load its segments.     |                              
       ------------------------------------------------------------- */

    elf64_elf_header_t* ELFHeader = (elf64_elf_header_t*) KernelBuffer;

    // An ELF header must begin with 0x7F, followed by the characters of 'ELF'.
    if (ELFHeader->ELF_Identification[0] != 0x7F 
        || ELFHeader->ELF_Identification[1] != 'E' 
        || ELFHeader->ELF_Identification[2] != 'L' 
        || ELFHeader->ELF_Identification[3] != 'F') 
    {
        Print(L"Hmmm...seems Kernel.elf isn't actually an ELF...");
        while(true);
    }
    
    // Find where program headers start.
    elf64_program_header_t* ProgramHeaders = (elf64_program_header_t*) (uint8_t*) (KernelBuffer + ELFHeader->ELF_ProgramHeadersOffset);

    // Loop through the program headers, each representing a segment
    for (UINT16 i = 0; i < ELFHeader->ELF_ProgramHeaderNumber; i++) {

        elf64_program_header_t* SingleProgramHeader = &ProgramHeaders[i];

        // Ignore the segment if it is marked as non-loadable.
        if (SingleProgramHeader->Program_HeaderType != ELF_SEGMENT_IS_LOADABLE) {
            continue;
        }

        // Get the rounded up number of 4KB pages needed for this segment.
        // @code: UINTN PagesNeeded = (SingleProgramHeader->Program_HeaderMemorySize + 0x1000) / 0x1000 * 0x1000
        // Or faster, bitwise version:
        UINTN PagesNeededForSegment = (SingleProgramHeader->Program_HeaderMemorySize + 4095) / 4096;

        // Get the physical address in memory where we need to load the segment (pages identity-mapped for now).
        EFI_PHYSICAL_ADDRESS SegmentAddress = SingleProgramHeader->Program_HeaderPhysicalAddress;

        // Allocate physical pages in RAM for the program where ELF asks them to be.
        Status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAddress, EfiLoaderData, PagesNeededForSegment, &SegmentAddress);

        if (EFI_ERROR(Status)) {
            Print(L"Failed to allocate pages for an ELF segment in Kernel.elf.\r\n");
            while(true);
        }

        // Copy bytes of segment into RAM. Header just means segment in this case, i.e. Program_HeaderOffset = offset of SEGMENT
        CopyMem((void*) SegmentAddress, (uint8_t*) KernelBuffer + SingleProgramHeader->Program_HeaderOffset, SingleProgramHeader->Program_HeaderFileSize);

        /* Zero out the remaining bytes in segment, starting from the perceived end of the segment to the end of the actual memory 
           it takes up, i.e. the .bss section. */
        SetMem((uint8_t*) SegmentAddress + SingleProgramHeader->Program_HeaderFileSize, SingleProgramHeader->Program_HeaderMemorySize - SingleProgramHeader->Program_HeaderFileSize, 0);

    }

    ///// REMOVE LATER - load Terminus font file into memory
    EFI_FILE *FontFile;
    Status = uefi_call_wrapper(Root->Open, 5, Root, &FontFile, L"Terminus.psf", EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to open Font.psf.\r\n");
        while(true);
    }
    EFI_FILE_INFO *FontFileInfo;
    UINTN FontFileInfoSize = sizeof(EFI_FILE_INFO) + 200;
    Status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, FontFileInfoSize, (void**) &FontFileInfo);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to allocate memory for Font.psf file info.\r\n");
        while(true);
    }
    EFI_GUID FontFileInfoGUID = EFI_FILE_INFO_ID;
    Status = uefi_call_wrapper(FontFile->GetInfo, 4, FontFile, &FontFileInfoGUID, &FontFileInfoSize, FontFileInfo);
    if (EFI_ERROR(Status)) {
        Print(L"Failed get file info for Font.psf.\r\n");
        while(true);
    }
    void *FontBufferArgument;
    Status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, FontFileInfo->FileSize, &FontBufferArgument);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to allocate memory for Font.psf.\r\n");
        while(true);
    }
    UINTN FontFileSize = FontFileInfo->FileSize;
    Status = uefi_call_wrapper(FontFile->Read, 3, FontFile, &FontFileSize, FontBufferArgument);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to read Font.psf into memory.\r\n");
        while(true);
    }
    PSF2Font_FileHeader* FontBufferArgument2 = (PSF2Font_FileHeader*) FontBufferArgument;
    UINTN GlyphBytesSize = FontBufferArgument2->GlyphCount * FontBufferArgument2->GlyphSize;
    UINTN TotalFontMemorySize = sizeof(PSF2Font_FileHeader) + GlyphBytesSize;
    PSF2Font_FileHeader* SafeFontBufferArgument = NULL;
    Status = uefi_call_wrapper(
        ST->BootServices->AllocatePool, 
        3, 
        EfiRuntimeServicesData, 
        TotalFontMemorySize, 
        (VOID**)&SafeFontBufferArgument
    );
    uefi_call_wrapper(
        ST->BootServices->CopyMem, 
        3, 
        SafeFontBufferArgument, 
        FontBufferArgument2, 
        sizeof(PSF2Font_FileHeader)
    );
    uint8_t* DestinationGlyphBytes = (uint8_t*) SafeFontBufferArgument + sizeof(PSF2Font_FileHeader);
    uint8_t* SourceGlyphBytes = (uint8_t*) FontBufferArgument + FontBufferArgument2->HeaderSize;
    uefi_call_wrapper(ST->BootServices->CopyMem, 3, DestinationGlyphBytes, SourceGlyphBytes, GlyphBytesSize);

    /* -------------------------------------------------------------
       | 9. Find devices.                                          |                 
       ------------------------------------------------------------- */

    ACPI_RSDP2_t* RSDP;
    EFI_GUID ACPI2_GUID = ACPI_20_TABLE_GUID;

    // We want to find the Multiple APIC Description Table (MADT).

    // Let's find the RSDP first.
    // Search through UEFI tables.
    for (UINTN i = 0; i < ST->NumberOfTableEntries; i++) {

        EFI_CONFIGURATION_TABLE* Table = &ST->ConfigurationTable[i];

        // Check if it's the RSDP.
        // If so, save it.
        if (CompareGuid(&Table->VendorGuid, &ACPI2_GUID) == 0) {
            RSDP = (ACPI_RSDP2_t*) Table->VendorTable;
            // Stop searching. 
            break;
        }

    }

    // Verify if the RSDP really DOES exist.
    if (!RSDP) {
        Print(L"Failed to find ACPI RSDP!\r\n");
        while(true);
    }

    // Parse XSDT to find "APIC" entry, which = the MADT
    ACPI_XSDT_t* XSDT = (ACPI_XSDT_t*) RSDP->XSDTAddress;

    // Get number of entries in XSDT - our struct doesn't specify it.
    // Each XSDT entry is a uint64_t pointer.
    uint32_t XSDTEntryCount = (XSDT->Header.Length - sizeof(ACPI_SDTHeader_t)) / sizeof(uint64_t);

    // Finally, search for the MADT and MCFG - that's all we're here for!
    // MCFG = Memory Mapped Configuration Space
    ACPI_MADT_t* MADT;
    ACPI_MCFG_t* MCFG;
    uint64_t ECAMBaseAddressArgument;
    uint64_t ECAMSizeArgument;

    for (uint32_t i = 0; i < XSDTEntryCount; i++) {

        ACPI_SDTHeader_t* SDTHeader = (ACPI_SDTHeader_t*) XSDT->Entries[i];
    
        // Stop searching if we've found them.

        if (CompareMem(SDTHeader->Signature, "APIC", 4) == 0) {
            MADT = (ACPI_MADT_t*) SDTHeader;
        }

        if (CompareMem(SDTHeader->Signature, "MCFG", 4) == 0) {
            MCFG = (ACPI_MCFG_t*) SDTHeader;
            ACPI_MCFG_Entry_t* Entry = &(MCFG->Entries[0]);
            // Only care about first entry in MCFG.
            ECAMBaseAddressArgument = Entry->BaseAddress;
            ECAMSizeArgument = (Entry->EndBus - Entry->StartBus + 1) * 0x100000;
        }

    }

    // Verify that we actually found it.

    if (!MADT) {
        Print(L"Failed to find MADT for APIC!\r\n");
        while(true);
    }

    if (!MCFG) {
        Print(L"Failed to find MCFG!\r\n");
        while(true);
    }

    // Scan MADT entries to find the IOAPIC.

    uint8_t* MADTEntryCursor = MADT->Entries;
    uint8_t* MADTEntriesEnd = (uint8_t*) MADT + MADT->Header.Length;
    uint32_t IOAPICAddressArgument;

    while (MADTEntryCursor < MADTEntriesEnd) {

        uint8_t MADTEntryType = MADTEntryCursor[0];
        uint8_t MADTEntryLength = MADTEntryCursor[1];

        // Type 1 = IOAPIC.
        if (MADTEntryType == 1) {
            ACPI_IOAPIC_Entry_t* IOAPIC = (ACPI_IOAPIC_Entry_t*) MADTEntryCursor;
            IOAPICAddressArgument = IOAPIC->IOAPICAddress;
        }

        MADTEntryCursor += MADTEntryLength;

    }


    /* -------------------------------------------------------------
       | 10. Get Graphics Output Protocol (GOP) and memory map.    |                      
       ------------------------------------------------------------- */
    
    // Get GOP
    EFI_GUID GraphicsOutputProtocol_GUID = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL* GraphicsOutputProtocolArgument;
    Status = uefi_call_wrapper(BS->LocateProtocol, 3, &GraphicsOutputProtocol_GUID, NULL, (void**) &GraphicsOutputProtocolArgument);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to get Graphics Output Protocol.\r\n");
        while(true);
    }

    // Passing in a memory map size of 0 tricks the firmware into telling us how much we ACTUALLY need, in MemoryMapSize.
    UINTN MemoryMapSize = 0;
    EFI_MEMORY_DESCRIPTOR* MemoryMap;
    
    // Memory map key being staying the same = memory hasn't been altered since we've been doing all these calls.
    UINTN MemoryMapKey = 0;
    UINTN DescriptorSize;
    UINT32 DescriptorVersion;

    // First call of this = that trick.
    Status = uefi_call_wrapper(BS->GetMemoryMap, 5, &MemoryMapSize, MemoryMap, &MemoryMapKey, &DescriptorSize, &DescriptorVersion);

    /* Allocate not the space of one, but TWO descriptors more, to act as a safety buffer as new memory
       fragments may be created in this process. */
    MemoryMapSize += 2 * DescriptorSize;
    Status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, MemoryMapSize, (void**) &MemoryMap);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to allocate memory for map.\r\n");
        while(true);
    }

    // Second call = gives us the actual stuff.
    Status = uefi_call_wrapper(BS->GetMemoryMap, 5, &MemoryMapSize, MemoryMap, &MemoryMapKey, &DescriptorSize, &DescriptorVersion);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to get memory map.\r\n");
        while(true);
    }

    /* -------------------------------------------------------------
       | 11. Exit boot services and jump to kernel!                |                    
       ------------------------------------------------------------- */
    
    // And finally, exit boot services, passing in our map key to prove that memory hasn't been altered after fetching it.
    Status = uefi_call_wrapper(BS->ExitBootServices, 2, ImageHandle, MemoryMapKey);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to exit boot services.\r\n");
        while(true);
    }

    // Create a BootInfo struct with essential information to pass to the kernel.

    BootInfo_t BootInfoArgument;
    BootInfoArgument.GraphicsOutputProtocol = GraphicsOutputProtocolArgument;
    BootInfoArgument.IOAPICAddress = IOAPICAddressArgument;
    BootInfoArgument.ECAMBaseAddress = ECAMBaseAddressArgument;
    BootInfoArgument.ECAMSize = ECAMSizeArgument;
    // Memory info
    BootInfoArgument.MemoryInfo.MemoryMap = MemoryMap;
    BootInfoArgument.MemoryInfo.MemoryMapSize = MemoryMapSize;
    BootInfoArgument.MemoryInfo.MemoryMapKey = MemoryMapKey;
    BootInfoArgument.MemoryInfo.DescriptorSize = DescriptorSize;
    BootInfoArgument.MemoryInfo.DescriptorVersion = DescriptorVersion;
    BootInfoArgument.FontBuffer = FontBufferArgument;
    
    void (*kmain)(BootInfo_t* BootInfoArgument) = (void (*)(BootInfo_t*)) ELFHeader->ELF_ProgramEntry;
    kmain(&BootInfoArgument);

    while(true);

}
