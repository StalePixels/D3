//
// Created by D Rimron-Soutter on 18/12/2020.
//

#include "less.h"

#include <stdio.h>
#include <stdlib.h>
#include <arch/zxn.h>
#include <arch/zxn/esxdos.h>
#include <errno.h>
#include <string.h>

#include "../common/ula.h"
#include "../common/d3.h"

static uint8_t total_8k_pages = 2;
uint8_t pages[225];
static uint8_t i;  // reusable iterator

void less_exit() {
    // Free all the memory we reserved
    total_8k_pages = 225;

    while(total_8k_pages--) {
        if(pages[total_8k_pages]) {
            esx_ide_bank_free(0, pages[total_8k_pages]);
        }
    }
}

void less(unsigned char text_in) {
    struct esxdos_stat finfo;  // = {0,0,0,0,0};
    esxdos_f_fstat(text_in, &finfo);
    atexit(less_exit);
    memset(pages,0,225);

    total_8k_pages = 2 + (finfo.size/16384);
    printf("8K pages required: %d [", total_8k_pages);
    while(total_8k_pages) {
        // Ask OS for memory
        pages[total_8k_pages] = esx_ide_bank_alloc(0);

        // Check we got the memory we asked for
        if(pages[total_8k_pages]==255) {
            printf("!]");
            exit((int)err_no_memory);
        }

        // Make it down, move along around, do it all again
        printf(".");
        --total_8k_pages;
    }
    printf("]");

}