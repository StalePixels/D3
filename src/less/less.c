//
// Created by D Rimron-Soutter on 18/12/2020.
//

#include "less.h"

#include <stdio.h>
#include <arch/zxn.h>
#include <arch/zxn/esxdos.h>
#include <errno.h>

#include "../common/ula.h"
#include "../common/d3.h"


void less(unsigned char file_in, uint8_t glyphs, uint8_t row) {
    static uint8_t rows, ascii;

    if(glyphs==96) {
        rows = 12;
        ascii = 32;
    }
    else {
        rows = 16;
        ascii = 0;
    }

    while(rows) {
        for (uint8_t col = 0; col < 8; ++col) {
            if(ascii<32) {
                printf("%2d= ", ascii);
            }
            else {
                printf(" %c= ", ascii);
            }

            for (uint8_t prow = 0; prow < 8; ++prow) {
                uint16_t addr = zx_pxy2saddr(24 + (col * 32), prow + (row * 8));
                errno = esxdos_f_read(file_in, addr, 1);
            }

            ++ascii;
        }
        printf("\n");
        --rows;
        ++row;
    }
}