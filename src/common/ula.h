//
// Created by D Rimron-Soutter on 04/11/2020.
//

#ifndef D3_ULA_H
#define D3_ULA_H

///////////////////////////////
// ULA Text Macros
///////////////////////////////
#define printInk(k)                 printf("\x10%c", '0'+k)
#define printPaper(k)               printf("\x11%c", '0'+k)
#define printBright(k)              printf("\x13%d", k);
#define printAt(row, col)           printf("\x16%c%c", col, row)
#define printAtStr(row, col, ...)   printf("\x16%c%c%s", (col)+1, (row)+1, __VA_ARGS__)

#endif //D3_ULA_H
