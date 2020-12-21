//
// Created by D Rimron-Soutter on 18/12/2020.
//

#include "less.h"

#include <stdio.h>
#include <stdlib.h>
#include <input.h>
#include <arch/zxn.h>
#include <arch/zxn/esxdos.h>
#include <errno.h>
#include <string.h>

extern unsigned char _z_page_table[];

#include "../common/ula.h"
#include "../common/d3.h"
#include "../common/liblayer3/liblayer3.h"
#include "../common/l3_textview.h"
#include "../common/startup.h"

#define MAX_PAGES       220

static uint8_t total_8k_pages = 1;

uint8_t page_table[MAX_PAGES];
uint8_t page_table_index;

static uint8_t ula2, ula3, ula6, ula7,
                i;  // reusable iterator

void less_exit() {
    l3_exit();

    // Free all the memory we reserved
    i = total_8k_pages;

    while(i) {
        --i;
        if(page_table[i]) {
            esx_ide_bank_free(0, page_table[i]);
        }
    }
    ZXN_WRITE_MMU6(ula6);
    ZXN_WRITE_MMU7(ula7);
}

void less(char *title, unsigned char text_in) {
    struct esxdos_stat finfo;  // = {0,0,0,0,0};
    esxdos_f_fstat(text_in, &finfo);
    atexit(less_exit);

    memset(page_table,0, MAX_PAGES);

    ula6 = ZXN_READ_REG(REG_MMU0 + 6);
    ula7 = ZXN_READ_REG(REG_MMU0 + 7);

    unsigned char row[81];
    total_8k_pages = 1 + (finfo.size/8192);
    printf("8K pages required: %d [", total_8k_pages);
    i = 0;
    while(i<total_8k_pages) {
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
        memset(0x4000, 0, 8192);
        if(i!=total_8k_pages-1) {
            esx_f_read(text_in, 0x4000, 8192);
        }
        else {
            esx_f_read(text_in, 0x4000, finfo.size % 8192);
        }
        memcpy(row, 0x4000, 80);
        ZXN_WRITE_MMU2(10);
        row[80]=0;

        ++i;
    }
    printf("]");

    ZXN_WRITE_MMU6(_z_page_table[82]);
    ZXN_WRITE_MMU7(_z_page_table[83]);
    l3_init();

    // Setup the viewer
    l3_textview_init(title, finfo.size);
    // display page
    l3_textview_draw();
    uint8_t c = 0;
    while(1) {

        // get input
//        in_wait_nokey();
        in_wait_key();
        c = in_inkey();

        // find result
        switch(c) {
            case 8:
                l3_textview_memory_scroll_left();
                break;
            case 9:
                l3_textview_memory_scroll_right();
                break;
            case 10:
                l3_textview_memory_scroll_down();
                break;
            case 11:
                l3_textview_memory_scroll_up();
                break;
            case 'q':
                goto exit;
        }
    }
    exit:
    less_exit();
}