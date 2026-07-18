#include <stdint.h>
#include <stdbool.h>
#include "Include/Keyboard.h"
#include "Include/Libraries.h"

const uint32_t UNKNOWN = 0xFFFFFFFF;
const uint32_t ESC = 0xFFFFFFFF - 1;
const uint32_t CTRL = 0xFFFFFFFF - 2;
const uint32_t LSHFT = 0xFFFFFFFF - 3;
const uint32_t RSHFT = 0xFFFFFFFF - 4;
const uint32_t ALT = 0xFFFFFFFF - 5;
const uint32_t F1 = 0xFFFFFFFF - 6;
const uint32_t F2 = 0xFFFFFFFF - 7;
const uint32_t F3 = 0xFFFFFFFF - 8;
const uint32_t F4 = 0xFFFFFFFF - 9;
const uint32_t F5 = 0xFFFFFFFF - 10;
const uint32_t F6 = 0xFFFFFFFF - 11;
const uint32_t F7 = 0xFFFFFFFF - 12;
const uint32_t F8 = 0xFFFFFFFF - 13;
const uint32_t F9 = 0xFFFFFFFF - 14;
const uint32_t F10 = 0xFFFFFFFF - 15;
const uint32_t F11 = 0xFFFFFFFF - 16;
const uint32_t F12 = 0xFFFFFFFF - 17;
const uint32_t SCRLCK = 0xFFFFFFFF - 18;
const uint32_t HOME = 0xFFFFFFFF - 19;
const uint32_t UP = 0xFFFFFFFF - 20;
const uint32_t LEFT = 0xFFFFFFFF - 21;
const uint32_t RIGHT = 0xFFFFFFFF - 22;
const uint32_t DOWN = 0xFFFFFFFF - 23;
const uint32_t PGUP = 0xFFFFFFFF - 24;
const uint32_t PGDOWN = 0xFFFFFFFF - 25;
const uint32_t END = 0xFFFFFFFF - 26;
const uint32_t INS = 0xFFFFFFFF - 27;
const uint32_t DEL = 0xFFFFFFFF - 28;
const uint32_t CAPS = 0xFFFFFFFF - 29;
const uint32_t NONE = 0xFFFFFFFF - 30;
const uint32_t ALTGR = 0xFFFFFFFF - 31;
const uint32_t NUMLCK = 0xFFFFFFFF - 32;


static const uint32_t Lowercase[128] = {
UNKNOWN,ESC,'1','2','3','4','5','6','7','8',
'9','0','-','=','\b','\t','q','w','e','r',
't','y','u','i','o','p','[',']','\n',CTRL,
'a','s','d','f','g','h','j','k','l',';',
'\'','`',LSHFT,'\\','z','x','c','v','b','n','m',',',
'.','/',RSHFT,'*',ALT,' ',CAPS,F1,F2,F3,F4,F5,F6,F7,F8,F9,F10,NUMLCK,SCRLCK,HOME,UP,PGUP,'-',LEFT,UNKNOWN,RIGHT,
'+',END,DOWN,PGDOWN,INS,DEL,UNKNOWN,UNKNOWN,UNKNOWN,F11,F12,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,
UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,
UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,
UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN
};

static const uint32_t Uppercase[128] = {
    UNKNOWN,ESC,'!','@','#','$','%','^','&','*','(',')','_','+','\b','\t','Q','W','E','R',
'T','Y','U','I','O','P','{','}','\n',CTRL,'A','S','D','F','G','H','J','K','L',':','"','~',LSHFT,'|','Z','X','C',
'V','B','N','M','<','>','?',RSHFT,'*',ALT,' ',CAPS,F1,F2,F3,F4,F5,F6,F7,F8,F9,F10,NUMLCK,SCRLCK,HOME,UP,PGUP,'-',
LEFT,UNKNOWN,RIGHT,'+',END,DOWN,PGDOWN,INS,DEL,UNKNOWN,UNKNOWN,UNKNOWN,F11,F12,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,
UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,
UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,
UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN
};

/* We'll use a buffer to store characters as they're being typed, but
   wait for the kernel to process them before we give them any more
   from the incoming interrupts. This is to ensure we don't miss any
   characters, which is likely with interrupts. */

static volatile char Buffer[50];
static volatile int Head = 0;          // Latest character grabbed from interrupt
static volatile int Tail = 0;          // Latest character grabbed by kernel
static bool Shift  = false;
static bool Capslock = false;


void Keyboard_Vector33InterruptHandlePassover(uint8_t Scancode) {

    // If it's a key RELEASE and it's Shift, set Shift to false.

    if (Scancode & 0x80) {

        if (Lowercase[Scancode] == LSHFT ||  Lowercase[Scancode] == RSHFT) {
            Shift = false;
        }
        
        return;
    }

    // If it's a key PRESS -> Shift = true if it's shift, or switch caps bool appropriately.

    if (Lowercase[Scancode] == CAPS) {
        Capslock = (Capslock) ? false : true;
        return;
    }
    
    if (Lowercase[Scancode] == LSHFT || Lowercase[Scancode] == RSHFT) {
        Shift = true;
        return;
    }

    char Character = (Capslock || Shift) ? Uppercase[Scancode] : Lowercase[Scancode];
    Buffer[Head] = Character;
    
    // Calculate next head index - use modulus as the head can circle back to the beginning.

    /* If it's caught up to the tail on its second round, cancel this read as we need to give
       the kernel some time to catch up and read the many characters. */

    int NextHead = (Head + 1) % 50;

    if (NextHead == Tail) {
        return;
    }

    // Otherwise, write the character to the buffer as normal.
    Buffer[Head] = Character;
    Head = NextHead;

}


char Keyboard_GetCharacter() {

    /* On the other hand, if the tail catches up to the head (perhaps if the user
       types really fast), we'll return 0, signalling no input right now, as the tail
       has run out of characters to read for now. Once interrupts bring in more characters,
       this can resume.*/

    if (Tail == Head) {
        return KEYBOARD_NO_INPUT_RIGHT_NOW;
    }

    char CharacterToReturn = Buffer[Tail];
    Tail = (Tail + 1) % 50;
    return CharacterToReturn;

}
