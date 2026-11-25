//
// XCOPY: copy one file
//

#include <arch/zxn/esxdos.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "xcopy.h"

unsigned char copy_buffer[2048];
static struct esx_stat st;

// Give path the MS-DOS attributes in attr: read-only, archive, and
// hidden plus system. In F_CHMOD bit 0 means writable, the opposite of
// the MS-DOS bit. Hidden and system change together, and are set with
// the hidden bit alone: setting both bits in one call sets read-only.
// Returns 0, or an esxDOS error code.
uint8_t copy_attr(const char *path, uint8_t attr)
{
    errno = 0;
    esx_f_chmod(path, 0x02, (attr & XC_A_HIDSYS) ? 0x02 : 0x00);
    if (!errno) esx_f_chmod(path, XC_A_ARCH, attr & XC_A_ARCH);
    if (!errno) esx_f_chmod(path, 0x01, (attr & XC_A_RDONLY) ? 0x00 : 0x01);
    return errno;
}

// Get dst_path ready to be written. A read-only file needs -r, and
// replacing a file needs -y or a yes at the prompt; a read-only file is
// then made writable. With same_size, a file of that size is complete
// and is left as it is.
// Returns XC_DST_NEW, XC_DST_OLD (open it with truncate) or XC_DST_SKIP.
uint8_t dst_prepare(const uint32_t *same_size)
{
    if (esx_f_stat(dst_path, &st)) return XC_DST_NEW;

    if (same_size && st.size == *same_size) {
        ++files_skipped;
        return XC_DST_SKIP;
    }
    if ((st.attr & XC_A_RDONLY) && !opt_overwrite_ro) {
        ui_error(dst_path, ESX_ERDONLY);
        return XC_DST_SKIP;
    }
    if (!opt_no_prompt) {
        printf("Overwrite %s (Yes/No/All)? ", dst_path);
        switch (ui_ask("yna")) {
            case 'n':
                return XC_DST_SKIP;
            case 'a':
                opt_no_prompt = 1;
        }
    }
    if (st.attr & XC_A_RDONLY) {
        errno = 0;
        if (esx_f_chmod(dst_path, 0x01, 0x01)) {
            ui_error(dst_path, errno);
            return XC_DST_SKIP;
        }
    }
    return XC_DST_OLD;
}

// Close both files. With remove set, delete the part written copy.
static void copy_close(uint8_t remove)
{
    if (fout != 0xff) {
        esx_f_close(fout);
        fout = 0xff;
        if (remove) esx_f_unlink(dst_path);
    }
    if (fin != 0xff) {
        esx_f_close(fin);
        fin = 0xff;
    }
}

// Open src_path (8.3 name sfn, long name lfn) as fin.
// Returns 0, or an error for src_path: err_mounted for a disk image that
// NextZXOS has mounted.
xc_err_t copy_open_src(const char *sfn, const char *lfn)
{
    const char *names[2];
    xc_err_t err;

    errno = 0;
    fin = esx_f_open(src_path, ESX_MODE_R | ESX_MODE_OPEN_EXIST);
    if (!errno) return 0;

    fin = 0xff;
    err = errno;
    if (err == ESX_EACCES) {
        names[0] = sfn;
        names[1] = lfn;
        if ((err_mounted_drive[0] = mounted_drive(names))) err = (xc_err_t)err_mounted;
    }
    return err;
}

// Copy src_path/sfn, with attributes attr, to dst_path/lfn, or to
// dst_path in file mode
void copy_file(const char *sfn, const char *lfn, uint8_t attr)
{
    uint16_t src_len, dst_len = 0xffff;
    uint16_t n;
    uint8_t exists;
    xc_err_t err;
    const char *base;

    if ((src_len = path_push(src_path, sfn)) == 0xffff) {
        ui_error(lfn, ESX_ENAMETOOLONG);
        return;
    }

    if ((attr & XC_A_HIDSYS) && !opt_hidden) goto done;

    if (dst_is_file) {
        base = path_basename(dst_path);
        if (!stricmp(canon_dst, canon_src) && (!stricmp(base, lfn) || !stricmp(base, sfn))) {
            ui_error(src_path, (xc_err_t)err_self_copy);
            goto done;
        }
    } else if ((dst_len = path_push(dst_path, lfn)) == 0xffff) {
        ui_error(lfn, ESX_ENAMETOOLONG);
        goto done;
    }

    if ((exists = dst_prepare(0)) == XC_DST_SKIP) goto done;

    XC_NO_SCROLL();
    progress_path();

    if ((err = copy_open_src(sfn, lfn))) {
        ui_error(src_path, err);
        goto done;
    }

    // dst_prepare is done with st: it takes the size of the source now
    errno = 0;
    if (esx_f_fstat(fin, &st)) {
        err = errno;
        copy_close(0);
        ui_error(src_path, err);
        goto done;
    }

    errno = 0;
    fout = esx_f_open(dst_path, ESX_MODE_W | (exists == XC_DST_OLD ? ESX_MODE_OPEN_CREAT_TRUNC : ESX_MODE_OPEN_CREAT_NOEXIST));
    if (errno) {
        err = errno;
        fout = 0xff;
        copy_close(0);
        ui_error(dst_path, err);
        goto done;
    }

    progress_start(st.size);
    for (;;) {
        ui_check_break();

        errno = 0;
        n = esx_f_read(fin, copy_buffer, sizeof(copy_buffer));
        if (errno) break;
        if (!n) break;

        if (esx_f_write(fout, copy_buffer, n) != n) {
            // a short write with no error code is a full drive
            if (!errno) errno = ESX_ENOSPC;
            break;
        }
        progress_add(n);
    }

    if (!errno) {
        esx_f_close(fin);
        fin = 0xff;
        errno = 0;
        esx_f_close(fout);
        if (!errno) fout = 0xff;
    }

    if (errno) {
        err = errno;
        copy_close(1);
        ui_error(dst_path, err);
        goto done;
    }

    if (opt_keep_attr && (err = copy_attr(dst_path, attr))) {
        ui_error(dst_path, err);
        goto done;
    }

    progress_end();
    ++files_copied;
    copy_frag_check();

done:
    if (dst_len != 0xffff) path_pop(dst_path, dst_len);
    path_pop(src_path, src_len);
}
