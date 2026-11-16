//
// XCOPY: copy files and directory trees
//

#include <z80.h>
#include <arch/zxn.h>
#include <arch/zxn/esxdos.h>
#include <errno.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/build.h"
#include "common/d3.h"

#include "xcopy/xcopy.h"

uint8_t opt_hidden;
uint8_t opt_assume_dir;
uint8_t opt_continue;
uint8_t opt_keep_attr;
uint8_t opt_recurse;
uint8_t opt_overwrite_ro;
uint8_t opt_no_prompt;
uint8_t opt_net;

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

const char *help_text[] = {
        "\nXCOPY",
          "=====",
        "\n Usage",
        ".XCOPY [-hickeryz] SRC DST",
        "\nSRC: file, directory or pattern",
        "-h copy hidden and system files",
        "-i new DST is a directory when",
        "   SRC is more than one file",
        "-c continue after an error",
        "-k copy attributes",
        "-e copy subdirs, even empty",
        "-r overwrite read-only files",
        "-y overwrite without asking",
        "-z copy from an NBN server:",
        "   SRC is nbn://host[:port]/path",
        "   (port 48128 if not given);",
        "   DST files of the same size",
        "   are complete, and skipped;",
        "   -h and -k do nothing"
};

static void exit_with_help(void)
{
    d3_logo(); printf(" XCOPY\n");
    for (uint8_t help = 0; help < sizeof(help_text) / sizeof(help_text[0]); help++)
        printf("%s\n", help_text[help]);
    exit(0);
}

// Runs at every exit, also after BREAK or a BASIC error at "scroll?":
// a copy that is not complete is deleted
static void xcopy_exit(void)
{
    nbn_exit();

    if (fout != 0xff) {
        esx_f_close(fout);
        esx_f_unlink(dst_path);
    }
    if (fin != 0xff) esx_f_close(fin);

    ZXN_NEXTREGA(REG_TURBO_MODE, old_cpu_speed);
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

static uint8_t is_dir(const char *path)
{
    unsigned char dh;

    errno = 0;
    dh = esx_f_opendir(path[0] ? path : ".");
    if (errno) return 0;
    esx_f_closedir(dh);
    return 1;
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
        src_pattern = "*.*";
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

int main(int argc, char **argv)
{
    const char *src = 0, *dst = 0, *p;
    uint8_t many, dst_slash, err;
    static struct esx_stat st;

    old_cpu_speed = ZXN_READ_REG(REG_TURBO_MODE);
    ZXN_NEXTREG(REG_TURBO_MODE, 3);
    atexit(xcopy_exit);

    // The CRT cuts the command line at 128 characters, with no message.
    // A line that may have been cut is refused.
    if (cmdline_len >= 127) exit((int)err_line_too_long);

    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-' && argv[i][1]) {
            for (p = argv[i] + 1; *p; ++p) {
                switch (*p | 0x20) {
                    case 'h': opt_hidden = 1; break;
                    case 'i': opt_assume_dir = 1; break;
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

    if (nbn_is_url(src) != opt_net) exit((int)(opt_net ? err_need_url : err_need_z));

    // Source: on a drive, or on an NBN server with -z

    many = opt_net ? nbn_open((char *)src) : local_open(src);
    many |= opt_recurse;

    // Destination: an existing directory or file, or a new one

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
        } else if (!dst_slash && !(opt_assume_dir && many)) {
            printf("Is %s a file or a directory (F/D)? ", dst_path);
            dst_is_file = (ui_ask("fd") == 'f');
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

    if (opt_net) nbn_walk(0);
    else walk(0);

    printf("%u file(s) copied\n", files_copied);
    if (files_skipped) printf("%u file(s) complete, skipped\n", files_skipped);
    if (files_fragmented) printf("%u file(s) need .DEFRAG\n", files_fragmented);
    if (errors_seen) printf("%u error(s)\n", errors_seen);

    return 0;
}
