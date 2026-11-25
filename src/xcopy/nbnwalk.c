//
// XCOPY: copy from an NBN server over the ESP WiFi (-z)
//
// The NBN client code in nbn/ is a copy of NBNtools clients/common
// (branch SDCC, ad7927d), less the functions .xcopy does not use.
// BLOCKSIZE is never sent: servers that do not know it close the link.
//

#include <arch/zxn.h>
#include <arch/zxn/esxdos.h>
#include <ctype.h>
#include <errno.h>
#include <intrinsic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xcopy.h"
#include "nbn/messages.h"
#include "nbn/uart.h"
#include "nbn/net.h"
#include "nbn/nbn.h"

// In the 8K dot part: the main bank keeps its free space for the stack
#pragma codeseg code_dot

#define NBN_TYPE_DIR        0
#define NBN_TYPE_FILE       1

// The directory now listed, and the page of it in nbnBlock (0: none)
static nbnDirectory_t dir = { nbnBuff };
static uint16_t dir_page;

// One listing entry, copied out of nbnBlock
static struct {
    uint32_t size;
    uint8_t type;
    char name[256];                 // a FAT long name is at most 255
} ent;

static uint8_t exact_found;

// Runs first in the exit cleanup, also after an error with the NBN pages
// in, and with the home page at $C000 for NBN_Free. The flags are cleared
// first: a timeout in NET_Close calls exit again, and the cleanup runs
// again from the start.
void nbn_exit(void)
{
    uint8_t state = net_state;

    if (!state) return;
    net_state = 0;

    NBN_PageOut();
    intrinsic_ei();

    if (state & NET_PAGES) NBN_Free();
    nbn_close(state);
}

static void send_status(char c)
{
    NET_PutCh(c);
    NET_Send("\x0D\x0A", 2);
}

// Change the server's directory to path.
// Returns 0, or an error for the path.
xc_err_t nbn_cd(char *path)
{
    // "CD path" and LF CR must fit in the 255 bytes UART_Send takes, and
    // in the server's 256-byte line
    if (strlen(path) > 250) return ESX_ENAMETOOLONG;

    dir_page = 0;
    if (NBN_ChangeDirectory(path) != NBN_BLOCK_SUCCESS) return (xc_err_t)err_file_not_found;
    NBN_ParseDirectoryHeader(&dir);
    return 0;
}

// Entry idx of page of the listing, into ent.
// Returns 0 when the page has no entry idx.
static uint8_t get_entry(uint16_t page, uint8_t idx)
{
    const unsigned char *e = nbnBlock;
    uint8_t i, n;

    if (dir_page != page) {
        dir.currentPage = page;
        NBN_GetDirectory(&dir);
        dir_page = page;
    }
    if (idx >= dir.currentPageSize) return 0;

    // Size, type and name of each entry, until entry idx
    for (i = 0; ; ++i) {
        NBN_IntsOff();
        NBN_PageIn();
        memcpy(&ent, e, 5);
        e += 5;
        for (n = 0; *e; ++e)
            if (n < sizeof(ent.name) - 1) ent.name[n++] = *e;
        ++e;
        NBN_PageOut();
        NBN_IntsBack();
        ent.name[n] = 0;
        if (i == idx) return 1;
    }
}

// One block of len bytes, asked for again on a bad checksum
static void get_block(uint16_t len)
{
    uint8_t tries = 3;

    while (!NBN_GetBlock(len)) {
        if (!--tries) NBN_Fail(err_transfer_error);
        send_status(NBN_BLOCK_FAIL);
    }
}

// Write the len bytes in nbnBlock to fout.
// Returns 0, or an esxDOS error code.
static uint8_t write_block(uint16_t len)
{
    uint16_t n;

    errno = 0;
    NBN_IntsOff();
    NBN_PageIn();
    n = esx_f_write(fout, nbnBlock, len);
    NBN_PageOut();
    NBN_IntsBack();

    if (errno) return errno;
    // a short write with no error code is a full drive
    if (n != len) return ESX_ENOSPC;
    return 0;
}

// Fetch ent from the server's directory to dst_path. A file there of
// the same size is complete and is skipped; any other is fetched again.
static void fetch(void)
{
    uint32_t size, blocks;
    uint16_t remainder, len;
    uint8_t how;
    xc_err_t err = 0;

    if ((how = dst_prepare(&ent.size)) == XC_DST_SKIP) return;

    XC_NO_SCROLL();
    progress_path();

    // the blocks of the file go where the listing is
    dir_page = 0;

    NET_Send("GET ", 4);
    NET_Send(ent.name, strlen(ent.name));
    NET_Send("\x0A", 1);

    // DST is opened only when the server sends the file, so a GET that
    // fails leaves an old copy as it was
    if (!NBN_CheckVersionByte(false)) {
        ui_error(dst_path, (xc_err_t)err_file_not_found);
        return;
    }

    NET_GetUInt32((uint8_t *)&size);
    NET_GetUInt32((uint8_t *)&blocks);
    NET_GetUInt16((uint8_t *)&remainder);
    while (NET_GetUChar()) ;                // the name, as in the listing

    errno = 0;
    fout = esx_f_open(dst_path, ESX_MODE_W | (how == XC_DST_OLD ? ESX_MODE_OPEN_CREAT_TRUNC : ESX_MODE_OPEN_CREAT_NOEXIST));
    if (errno) {
        fout = 0xff;
        err = errno;
    }

    // "!" asks for each block, and after the last one ends the GET.
    // After an open or write error the rest is read and dropped.
    progress_start(size);
    for (;;) {
        ui_check_break();
        send_status(NBN_BLOCK_SUCCESS);
        len = blocks ? NBN_MAX_BLOCKSIZE : remainder;
        get_block(len);
        if (!err && len) err = write_block(len);
        progress_add(len);
        if (!blocks) break;
        --blocks;
    }
    send_status(NBN_BLOCK_SUCCESS);

    if (!err) {
        errno = 0;
        esx_f_close(fout);
        err = errno;
    }
    if (err) {
        if (fout != 0xff) {
            esx_f_close(fout);
            esx_f_unlink(dst_path);
        }
        fout = 0xff;
        ui_error(dst_path, err);
        return;
    }
    fout = 0xff;

    progress_end();
    ++files_copied;
    copy_frag_check();
}

// Copy the files in the server's directory src_path that match, then
// (with -e) each subdirectory, to dst_path
void nbn_walk(uint8_t depth)
{
    uint16_t page, src_len, dst_len;
    uint8_t idx;
    xc_err_t err;

    for (page = 1; ; ++page) {
        for (idx = 0; get_entry(page, idx); ++idx) {
            if (ent.type != NBN_TYPE_FILE) continue;
            if (src_exact ? stricmp(ent.name, src_exact) : !nbn_match(src_pattern, ent.name)) continue;

            if (!depth) exact_found = 1;

            // The server closes the link on a GET of a name over 127
            // characters. The file calls would take a wildcard as a pattern.
            if (strlen(ent.name) > 127) {
                ui_error(ent.name, ESX_ENAMETOOLONG);
            } else if (strpbrk(ent.name, "*?")) {
                ui_error(ent.name, ESX_EINVAL);
            } else if (dst_is_file) {
                fetch();
            } else if ((dst_len = path_push(dst_path, ent.name)) == 0xffff) {
                ui_error(ent.name, ESX_ENAMETOOLONG);
            } else {
                fetch();
                path_pop(dst_path, dst_len);
            }
        }
        if (page >= dir.totalPages) break;
    }

    if (!depth && src_exact && !exact_found) ui_error(src_exact, (xc_err_t)err_file_not_found);

    if (!opt_recurse) return;

    for (page = 1; ; ++page) {
        for (idx = 0; get_entry(page, idx); ++idx) {
            if (ent.type != NBN_TYPE_DIR || !strcmp(ent.name, "..") || !strcmp(ent.name, ".")) continue;

            if (strpbrk(ent.name, "*?")) {
                ui_error(ent.name, ESX_EINVAL);
                continue;
            }

            if (depth + 1 >= XC_DEPTH_MAX) {
                ui_error(src_path, ESX_ENAMETOOLONG);
                continue;
            }
            if ((src_len = path_push(src_path, ent.name)) == 0xffff) {
                ui_error(ent.name, ESX_ENAMETOOLONG);
                continue;
            }
            if ((dst_len = path_push(dst_path, ent.name)) == 0xffff) {
                ui_error(src_path, ESX_ENAMETOOLONG);
                path_pop(src_path, src_len);
                continue;
            }

            if ((err = walk_make_dir())) {
                ui_error(dst_path, err);
            } else if ((err = nbn_cd(src_path))) {
                ui_error(src_path, err);
            } else {
                nbn_walk(depth + 1);
            }

            path_pop(dst_path, dst_len);
            path_pop(src_path, src_len);

            // back to this directory; get_entry lists the page again
            if (nbn_cd(src_path)) NBN_Fail(err_nbn_protocol);
        }
        if (page >= dir.totalPages) break;
    }
}
