//
// Created by D Rimron-Soutter on 16/10/2020.
//

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
#include "ini/ini_common.h"

unsigned char file_in, file_out;
uint16_t counter = 0;

const char database[] = "/SYS/EVERY.CFG";
char *tmp_filename[] = "/SYS/EVERY$$$.CFG";
char default_key[] = "default";

char *user_key;
int stored_value;
char user_buffer[8];

uint8_t errno_file_not_found[] = { 5, 0 };

const char *help_text[] = {
        "\nSimple Scheduler",
        "================",
        "\n Usage examples",
        "\nName: default, \"True\" every 10th",
        " .EVERY 10",
        "\nName: ESP, \"True\" every 5th",
        " .EVERY ESP 5",
        "\nNextReg 0x7F is 1 on True else 0",
        "\nDetails at http://zxn.gg/every"
};

void exit_with_help() {
    d3_logo(); printf(" EVERY\n");
    for(uint8_t help=0;help<9;help++)
        printf("%s\n", help_text[help]);
    exit(errno);
}

int main(int argc, char** argv) {
    startup();

    // Two args, minimum
    if(argc < 2) exit_with_help();

    // Turn the first arg into an int.
    counter = atoi(argv[1]);

    // And check it's not too small
    if(counter<2) exit_with_help();

    // If the user passed a key, use it instead of the default
    if(argc>2) {
        user_key = argv[2];
    }
    else {
        user_key = default_key;
    }

    // setup the INI reader to permit there to be no source file
    errno_filter = (uint8_t*)&errno_file_not_found;

    //  Use fast-access utility method to retrieve a single key, and close file again
    ini_get_one(database, user_key);

    stored_value = atoi(ini_value);
    // increment it - as this is (at least) the first run...
    ++stored_value;     // (users don't count from zero)

    IO_NEXTREG_REG = 0x7F;
    if(counter<=stored_value) {
        stored_value = 0;
        IO_NEXTREG_DAT = 1;
    }
    else {
        IO_NEXTREG_DAT = 0;
    }


    sprintf(user_buffer, "%d", stored_value);
    ini_set_one(database, user_key, user_buffer);

    exit(errno);
}