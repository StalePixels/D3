/*
 * liblayer3 for Spectrum Next, by D. Rimron-Soutter
 *
 * BEGAN AS:   original libcuss © 2019 David Given
 * This library is distributable under the terms of the 2-clause BSD license.
 * See COPYING.cpmish in the distribution root directory for more information.
 *
 */

#include "liblayer3.h"

void l3_puts_raw(const char* s, const uint8_t len)
{
	while (len--)
	{
		uint16_t c = *s++;
		l3_putc_raw(c);
	}
}

