//
// TELNET: the screen. An 80x32 text mode tilemap with 512 tiles, over all
// of the display. telnet.h has the memory map and the attribute bits.
//

#include <compress/zx7.h>
#include <string.h>

#include "telnet.h"
#include "shell.h"

#define FONT_SIZE           (256 * 8)       // tiles 0-255, 8 bytes each

// 9-bit colour RRRGGGBBB, each part 0-7
#define RGB(r, g, b)        (((r) << 6) | ((g) << 3) | (b))

// ANSI colours 0-7, then the bright ones for bold ink. Bright black is
// grey.
static const uint16_t colours[16] = {
    RGB(0, 0, 0), RGB(5, 0, 0), RGB(0, 5, 0), RGB(5, 5, 0),
    RGB(0, 0, 5), RGB(5, 0, 5), RGB(0, 5, 5), RGB(5, 5, 5),
    RGB(3, 3, 3), RGB(7, 0, 0), RGB(0, 7, 0), RGB(7, 7, 0),
    RGB(0, 0, 7), RGB(7, 0, 7), RGB(0, 7, 7), RGB(7, 7, 7)
};

// In text mode the tilemap's transparent colour is the global one, $14,
// compared with bits 8-1 of a pixel's colour. No colour above has red 1.
#define TRANSPARENT         (RGB(1, 0, 0) >> 1)

static const uint8_t full_clip[4] = { 0, 159, 0, 255 };

// The status bar: black on white, so it stands out from the terminal
#define STATUS_ATTR         TN_ATTR(0, 7, 0)

struct tn_cell tn_status[TN_COLS];

static void colour_write(uint16_t c)
{
    tn_nr_write(0x44, c >> 1);
    tn_nr_write(0x44, c & 1);
}

// Tilemap palette 0: for each attribute, paper then ink.
// Attribute bits 7-1 are bold, paper and ink: entry 2n is paper, 2n + 1 ink.
// With reverse not 0 the two swap, for the terminal's DECSCNM.
void tn_screen_palette(uint8_t reverse) __z88dk_fastcall
{
    uint8_t n = 0;

    tn_palette_edit(TN_PALETTE_TILEMAP);
    tn_nr_write(0x40, 0);
    do {
        uint16_t paper = colours[(n >> 3) & 7];
        uint16_t ink = colours[(n & 7) | ((n >> 3) & 8)];  // bright if bold

        colour_write(reverse ? ink : paper);
        colour_write(reverse ? paper : ink);
    } while (++n < 128);
}

// The font in tiles 0-255, and the same inverted in tiles 256-511
static void font_setup(void)
{
    uint8_t *t = TN_TILES;

    memset(t, 0, 32 * 8);                       // no glyphs for 0-31
    dzx7_standard(tn_font_zx7, t + 32 * 8);
    for (uint16_t i = 0; i < FONT_SIZE; i++) t[FONT_SIZE + i] = ~t[i];
}

static void map_clear(void)
{
    struct tn_cell *c = TN_MAP;

    for (uint16_t n = 0; n < TN_COLS * TN_ROWS; n++, c++) {
        c->ch = ' ';
        c->attr = TN_ATTR_DEFAULT;
    }
}

void tn_status_show(const char *left, const char *right)
{
    uint8_t n = strlen(right);
    uint8_t i;

    for (i = 0; i < TN_COLS; i++) {
        tn_status[i].ch = ' ';
        tn_status[i].attr = STATUS_ATTR;
    }
    for (i = 0; left[i] && i < TN_COLS - n; i++) tn_status[i].ch = left[i];
    for (i = 0; i < n; i++) tn_status[TN_COLS - n + i].ch = right[i];
    tn_term_status();
}

// Called with interrupts off, the ULA and the tilemap off
void tn_screen_setup(void)
{
    font_setup();
    map_clear();
    tn_screen_palette(0);

    tn_nr_write(0x6E, (uint16_t)TN_MAP >> 8 & 0x3F);      // map base in bank 5
    tn_nr_write(0x6F, (uint16_t)TN_TILES >> 8 & 0x3F);    // tile base in bank 5
    tn_nr_write(0x6C, 0);
    tn_nr_write(0x2F, 0);
    tn_nr_write(0x30, 0);
    tn_nr_write(0x31, 0);
    tn_nr_write(0x4C, 0x0F);            // a 1-bit pixel never has index 15
    tn_nr_write(0x14, TRANSPARENT);
    tn_nr_write(0x4A, 0);               // black where nothing shows
    tn_nr_write(0x1C, 0x08);
    for (uint8_t i = 0; i < 4; i++) tn_nr_write(0x1B, full_clip[i]);

    // on, 80x32, attributes in the map, palette 0, text mode, 512 tiles
    tn_nr_write(0x6B, 0xCA);
}
