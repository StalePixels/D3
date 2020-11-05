//
// Created by D Rimron-Soutter on 04/11/2020.
//

#include "d3.h"
#include "build.h"
#include "ula.h"

#include <z80.h>
#include <arch/zxn.h>
#include <intrinsic.h>
#include <arch/zxn/esxdos.h>
#include <errno.h>

#include <stdio.h>
#include <stdlib.h>

void d3_logo() {
    zx_cls(PAPER_WHITE); printAt(0,0);
    printf(" _    _          ###, ###\n"
           "[_)s [_)eveloper #  #  _#\n"
           "[_)otcommands    #  #   #\n"
           "          v" D3_VER " ###' ###");
}
