//
// Created by D Rimron-Soutter on 3/11/2020.
//

#include "ini_common.h"
#include "../util/trim_whitespace.h"
#include <stdio.h>

#include <ctype.h>
#include <string.h>

bool ini_get_one(const char *filename, const char *key) {
    ini_value = 0;
    ini_in = esxdos_f_open(filename, ESXDOS_MODE_R | ESXDOS_MODE_OE);
    if(errno) exit(errno);

    while(ini_get_line(false)) {
        ini_key = trim_whitespace(strtok(ini_line, "="));
        if(!strcmp(ini_key, key)) {
            ini_value = trim_whitespace(strtok(NULL, "\n"));
            goto done;
        }
    }

    done:
    esxdos_f_close(ini_in);

    return (ini_value ? true : false);
}