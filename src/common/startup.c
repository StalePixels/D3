//
// Created by D Rimron-Soutter on 06/11/2020.
//

#include <z80.h>
#include <arch/zxn.h>
#include <intrinsic.h>
#include <arch/zxn/esxdos.h>
#include <errno.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "startup.h"

static unsigned char old_cpu_speed;

static void at_exit() {
    // Restore CPU Speed
    ZXN_NEXTREGA(REG_TURBO_MODE, old_cpu_speed);
    intrinsic_ei();
}

void startup() {
    // Store CPU speed
    old_cpu_speed = ZXN_READ_REG(REG_TURBO_MODE);
    // Set CPU speed to 28Mhz
    ZXN_NEXTREG(REG_TURBO_MODE, 3);
    // Restore to original atexit
    atexit(at_exit);
    errno = 0;
}