#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint16_t Limit;
    uint64_t Base;
} GDT_Descriptor_t;

void GDT_Initialise();
