//
// XCOPY: keys, prompts and error reports
//

#include <arch/zxn/esxdos.h>
#include <ctype.h>
#include <input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xcopy.h"

static char msg[33];

// BREAK (CAPS SHIFT + SPACE) stops the command; the exit cleanup
// removes a file that is part written
void ui_check_break(void)
{
    if (in_key_pressed(IN_KEY_SCANCODE_SPACE | 0x8000)) {
        in_wait_nokey();
        exit((int)err_break);
    }
}

// Wait for one of the lower case keys in keys, echo it, return it
char ui_ask(const char *keys)
{
    char c;

    while (in_inkey()) ui_check_break();

    do {
        ui_check_break();
        c = tolower(in_inkey());
    } while (!c || !strchr(keys, c));

    printf("%c\n", c);
    return c;
}

void ui_print_msg(xc_err_t err)
{
    const char *p;

    if (err < 256) {
        esx_m_geterr(err, msg);
        printf("%s", msg);
    } else {
        for (p = (const char *)err; !(*p & 0x80); ++p) putchar(*p);
        putchar(*p & 0x7f);
    }
}

// Report an error for path. Without -c the command stops with err.
void ui_error(const char *path, xc_err_t err)
{
    ++errors_seen;
    printf("%s\n", path);
    if (!opt_continue) exit((int)err);
    printf("  ");
    ui_print_msg(err);
    printf("\n");
}
