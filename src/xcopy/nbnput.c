//
// XCOPY: copy to an NBN PersonalServer over the ESP WiFi (-z with an
// nbn:// DST). MKDIR and PUT are PersonalServer commands.
//
// The server reads lines of up to 256 bytes, so each command names one
// directory or file relative to the server's directory. UART_Send takes
// up to 255 bytes, and that is the limit for a whole line.
//

#include <arch/zxn.h>
#include <arch/zxn/esxdos.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xcopy.h"
#include "nbn/messages.h"
#include "nbn/uart.h"
#include "nbn/net.h"
#include "nbn/nbn.h"

// Longest names that fit: "MKDIR name" and "PUT size name", with a size
// of up to 10 digits, and CR LF
#define MKDIR_NAME_MAX      (255 - 8)
#define PUT_NAME_MAX        (255 - 17)

// F_FSTAT writes it: in main memory
static struct esx_stat st;

static char size_text[11];

static char dotdot[] = "..";

// MKDIR name, relative to the server's directory. The server also makes
// any missing directories above it, and takes an existing one.
// Returns 0, or an error for name.
xc_err_t put_mkdir(char *name)
{
    if (strlen(name) > MKDIR_NAME_MAX) return ESX_ENAMETOOLONG;

    NET_Send("MKDIR ", 6);
    NET_Send(name, strlen(name));
    NET_Send("\x0D\x0A", 2);

    // the server refuses when a file has the name
    return NBN_GetStatus() == NBN_BLOCK_SUCCESS ? 0 : ESX_EEXIST;
}

// Make directory lfn in the server's directory, and change to it.
// Returns 0, or an error.
xc_err_t nbn_put_dir(char *lfn)
{
    xc_err_t err;

    if ((err = put_mkdir(lfn))) return err;
    return nbn_cd(lfn);
}

// Change the server's directory back up, after nbn_put_dir
void nbn_put_up(void)
{
    if (nbn_cd(dotdot)) NBN_Fail(err_nbn_protocol);
}

// Read len bytes of fin into nbnBlock.
// Returns 0, or an esxDOS error code.
static uint8_t read_block(uint16_t len)
{
    uint16_t n;

    // F_READ of no bytes is an error
    if (!len) return 0;

    errno = 0;
    NBN_IntsOff();
    NBN_PageIn();
    n = esx_f_read(fin, nbnBlock, len);
    NBN_PageOut();
    NBN_IntsBack();

    if (errno) return errno;
    // the file is shorter than F_FSTAT said
    if (n != len) return ESX_EIO;
    return 0;
}

// Send src_path/sfn (long name lfn, attributes attr) to the server's
// directory as lfn, or in file mode as the name at the end of dst_path.
// PUT overwrites a file of that name.
void nbn_put(const char *sfn, char *lfn, uint8_t attr)
{
    uint16_t src_len, dst_len = 0xffff, len;
    uint32_t blocks;
    uint8_t tries;
    char *name = lfn;
    xc_err_t err;

    if ((src_len = path_push(src_path, sfn)) == 0xffff) {
        ui_error(lfn, ESX_ENAMETOOLONG);
        return;
    }

    if ((attr & XC_A_HIDSYS) && !opt_hidden) goto done;

    if (dst_is_file) {
        name = (char *)path_basename(dst_path);
    } else if ((dst_len = path_push(dst_path, lfn)) == 0xffff) {
        ui_error(lfn, ESX_ENAMETOOLONG);
        goto done;
    }

    if (strlen(name) > PUT_NAME_MAX) {
        ui_error(dst_path, ESX_ENAMETOOLONG);
        goto done;
    }

    XC_NO_SCROLL();
    progress_path();

    // a disk image that NextZXOS has mounted fails here, before the PUT
    if ((err = copy_open_src(sfn, lfn))) {
        ui_error(src_path, err);
        goto done;
    }

    errno = 0;
    if (esx_f_fstat(fin, &st)) {
        err = errno;
        esx_f_close(fin);
        fin = 0xff;
        ui_error(src_path, err);
        goto done;
    }

    // End the line with CR LF: every byte after the LF is file data
    ultoa(st.size, size_text, 10);
    NET_Send("PUT ", 4);
    NET_Send(size_text, strlen(size_text));
    NET_PutCh(' ');
    NET_Send(name, strlen(name));
    NET_Send("\x0D\x0A", 2);

    // the server refuses when the name is a directory
    if (NBN_GetStatus() != NBN_BLOCK_SUCCESS) {
        esx_f_close(fin);
        fin = 0xff;
        ui_error(dst_path, ESX_EACCES);
        goto done;
    }

    // Full blocks of 4096 bytes, then one with the rest, which can be
    // empty. The server has the file open now, and cannot be told to
    // stop: a read error ends the command.
    progress_start(st.size);
    for (blocks = st.size >> 12; ; --blocks) {
        ui_check_break();
        len = blocks ? NBN_MAX_BLOCKSIZE : (uint16_t)st.size & (NBN_MAX_BLOCKSIZE - 1);
        if ((err = read_block(len))) {
            progress_end();
            XC_NO_SCROLL();
            printf("%s\n", src_path);
            exit((int)err);
        }

        // a block with a bad checksum is sent again; the server takes
        // three resends, and ends the session on a fourth bad checksum
        for (tries = 4; NBN_PutBlock(len) != NBN_BLOCK_SUCCESS; )
            if (!--tries) NBN_Fail(err_transfer_error);
        progress_add(len);

        if (!blocks) break;
    }

    esx_f_close(fin);
    fin = 0xff;
    progress_end();
    ++files_copied;

done:
    if (dst_len != 0xffff) path_pop(dst_path, dst_len);
    path_pop(src_path, src_len);
}
