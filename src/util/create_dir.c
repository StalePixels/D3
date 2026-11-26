//
// Created by D Rimron-Soutter on 06/11/2020.
//

#include "create_dir.h"
#include <z80.h>
#include <arch/zxn.h>
#include <intrinsic.h>
#include <arch/zxn/esxdos.h>
#include <errno.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void create_dir(char *pathname) {
    char *end, *p;
    char saved;
    int r;

    /* Leave out trailing '/' and "." entries: "./" is the current
       directory, and "a/./" is "a". */
    end = pathname + strlen(pathname);
    while (end > pathname && (end[-1] == '/' ||
           (end[-1] == '.' && (end - 1 == pathname || end[-2] == '/'))))
        --end;
    if (end == pathname)
        return;         // the current directory: nothing to make
    saved = *end;
    *end = '\0';

    /* Try creating the directory. */
    r = esx_f_mkdir(pathname);

    // An existing directory is not an error
    if(errno==18) {
        errno = 0;
        *end = saved;
        return;
    }
    if (r != 0) {
        /* On failure, try creating parent directory. */
        p = strrchr(pathname, '/');
        if (p != NULL) {
            *p = '\0';
            create_dir(pathname);
            *p = '/';
            r = esx_f_mkdir(pathname);
        }
    }
    if (r != 0)
        printf("Could not create directory %s\n", pathname);
    else
        errno = 0;      // a failed first try left its error code
    *end = saved;
}