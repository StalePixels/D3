//
// Created by D Rimron-Soutter on 18/11/2020.
//

#pragma define CLIB_EXIT_STACK_SIZE = 1

#include <z80.h>
#include <arch/zxn.h>
#include <intrinsic.h>
#include <arch/zxn/esxdos.h>
#include <errno.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <input.h>

#include "common/build.h"
#include "common/d3.h"
#include "common/startup.h"
#include "common/ula.h"

uint8_t verbose = 0;

const char *help_text[] = {
        "\nINKEY echoer",
        "============",
        "\nDetails at http://zxn.gg/inkey"

};

void help() {
    d3_logo(); printf("INKEY");
    for(uint8_t help=0;help<3;help++)
        printf("%s\n", help_text[help]);
}

int main(int argc, char** argv) {
    startup();

    help();

    uint8_t inkey = 0;

    while(inkey!=3) {
        inkey = in_inkey();
        printAt(0, 9);
        printf("INKEY read: %d  \n", inkey);
        printf("            %c (0x%02X)           \n         ", inkey, inkey);

    }

    exit(0);
}