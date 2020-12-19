/*
 * liblayer3 for Spectrum Next, by D. Rimron-Soutter
 *
 * BEGAN AS:   original libcuss © 2019 David Given
 * This library is distributable under the terms of the 2-clause BSD license.
 * See COPYING.cpmish in the distribution root directory for more information.
 *
 */
#ifndef LIBLAYER3_H
#define LIBLAYER3_H

#include <stdint.h>


#define RPC_SELECT_TILEMAP_PALETTE_0 	0x30
#define RPC_SELECT_TILEMAP_PALETTE_1	0x70

#define SYSTEM_PALETTE_DEFAULT_BACKGROUND		0
#define SYSTEM_PALETTE_DEFAULT_FOREGROUND		1
#define SYSTEM_PALETTE_CURSOR_BACKGROUND		128
#define SYSTEM_PALETTE_CURSOR_FOREGROUND		129
#define SYSTEM_PALETTE_STATUS_BACKGROUND		2
#define SYSTEM_PALETTE_STATUS_FOREGROUND		3
#define SYSTEM_PALETTE_ERROR_BACKGROUND			4
#define SYSTEM_PALETTE_ERROR_FOREGROUND			5


#define WAIT_FOR_SCANLINE(line)         while (ZXN_READ_REG(REG_ACTIVE_VIDEO_LINE_L) == line); \
                                        while (ZXN_READ_REG(REG_ACTIVE_VIDEO_LINE_L) != line)

#define TEXTMODE_ULA_BEHIND             0x00
#define TEXTMODE_ULA_FRONT              0x01

#define TEXTMODE_DEFAULT_COLOUR			0
#define TEXTMODE_REVERSE_COLOUR			128

struct __tilemap
{
    unsigned char tile;
    unsigned char flags;
};

extern struct __tilemap tilemap[32][80];
extern struct __tilemap L3tilemap[32][80];

struct __tiles
{
    unsigned char bmp[8];           // each tile image is 8 bytes (64px@1bpp)
};

extern const uint8_t 	L3font[];
extern unsigned char ula_bank[];

extern struct __tiles tiles[256];   //
extern struct __tiles L3tiles[256];   //

extern uint8_t L3ScreenX, L3ScreenY;
extern uint16_t L3ScreenColour;
extern uint8_t L3CursorFlashRate;
extern uint8_t L3RepeatStart;
extern uint8_t L3RepeatKey;
extern uint8_t L3ULATop, L3ULABottom;
extern uint8_t *L3PalleteStore[];

#define LIBLAYER3_REPEAT_START 25
#define LIBLAYER3_REPEAT_KEY 15
#define LIBLAYER3_CURSOR_FLASH_RATE 10


extern void l3_goto(uint16_t x, uint16_t y);
extern void l3_clear(void);
extern void l3_putc(uint16_t c);
extern void l3_puts(const char* s);
extern uint8_t l3_getc(void);
extern void l3_newline(void);
extern void l3_clear_to_eol(void);
extern void l3_revon(void);
extern void l3_revoff(void);
extern void l3_init();
extern void l3_exit();
void l3_palette_restore(uint8_t PaletteType, uint8_t *MemorySource);

#define LIBLAYER3_SCREENWIDTH 80
#define LIBLAYER3_SCREENHEIGHT 32
#define LIBLAYER3_ANSI

// OCTAL BATSHITTERY AHEAD!
#define LIBLAYER3_CLEAR "\033[2J"
#define LIBLAYER3_CLEAREOL "\033[K"
#define LIBLAYER3_NEWLINE "\r\n"

#define LIBLAYER3_KEY_LEFT "\010"
#define LIBLAYER3_KEY_DOWN "\012"
#define LIBLAYER3_KEY_UP "\013"
//#define LIBLAYER3_KEY_RIGHT "\014"
#define LIBLAYER3_KEY_RIGHT "\011"

#endif // #LIBLAYER3_H
