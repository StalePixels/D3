//
// Created by D Rimron-Soutter on 16/12/2020.
//

#ifndef WIKI_TEXTVIEW_H
#define WIKI_TEXTVIEW_H

#include <stdint.h>

void l3_textview_init(char *title, uint32_t size);
void l3_textview_draw();
void l3_textview_draw_window();
void l3_textview_draw_status();
void l3_textview_memory_scroll_up(uint8_t steps);
void l3_textview_memory_scroll_down(uint8_t steps);
void l3_textview_memory_scroll_right(uint8_t steps);
void l3_textview_memory_scroll_left(uint8_t steps);

extern uint8_t page_table[];
extern uint8_t page_table_index;
extern uint8_t l3_textview_mode;

#endif //WIKI_TEXTVIEW_H
