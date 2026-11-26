//
// TELNET: the telnet protocol (RFC 854) on the bytes from the remote end.
//
// A telnet command starts with IAC, byte 255. This client takes up no
// options itself: it answers every DO with WONT. It lets the remote end
// take up two options, ECHO and SUPPRESS-GO-AHEAD, and answers WILL for
// any other option with DONT. With those two on, the server echoes each
// key as it comes. The client has no local echo: refused ECHO, a server
// such as the NetBSD telnetd turns its echo off, and typed text never
// shows.
//
// RFC 854 says a request for the state an option is already in gets no
// answer, so two ends cannot answer each other for ever. The client keeps
// the state of the two options. It answers a WILL only when it turns
// that option on, and a WONT only when it turns that option off. A WONT
// for any other option, and a DONT, need no answer: those options are
// always off.
//
// It skips subnegotiations (SB ... IAC SE). IAC IAC is the data byte 255.
// Commands do not reach the screen.
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

// The options the remote end may take up (RFC 857, RFC 858)
#define OPT_ECHO            1
#define OPT_SGA             3

enum {
    S_DATA,                 // data bytes
    S_IAC,                  // after IAC
    S_WILL,                 // after IAC WILL: the option comes next
    S_DO,                   // after IAC DO
    S_WONT,                 // after IAC WONT
    S_SKIP,                 // after IAC DONT
    S_SB,                   // in a subnegotiation
    S_SB_IAC                // IAC in a subnegotiation
};

static uint8_t state = S_DATA;
static uint8_t remote_on;   // the options the remote end has on: their bits

// The bit for an option in remote_on, or 0 for an option it may not take up
static uint8_t remote_bit(uint8_t option)
{
    if (option == OPT_ECHO) return 1;
    if (option == OPT_SGA) return 2;
    return 0;
}

// IAC, then cmd, then the option. All three or none: 1 if they went in
// the queue.
static uint8_t answer(uint8_t cmd, uint8_t option)
{
    if (255 - tn_keyq_avail() < 3) return 0;
    tn_keyq_push(IAC);
    tn_keyq_push(cmd);
    tn_keyq_push(option);
    return 1;
}

void tn_telnet_rx(uint8_t c) __z88dk_fastcall
{
    uint8_t bit;

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
        case WONT:  state = S_WONT; return;
        case DONT:  state = S_SKIP; return;
        case SB:    state = S_SB; return;
        }
        state = S_DATA;     // a command with no option (NOP, GA ...)
        return;

    case S_WILL:
        bit = remote_bit(c);
        if (!bit) answer(DONT, c);
        else if (!(remote_on & bit) && answer(DO, c)) remote_on |= bit;
        break;

    case S_WONT:
        bit = remote_bit(c);
        if ((remote_on & bit) && answer(DONT, c)) remote_on &= ~bit;
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
    state = S_DATA;         // S_WILL, S_DO, S_WONT and S_SKIP end at the option
}
