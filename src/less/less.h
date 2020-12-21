//
// Created by D Rimron-Soutter on 18/12/2020.
//

#ifndef D3_LESS_H
#define D3_LESS_H

#include <stdio.h>

extern unsigned char err_no_memory[];
extern unsigned char err_missing_filename[];

void less_exit();
void less(char *title, unsigned char text_in, uint8_t viewer_mode);
#endif //D3_LESS_H
