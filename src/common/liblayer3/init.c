//
// Created by D Rimron-Soutter on 17/12/2020.
//

#include <arch/zxn/esxdos.h>
#include <compress/zx7.h>
#include <arch/zxn.h>
#include <string.h>
#include <stdio.h>
#include "../ula.h"
#include "../palette.h"
#include "liblayer3.h"

uint8_t SystemBackupTilemapPalette[512];
uint8_t SystemBackupULAPalette[512];

uint8_t LogoTile1[8] = {
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000011,
        0b00001111,
        0b00111111,
};
uint8_t LogoTile2[8] = {
        0b00000000,
        0b00000011,
        0b00001111,
        0b00111111,
        0b11111111,
        0b11111111,
        0b11111111,
        0b11111111,
};

void l3_init() {
    // We're going to trash this area for the Editor canvas, so let's back it up so we can restore it
    memcpy(L3tilemap, tilemap, sizeof(L3tilemap));
    memcpy(L3tiles, tiles, sizeof(L3tiles));

    dzx7_standard(L3font, ((unsigned char *)0x5D00));           // Not the entire tileset, because we don't
                                                                //  have the first 32 glyphs
    // And copy in our logo characters
    memcpy(tiles[30], LogoTile1, 8);
    memcpy(tiles[31], LogoTile2, 8);

    // 0x6E (110) R/W =>  Tilemap Base Address
    //  bits 7-6 = Read back as zero, write values ignored
    //  bits 5-0 = MSB of address of the tilemap in Bank 5
    ZXN_NEXTREG(0x6e, 0x6C);                                    // tilemap base address is 0x6C00

    // 0x6F (111) R/W => Tile Definitions Base Address
    //  bits 7-6 = Read back as zero, write values ignored
    //  bits 5-0 = MSB of address of tile definitions in Bank 5
    ZXN_NEXTREG(0x6f, 0x5C);                                    // base address 0x5c00
    // (vis.chars(32+) at 0x5D00)

    ZXN_NEXTREG(REG_GLOBAL_TRANSPARENCY_COLOR, 0xE3);
    ZXN_NEXTREG(REG_FALLBACK_COLOR, 0x00);

    // Engage the colour trickery
    // 1) Save the ATTR_P sysvar in a useful format, so we can use it..
    uint8_t bright = _SYSVAR_ATTR_P_BRIGHT << 4;
    uint8_t ink =  _SYSVAR_ATTR_P_INK << 1;
    uint8_t paper =  _SYSVAR_ATTR_P_PAPER << 1;

    // 2) And the tilemap current palette, before we trash it...
    palette_save(RPC_SELECT_TILEMAP_PALETTE_0, SystemBackupTilemapPalette);

    // 3) and the ULA palette, so we can inspect it
    palette_save(RPC_SELECT_ULA_PALETTE_0, SystemBackupULAPalette);

    // 4) construct a new palette based on the data taken from Sysvars, and nextregs
    uint8_t l3_palette[512];
#define L3_BRIGHT 8
    // Text BG
    l3_palette[0] = SystemBackupULAPalette[bright + paper];
    l3_palette[1] = SystemBackupULAPalette[bright + paper + 1];
    // Text FG
    l3_palette[2] = SystemBackupULAPalette[bright + ink];
    l3_palette[3] = SystemBackupULAPalette[bright + ink + 1];
    // Status Bar BG
    l3_palette[4] = SystemBackupULAPalette[ (PAPER_BLACK | L3_BRIGHT ) << 1];
    l3_palette[5] = SystemBackupULAPalette[ ((PAPER_BLACK | L3_BRIGHT ) << 1 ) + 1];
    // Status Bar FG
    l3_palette[6] = SystemBackupULAPalette[ (INK_WHITE | L3_BRIGHT ) << 1];
    l3_palette[7] = SystemBackupULAPalette[ ((INK_WHITE | L3_BRIGHT ) << 1 ) + 1];
    // Logo 1 BG
    l3_palette[8] = SystemBackupULAPalette[ (PAPER_BLACK | L3_BRIGHT ) << 1];
    l3_palette[9] = SystemBackupULAPalette[ ((PAPER_BLACK | L3_BRIGHT ) << 1 ) + 1];
    // Logo 1 FG
    l3_palette[10] = SystemBackupULAPalette[ (INK_RED | L3_BRIGHT ) << 1];
    l3_palette[11] = SystemBackupULAPalette[ ((INK_RED | L3_BRIGHT ) << 1 ) + 1];
    // Logo 2 BG
    l3_palette[12] = SystemBackupULAPalette[ (INK_RED | L3_BRIGHT ) << 1];
    l3_palette[13] = SystemBackupULAPalette[ ((INK_RED | L3_BRIGHT ) << 1 ) + 1];
    // Logo 2 FG
    l3_palette[14] = SystemBackupULAPalette[ (INK_YELLOW | L3_BRIGHT ) << 1];
    l3_palette[15] = SystemBackupULAPalette[ ((INK_YELLOW | L3_BRIGHT ) << 1 ) + 1];
    // Logo 3 BG
    l3_palette[16] = SystemBackupULAPalette[ (INK_YELLOW | L3_BRIGHT ) << 1];
    l3_palette[17] = SystemBackupULAPalette[ ((INK_YELLOW | L3_BRIGHT ) << 1 ) + 1];
    // Logo 3 FG
    l3_palette[18] = SystemBackupULAPalette[ (INK_GREEN| L3_BRIGHT ) << 1];
    l3_palette[19] = SystemBackupULAPalette[ ((INK_GREEN | L3_BRIGHT ) << 1 ) + 1];
    // Logo 4 BG
    l3_palette[20] = SystemBackupULAPalette[ (INK_GREEN | L3_BRIGHT ) << 1];
    l3_palette[21] = SystemBackupULAPalette[ ((INK_GREEN | L3_BRIGHT ) << 1 ) + 1];
    // Logo 4 FG
    l3_palette[22] = SystemBackupULAPalette[ (INK_CYAN| L3_BRIGHT ) << 1];
    l3_palette[23] = SystemBackupULAPalette[ ((INK_CYAN | L3_BRIGHT ) << 1 ) + 1];
    // Logo 5 BG
    l3_palette[24] = SystemBackupULAPalette[ (INK_CYAN | L3_BRIGHT ) << 1];
    l3_palette[25] = SystemBackupULAPalette[ ((INK_CYAN | L3_BRIGHT ) << 1 ) + 1];
    // Logo 5 FG
    l3_palette[26] = SystemBackupULAPalette[ (INK_BLACK| L3_BRIGHT ) << 1];
    l3_palette[27] = SystemBackupULAPalette[ ((INK_BLACK | L3_BRIGHT ) << 1 ) + 1];

    // 5) Load the newly created palette stub back into the graphics hardware
    palette_restore(RPC_SELECT_TILEMAP_PALETTE_0, l3_palette);

    // If all that palette trickery worked we'll just look like we extended the current default
    //  paper into the existing border, and will seamlessly revert once finished...  *if*
    //  it worked, and the user wasn't using FLASH 1 (I mean, who does that?!)

    l3_clear();

    ZXN_NEXTREG(0x6b, /*0b11001000*/ 0xC8);                     // enable tilemap, 80x32 mode,
                                                                //     1bit palette "textmode"


}