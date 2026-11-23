//
// XCOPY: copy files and directory trees
//

#ifndef D3_XCOPY_H
#define D3_XCOPY_H

#include <stdint.h>
#include <arch/zxn/esxdos.h>

// Longest path .XCOPY builds, without the terminator
#define XC_PATH_MAX         ESX_PATHNAME_MAX

// Each level of the tree adds at least two characters to a path
#define XC_DEPTH_MAX        (XC_PATH_MAX / 2)

// Sets the ROM's scroll count (SCR_CT, 23692) to 255, so that the ROM
// does not stop the output with "scroll?". Used once for each file.
#define XC_NO_SCROLL()      (*(volatile uint8_t *)23692 = 255)

// What dst_prepare found at dst_path
#define XC_DST_NEW          0
#define XC_DST_OLD          1
#define XC_DST_SKIP         2

// MS-DOS attribute bits, as F_STAT and F_READDIR return them
#define XC_A_RDONLY         0x01
#define XC_A_HIDSYS         0x06
#define XC_A_ARCH           0x20

// An error is 0 (none), an esxDOS error code (below 256), or the address
// of a custom message whose last character has bit 7 set
typedef uint16_t xc_err_t;

// Options
extern uint8_t opt_hidden;          // -h
extern uint8_t opt_continue;        // -c
extern uint8_t opt_keep_attr;       // -k
extern uint8_t opt_recurse;         // -e
extern uint8_t opt_overwrite_ro;    // -r
extern uint8_t opt_no_prompt;       // -y, or "All" at the overwrite prompt
extern uint8_t opt_net;             // -z
extern uint8_t net_put;             // -z with an nbn:// DST

// Source: directory being read (on the server with an nbn:// SRC), and the
// pattern or exact name to copy
extern char src_path[XC_PATH_MAX + 1];
extern const char *src_pattern;     // wildcard pattern given to DOS_CATALOG
extern const char *src_exact;       // exact name (long or 8.3), or 0
extern char src_name[XC_PATH_MAX + 1];

// Destination: directory being written, or the one file every copy goes to.
// With net_put, the path on the server.
extern char dst_path[XC_PATH_MAX + 1];
extern uint8_t dst_is_file;

// Canonical source directory, and the canonical directory of the
// destination (in file mode, the directory the file is in)
extern char canon_src[XC_PATH_MAX + 1];
extern char canon_dst[XC_PATH_MAX + 1];

// Handles open during a copy (0xff when closed), for the exit cleanup
extern unsigned char fin, fout;

extern uint16_t files_copied;
extern uint16_t errors_seen;
extern uint16_t files_fragmented;
extern uint16_t files_skipped;

// Custom error messages (xcopy.asm), in the bank
extern const char err_break[];
extern const char err_self_copy[];
extern const char err_dst_in_src[];
extern const char err_invalid_option[];
extern const char err_need_z[];
extern const char err_need_url[];
extern const char err_two_urls[];
extern const char err_no_memory[];
extern const char err_line_too_long[];

// "Mounted as drive X:" (xcopy.asm), in main memory: the drive letter is
// written in at run time
extern const char err_mounted[];
extern char err_mounted_drive[];

// Length of the command line, before the CRT cut it (cmdline.asm)
extern uint16_t cmdline_len;

// bank.asm. Each function below marked "in bank.asm" is an entry there,
// which pages the bank in at $C000 for the function of the same name with
// "_banked" added. Code in the bank calls them by these names too.
void xc_page_home(void);
void xc_page_bank(void);

// start.c, in the bank. Entry in bank.asm:
void xc_start(char **argv) __z88dk_fastcall;

// report.c, in the bank. Entries in bank.asm:
void ui_print_msg(xc_err_t err) __z88dk_fastcall;
char mounted_drive(const char *const *names) __z88dk_fastcall;
void copy_frag_check(void);
void xc_summary(void);

// path.c
uint16_t path_push(char *path, const char *name);
void path_pop(char *path, uint16_t len);
const char *path_basename(const char *path);

// ui.c
void ui_check_break(void);
char ui_ask(const char *keys);
void ui_error(const char *path, xc_err_t err);

// copy.c
extern unsigned char copy_buffer[2048];
void copy_file(const char *sfn, const char *lfn, uint8_t attr);
uint8_t copy_attr(const char *path, uint8_t attr);
uint8_t dst_prepare(const uint32_t *same_size);
xc_err_t copy_open_src(const char *sfn, const char *lfn);

// walk.c
extern const char pattern_all[];
void walk(uint8_t depth);
uint8_t walk_make_dir(void);

// nbnopen.c, in the bank. Only nbn_close has an entry in bank.asm; the
// rest are called from the bank.

// What nbn_exit has to undo
#define NET_PAGES           0x01
#define NET_LINK            0x02
#define NET_UART            0x04
extern uint8_t net_state;

uint8_t nbn_is_url(const char *s);
uint8_t nbn_open(char *url);
void nbn_put_open(char *url, uint8_t many);
void nbn_close(uint8_t state) __z88dk_fastcall;

// nbnwalk.c
xc_err_t nbn_cd(char *path);
void nbn_walk(uint8_t depth);
void nbn_exit(void);

// nbnput.c
xc_err_t put_mkdir(char *name);
xc_err_t nbn_put_dir(char *lfn);
void nbn_put_up(void);
void nbn_put(const char *sfn, char *lfn, uint8_t attr);

// nbnmatch.c
uint8_t nbn_match(const char *pat, const char *name);

#endif //D3_XCOPY_H
