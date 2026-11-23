//
// XCOPY: what .XCOPY reports: error messages, disk images that NextZXOS
// has mounted, copies that must be in one piece, and the summary at the
// end. In the bank (see bank.asm).
//

#include <arch/zxn/esxdos.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "xcopy.h"

#pragma codeseg PAGE_12
#pragma constseg PAGE_12

// M_GETERR writes it: in main memory
static char msg[33];

// DISK_FILEMAP (xcopy.asm); the map goes in copy_buffer
unsigned char fm_handle;
void *fm_buf;
unsigned char *fm_end;
uint8_t fm_flags;
extern uint8_t fm_filemap(void);

// IDE_DOS_MAPPING (xcopy.asm) for drive dm_drive; the description goes
// in dm_buf, ended by 0xff. An M_P3DOS call: called through
// dm_mapping_home (bank.asm).
unsigned char dm_drive;
void *dm_buf;
extern uint8_t dm_mapping_home(void);

uint16_t files_fragmented;
uint16_t files_skipped;

void ui_print_msg_banked(xc_err_t err) __z88dk_fastcall
{
    const char *p;

    if (err < 256) {
        esx_m_geterr(err, msg);
        printf("%s", msg);
    } else {
        for (p = (const char *)err; !(*p & 0x80); ++p) putchar(*p);
        putchar(*p & 0x7f);
    }
}

// The drive on which NextZXOS has mounted the file src_path (8.3 name
// names[0], long name names[1]) as a disk image, or 0. IDE_DOS_MAPPING
// describes such a drive as "Mount=" and the path the image was mounted
// with, as it was given: only a path with a drive letter is compared. A
// drive mounted at startup names the file as an 8.3 directory entry, such
// as "DRV-B   P3D".
char mounted_drive_banked(const char *const *names) __z88dk_fastcall
{
    unsigned char *desc = copy_buffer;
    char *canon = (char *)copy_buffer + 256;
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
        if (dm_mapping_home() != 0xff) continue;

        for (p = (char *)desc; *(unsigned char *)p != 0xff; ++p) ;
        *p = 0;
        p = (char *)desc + 6;
        if (desc[5] != '=' || p[1] != ':') continue;

        errno = 0;
        canon[0] = 0;
        esx_f_get_canonical_path(p, canon);
        if (errno || stricmp(canon, src_dir)) continue;

        p = (char *)path_basename(p);
        if (!stricmp(p, names[0]) || !stricmp(p, names[1])) return d;

        if (strlen(p) == 11 && !strchr(p, '.')) {
            for (q = name, i = 0; i != 11; ++i) {
                if (i == 8) *q++ = '.';
                if (p[i] != ' ') *q++ = p[i];
            }
            *q = 0;
            if (!stricmp(name, names[0])) return d;
        }
    }
    return 0;
}

// Copies with these extensions must be unfragmented (mounted disk images
// and swap files), so they are checked after the copy. No table of them:
// SDCC splits the address of a table into bytes, and the bank's addresses
// do not fit in 16 bits until the linker cuts them.
static uint8_t needs_frag_check(const char *name)
{
    const char *ext = strrchr(name, '.');

    if (!ext) return 0;
    ++ext;
    return !stricmp(ext, "P3D") || !stricmp(ext, "DSK") || !stricmp(ext, "P3S");
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
    esx_f_read(fin, copy_buffer, 1);

    fm_handle = fin;
    fm_buf = copy_buffer;
    if (!fm_filemap()) {
        for (e = copy_buffer; e + 6 < fm_end; e += 6) {
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

// After a copy to dst_path: warn if it must be in one piece and is not,
// unless the source file on a drive was not in one piece either
void copy_frag_check_banked(void)
{
    if (needs_frag_check(path_basename(dst_path)) && is_fragmented(dst_path) && (opt_net || !is_fragmented(src_path))) {
        ++files_fragmented;
        printf("%s is fragmented: run .DEFRAG on it\n", dst_path);
    }
}

void xc_summary_banked(void)
{
    printf("%u file(s) copied\n", files_copied);
    if (files_skipped) printf("%u file(s) complete, skipped\n", files_skipped);
    if (files_fragmented) printf("%u file(s) need .DEFRAG\n", files_fragmented);
    if (errors_seen) printf("%u error(s)\n", errors_seen);
}
