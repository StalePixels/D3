//
// Created by D Rimron-Soutter on 04/11/2020.
//

#ifndef D3_ULA_H
#define D3_ULA_H


#define _SYSVAR_ATTR_P          (*(int8_t *)(0x5C8D))
#define _SYSVAR_ATTR_P_INK      (_SYSVAR_ATTR_P & 7)
#define _SYSVAR_ATTR_P_PAPER    (_SYSVAR_ATTR_P >> 3 & 7)
#define _SYSVAR_ATTR_P_BRIGHT   ((_SYSVAR_ATTR_P >> 6) & 1)
#define _SYSVAR_ATTR_P_FLASH    ((_SYSVAR_ATTR_P >> 7) & 1)

///////////////////////////////
// ULA Text Macros
///////////////////////////////
#define printInk(k)                 printf("\x10%c", '0'+k)
#define printPaper(k)               printf("\x11%c", '0'+k)
#define printBright(k)              printf("\x13%d", k);
#define printAt(row, col)           printf("\x16%c%c", (col), (row))
#define printAtStr(row, col, ...)   printf("\x16%c%c%s", (col), (row), __VA_ARGS__)

#endif //D3_ULA_H
