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

#include "create_file.h"
#include "create_dir.h"

// Returns the file handle, or 0xff with errno set when the file cannot be
// created. esxdos_f_open reports an error that way.
unsigned char create_file(char *pathname) {
    unsigned char file_out;
    errno = 0;
    file_out = esxdos_f_open(pathname, ESXDOS_MODE_W | ESXDOS_MODE_CT);
    if (errno) {
        /* Try creating parent dir and then creating file. */
        char *p = strrchr(pathname, '/');
        if (p != NULL) {
            *p = '\0';
            create_dir(pathname);
            *p = '/';
            errno = 0;
            file_out = esxdos_f_open(pathname, ESXDOS_MODE_W | ESXDOS_MODE_CT);
        }
    }
    return (file_out);
}
