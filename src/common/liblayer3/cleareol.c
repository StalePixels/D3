/*
 * liblayer3 for Spectrum Next, by D. Rimron-Soutter
 *
 * BEGAN AS:   original libcuss © 2019 David Given
 * This library is distributable under the terms of the 2-clause BSD license.
 * See COPYING.cpmish in the distribution root directory for more information.
 *
 */
#include <stdio.h>
#include "liblayer3.h"

void l3_clear_to_eol(void)
{
    // (this happens if the bottom line wraps to the nex column, and then you try to clear)
    if(L3ScreenY >= LIBLAYER3_SCREENHEIGHT)
        return;

    uint8_t i = L3ScreenX;

    while (i<(LIBLAYER3_SCREENWIDTH)) {
        tilemap[L3ScreenY][i].tile = ' ';
        tilemap[L3ScreenY][i].flags = L3ScreenColour;
        i++;
    }

    l3_goto(L3ScreenX, L3ScreenY);
}

