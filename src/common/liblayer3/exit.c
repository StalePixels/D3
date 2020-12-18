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
    ZXN_WRITE_MMU3(L3ULABottom);
    memcpy(0x4000, 0x6000, 8192);
    ZXN_WRITE_MMU3(11);
    ZXN_WRITE_MMU2(L3ULATop);
    memcpy(0x6000, 0x4000, 8192);
    ZXN_WRITE_MMU2(10);
	// Free buffers
	esx_ide_bank_free(0, L3ULABottom);
	esx_ide_bank_free(0, L3ULATop);

	// Restore Palettes
//	system_palette_restore(RPC_SELECT_TILEMAP_PALETTE_0, SystemBackupTilemapPalette);
//	system_palette_restore(RPC_SELECT_ULA_PALETTE_0, SystemBackupULAPalette);

	// Clear ULA to default colours
	zx_cls(PAPER_WHITE|INK_BLACK);
	zx_border(INK_WHITE);

}