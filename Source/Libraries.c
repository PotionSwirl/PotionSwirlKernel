#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>
#include "Include/Graphics.h"

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

static void put_int(int val) {
    if (val == 0) {
        putc('0');
        return;
    }

    // Don't need the [10] but makes it clearer that we access a digit here by (val % 10) as index
    char digits[10] = "0123456789";
    // This will store the individual chars to print for the number
    char buffer[12];   // -2147483648 lowest 32-bit signed int you can have on computers
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

static void put_hex(unsigned int val) {

    char digits[16] = "0123456789ABCDEF";
    char buffer[8];
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
                    int n = va_arg(args, int);
                    put_int(n);
                    break;
                }
                case 'x': {
                    int n = va_arg(args, unsigned int);
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
