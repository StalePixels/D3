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
        0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
        0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
        0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
        0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
        0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
        0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
        0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
        0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
//        0, 0, 0, 0, 0, 0, 0, 0,
//        0, 0, 0, 0, 0, 0, 0, 0,
//        0, 0, 0, 0, 0, 0, 0, 0,
//        0, 0, 0, 0, 0, 0, 0, 0
};


static uint8_t TEXTVIEW_MAX_ROWS = 31;
static uint8_t TEXTVIEW_MAX_COLS = 80;

char row[81] = "";
uint8_t i, j, next_line;
uint16_t next_char;
uint16_t left_inset;
char *text_title;
uint32_t text_size;
uint32_t text_seen;
uint32_t text_offset;
uint8_t l3_textview_mode;

static bool overflow_right = false;

void l3_textview_memory_data() {
    ZXN_WRITE_MMU2(page_table[page_table_index]);
    ZXN_WRITE_MMU3(page_table[page_table_index + 1]);
}

void l3_textview_memory_display() {
    ZXN_WRITE_MMU2(10);
    ZXN_WRITE_MMU3(11);
}

void l3_textview_draw_status() {
    L3ScreenColour = 2;
    l3_goto(0, TEXTVIEW_MAX_ROWS);
    sprintf(row, " %-44s %8lu/%8lubytes ", text_title, text_seen, text_size);
    l3_puts(row);
    do {
        L3ScreenColour= L3ScreenColour + 2;
        l3_putc(30);
        l3_putc(31);
    } while(L3ScreenColour<12);
    L3ScreenColour = 2;
    l3_puts("  ");
    L3ScreenColour = 0;
}

void l3_textview_init(char *title, uint32_t size) {
    text_size = size;
    text_title = title;
    text_offset = 0;

    next_line = 0;
    next_char = 0;
    text_seen = 0;
    left_inset = 0;
    row[TEXTVIEW_MAX_COLS]=0;
    textview_lines[next_line] = next_char;

    if(l3_textview_mode == 0) {
        /* PAGE OUT GRAPHICS BANKS! */
        l3_textview_memory_data();
        /****************************/
        while (ula_bank[next_char] && next_char < size) {
            if (ula_bank[next_char] == 10) {
                textview_lines[++next_line] = ++next_char;
            } else {
                ++next_char;
            }

            if (next_line == TEXTVIEW_MAX_ROWS - 1) goto done;
        }
        done:
        /* PAGE *IN* GRAPHICS BANKS */
        l3_textview_memory_display();
        /****************************/
    }
    else {
        for(uint8_t i = 1; i < TEXTVIEW_MAX_ROWS; ++i) {
            textview_lines[i] = textview_lines[i-1] + 16;

            l3_goto(0, i);
            l3_clear_to_eol();
            L3ScreenColour = 2;
            l3_putc(186);
            l3_goto(TEXTVIEW_MAX_COLS-1, i);
            l3_putc(186);
            L3ScreenColour = 0;
        }

        l3_goto(0, 0);
        L3ScreenColour = 2;
        l3_puts("\xC9\xCD\xCD""Offset\xCD\xCD\xCB\xCD\xCD\xCD\xCD"
                    "\xCD\xCD\xCD""Bytes 0\xAF""7\xCD\xCD\xCD\xCD\xCD"
                    "\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD""Bytes 8\xAF""f"
//                    "\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
                    "\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCB"
                    "Word 1\xF62\xCD\xCDWord 2\xF62\xBB");
        L3ScreenColour = 2;
        l3_putc(186);
        L3ScreenColour = 0;
    }
}

void l3_textview_page_up() {
    --page_table_index;
    for (uint8_t i = 0; i < TEXTVIEW_MAX_ROWS; i++) {
        if (textview_lines[i]) {
            textview_lines[i] = textview_lines[i] + 8192;
            text_offset = text_offset + 8192;
        }
    }
}
void l3_textview_page_down() {
    ++page_table_index;
    for (uint8_t i = 0; i < TEXTVIEW_MAX_ROWS; i++) {
        if (textview_lines[i]) {
            textview_lines[i] = textview_lines[i] - 8192;
            text_offset = text_offset - 8192;
        }
    }
}

void l3_textview_scroll_up(uint8_t steps) {
    bool to_draw = false;
    while(steps--) {
        if (textview_lines[0] < (3 * 1024) && page_table_index) {
            l3_textview_page_up();
        }
        else if (!textview_lines[0]) goto exit;

        for (next_line = TEXTVIEW_MAX_ROWS - 1; next_line != 0;) {
            uint8_t old_line = next_line--;
            textview_lines[old_line] = textview_lines[next_line];
        }

        if (l3_textview_mode == 0) {
            // search backwards for text
            next_char = textview_lines[0] - 2;

            /* PAGE OUT GRAPHICS BANKS! */
            l3_textview_memory_data();
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
            // rewind the display counter
            text_seen = text_seen - (textview_lines[1] - textview_lines[0]);

            /* PAGE *IN* GRAPHICS BANKS */
            l3_textview_memory_display();
            /****************************/
        } else {
            // skip backwards 16 bytes
            textview_lines[0] = textview_lines[0] - 16;
            text_seen = text_seen - 16;
        }
        to_draw = true;
    }
    exit:
    if(to_draw) l3_textview_draw();
}

void l3_textview_scroll_down(uint8_t steps) {
    bool to_draw = false;
    while(steps--) {
        if (textview_lines[TEXTVIEW_MAX_ROWS - 1] == 0xFFFF) goto exit;
        if (text_seen+(16*TEXTVIEW_MAX_ROWS) >= text_size) goto exit;

        uint8_t old_line = 0, next_line = 1;

        text_seen = text_seen + (textview_lines[next_line] - textview_lines[old_line]);
        for (; next_line < TEXTVIEW_MAX_ROWS; ++next_line) {
            textview_lines[old_line] = textview_lines[next_line];
            old_line = next_line;
        }

        if (l3_textview_mode == 0) {
            // search forwards for text
            next_char = textview_lines[TEXTVIEW_MAX_ROWS - 1];

            /* PAGE OUT GRAPHICS BANKS! */
            l3_textview_memory_data();
            /****************************/
            while (ula_bank[next_char]) {
                if (ula_bank[next_char] == 10) {
                    textview_lines[TEXTVIEW_MAX_ROWS - 1] = ++next_char;
                    goto found;
                } else {
                    ++next_char;
                }
            }
            textview_lines[TEXTVIEW_MAX_ROWS - 1] = 0xFFFF;

            found:
            /* PAGE *IN* GRAPHICS BANKS */
            l3_textview_memory_display();
            /****************************/
        } else {
            // skip forwards 16 bytes
            textview_lines[TEXTVIEW_MAX_ROWS - 1] = textview_lines[TEXTVIEW_MAX_ROWS - 1] + 16;
        }

        // Manage fold-over to next 8k page
        if (textview_lines[0] > (13*1024)) {
            l3_textview_page_down();
        }
        to_draw = true;
    }
    exit:
    if(to_draw) l3_textview_draw();
}

void l3_textview_scroll_right() {
    if(l3_textview_mode == 0) {
        if (overflow_right) {
            ++left_inset;
            l3_textview_draw_window();
            return;
        }
    }
    else {
        if(text_size>text_seen+8192) {
            ++page_table_index;
            text_offset = text_offset + 8192;
            text_seen = text_seen + 8192;
            l3_textview_draw_window();
            l3_textview_draw_status();
        }
    }
}

void l3_textview_scroll_left() {
    if(l3_textview_mode == 0) {
        if (left_inset) {
            --left_inset;
            l3_textview_draw_window();
            return;
        }
    }
    else {
        if(text_seen>8191) {
            --page_table_index;
            text_offset = text_offset - 8192;
            text_seen = text_seen - 8192;
            l3_textview_draw_window();
            l3_textview_draw_status();
        }
    }
}

void l3_textview_draw_window() {
    memset(row, 32, 80);
    if(l3_textview_mode == 0) {
        overflow_right = false;
        for (i = 0; i < TEXTVIEW_MAX_ROWS; i++) {

            if (textview_lines[i] == 0xFFFF) goto blank_row;

            /* PAGE OUT GRAPHICS BANKS! */
            l3_textview_memory_data();
            /****************************/
            if (i < TEXTVIEW_MAX_ROWS - 1 &&
                &ula_bank[textview_lines[i] + left_inset] < &ula_bank[textview_lines[i + 1]]) {
                memcpy(row, &ula_bank[textview_lines[i] + left_inset], TEXTVIEW_MAX_COLS);
                goto clip_textview_row;
            } else if (i == TEXTVIEW_MAX_ROWS - 1) {
                // Is this line long enough to print?
                if (strlen(&ula_bank[textview_lines[i]]) > left_inset
                    && strchr(&ula_bank[textview_lines[i]], 10) > &ula_bank[textview_lines[i]] + left_inset) {
                    for (j = 0; j < TEXTVIEW_MAX_COLS; j++) {
                        row[j] = ula_bank[textview_lines[i] + left_inset + j];
                        if (row[j] == 10) {
                            row[j] = 0; // Set the string terminator
                            goto print_textview_row;
                        } // Found a string terminator
                        if (row[j] == 0) {
                            goto print_textview_row;
                        }
                    }
                    goto clip_textview_row;
                }
            }
            blank_row:
            // This is basically a blank row (outside of viewport)
            row[0] = 0;

            goto print_textview_row;

            clip_textview_row:
            for (j = 0; j < TEXTVIEW_MAX_COLS; j++) {
                if (row[j] == 10) {
                    row[j] = 0;
                    goto print_textview_row;
                }
            }
            overflow_right = true;
            row[TEXTVIEW_MAX_COLS - 1] = 175;
            row[TEXTVIEW_MAX_COLS] = 0;

            print_textview_row:
            /* PAGE *IN* GRAPHICS BANKS */
            l3_textview_memory_display();
            /****************************/

            l3_goto(0, i);
            l3_puts(row);
            if (L3ScreenY == i) l3_clear_to_eol();
        }

        return;
    }
    else {
        uint8_t hex_row[16];
        for (i = 0; i < TEXTVIEW_MAX_ROWS-1; i++) {
            /* PAGE OUT GRAPHICS BANKS! */
            l3_textview_memory_data();
            /****************************/
            memcpy(hex_row, &ula_bank[textview_lines[i]], 16);
            sprintf(row, " %08lx \xB3%02x %02x %02x %02x %02x %02x %02x %02x  %02x %02x %02x %02x %02x %02x %02x %02x\xB3",
                    text_seen + (i*16),
                    hex_row[0], hex_row[1], hex_row[2], hex_row[3], hex_row[4], hex_row[5], hex_row[6], hex_row[7],
                    hex_row[8], hex_row[9], hex_row[10],hex_row[11],hex_row[12],hex_row[13],hex_row[14],hex_row[15]
                );

            /* PAGE *IN* GRAPHICS BANKS */
            l3_textview_memory_display();
            /****************************/
            l3_goto(1, i+1);
            l3_puts_raw(row, 60);
            l3_puts_raw(&hex_row[0], 8);
            L3ScreenX = L3ScreenX + 2;
            l3_puts_raw(&hex_row[8], 8);
        }
    }
}

void l3_textview_draw() {
    l3_textview_draw_window();
    l3_textview_draw_status();
}