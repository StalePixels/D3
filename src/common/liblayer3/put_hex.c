#include "liblayer3.h"

void l3_print_hex(uint8_t hex) {
    unsigned char c;

    // Nybble 1
    c = ((hex >> 4) & 15) + 48;
    if(c>57) c = c + 39;
    l3_putc(c);

    // Nybble 2
    c = (hex & 15) + 48;
    if(c>57) c = c + 39;
    l3_putc(c);
}