//
// GUNZIP: decompress a gzip file
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

#include "gunzip/gunzip.h"

unsigned char fin = 0xff;
unsigned char fout = 0xff;
static char out_name[ESX_PATHNAME_MAX + 1];

static unsigned char old_cpu_speed;

const char *help_text[] = {
        "\nGUNZIP",
          "======",
        "\n Usage",
        ".GUNZIP FILE.gz",
        "\nWrites FILE and keeps FILE.gz.",
        "FILE.tgz gives FILE.tar.",
        "Quote a name with a drive:",
        ".GUNZIP \"C:/FILE.gz\""
};

static void exit_with_help(void)
{
    d3_logo(); printf(" GUNZIP\n");
    for (uint8_t help = 0; help < sizeof(help_text) / sizeof(help_text[0]); help++)
        printf("%s\n", help_text[help]);
    exit(0);
}

// Runs at every exit: an output file that is not complete is deleted
static void gunzip_exit(void)
{
    gzip_exit();

    if (fout != 0xff) {
        esx_f_close(fout);
        esx_f_unlink(out_name);
    }
    if (fin != 0xff) esx_f_close(fin);

    ZXN_NEXTREGA(REG_TURBO_MODE, old_cpu_speed);
}

// The output name: FILE.gz gives FILE, FILE.tgz gives FILE.tar
static void make_out_name(const char *in)
{
    uint16_t len = strlen(in);
    char c;

    if (len > ESX_PATHNAME_MAX) exit(ESX_ENAMETOOLONG);
    strcpy(out_name, in);

    if (len > 4 && !stricmp(in + len - 4, ".tgz")) {
        // keep the case of the letters it replaces
        out_name[len - 2] = 'A' | (in[len - 2] & 0x20);
        out_name[len - 1] = 'R' | (in[len - 1] & 0x20);
    } else if (len > 3 && !stricmp(in + len - 3, ".gz")) {
        out_name[len - 3] = 0;
    } else {
        exit((int)err_bad_name);
    }

    // nothing left of the name before the extension
    c = out_name[strlen(out_name) - 1];
    if (c == '/' || c == ':' || c == '.') exit((int)err_bad_name);
}

int main(int argc, char **argv)
{
    old_cpu_speed = ZXN_READ_REG(REG_TURBO_MODE);
    ZXN_NEXTREG(REG_TURBO_MODE, 3);
    atexit(gunzip_exit);

    if (argc != 2) exit_with_help();

    make_out_name(argv[1]);

    errno = 0;
    fin = esx_f_open(argv[1], ESX_MODE_R | ESX_MODE_OPEN_EXIST);
    if (errno) {
        fin = 0xff;
        exit(errno);
    }

    // An existing output file is not overwritten
    errno = 0;
    fout = esx_f_open(out_name, ESX_MODE_W | ESX_MODE_OPEN_CREAT_NOEXIST);
    if (errno) {
        fout = 0xff;
        exit(errno);
    }

    // Prevent scroll messages
    z80_bpoke(23692, 255);
    printf("%s\n", out_name);

    gzip_decompress();

    errno = 0;
    esx_f_close(fout);
    if (errno) exit(errno);
    fout = 0xff;

    printf("%lu bytes\n", out_total);

    return 0;
}
