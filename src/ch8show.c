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
#include "ch8/ch8show.h"

#include "util/parse_octal.h"

const char *help_text[] = {
        "\nCH8SHOW",
        "=======",
        "\n Usage examples",
        "\nShow a font file",
        ".CH8SHOW /path/to/file.ch8",
        "\nDetails at http://zxn.gg/ch8show",

};

void exit_with_help() {
    d3_logo(); printf("CH8SHOW\n");
    for(uint8_t help=0;help<6;help++)
        printf("%s\n", help_text[help]);
    exit(errno);
}

int main(int argc, char **argv)
{
    startup();

    // One arg, minimum
    if(argc < 2) exit_with_help();

    unsigned char font_in = esxdos_f_open(argv[1], ESXDOS_MODE_R | ESXDOS_MODE_OE);

    if(errno) {
        printf("Unable to open %s\n", argv[1]);
        exit(errno);
    }

    struct esxdos_stat finfo;  // = {0,0,0,0,0};
    esxdos_f_fstat(font_in, &finfo);

    d3_logo(); printf("CH8SHOW\n");
    printf("Showing Font: %18s\n\n", argv[1]);

    if(finfo.size==768) {
        ch8show(font_in, 96, 6);
    }
    else if(finfo.size==1024) {
        ch8show(font_in, 128, 6);
    }

    esxdos_f_close(font_in);


    exit(0);
}