// I'm a banker, move my stack down...
#pragma output REGISTER_SP = 0xBFE0
#pragma printf = "%d %s %c %X %lu %f lx %x"

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
        "====",
        "\noptions:",
        " -h      Show this help screen",
        " -x      Start in HEX mode",
        "\n Usage examples",
        "\nShow a text file",
        " .LESS /path/to/file.txt",
        "Show a binary file",
        " .LESS -x /path/to/file.bin",
        "\nDetails at http://zxn.gg/less",
};


void exit_with_help(int exit_value) {
    d3_logo(); printf("LESS\n");
    for(uint8_t help=0;help<11;help++)
        printf("%s\n", help_text[help]);
    exit(exit_value);
}

uint8_t counter, fileArg;
char *filename;

int main(int argc, char **argv)
{
    startup();

    // One args, minimum
    if(argc < 2) exit_with_help((int)err_missing_filename);

    uint8_t start_in_hex = 0;

    counter = 0;
    // Let's check some options out
    while(counter<argc) {
        counter=counter+1;
        if (stricmp(argv[counter], "-h") == 0) {
            // Dump the help file
            exit_with_help(0);
        }
        else if (stricmp(argv[counter], "-x") == 0) {
            // Turn on hexmode
            start_in_hex = 1;
        }
        else if (argv[counter][0]=='-') {
            // Error
            exit_with_help((int)err_missing_filename);
        }
        else if (counter+1 == argc) {
            // This is the last option
            fileArg = counter;
            break;
        }
        else {
            // Error
            exit_with_help((int)err_missing_filename);
        }
    }

    unsigned char text_in = esxdos_f_open(argv[fileArg], ESXDOS_MODE_R | ESXDOS_MODE_OE);

    if(errno) {
        printf("Unable to open %s\n", argv[fileArg]);
        exit(errno);
    }

    less(argv[fileArg], text_in, start_in_hex);

    esxdos_f_close(text_in);

    exit(0);
}