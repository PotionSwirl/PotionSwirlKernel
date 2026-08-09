#include <efi.h>
#include <stdbool.h>
#include "Include/Bootx64.h"
#include "Include/PSF2Font.h"
#include "Include/Libraries.h"
#include "Include/Graphics.h"
#include "Include/CPU.h"
#include "Include/GDT.h"
#include "Include/APIC.h"
#include "Include/IDT.h"
#include "Include/PCI.h"
#include "Include/Paging.h"
#include "Include/IHDA.h"
#include "Include/ACPI.h"

// Load font through USB + find safer space for memory map!

// This kernel requires: TSC, APIC, NVMe, Intel High Definition Audio with MSI, FAT32 System Partition

// Create a copy of the BootInfo and its structs as the original one from Bootx64.efi may become overwritten at some point

static BootInfo_t BootInfoCopy;
static EFI_GRAPHICS_OUTPUT_PROTOCOL GraphicsOutputProtocolCopy;
static EFI_MEMORY_DESCRIPTOR* MemoryMapCopy;

BootInfo_t* BootInfo;


void kmain(BootInfo_t* BootInfoArgument) {

   // Update BootInfo to point to our own copy of the GOP as the original may also be overwritten

   BootInfoCopy = *BootInfoArgument;
   BootInfo = &BootInfoCopy;

   GraphicsOutputProtocolCopy = *(BootInfo->GraphicsOutputProtocol);
   BootInfo->GraphicsOutputProtocol = &GraphicsOutputProtocolCopy;

   // Initialise the CPU Timestamp Counter.
   CPU_SleepInitialise();

   // Give the Graphics Output Protocol and font data to the Graphics library to initialise its own variables.
   Graphics_Initialise(BootInfo->GraphicsOutputProtocol, BootInfo->FontBuffer);

   // Set up the GDT.
   GDT_Initialise();

   // Get important information from the ACPI.
   ACPI_Initialise(BootInfo->RSDP);

   // Enable the APIC and find the Local APIC.
   APIC_Initialise(ACPI_GetIOAPICAddress());

   // Set up the IDT, which also disables legacy PIC before we set up more modern APIC.
   IDT_Initialise();

   // Set up paging to manage memory!
   Paging_Initialise(BootInfo);

   // Initialise the devices that we'll be using.
   PCI_InitialiseDevices(ACPI_GetECAMBaseAddress());

   // The FAT32 filesystem initialisation function is called by the available storage device initialisation function.

   // Print welcome message!
   printf("---------------------------------\nWelcome to Brainrot26!\n");

   IHDA_PlayWAV("ThisIsATestFolder\\windows-longhorn-error.wav");

   while(true);

}
