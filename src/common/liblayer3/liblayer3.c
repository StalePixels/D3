//
// Created by D Rimron-Soutter on 17/12/2020.
//

#include <stdint.h>
#include "liblayer3.h"

uint8_t L3ScreenX, L3ScreenY;
uint16_t L3ScreenColour = TEXTMODE_DEFAULT_COLOUR;
uint8_t L3CursorFlashRate = LIBLAYER3_CURSOR_FLASH_RATE;
uint8_t L3RepeatStart = LIBLAYER3_REPEAT_START;
uint8_t L3RepeatKey = LIBLAYER3_REPEAT_KEY;

uint8_t *L3PalleteStore[];

uint8_t L3ULATop, L3ULABottom;

