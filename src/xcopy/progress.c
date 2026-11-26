//
// XCOPY: the progress of one copy, on the line with the name of the file:
// the name, then the bytes done and the total, as " 120K/1.4M".
//
// The ROM output drops \r, so each new count goes back over the one
// before with code 8, once for each column. Code 8 at the left edge goes
// up to the end of the line above, so a name that wraps is never cut.
//

#include <stdio.h>

#include "xcopy.h"

// In the 8K dot part: resident, so the copy loops can call it with
// arguments on the stack, and the main bank keeps its free space for the
// stack
#pragma codeseg code_dot
#pragma constseg code_dot

// Is the line with the name open, how many columns of count are on it,
// and the bytes of the copy done and in all
static uint8_t line_open;
static uint8_t shown;
static uint32_t done;
static uint32_t total;

// Print bytes as K, or as M with one decimal place.
// Returns the number of characters printed.
static uint8_t size_print(uint32_t bytes)
{
    uint32_t k = bytes >> 10;
    uint16_t tenths;

    if (k < 1024) return printf("%uK", (uint16_t)k);
    tenths = (uint16_t)((k * 10) >> 10);
    return printf("%u.%uM", tenths / 10, tenths % 10);
}

// Print dst_path, the file being written, with no newline
void progress_path(void)
{
    printf("%s", dst_path);
    line_open = 1;
    shown = 0;
}

// Show done and total after the name. Spaces cover the end of a longer
// count. If something has ended the line, the name starts a new one.
static void show(void)
{
    uint8_t n;

    if (!line_open) progress_path();
    for (n = shown; n; --n) putchar(8);

    putchar(' ');
    n = 2 + size_print(done);
    putchar('/');
    n += size_print(total);

    for (; n < shown; ++n) putchar(' ');
    shown = n;
}

// Start the count of a copy of size bytes
void progress_start(uint32_t size)
{
    done = 0;
    total = size;
    show();
}

// Add n bytes to the count
void progress_add(uint16_t n) __z88dk_fastcall
{
    done += n;
    show();
}

// End the line with the name, if it is open
void progress_end(void)
{
    if (line_open) putchar('\n');
    line_open = 0;
    shown = 0;
}
