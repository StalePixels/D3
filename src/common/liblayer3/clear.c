/*
 * liblayer3 for Spectrum Next, by D. Rimron-Soutter
 *
 * BEGAN AS:   original libcuss © 2019 David Given
 * This library is distributable under the terms of the 2-clause BSD license.
 * See COPYING.cpmish in the distribution root directory for more information.
 *
 */
#include <string.h>
#include "liblayer3.h"

void l3_clear(void)
{
    memset(tilemap, 0, sizeof(tilemap));
	L3ScreenX = L3ScreenY = 0;
}
