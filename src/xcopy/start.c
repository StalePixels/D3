//
// XCOPY: options, help, and the checks of SRC and DST before the copy.
// In the bank (see bank.asm). Every address that goes to NextZXOS, or that
// main memory keeps after xc_start, is in main memory.
//

#include <arch/zxn/esxdos.h>
#include <errno.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/d3.h"

#include "xcopy.h"

#pragma codeseg PAGE_12
#pragma constseg PAGE_12

static const char *const help_text[] = {
        "\nXCOPY",
          "=====",
        "\n Usage",
        ".XCOPY [-hckeryz] SRC DST",
        "\nSRC: file, directory or pattern",
        "-h copy hidden and system files",
        "-c continue after an error",
        "-k copy attributes",
        "-e copy subdirs, even empty",
        "-r overwrite read-only files",
        "-y overwrite without asking",
        "-z copy from or to an NBN",
        "   server; SRC or DST is",
        "   nbn://host[:port]/path",
        "   (port 48128 if not given)",
        "   From a server: DST files",
        "   of the same size are",
        "   complete, and skipped;",
        "   -h and -k do nothing",
        "   To a PersonalServer: files",
        "   are overwritten; -k, -r",
        "   and -y do nothing"
};

static void exit_with_help(void)
{
    d3_logo(); printf(" XCOPY\n");
    for (uint8_t help = 0; help < sizeof(help_text) / sizeof(help_text[0]); help++)
        printf("%s\n", help_text[help]);
    exit(0);
}

// Remove a trailing "/" from path, unless it is the root.
// Returns 1 if there was one.
static uint8_t strip_slash(char *path)
{
    uint16_t len = strlen(path);

    if (len > 1 && path[len - 1] == '/' && path[len - 2] != ':') {
        path[len - 1] = 0;
        return 1;
    }
    return 0;
}

// Copy arg into path, without a trailing "/" unless it is the root.
// Returns 1 if there was a trailing "/".
static uint8_t take_path(char *path, const char *arg)
{
    if (strlen(arg) > XC_PATH_MAX) exit(ESX_ENAMETOOLONG);
    strcpy(path, arg);
    return strip_slash(path);
}

// Data, so in main memory: F_OPENDIR reads it
static char cur_dir[] = ".";

static uint8_t is_dir(const char *path)
{
    unsigned char dh;

    errno = 0;
    dh = esx_f_opendir(path[0] ? path : cur_dir);
    if (errno) return 0;
    esx_f_closedir(dh);
    return 1;
}

// Canonical form of directory dir in out: drive, 8.3 names in upper
// case and a trailing "/", whatever spelling dir uses (long or short
// names, any case, relative, "..").
// Returns 0, or an esxDOS error code.
static uint8_t path_canonical(char *dir, char *out)
{
    uint16_t len = path_push(dir, "X");

    if (len == 0xffff) return ESX_ENAMETOOLONG;

    errno = 0;
    out[0] = 0;
    esx_f_get_canonical_path(dir, out);
    path_pop(dir, len);

    if (errno) return errno;
    if ((out[1] != ':') || (out[2] != '/')) return ESX_ENOTDIR;
    return 0;
}

// Is canonical directory inner the same as, or below, canonical outer?
static uint8_t path_inside(const char *inner, const char *outer)
{
    return strnicmp(inner, outer, strlen(outer)) == 0;
}

// Source on a drive: a directory, or a directory part and a pattern or
// name. Returns 1 if it can be more than one file.
static uint8_t local_open(const char *src)
{
    char *base;
    uint8_t many, err;

    take_path(src_path, src);
    base = (char *)path_basename(src_path);

    if (!strpbrk(base, "*?") && is_dir(src_path)) {
        src_pattern = pattern_all;
        many = 1;
    } else {
        strcpy(src_name, base);
        *base = 0;
        strip_slash(src_path);
        if (strpbrk(src_name, "*?")) {
            src_pattern = src_name;
            many = 1;
        } else {
            src_exact = src_name;
            many = 0;
        }
        if (!is_dir(src_path)) exit(errno ? errno : ESX_ENOTDIR);
    }

    if ((err = path_canonical(src_path, canon_src))) exit(err);

    return many;
}

// Take the options, SRC and DST from argv (ended by a null pointer), and
// get SRC and DST ready for the copy
void xc_start_banked(char **argv) __z88dk_fastcall
{
    const char *src = 0, *dst = 0, *p;
    uint8_t many, dst_slash, err;
    static struct esx_stat st;

    // The CRT cuts the command line at 128 characters, with no message.
    // A line that may have been cut is refused.
    if (cmdline_len >= 127) exit((int)err_line_too_long);

    for (int i = 1; argv[i]; ++i) {
        if (argv[i][0] == '-' && argv[i][1]) {
            for (p = argv[i] + 1; *p; ++p) {
                switch (*p | 0x20) {
                    case 'h': opt_hidden = 1; break;
                    case 'c': opt_continue = 1; break;
                    case 'k': opt_keep_attr = 1; break;
                    case 'e': opt_recurse = 1; break;
                    case 'r': opt_overwrite_ro = 1; break;
                    case 'y': opt_no_prompt = 1; break;
                    case 'z': opt_net = 1; break;
                    default: exit((int)err_invalid_option);
                }
            }
        } else if (!src) {
            src = argv[i];
        } else if (!dst) {
            dst = argv[i];
        } else {
            exit_with_help();
        }
    }

    if (!dst) exit_with_help();

    // the file calls would take a wildcard in DST as a pattern
    if (strpbrk(dst, "*?")) exit(ESX_EINVAL);

    // -z with one nbn:// side: SRC, or DST on a PersonalServer
    net_put = nbn_is_url(dst);
    if (net_put && nbn_is_url(src)) exit((int)err_two_urls);
    if ((nbn_is_url(src) | net_put) != opt_net) exit((int)(opt_net ? err_need_url : err_need_z));

    // Source: on a drive, or on an NBN server

    many = (opt_net && !net_put) ? nbn_open((char *)src) : local_open(src);
    many |= opt_recurse;

    // Destination: an existing directory or file, or a new one

    if (net_put) {
        nbn_put_open((char *)dst, many);
        return;
    }

    dst_slash = take_path(dst_path, dst);

    if (is_dir(dst_path)) {
        if ((err = path_canonical(dst_path, canon_dst))) exit(err);
        if (!opt_net && !stricmp(canon_dst, canon_src)) exit((int)err_self_copy);
        if (!opt_net && opt_recurse && path_inside(canon_dst, canon_src)) exit((int)err_dst_in_src);
    } else {
        errno = 0;
        if (!esx_f_stat(dst_path, &st)) {
            if (dst_slash) exit(ESX_ENOTDIR);
            dst_is_file = 1;
        } else if (errno != ESX_ENOENT) {
            exit(errno);
        } else if (!dst_slash && !many) {
            dst_is_file = 1;                // one file to a new name
        }

        // the directory DST is in (the filename part is ignored)
        errno = 0;
        canon_dst[0] = 0;
        esx_f_get_canonical_path(dst_path, canon_dst);
        if (errno) exit(errno);

        if (dst_is_file) {
            if (opt_recurse) exit(ESX_ENOTDIR);
        } else {
            if (!opt_net && opt_recurse && path_inside(canon_dst, canon_src)) exit((int)err_dst_in_src);
            errno = 0;
            if (esx_f_mkdir(dst_path)) exit(errno);
        }
    }
}
