//
// Created by D Rimron-Soutter on 3/11/2020.
//

#pragma output CLIB_MALLOC_HEAP_SIZE = 1024

#include <z80.h>
#include <arch/zxn.h>
#include <intrinsic.h>
#include <arch/zxn/esxdos.h>
#include <errno.h>

#include <stdio.h>
#include <stdlib.h>

#include "common/build.h"
#include "ini/ini_common.h"
#include "common/d3.h"

uint16_t counter = 0;
uint16_t old_counter = 0;
uint8_t verbose = 1;

const char *database;
const char *tmp_filename[] = "/TMP/INI$$$$$.TMP";

char *user_key;

uint8_t errno_filter_none[] = { 0 };

static unsigned char old_cpu_speed;
static void at_exit() {
    // Restore CPU Speed
    ZXN_NEXTREGA(REG_TURBO_MODE, old_cpu_speed);
}

void exit_with_help() {
    d3_logo(); printf(" INI\n");
    printf("\n\nexit_with_help\n\n");
    exit(errno);
}

int main(int argc, char** argv) {
    // Store CPU speed
    old_cpu_speed = ZXN_READ_REG(REG_TURBO_MODE);
    // Set CPU speed to 28Mhz
    ZXN_NEXTREG(REG_TURBO_MODE, 3);
    // Restore to original atexit
    atexit(at_exit);
    errno = 0;

    // Two args, minimum
    if(argc < 3) exit_with_help();

    // HANDLE SWITCHES HERE, maybe?

    // Tell the INI parter that "no errors are acceptable"
    errno_filter = (uint8_t*)&errno_filter_none;

    if(argc==4) {
        if (ini_set_one(argv[1], argv[2], argv[3])) {
            if (verbose) printf("UPDATED \"%s\" = \"%s\"\n", argv[2], argv[3]);
        } else {
            if (verbose) printf("CREATED \"%s\" = \"%s\"\n", argv[2], argv[3]);
        }
    }
    else if(argc==3){
        //  Use fast-access utility method to retrieve a single key, and close file again
        if(ini_get_one(argv[1], argv[2])) {
            if(verbose) {
                printf("%s = ", ini_key);
            }
            printf("%s\n", ini_value);
        }
        else {
            if (verbose) printf("KEY \"%s\" not found\n", argv[2]);
        }
    }
    exit(errno);
}