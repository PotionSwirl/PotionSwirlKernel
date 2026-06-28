#pragma once

#include <stdint.h>

#define GRAPHICS_CONSOLE_HORIZONTAL_PADDING 10
#define GRAPHICS_COLOUR_WHITE 0xFFFFFF
#define GRAPHICS_COLOUR_BLACK 0x000000

typedef struct {
    int X;
    int Y;
} Graphics_Point_t;

void Graphics_Initialise();
Graphics_Point_t Graphics_CreatePoint(int X, int Y);
void Graphics_RenderCharacter(Graphics_Point_t FrameBufferStartPixel, unsigned char Character, uint32_t Colour);
void Graphics_RenderText(Graphics_Point_t FrameBufferStartPixel, const char* String, uint32_t Colour);
void Graphics_PrintCharacter(char Character);
void Graphics_FrameBufferDrawRectangle(Graphics_Point_t FrameBufferStartPixel, Graphics_Point_t FrameBufferEndPixel, uint32_t Colour);
int Graphics_GetConsoleTextWidth();
int Graphics_GetConsoleTextHeight();
int Graphics_GetHorizontalResolution();
int Graphics_GetVerticalResolution();
int Graphics_GetFontWidth();
int Graphics_GetFontHeight();
void Graphics_ConsolePlot(Graphics_Point_t ConsolePoint, uint32_t Colour);
void Graphics_ConsoleDrawRectangle(Graphics_Point_t ConsoleStartPoint, Graphics_Point_t ConsoleEndPoint, uint32_t Colour);
