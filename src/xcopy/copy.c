//
// XCOPY: copy one file
//

#include <arch/zxn/esxdos.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "xcopy.h"

static unsigned char buffer[2048];
static struct esx_stat st;

// Copies with these extensions must be unfragmented (mounted disk images
// and swap files), so they are checked after the copy
static const char *const frag_ext[] = { "P3D", "DSK", "P3S" };

// DISK_FILEMAP (xcopy.asm); the map goes in buffer
unsigned char fm_handle;
void *fm_buf;
unsigned char *fm_end;
uint8_t fm_flags;
extern uint8_t fm_filemap(void);

// IDE_DOS_MAPPING (xcopy.asm) for drive dm_drive; the description goes
// in dm_buf, ended by 0xff
unsigned char dm_drive;
void *dm_buf;
extern uint8_t dm_mapping(void);

uint16_t files_fragmented;
uint16_t files_skipped;

static uint8_t needs_frag_check(const char *name)
{
    const char *ext = strrchr(name, '.');
    uint8_t i;

    if (!ext) return 0;
    for (i = 0; i != sizeof(frag_ext) / sizeof(frag_ext[0]); ++i)
        if (!stricmp(ext + 1, frag_ext[i])) return 1;
    return 0;
}

// Is the file at path fragmented? As .DEFRAG checks it: the file is
// in one piece if each DISK_FILEMAP entry starts where the one before
// ends. Each entry covers at most about 32 MB, so a file under that size
// is in one piece only with a single entry.
static uint8_t is_fragmented(const char *path)
{
    unsigned char *e;
    uint32_t next, addr;
    uint16_t count;
    uint8_t frag = 0;

    errno = 0;
    fin = esx_f_open(path, ESX_MODE_R | ESX_MODE_OPEN_EXIST);
    if (errno) {
        fin = 0xff;
        return 0;
    }

    // the API asks for a seek to the start and a one byte read first
    esx_f_seek(fin, 0, ESX_SEEK_SET);
    esx_f_read(fin, buffer, 1);

    fm_handle = fin;
    fm_buf = buffer;
    if (!fm_filemap()) {
        for (e = buffer; e + 6 < fm_end; e += 6) {
            memcpy(&addr, e, 4);
            memcpy(&count, e + 4, 2);
            next = addr + ((fm_flags & 0x02) ? (uint32_t)count : (uint32_t)count * 512);
            memcpy(&addr, e + 6, 4);
            if (addr != next) {
                frag = 1;
                break;
            }
        }
    }

    esx_f_close(fin);
    fin = 0xff;
    return frag;
}

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

// After a copy to dst_path: warn if it must be in one piece and is not,
// unless the source file on a drive was not in one piece either
void copy_frag_check(void)
{
    if (needs_frag_check(path_basename(dst_path)) && is_fragmented(dst_path) && (opt_net || !is_fragmented(src_path))) {
        ++files_fragmented;
        printf("%s is fragmented: run .DEFRAG on it\n", dst_path);
    }
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

// The drive on which NextZXOS has mounted the file src_path (8.3 name
// sfn, long name lfn) as a disk image, or 0. IDE_DOS_MAPPING describes
// such a drive as "Mount=" and the path the image was mounted with, as
// it was given: only a path with a drive letter is compared. A drive
// mounted at startup names the file as an 8.3 directory entry, such as
// "DRV-B   P3D".
static char mounted_drive(const char *sfn, const char *lfn)
{
    unsigned char *desc = buffer;
    char *canon = (char *)buffer + 256;
    char *src_dir = canon + 256;
    char *name = src_dir + 256;
    char *p, *q;
    uint8_t i;
    char d;

    errno = 0;
    src_dir[0] = 0;
    esx_f_get_canonical_path(src_path, src_dir);
    if (errno) return 0;

    dm_buf = desc;
    for (d = 'A'; d <= 'P'; ++d) {
        dm_drive = d;
        desc[256] = 0xff;
        if (dm_mapping() != 0xff) continue;

        for (p = (char *)desc; *(unsigned char *)p != 0xff; ++p) ;
        *p = 0;
        p = (char *)desc + 6;
        if (desc[5] != '=' || p[1] != ':') continue;

        errno = 0;
        canon[0] = 0;
        esx_f_get_canonical_path(p, canon);
        if (errno || stricmp(canon, src_dir)) continue;

        p = (char *)path_basename(p);
        if (!stricmp(p, sfn) || !stricmp(p, lfn)) return d;

        if (strlen(p) == 11 && !strchr(p, '.')) {
            for (q = name, i = 0; i != 11; ++i) {
                if (i == 8) *q++ = '.';
                if (p[i] != ' ') *q++ = p[i];
            }
            *q = 0;
            if (!stricmp(name, sfn)) return d;
        }
    }
    return 0;
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

    printf("%s\n", dst_path);

    errno = 0;
    fin = esx_f_open(src_path, ESX_MODE_R | ESX_MODE_OPEN_EXIST);
    if (errno) {
        fin = 0xff;
        err = errno;
        if (err == ESX_EACCES && (err_mounted_drive[0] = mounted_drive(sfn, lfn))) err = (xc_err_t)err_mounted;
        ui_error(src_path, err);
        goto done;
    }

    fout = esx_f_open(dst_path, ESX_MODE_W | (exists == XC_DST_OLD ? ESX_MODE_OPEN_CREAT_TRUNC : ESX_MODE_OPEN_CREAT_NOEXIST));
    if (errno) {
        err = errno;
        fout = 0xff;
        copy_close(0);
        ui_error(dst_path, err);
        goto done;
    }

    for (;;) {
        ui_check_break();

        errno = 0;
        n = esx_f_read(fin, buffer, sizeof(buffer));
        if (errno) break;
        if (!n) break;

        if (esx_f_write(fout, buffer, n) != n) {
            // a short write with no error code is a full drive
            if (!errno) errno = ESX_ENOSPC;
            break;
        }
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

    ++files_copied;
    copy_frag_check();

done:
    if (dst_len != 0xffff) path_pop(dst_path, dst_len);
    path_pop(src_path, src_len);
}
