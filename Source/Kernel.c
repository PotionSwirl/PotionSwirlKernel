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


void kmain(BootInfo_t BootInfoArgument) {

   // Give the Graphics Output Protocol and font data to the Graphics library to initialise its own variables.
   Graphics_Initialise(BootInfoArgument.GraphicsOutputProtocol, BootInfoArgument.FontBuffer);

   // Check if CPU supports the APIC (Advanced Programmable Interrupt Controller).
   if (!CPU_CPUSupportsAPIC()) {
      // If it doesn't, we can't continue since we don't have access to interrupts.
      printf("This computer's CPU doesn't support APIC - which we need!\n");
      while(true);
   }

   // Set up the GDT.
   GDT_Initialise();

   // Set up paging to manage memory!
   Paging_Initialise(BootInfoArgument);

   // If it DOES have an APIC, then enable it and find the Local APIC.
   APIC_Initialise(BootInfoArgument.IOAPICAddress);

   // Set up the IDT, which also disables legacy PIC before we set up more modern APIC.
   IDT_Initialise();

   // Initialise the devices that we'll be using, i.e. NVMe drive.
   PCI_InitialiseDevices(BootInfoArgument.ECAMBaseAddress);

   // Print welcome message.
   printf("Welcome to Brainrot26!");

   while(true);

}
