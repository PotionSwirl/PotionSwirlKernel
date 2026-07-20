#pragma once

#include <stdint.h>
#include "FAT32.h"

/************ PRINTING ************/

void putc(char c);
void puts(const char* s);

// THE GRAND PRINTF - only supports string, int and hex for now
void printf(const char* fmt, ...);


/************ MEMORY ************/

void memset(void* Object, char Value, uint32_t Count);
void memcpy(void* Destination, const void* Source, uint32_t Count);
int memcmp(const void* Source1, const void* Source2, uint32_t Count);


/************ STRING ************/

int strcmp(const char* s1, const char *s2);
int strlen(const char *s);
bool Libraries_StringContains(const char* String, char Character);


/************ FILES ************/

typedef FAT32_DirectoryEntry_t FILE;


FILE* fopen(const char* FilePath);
void fclose(FILE* FilePointer);
void fread(void* Buffer, FILE* FilePointer);
