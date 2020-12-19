// I'm a banker, move my stack down...
#pragma output REGISTER_SP = 0xBFE0

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
#include "less/less.h"


const char *help_text[] = {
        "\nLESS",
        "=====",
        "\n Usage examples",
        "\nShow a test file",
        ".LESS /path/to/file.txt",
        "\nDetails at http://zxn.gg/less",
};

void exit_with_help() {
    d3_logo(); printf("LESS\n");
    for(uint8_t help=0;help<6;help++)
        printf("%s\n", help_text[help]);
    exit(errno);
}

int main(int argc, char **argv)
{
    startup();

    // One arg, minimum
    if(argc < 2) exit_with_help();

    unsigned char text_in = esxdos_f_open(argv[1], ESXDOS_MODE_R | ESXDOS_MODE_OE);

    if(errno) {
        printf("Unable to open %s\n", argv[1]);
        exit(errno);
    }

    less(text_in);

    esxdos_f_close(text_in);

    exit(0);
}