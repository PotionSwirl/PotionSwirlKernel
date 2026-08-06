#include <stdint.h>
#include <efi.h>
#include "Include/Graphics.h"
#include "Include/PSF2Font.h"
#include "Include/Libraries.h"

// Initialised in kmain(), in Kernel.c.
static EFI_GRAPHICS_OUTPUT_PROTOCOL* GraphicsOutputProtocol;
uint32_t* Graphics_FrameBuffer;
static PSF2Font_FileHeader* FontBuffer;

// Divide our screen into VGA-like cells to form a console.
static int ConsoleTextWidth;
static int ConsoleTextHeight;

// The next available console cell to print to.
static Graphics_Point_t ConsoleCurrentPoint;


void Graphics_Initialise(EFI_GRAPHICS_OUTPUT_PROTOCOL* GraphicsOutputProtocolArgument, PSF2Font_FileHeader* FontBufferArgument) {

    GraphicsOutputProtocol = GraphicsOutputProtocolArgument;
    Graphics_FrameBuffer = (uint32_t*) GraphicsOutputProtocol->Mode->FrameBufferBase;
    FontBuffer = FontBufferArgument;
    ConsoleTextWidth = (GraphicsOutputProtocol->Mode->Info->HorizontalResolution - GRAPHICS_CONSOLE_HORIZONTAL_PADDING * 2) / FontBuffer->Width;
    ConsoleTextHeight = GraphicsOutputProtocol->Mode->Info->VerticalResolution / FontBuffer->Height;
    ConsoleCurrentPoint = Graphics_CreatePoint(0, 0);
    
}


Graphics_Point_t Graphics_CreatePoint(int X, int Y) {
    Graphics_Point_t Point;
    Point.X = X;
    Point.Y = Y;
    return Point;
}


void Graphics_RenderCharacter(Graphics_Point_t FrameBufferStartPixel, unsigned char Character, uint32_t Colour) {

    int FrameBufferStartX = FrameBufferStartPixel.X;
    int FrameBufferStartY = FrameBufferStartPixel.Y;
    int StartPixelPosition = FrameBufferStartY * GraphicsOutputProtocol->Mode->Info->PixelsPerScanLine + FrameBufferStartX;
    void* GlyphData = (void*) ((uint64_t) FontBuffer + FontBuffer->HeaderSize);
    int BytesPerRow = (FontBuffer->Width + 7) / 8;
    uint8_t* CharacterGlyph = GlyphData + Character * BytesPerRow * FontBuffer->Height;

    for (uint32_t RenderRow = 0; RenderRow < FontBuffer->Height; RenderRow++) {
        for (int RowByte = 0; RowByte < BytesPerRow; RowByte++) {
            for (int ByteBit = 0; ByteBit < 8; ByteBit++) {
                // Access 1 uin8_t byte of the glyph data at a time
                if (CharacterGlyph[RenderRow * BytesPerRow + RowByte] & (0b10000000 >> ByteBit)) {
                    int RenderPixelPosition = StartPixelPosition + RenderRow * GraphicsOutputProtocol->Mode->Info->PixelsPerScanLine + RowByte * 8 + ByteBit;
                    Graphics_FrameBuffer[RenderPixelPosition] = Colour;
                }
            }
        }
    }
}


void Graphics_RenderText(Graphics_Point_t FrameBufferStartPixel, const char* String, uint32_t Colour) {

    while (*String != 0) {
        Graphics_RenderCharacter(FrameBufferStartPixel, *String, Colour);
        String++;
        FrameBufferStartPixel.X += FontBuffer->Width;
    }

}


void Graphics_PrintCharacter(char Character) {

    int StartFrameBufferX = ConsoleCurrentPoint.X * FontBuffer->Width + GRAPHICS_CONSOLE_HORIZONTAL_PADDING;
    int StartFrameBufferY = ConsoleCurrentPoint.Y * FontBuffer->Height;

    ConsoleCurrentPoint.X++;
    
    if (ConsoleCurrentPoint.X > ConsoleTextWidth || Character == '\n') {

        ConsoleCurrentPoint.Y++;
        ConsoleCurrentPoint.X = 0;

    // Deal with backspace and tab too!
    } else if (Character == '\b') {

        ConsoleCurrentPoint.X -= 3;
        Graphics_PrintCharacter(']');

    } else if (Character == '\t') {

        for (int i = 0; i < 4; i++) {
            Graphics_PrintCharacter(' ');
        }

    }

    // Ignore newlines, backspaces and tabs.
    if (Character == '\n' || Character == '\b' || Character == '\t') {
        return;
    }
    
    Graphics_Point_t FrameBufferStartPixel = Graphics_CreatePoint(StartFrameBufferX, StartFrameBufferY);
    Graphics_RenderCharacter(FrameBufferStartPixel, Character, GRAPHICS_COLOUR_WHITE);

}


void Graphics_FrameBufferDrawRectangle(Graphics_Point_t FrameBufferStartPixel, Graphics_Point_t FrameBufferEndPixel, uint32_t Colour) {
    for (int X = FrameBufferStartPixel.X; X < FrameBufferEndPixel.X; X++) {
        for (int Y = FrameBufferStartPixel.Y; Y < FrameBufferEndPixel.Y; Y++) {
            Graphics_FrameBuffer[Y * GraphicsOutputProtocol->Mode->Info->PixelsPerScanLine + X] = Colour;
        }
    }
}


int Graphics_GetConsoleTextWidth() {
    return ConsoleTextWidth;
}


int Graphics_GetConsoleTextHeight() {
    return ConsoleTextHeight;
}


int Graphics_GetHorizontalResolution() {
    return GraphicsOutputProtocol->Mode->Info->HorizontalResolution;
}


int Graphics_GetVerticalResolution() {
    return GraphicsOutputProtocol->Mode->Info->VerticalResolution;
}


int Graphics_GetFontWidth() {
    return FontBuffer->Width;
}


int Graphics_GetFontHeight() {
    return FontBuffer->Height;
}


void Graphics_ConsolePlot(Graphics_Point_t ConsolePoint, uint32_t Colour) {
    Graphics_Point_t FrameBufferStartPixel = Graphics_CreatePoint(ConsolePoint.X * FontBuffer->Width, ConsolePoint.Y * FontBuffer->Height);
    Graphics_Point_t FrameBufferEndPixel = Graphics_CreatePoint(FrameBufferStartPixel.X + FontBuffer->Width, FrameBufferStartPixel.Y + FontBuffer->Height);
    Graphics_FrameBufferDrawRectangle(FrameBufferStartPixel, FrameBufferEndPixel, Colour);
}


void Graphics_ConsoleDrawRectangle(Graphics_Point_t ConsoleStartPoint, Graphics_Point_t ConsoleEndPoint, uint32_t Colour) {
    for (int X = ConsoleStartPoint.X; X < ConsoleEndPoint.X; X++) {
        for (int Y = ConsoleStartPoint.Y; Y < ConsoleEndPoint.Y; Y++) {
            Graphics_Point_t ConsolePoint = Graphics_CreatePoint(X, Y);
            Graphics_ConsolePlot(ConsolePoint, Colour);
        }
    }
}


void Graphics_ClearScreen() {
    memset(Graphics_FrameBuffer, 0, GraphicsOutputProtocol->Mode->FrameBufferSize);
    ConsoleCurrentPoint = Graphics_CreatePoint(0, 0);
}
