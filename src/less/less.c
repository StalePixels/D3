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
#include "../common/liblayer3/liblayer3.h"

#define MAX_PAGES       220

static uint8_t total_8k_pages = 1;
static uint8_t page_table[MAX_PAGES];
static uint8_t i;  // reusable iterator

void less_exit() {
    // Free all the memory we reserved
    i = total_8k_pages;

    while(i--) {
        if(page_table[i]) {
            esx_ide_bank_free(0, page_table[i]);
        }
    }
}

void less(unsigned char text_in) {
    struct esxdos_stat finfo;  // = {0,0,0,0,0};
    esxdos_f_fstat(text_in, &finfo);
    atexit(less_exit);
    memset(page_table,0, MAX_PAGES);

    L3ULABottom = ZXN_READ_REG(REG_MMU0 + 2);
    L3ULATop = ZXN_READ_REG(REG_MMU0 + 3);

    total_8k_pages = i = 1 + (finfo.size/8192);
    printf("8K pages required: %d [", total_8k_pages);
    while(i) {
        // Ask OS for memory
        page_table[i] = esx_ide_bank_alloc(0);

        // Check we got the memory we asked for
        if(page_table[i]==255) {
            printf("!]");
            exit((int)err_no_memory);
        }

        // Make it down, move along around, do it all again
        printf(".");

        ZXN_WRITE_MMU2(page_table[i]);
        if(i>1) {
            esx_f_read(text_in, 0x4000, 8192);
        }
        else {
            esx_f_read(text_in, 0x4000, finfo.size % 8192);
        }
        ZXN_WRITE_MMU2(L3ULABottom);

        --i;
    }
    printf("]");


}