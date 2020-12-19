//
// Created by D Rimron-Soutter on 16/12/2020.
//

#include <stddef.h>
#include <arch/zxn.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "l3_textview.h"
#include "ula.h"
#include "liblayer3/liblayer3.h"

static uint16_t textview_lines[32] = {
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0
};

 unsigned char row[81] = "";
 uint8_t i, j, next_line;
 uint16_t next_char;
 uint16_t left_inset;

void textview_memory_data() {
    ZXN_WRITE_MMU2(page_table[page_table_index]);
    ZXN_WRITE_MMU3(page_table[page_table_index + 1]);
}

void textview_memory_display() {
    ZXN_WRITE_MMU2(10);
    ZXN_WRITE_MMU3(11);
}

void display_textview_init() {
    next_line = 0;
    next_char = 0;
    left_inset = 0;
    row[80]=0;

    printAt(0, 0);

    /* PAGE OUT GRAPHICS BANKS! */
    textview_memory_data();
    /****************************/

    textview_lines[next_line] = next_char;
    while(ula_bank[next_char]) {
        if(ula_bank[next_char]==10) {
            textview_lines[++next_line] = ++next_char;
        } else {
            ++next_char;
        }

        if(next_line>31) break;

    }
    /* PAGE *IN* GRAPHICS BANKS */
    textview_memory_display();
    /****************************/
}

void textview_memory_scroll_up() {
    if (textview_lines[0] < 1000 && page_table_index) {
        --page_table_index;
        for (uint8_t i = 0; i < 32; i++) {
            if (textview_lines[i]) textview_lines[i] = textview_lines[i] + 8192;
        }
        zx_border(page_table_index);
    }
    if (!textview_lines[0]) return;

    for (next_line = 31; next_line != 0;) {
        uint8_t old_line = next_line--;
        textview_lines[old_line] = textview_lines[next_line];
    }


    next_char = textview_lines[0] - 2;

    /* PAGE OUT GRAPHICS BANKS! */
    textview_memory_data();
    /****************************/
    while (next_char) {
        if (ula_bank[next_char] == 10) {
            textview_lines[0] = ++next_char;
            goto found;
        } else {
            --next_char;
        }

        if (next_line != 0) break;
    }
    textview_lines[0] = 0;

    found:
    /* PAGE *IN* GRAPHICS BANKS */
    textview_memory_display();
    /****************************/
}

void textview_memory_scroll_down() {
    for (next_line = 0; next_line < 31;) {
        uint8_t old_line = next_line++;
        textview_lines[old_line] = textview_lines[next_line];
    }

    next_char = textview_lines[next_line];

    /* PAGE OUT GRAPHICS BANKS! */
    textview_memory_data();
    /****************************/
    while (ula_bank[next_char]) {
        if (ula_bank[next_char] == 10) {
            textview_lines[31] = ++next_char;
            goto found;
        } else {
            ++next_char;
        }
    }
    found:
    /* PAGE *IN* GRAPHICS BANKS */
    textview_memory_display();
    /****************************/

    if (textview_lines[0] > 8192) {
        ++page_table_index;
        for (uint8_t i = 0; i < 32; i++) {
            if (textview_lines[i]) textview_lines[i] = textview_lines[i] - 8192;
        }
        zx_border(page_table_index);
    }
}

static bool overflow_right = false;
void textview_memory_scroll_right() {
    if(overflow_right) {
        ++left_inset;
    }
}
void textview_memory_scroll_left() {
    if(left_inset) {
        --left_inset;
    }
}
void display_textview_draw() {
    overflow_right = false;
    for(i=0;i<32;i++) {
        /* PAGE OUT GRAPHICS BANKS! */
        textview_memory_data();
        /****************************/
        if(/*i<31 &&*/ (&ula_bank[textview_lines[i] + left_inset] < &ula_bank[textview_lines[i+1]])) {
            memcpy(row, &ula_bank[textview_lines[i] + left_inset], 80);
            goto clip_textview_row;
        }
        else
        if(i==31) {
            for(j=0;j<80;j++) {
                row[j]= ula_bank[textview_lines[31] + left_inset + j];
                if(row[j]==0) {
                    goto print_textview_row;
                }
            }
            goto clip_textview_row;
        }
        // This is basically a blank row (outside of viewport)
        row[0] = 0;

        goto print_textview_row;

        clip_textview_row:
        for(j=0;j<80;j++) {
            if(row[j]==10) {
                row[j]=0;
                goto print_textview_row;
            }
        }
        overflow_right = true;
        row[79]=175;
        row[80]=0;

        print_textview_row:
        /* PAGE *IN* GRAPHICS BANKS */
        textview_memory_display();
        /****************************/

        l3_goto(0,i);
        l3_puts(row);
        l3_clear_to_eol();
    }
}