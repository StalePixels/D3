#include <z80.h>
#include <arch/zxn.h>
#include <intrinsic.h>
#include <arch/zxn/esxdos.h>
#include <errno.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/build.h"
#include "common/d3.h"
#include "common/startup.h"

#include "util/parse_octal.h"

#include "untar/untar.h"

const char *help_text[] = {
        "\nUNTAR",
        "=====",
        "\n Usage examples",
        "\nExtract a file",
        ".UNTAR /path/to/file.tar",
        "\nSee http://zxn.gg/untar for more",

};

void exit_with_help() {
    d3_logo(); printf(" UNTAR\n");
    for(uint8_t help=0;help<6;help++)
        printf("%s\n", help_text[help]);
    exit(errno);
}

int main(int argc, char **argv)
{
    startup();

    // One arg, minimum
    if(argc < 2) exit_with_help();

    unsigned char tar_in = esxdos_f_open(argv[1], ESXDOS_MODE_R | ESXDOS_MODE_OE);

    if(errno) {
        printf("Unable to open %s\n", argv[1]);
        exit(errno);
    }
    else {
        untar(tar_in, argv[1]);
        esxdos_f_close(tar_in);
    }

    exit(errno);
}