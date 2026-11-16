//
// XCOPY: read the source directories through DOS_CATALOG
//

#include <arch/zxn/esxdos.h>
#include <errno.h>
#include <string.h>

#include "xcopy.h"
#include "nbn/messages.h"

// DOS_CATALOG and IDE_GET_LFN read these, so they must be in memory
// NextZXOS can see, $4000-$BFE0: the main bank of a dotn command
static struct esx_cat catalog;
static struct esx_lfn lfn;
static char cat_spec[XC_PATH_MAX + 1];

static char sfn[13];

// One-name lookup with F_OPENDIR/F_READDIR (xcopy.asm)
const char *wd_dir;
const char *wd_pat;
void *wd_buf;
unsigned char wd_handle;
extern uint8_t wd_opendir(void);
extern uint8_t wd_readdir(void);

// F_READDIR entry: attributes, then the name
static unsigned char entry[1 + ESX_FILENAME_LFN_MAX + 1 + 8];

// Per level: the catalog entry to carry on from after a subdirectory,
// and the path lengths to go back to
static struct esx_cat_entry resume[XC_DEPTH_MAX];
static uint16_t src_len[XC_DEPTH_MAX];
static uint16_t dst_len[XC_DEPTH_MAX];

static const char all[] = "*.*";

static uint8_t exact_found;

// +3DOS error codes (DOS_CATALOG, IDE_GET_LFN) and the esxDOS codes
// reported for them; any other is an I/O error
static const uint8_t p3_errs[] = {
    20, ESX_EINVAL,                 // rc_badname
    22, ESX_ENODRV,                 // rc_nodrive
    23, ESX_ENOENT,                 // rc_nofile
    30, ESX_EACCES,                 // rc_denied
    69, ESX_ENOTDIR                 // rc_notdir
};

static uint8_t p3_err(uint8_t code)
{
    uint8_t i;

    for (i = 0; i != sizeof(p3_errs); i += 2)
        if (p3_errs[i] == code) return p3_errs[i + 1];
    return ESX_EIO;
}

// DOS_CATALOG returns 0xff on an error, with a +3DOS code in errno
// (0 is rc_ready)
static uint8_t cat_result(uint8_t n)
{
    if (n == 1) return 1;
    if (n == 0xff) ui_error(src_path, p3_err(errno));
    return 0;
}

// DOS_CATALOG of src_path/pattern, one entry at a time.
// Returns 1 with an entry in catalog.cat[1], or 0 at the end.
static uint8_t cat_first(const char *pattern)
{
    strcpy(cat_spec, src_path);
    if (path_push(cat_spec, pattern) == 0xffff) {
        ui_error(src_path, ESX_ENAMETOOLONG);
        return 0;
    }

    catalog.filter = ESX_CAT_FILTER_SYSTEM | ESX_CAT_FILTER_LFN | ESX_CAT_FILTER_DIR;
    catalog.filename = p3dos_cstr_to_pstr(cat_spec);
    catalog.cat_sz = 2;
    lfn.cat = &catalog;

    return cat_result(esx_dos_catalog(&catalog));
}

static uint8_t cat_next(void)
{
    return cat_result(esx_dos_catalog_next(&catalog));
}

// The 8.3 name, long name, attributes and type of the entry in
// catalog.cat[1]. IDE_GET_LFN must come straight after DOS_CATALOG.
// The attributes come from F_READDIR of this one name, because F_STAT
// fails on directories.
// Returns 0 for a file, 0x80 for a directory, with 1 added when
// IDE_GET_LFN failed: there is then no long name, and the entry is
// not copied.
static uint8_t cat_entry(uint8_t *attr)
{
    uint8_t is_dir = catalog.cat[1].filename[7] & 0x80;

    p3dos_dosname_from_catname(sfn, catalog.cat[1].filename);
    if (esx_ide_get_lfn(&lfn, &catalog.cat[1])) return is_dir | 1;

    *attr = 0;
    wd_dir = src_path[0] ? src_path : ".";
    wd_pat = sfn;
    wd_buf = entry;
    if (!wd_opendir()) {
        if (wd_readdir()) *attr = entry[0];
        esx_f_closedir(wd_handle);
    }

    return is_dir;
}

// Make directory dst_path. An existing directory is fine.
// Returns 0, or an esxDOS error code.
uint8_t walk_make_dir(void)
{
    unsigned char dh;

    errno = 0;
    if (!esx_f_mkdir(dst_path)) return 0;
    if (errno != ESX_EEXIST) return errno;

    errno = 0;
    dh = esx_f_opendir(dst_path);
    if (errno) return ESX_EEXIST;
    esx_f_closedir(dh);
    return 0;
}

// Copy the files in src_path that match, then (with -e) each
// subdirectory, to dst_path
void walk(uint8_t depth)
{
    uint8_t attr, err, type;

    if (cat_first(src_exact ? all : src_pattern)) {
        do {
            type = cat_entry(&attr);
            if (type == 1) {
                if (!src_exact || !stricmp(sfn, src_exact)) ui_error(sfn, p3_err(errno));
            } else if (!type && (!src_exact || !stricmp(lfn.filename, src_exact) || !stricmp(sfn, src_exact))) {
                if (!depth) exact_found = 1;
                copy_file(sfn, lfn.filename, attr);
            }
        } while (cat_next());
    }

    if (!depth && src_exact && !exact_found) ui_error(src_exact, (xc_err_t)err_file_not_found);

    if (!opt_recurse) return;

    if (!cat_first(all)) return;

    do {
        type = cat_entry(&attr);
        if (type == 0x81) ui_error(sfn, p3_err(errno));
        if (type != 0x80 || !strcmp(sfn, ".") || !strcmp(sfn, "..")) continue;

        if ((attr & XC_A_HIDSYS) && !opt_hidden) continue;

        if (depth + 1 >= XC_DEPTH_MAX) {
            ui_error(src_path, ESX_ENAMETOOLONG);
            continue;
        }

        memcpy(&resume[depth], &catalog.cat[1], sizeof(struct esx_cat_entry));

        if ((src_len[depth] = path_push(src_path, sfn)) == 0xffff) {
            ui_error(lfn.filename, ESX_ENAMETOOLONG);
            continue;
        }
        if ((dst_len[depth] = path_push(dst_path, lfn.filename)) == 0xffff) {
            ui_error(src_path, ESX_ENAMETOOLONG);
            path_pop(src_path, src_len[depth]);
            continue;
        }

        if ((err = walk_make_dir())) {
            ui_error(dst_path, err);
        } else {
            walk(depth + 1);
            if (opt_keep_attr && (err = copy_attr(dst_path, attr))) ui_error(dst_path, err);
        }

        path_pop(dst_path, dst_len[depth]);
        path_pop(src_path, src_len[depth]);

        // carry on in this directory from the saved entry
        if (!cat_first(all)) return;
        memcpy(&catalog.cat[1], &resume[depth], sizeof(struct esx_cat_entry));
        catalog.completed_sz = 1;

    } while (cat_next());
}
