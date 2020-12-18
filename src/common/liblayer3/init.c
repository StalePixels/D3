//
// Created by D Rimron-Soutter on 17/12/2020.
//

#include <arch/zxn/esxdos.h>
#include <compress/zx7.h>
#include <arch/zxn.h>
#include <string.h>
#include "liblayer3.h"

uint8_t l3_palette[512] = {
        // FIRST EIGHT (32 vals)
        // DEFAULT TEXT COLOUR
        0x01, 0x01,     /* Default Blue */		0xFF, 0x01,		/* Bright White */
        // DEFAULT STATUS BAR
        0xFF, 0x01,		/* Bright White */		0x01, 0x01,     /* Default Blue */
        0x01, 0x01,     /* Default Blue */		0xFF, 0x01,		/* Bright White */
        0x01, 0x01,     /* Default Blue */		0xFF, 0x01,		/* Bright White */
        0x01, 0x01,     /* Default Blue */		0xFF, 0x01,		/* Bright White */
        0x01, 0x01,     /* Default Blue */		0xFF, 0x01,		/* Bright White */
        0x01, 0x01,     /* Default Blue */		0xFF, 0x01,		/* Bright White */
        0x01, 0x01,     /* Default Blue */		0xFF, 0x01,		/* Bright White */
        // SECOND EIGHT (32 vals)
        0x00, 0x00,		/* Black */				0xFF, 0x01,     /* Bright White */
        0x00, 0x00,		/* Black */				0xFF, 0x01,     /* Bright White */
        0x00, 0x00,		/* Black */				0xFF, 0x01,     /* Bright White */
        0x00, 0x00,		/* Black */				0xFF, 0x01,     /* Bright White */
        0x00, 0x00,		/* Black */				0xFF, 0x01,     /* Bright White */
        0x00, 0x00,		/* Black */				0xFF, 0x01,     /* Bright White */
        0x00, 0x00,		/* Black */				0xFF, 0x01,     /* Bright White */
        0x00, 0x00,		/* Black */				0xFF, 0x01,     /* Bright White */
        // THIRD EIGHT (32 vals)
        0x01, 0x01,     /* Default Blue */		0xFF, 0x01,		/* Bright White */
        0x01, 0x01,     /* Default Blue */		0xFF, 0x01,		/* Bright White */
        0x01, 0x01,     /* Default Blue */		0xFF, 0x01,		/* Bright White */
        0x01, 0x01,     /* Default Blue */		0xFF, 0x01,		/* Bright White */
        0x01, 0x01,     /* Default Blue */		0xFF, 0x01,		/* Bright White */
        0x01, 0x01,     /* Default Blue */		0xFF, 0x01,		/* Bright White */
        0x01, 0x01,     /* Default Blue */		0xFF, 0x01,		/* Bright White */
        0x01, 0x01,     /* Default Blue */		0xFF, 0x01,		/* Bright White */
        // FOURTH EIGHT (32 vals)
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        // FIFTH EIGHT (32 vals)
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        // SIXTH EIGHT (32 vals)
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        // SEVENTH EIGHT (32 vals)
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        // EIGTHTH EIGHT (32 vals)
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        // x9 EIGHT (32 vals)
        // 				DEFAULT_CURSOR_COLOUR
        0x1F, 0x01,     /* Bright Cyan */		0xFC, 0x00,     /* Bright Yellow */
        0x1F, 0x01,     /* Bright Cyan */		0xFC, 0x00,     /* Bright Yellow */
        0x1F, 0x01,     /* Bright Cyan */		0xFC, 0x00,     /* Bright Yellow */
        0x1F, 0x01,     /* Bright Cyan */		0xFC, 0x00,     /* Bright Yellow */
        0x1F, 0x01,     /* Bright Cyan */		0xFC, 0x00,     /* Bright Yellow */
        0x1F, 0x01,     /* Bright Cyan */		0xFC, 0x00,     /* Bright Yellow */
        0x1F, 0x01,     /* Bright Cyan */		0xFC, 0x00,     /* Bright Yellow */
        0x1F, 0x01,     /* Bright Cyan */		0xFC, 0x00,     /* Bright Yellow */
        // x10 EIGHT (32 vals)
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        // x11 EIGHT (32 vals)
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        // x12 EIGHT (32 vals)
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        // x13 EIGHT (32 vals)
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        // x14 EIGHT (32 vals)
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        // x15 EIGHT (32 vals)
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        // x16 EIGHT (32 vals)
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
        0xE0, 0x00,     /* Bright Red */		0xFF, 0x01,		/* Bright White */
};

void l3_init() {
    // We're going to trash this area for the Editor canvas, so let's back it up so we can restore it
    L3ULABottom = esx_ide_bank_alloc(0);
    ZXN_WRITE_MMU3(L3ULABottom);
    memcpy(0x6000, 0x4000, 8192);
    ZXN_WRITE_MMU3(11);
    L3ULATop = esx_ide_bank_alloc(0);
    ZXN_WRITE_MMU2(L3ULATop);
    memcpy(0x4000, 0x6000, 8192);
    ZXN_WRITE_MMU2(10);

    dzx7_standard(font, ((unsigned char *)0x5D00));

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

//    system_palette_save(RPC_SELECT_TILEMAP_PALETTE_0, SystemBackupTilemapPalette);

    l3_palette_restore(RPC_SELECT_TILEMAP_PALETTE_0, l3_palette);
//    system_palette_save(RPC_SELECT_ULA_PALETTE_0, SystemBackupULAPalette);

    // Force ULA bright magenta to be transparent version
    ZXN_NEXTREG(REG_PALETTE_INDEX, 27);
    ZXN_NEXTREG(REG_PALETTE_VALUE_8, 0xE3);

//    system_palette_set(RPC_SELECT_ULA_PALETTE_0, 19, SystemTilemapPalette);

    l3_clear();

    ZXN_NEXTREG(0x6b, /*0b11001000*/ 0xC8);                     // enable tilemap, 80x32 mode,
    // 1bit palette "textmode"
    // Setup the ULA for overlay UI
    zx_cls(INK_MAGENTA|PAPER_MAGENTA|BRIGHT);
    zx_border(INK_WHITE);
}