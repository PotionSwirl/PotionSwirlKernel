#pragma once

#include <stdint.h>

#define KEYBOARD_NO_INPUT_RIGHT_NOW 0

void Keyboard_Vector33InterruptHandlePassover(uint8_t Scancode);
char Keyboard_GetCharacter();
