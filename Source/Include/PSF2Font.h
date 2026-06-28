#pragma once

#include <stdint.h>

#define PSF1FONTFILE_CHARACTER_PIXEL_WIDTH 8

// PSF1 font file header
typedef struct {
    uint32_t Magic;
    uint32_t Version;
    uint32_t HeaderSize;
    uint32_t Flags;
    uint32_t GlyphCount;
    uint32_t GlyphSize;
    uint32_t Height;
    uint32_t Width;
} PSF2Font_FileHeader;
