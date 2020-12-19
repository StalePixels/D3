//
// Created by D Rimron-Soutter on 16/12/2020.
//

#ifndef WIKI_TEXTVIEW_H
#define WIKI_TEXTVIEW_H

#include <stdint.h>

void display_textview_init();
void display_textview_draw();
void textview_memory_scroll_up();
void textview_memory_scroll_down();
void textview_memory_scroll_right();
void textview_memory_scroll_left();

extern uint8_t page_table[];
extern uint8_t page_table_index;

#endif //WIKI_TEXTVIEW_H
