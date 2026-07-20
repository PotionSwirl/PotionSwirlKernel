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


// This OS requires: APIC, NVMe, FAT32 system partition


void kmain(BootInfo_t* BootInfoArgument) {

   // Give the Graphics Output Protocol and font data to the Graphics library to initialise its own variables.
   Graphics_Initialise(BootInfoArgument->GraphicsOutputProtocol, BootInfoArgument->FontBuffer);

   // Set up the GDT.
   GDT_Initialise();

   // Set up paging to manage memory!
   Paging_Initialise(BootInfoArgument);

   // Enable the APIC and find the Local APIC.
   APIC_Initialise(BootInfoArgument->IOAPICAddress);

   // Set up the IDT, which also disables legacy PIC before we set up more modern APIC.
   IDT_Initialise();

   // Initialise the devices that we'll be using, i.e. NVMe drive.
   PCI_InitialiseDevices(BootInfoArgument->ECAMBaseAddress);

   // The FAT32 filesystem initialisation function is called by the available storage device initialisation function.

   // Print welcome message.
   printf("Welcome to Brainrot26!\n");

   FILE* fptr = fopen("folder\\test.txt");
   char* test;
   fread(test, fptr);

   for (uint32_t i = 0; i < fptr->FileSize; i++) {
      putc(test[i]);
   }

   fclose(fptr);

   while(true);

}
