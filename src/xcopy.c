//
// XCOPY: copy files and directory trees.
// main and the exit cleanup; the start-up is in xcopy/start.c, in the bank.
//

#include <z80.h>
#include <arch/zxn.h>
#include <arch/zxn/esxdos.h>

#include <stdlib.h>

#include "xcopy/xcopy.h"

// The stack is in the main bank, below $C000: $C000-$FFFF is free for the
// bank (xcopy/bank.asm). M_P3DOS takes addresses up to $BFE0.
#pragma output REGISTER_SP = 0xBFE0

// The highest logical page the dotn loader handles: the bank, page 12.
// The loader's two tables in the 8K dot part have a byte for each page.
#pragma output DOTN_LAST_PAGE = 12

uint8_t opt_hidden;
uint8_t opt_continue;
uint8_t opt_keep_attr;
uint8_t opt_recurse;
uint8_t opt_overwrite_ro;
uint8_t opt_no_prompt;
uint8_t opt_net;
uint8_t net_put;

char src_path[XC_PATH_MAX + 1];
const char *src_pattern;
const char *src_exact;
char src_name[XC_PATH_MAX + 1];

char dst_path[XC_PATH_MAX + 1];
uint8_t dst_is_file;

char canon_src[XC_PATH_MAX + 1];
char canon_dst[XC_PATH_MAX + 1];

unsigned char fin = 0xff;
unsigned char fout = 0xff;

uint16_t files_copied;
uint16_t errors_seen;

static unsigned char old_cpu_speed;
static unsigned char old_scr_ct;

// Runs at every exit, also after BREAK or a BASIC error at "scroll?", and
// with the bank paged in or not: a copy that is not complete is deleted.
// The home page goes back at $C000 first, because NBN_Free is an M_P3DOS
// call. The bank is paged in last: an error message from xcopy.asm or
// nbn/messages.asm is in the bank, and the CRT copies it after this.
static void xcopy_exit(void)
{
    xc_page_home();
    nbn_exit();

    if (fout != 0xff) {
        esx_f_close(fout);
        esx_f_unlink(dst_path);
    }
    if (fin != 0xff) esx_f_close(fin);

    ZXN_NEXTREGA(REG_TURBO_MODE, old_cpu_speed);
    *(volatile unsigned char *)23692 = old_scr_ct;

    xc_page_bank();
}

int main(int argc, char **argv)
{
    old_cpu_speed = ZXN_READ_REG(REG_TURBO_MODE);
    old_scr_ct = *(volatile unsigned char *)23692;
    ZXN_NEXTREG(REG_TURBO_MODE, 3);
    atexit(xcopy_exit);

    // argv ends with a null pointer, after the argc entries
    (void)argc;
    xc_start(argv);

    if (opt_net && !net_put) nbn_walk(0);
    else walk(0);

    xc_summary();
    return 0;
}
