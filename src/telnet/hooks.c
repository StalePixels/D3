//
// TELNET: the frame hooks. The frame interrupt calls them (irq.asm), with
// interrupts off; telnet.h says what they may do.
//

#include "telnet.h"
#include "shell.h"

// Cursor blink (term.asm)
void tn_hook_cursor(void)
{
    tn_term_blink();
}

// Frames since the start of the session, for time limits
volatile uint16_t tn_frames;

// Keyboard scan (keys.c)
void tn_hook_keys(void)
{
    ++tn_frames;
    tn_keys_scan();
}
