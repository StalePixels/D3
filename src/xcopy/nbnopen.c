//
// XCOPY: open and close the link to an NBN server (-z). In the bank (see
// bank.asm). NBN_Malloc is an M_P3DOS call: it runs through
// NBN_Malloc_home (bank.asm).
//

#include <arch/zxn.h>
#include <arch/zxn/esxdos.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xcopy.h"
#include "nbn/messages.h"
#include "nbn/uart.h"
#include "nbn/net.h"
#include "nbn/nbn.h"

#pragma codeseg PAGE_12
#pragma constseg PAGE_12

uint8_t net_state;

// Port 0x153B as it was: the UART selected, and the top bits of its
// prescaler. The lower 14 bits of the prescaler cannot be read back.
static uint8_t old_uart_select;

static char default_port[] = "48128";
static const unsigned long uart_clock[] = { CLK_28_0, CLK_28_1, CLK_28_2, CLK_28_3, CLK_28_4, CLK_28_5, CLK_28_6, CLK_28_7 };

extern bool NBN_Malloc_home(void);

uint8_t nbn_is_url(const char *s)
{
    return strnicmp(s, "nbn://", 6) == 0;
}

// The link and UART part of nbn_exit, for the flags in state
void nbn_close_banked(uint8_t state) __z88dk_fastcall
{
    if (state & NET_LINK) NET_Close(true);
    if (state & NET_UART) IO_UART_CONTROL = (old_uart_select & 0x47) | 0x10;
}

// Connect to host:port and open the NBN session
static void connect(char *host, char *port)
{
    unsigned int prescalar;

    if (!NBN_Malloc_home()) {
        net_state = NET_PAGES;
        exit((int)err_no_memory);
    }
    net_state = NET_PAGES;

    // The ESP UART, at 115,200 baud at the clock of this video timing.
    // Bit 4 writes the top 3 bits of the prescaler, all 0 for this rate.
    old_uart_select = IO_UART_CONTROL;
    net_state |= NET_UART;
    IO_UART_CONTROL = 0x10;
    prescalar = uart_clock[ZXN_READ_REG(REG_VIDEO_TIMING) & 7] / 115200UL;
    IO_UART_BAUD_RATE = prescalar & 0x7f;
    IO_UART_BAUD_RATE = ((prescalar >> 7) & 0x7f) | 0x80;

    // an ESP still in passthrough answers with an error, so close it
    if (NET_GetOK(false)) NET_Close(true);

    printf("Connecting to %s\n", host);
    net_state |= NET_LINK;
    NET_Connect(host, port);
    if (UART_WaitOK(false)) exit((int)err_failed_connection);

    NET_ModeSingle();
    NET_OpenSocket();

    // the ">" prompt of CIPSEND
    for (prescalar = 255; NET_GetUChar() != '>'; )
        if (!--prescalar) exit((int)err_failed_connection);
}

// Take the URL nbn://host[:port]/path, put the path in path ("/" if
// there is none, and no trailing "/"), and connect
static void nbn_connect_url(char *url, char *path)
{
    char *host = url + 6;
    char *port = default_port;
    char *p;
    uint32_t n = 0;

    for (p = host; *p && *p != ':' && *p != '/'; ++p) ;
    if (p == host) exit((int)err_bad_server);

    if (*p == ':') {
        *p++ = 0;
        port = p;
        for (; isdigit(*p) && n <= 65535; ++p) n = n * 10 + (*p - '0');
        if (p == port || !n || n > 65535 || (*p && *p != '/')) exit((int)err_bad_port);
    }

    if (strlen(p) > XC_PATH_MAX) exit(ESX_ENAMETOOLONG);
    strcpy(path, *p ? p : "/");
    *p = 0;                                 // ends the host or the port

    p = path + strlen(path) - 1;
    if (p != path && *p == '/') *p = 0;

    connect(host, port);
}

// Take the source URL nbn://host[:port]/path, connect, and find what to
// copy: a directory, a pattern in a directory, or one name in it.
// Leaves the server in the directory, and its path in src_path.
// Returns 1 if the source can be more than one file.
uint8_t nbn_open(char *url)
{
    char *base;
    xc_err_t err;

    nbn_connect_url(url, src_path);

    base = (char *)path_basename(src_path);

    if (!strpbrk(base, "*?") && !nbn_cd(src_path)) {
        src_pattern = pattern_all;
        return 1;
    }

    strcpy(src_name, base);
    *base = 0;
    if (base - 1 != src_path) base[-1] = 0;     // "/dir/name" to "/dir", "/name" to "/"

    if ((err = nbn_cd(src_path))) {
        printf("%s\n", src_path);
        exit((int)err);
    }

    if (strpbrk(src_name, "*?")) {
        src_pattern = src_name;
        return 1;
    }
    src_exact = src_name;
    return 0;
}

// Take the destination URL, connect, and change the server's directory
// to the one the files go to. As for a DST on a drive: an existing
// directory takes the files, and a path with no trailing "/" names the
// file when the source is one file. Any other path is made.
void nbn_put_open(char *url, uint8_t many)
{
    uint8_t slash = url[strlen(url) - 1] == '/';
    char *base;
    xc_err_t err;

    nbn_connect_url(url, dst_path);
    if (!nbn_cd(dst_path)) return;

    if (!slash && !many) {
        // one file to a new name, in the directory above
        dst_is_file = 1;
        strcpy(canon_dst, dst_path);
        base = (char *)path_basename(canon_dst);
        if (base - 1 != canon_dst) base[-1] = 0;   // "/dir/name" to "/dir"
        else *base = 0;                             // "/name" to "/"
        err = nbn_cd(canon_dst);
    } else if (!(err = put_mkdir(dst_path))) {
        err = nbn_cd(dst_path);
    }

    if (err) {
        printf("%s\n", dst_path);
        exit((int)err);
    }
}
