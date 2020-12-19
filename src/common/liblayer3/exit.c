//
// Created by D Rimron-Soutter on 20/11/2020.
//

#include "liblayer3.h"
#include <arch/zxn.h>
#include <arch/zxn/esxdos.h>
#include <string.h>

void l3_exit() {
    // disable textmode
    ZXN_NEXTREG(0x6b, 0);                                    // disable tilemap

    // Restore memory we trashed
    memcpy(tilemap, L3tilemap, sizeof(L3tilemap));
    memcpy(tiles, L3tiles, sizeof(L3tiles));

	// Restore Palettes
//	system_palette_restore(RPC_SELECT_TILEMAP_PALETTE_0, SystemBackupTilemapPalette);
//	system_palette_restore(RPC_SELECT_ULA_PALETTE_0, SystemBackupULAPalette);

//	zx_cls(PAPER_WHITE|INK_BLACK);
//	zx_border(INK_WHITE);

}