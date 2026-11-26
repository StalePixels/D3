//
// TELNET: the telnet protocol (RFC 854) on the bytes from the remote end.
//
// A telnet command starts with IAC, byte 255. This client takes up no
// options: it answers DO with WONT and WILL with DONT, and does not
// answer WONT and DONT. It skips subnegotiations (SB ... IAC SE). IAC IAC
// is the data byte 255. Commands do not reach the screen.
//
// The answers go in the key queue: the ESP may still be in AT command
// mode when the first commands come, and the main loop sends the queue
// only once the link is open.
//

#include "telnet.h"
#include "shell.h"

#define IAC                 255
#define DONT                254
#define DO                  253
#define WONT                252
#define WILL                251
#define SB                  250
#define SE                  240

enum {
    S_DATA,                 // data bytes
    S_IAC,                  // after IAC
    S_WILL,                 // after IAC WILL: the option comes next
    S_DO,                   // after IAC DO
    S_SKIP,                 // after IAC WONT or IAC DONT
    S_SB,                   // in a subnegotiation
    S_SB_IAC                // IAC in a subnegotiation
};

static uint8_t state = S_DATA;

// IAC, then cmd, then the option. All three or none.
static void answer(uint8_t cmd, uint8_t option)
{
    if (255 - tn_keyq_avail() < 3) return;
    tn_keyq_push(IAC);
    tn_keyq_push(cmd);
    tn_keyq_push(option);
}

void tn_telnet_rx(uint8_t c) __z88dk_fastcall
{
    switch (state) {
    case S_DATA:
        if (c == IAC) state = S_IAC;
        else tn_term_put(c);
        return;

    case S_IAC:
        switch (c) {
        case IAC:   tn_term_put(IAC); state = S_DATA; return;
        case WILL:  state = S_WILL; return;
        case DO:    state = S_DO; return;
        case WONT:
        case DONT:  state = S_SKIP; return;
        case SB:    state = S_SB; return;
        }
        state = S_DATA;     // a command with no option (NOP, GA ...)
        return;

    case S_WILL:
        answer(DONT, c);
        break;

    case S_DO:
        answer(WONT, c);
        break;

    case S_SB:
        if (c == IAC) state = S_SB_IAC;
        return;

    case S_SB_IAC:
        state = (c == SE) ? S_DATA : S_SB;
        return;
    }
    state = S_DATA;         // S_WILL, S_DO and S_SKIP end at the option
}
