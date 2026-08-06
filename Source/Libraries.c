#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>
#include "Include/Libraries.h"
#include "Include/Graphics.h"
#include "Include/FAT32.h"
#include "Include/Paging.h"

/************ PRINTING ************/

void putc(char c) {
    Graphics_PrintCharacter(c);
}


void puts(const char* s) {
    while (*s != 0) {
        putc(*s);
        s++;
    }
}


static void put_int(int64_t val) {
    if (val == 0) {
        putc('0');
        return;
    }

    // Don't need the [10] but makes it clearer that we access a digit here by (val % 10) as index
    char digits[10] = "0123456789";
    // This will store the individual chars to print for the number
    char buffer[20];   // -9,223,372,036,854,775,808 lowest 64-bit signed int you can have on computers
    int cur = 0;
    bool is_neg = false;

    if (val < 0) {
        is_neg = true;
        val = -val;   // We only want the magnitude - we'll insert the '-' ourselves
    }

    // Build the number in reverse as it'll get smaller through repeated divisions by 10
    while (val > 0) {
        buffer[cur++] = digits[val % 10];
        val /= 10;
    }

    // Deal with negative sign
    if (is_neg) {
        // We COULD leave ++ out here but if this doesn't run, the ++ above still runs, so might as well minus 1 below
        buffer[cur++] = '-';
    }

    // And finally re-reverse the reversed number 😹 and print
    for (int i = cur - 1; i >= 0; i--) {
        putc(buffer[i]);
    }

}



static void put_hex(uint64_t val) {

    if (val == 0) {
        putc('0');
        return;
    }

    char digits[16] = "0123456789ABCDEF";
    char buffer[16];
    int cur = 0;

    while (val > 0) {
        buffer[cur++] = digits[val % 16];
        val /= 16;
    }

    for (int i = cur - 1; i >= 0; i--) {
        putc(buffer[i]);
    }

}


// THE GRAND PRINTF - only supports string, int and hex for now
void printf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);  // Populates the args list for us

    for (int i = 0; fmt[i] != '\0'; i++) {

        // If we see a %, then deal with the next char - the type specifier
        if (fmt[i] == '%') {
            i++;

            switch(fmt[i]) {
                case 's': {
                    const char* s = va_arg(args, const char*);
                    puts(s);
                    break;
                }
                case 'c' : {
                    char c = va_arg(args, int);
                    putc(c);
                    break;
                }
                case 'd': {
                    int64_t n = va_arg(args, int64_t);
                    put_int(n);
                    break;
                }
                case 'x': {
                    uint64_t n = va_arg(args, uint64_t);
                    put_hex(n);
                    break;
                }
                case '%': {
                    putc('%');
                    break;
                }
                default: {
                    putc('?');
                    break;
                }
            }

        } else {
            // Normal character
            putc(fmt[i]);
        }

    }

    // We're finished
    va_end(args);
}


/************ MEMORY ************/

void memset(void* Object, char Value, uint32_t Count) {

    char* Cursor = (char*) Object;

    while (Count > 0) {
        *Cursor++ = Value;
        Count--;
    }

}


void memcpy(void* Destination, const void* Source, uint32_t Count) {

    uint8_t* DestinationPointer = (uint8_t*) Destination;
    const uint8_t* SourcePointer = (uint8_t*) Source;

    while (Count--) {
        *DestinationPointer++ = *SourcePointer++;
    }

}


int memcmp(const void* Source1, const void* Source2, uint32_t Count) {

    const unsigned char* Source1Bytes = (const unsigned char*) Source1;
    const unsigned char* Source2Bytes = (const unsigned char*) Source2;

    for (uint32_t i = 0; i < Count; i++) {
        if (Source1Bytes[i] != Source2Bytes[i]) {
            return 1;
        }
    }

    return 0;

}


/************ STRING ************/

int strcmp(const char* s1, const char *s2) {

    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    };

    return (uint8_t) *s1 - (uint8_t) *s2;

}


int strlen(const char *s) {
    int retval = 0;

    while(*s != '\0') {
        retval++;
        s++;
    }

    return retval;
}


bool Libraries_StringContains(const char* String, char Character) {

    while (*String) {

        if (*String == Character) {
            return true;
        }

        String++;

    }

    return false;

}


/************ FILES ************/


FILE* fopen(const char* FilePath) {

    FAT32_DirectoryEntry_t* DirectoryEntry = FAT32_GetDirectoryEntry(FilePath);

    FILE* FilePointer = (FILE*) DirectoryEntry;
    return FilePointer;
    
}


void fclose(FILE* FilePointer) {

    /* This will free the page containing the cluster of directory entries in which
       the file's directory entry was found. */

    Paging_FreePage((void*) Paging_PageAlignRoundDown((uint64_t) FilePointer));

}


void fread(void* Buffer, FILE* FilePointer) {

    // Read the file to the buffer
    // Remember - FILE & FAT32_DirectoryEntry_t are the same type

    FAT32_DirectoryEntry_t* DirectoryEntry = FilePointer;
    FAT32_BPB_t* BPB = FAT32_GetBPB();

    /* We'll pass in a page-aligned buffer to the drive as it may require it (NVMe does), 
       copy the contents into the requested buffer, then free the page. */

    // We'll cast this temporary buffer to uint8_t* so that we can increment it in bytes

    void* DriveBuffer = Paging_AllocatePage();
    uint8_t* DriveBufferCursor = DriveBuffer;
    memset(DriveBuffer, 0, PAGING_PAGE_SIZE);

    uint32_t CurrentCluster = (DirectoryEntry->FirstClusterHigh << 16) | DirectoryEntry->FirstClusterLow;

    while (CurrentCluster < FAT32_EOF) {
        uint32_t StartSector = FAT32_ClusterToSector(CurrentCluster);
        FAT32_DriveRead(StartSector, BPB->SectorsPerCluster, DriveBufferCursor);
        DriveBufferCursor += BPB->SectorsPerCluster * BPB->BytesPerSector;
        CurrentCluster = FAT32_GetNextCluster(CurrentCluster);
    }

    // Copy the temporary buffer to the requested one

    memcpy(Buffer, DriveBuffer, DirectoryEntry->FileSize);

    // And free the page holding the temporary buffer

    Paging_FreePage(DriveBuffer);

}
