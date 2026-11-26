//
// TELNET: the keyboard. The frame interrupt calls tn_keys_scan (hooks.c),
// which reads the Next's 8x5 key matrix and puts the bytes for each new
// key in the key queue. A key held down repeats.
//
// The keys:
//
//   letters, digits, SPACE     as typed; CAPS SHIFT gives capitals
//   SYMBOL SHIFT + a key       the symbol printed on the key in red; the
//                              BASIC keywords (AND, STOP, ...) give nothing
//   EXTEND, then SYMBOL SHIFT  [ ] ~ | \ { } on Y U A S D F G, as in the
//     + a key                  Spectrum's E mode
//   EXTEND + a letter          the control code of the letter (B gives 2):
//                              hold EXTEND (CAPS SHIFT and SYMBOL SHIFT) and
//                              press the letter, or press EXTEND, let go,
//                              then press the letter
//   ENTER                      CR LF. With -pi CR, or CR LF when the
//                              terminal has set LNM.
//   DELETE   (CAPS SHIFT + 0)  BS
//   EDIT     (CAPS SHIFT + 1)  the menu: it is not sent, and the next
//                              key is a command for TELNET (tn_escape,
//                              below)
//   CAPS LOCK (CAPS SHIFT + 2) capitals on or off
//   cursor keys (CAPS SHIFT + 5 to 8)
//                              ESC [ A to D, or ESC O A to D when the
//                              terminal has set DECCKM
//   BREAK (CAPS SHIFT + SPACE) ESC
//
// After EDIT the next key does not go to the key queue. tn_escape is 1
// from EDIT until that key comes, then 2 with the key's code in
// tn_escape_key; the main loop reads it and sets tn_escape back to 0.
// That key does not repeat.
//
// The Next keyboard's extra keys (the cursor keys, DELETE, EDIT, EXTEND,
// ; " , . and the others) come into the matrix as the key combinations
// above. The shell clears NextReg $68 bit 4 for the session, so they do.
//

#include <z80.h>

#include "telnet.h"
#include "shell.h"

// Frames from a key going down to its first repeat, and between repeats
#define REPEAT_DELAY        25
#define REPEAT_RATE         3

// The matrix: 8 half rows of 5 keys. Key number = row * 5 + bit, with the
// rows in the order of their ports, $FEFE, $FDFE ... $7FFE.
#define KEYS                40
#define ROW_CAPS            0           // CAPS SHIFT: row 0, bit 0
#define ROW_SYMBOL          7           // SYMBOL SHIFT: row 7, bit 1

// Codes in the tables below that are not a byte to send
#define K_NONE              0
#define K_ENTER             0x80
#define K_UP                0x81
#define K_DOWN              0x82
#define K_RIGHT             0x83
#define K_LEFT              0x84
#define K_CAPS_LOCK         0x85
#define K_EDIT              0x86

#define ESC                 0x1B

// No shift. Key 0 is CAPS SHIFT, key 36 SYMBOL SHIFT.
static const uint8_t plain[KEYS] = {
    K_NONE,  'z', 'x', 'c', 'v',            // $FEFE
    'a',     's', 'd', 'f', 'g',            // $FDFE
    'q',     'w', 'e', 'r', 't',            // $FBFE
    '1',     '2', '3', '4', '5',            // $F7FE
    '0',     '9', '8', '7', '6',            // $EFFE
    'p',     'o', 'i', 'u', 'y',            // $DFFE
    K_ENTER, 'l', 'k', 'j', 'h',            // $BFFE
    ' ',  K_NONE, 'm', 'n', 'b'             // $7FFE
};

// CAPS SHIFT: capitals, and the functions of the digits
static const uint8_t caps[KEYS] = {
    K_NONE,  'Z', 'X', 'C', 'V',
    'A',     'S', 'D', 'F', 'G',
    'Q',     'W', 'E', 'R', 'T',
    K_EDIT,  K_CAPS_LOCK, K_NONE, K_NONE, K_LEFT,  // EDIT, CAPS LOCK, TRUE VIDEO, INV VIDEO
    0x08,    K_NONE, K_RIGHT, K_UP, K_DOWN,        // DELETE, GRAPH
    'P',     'O', 'I', 'U', 'Y',
    K_ENTER, 'L', 'K', 'J', 'H',
    ESC,     K_NONE, 'M', 'N', 'B'                  // BREAK
};

// SYMBOL SHIFT: the red symbols. X is the pound sign, code $60 in the
// Spectrum's character set, which is the backquote in ASCII.
static const uint8_t symbol[KEYS] = {
    K_NONE,  ':', '`', '?', '/',
    K_NONE,  K_NONE, K_NONE, K_NONE, K_NONE,
    K_NONE,  K_NONE, K_NONE, '<', '>',
    '!',     '@', '#', '$', '%',
    '_',     ')', '(', '\'', '&',
    '"',     ';', K_NONE, K_NONE, K_NONE,
    K_ENTER, '=', '+', '-', '^',
    ' ',  K_NONE, '.', ',', '*'
};

// EXTEND, then SYMBOL SHIFT
static const uint8_t extend_symbol[KEYS] = {
    K_NONE,  K_NONE, K_NONE, K_NONE, K_NONE,
    '~',     '|', '\\', '{', '}',
    K_NONE,  K_NONE, K_NONE, K_NONE, K_NONE,
    K_NONE,  K_NONE, K_NONE, K_NONE, K_NONE,
    K_NONE,  K_NONE, K_NONE, K_NONE, K_NONE,
    K_NONE,  K_NONE, K_NONE, ']', '[',
    K_NONE,  K_NONE, K_NONE, K_NONE, K_NONE,
    K_NONE,  K_NONE, K_NONE, K_NONE, K_NONE
};

volatile uint8_t tn_escape;
volatile uint8_t tn_escape_key;

static uint8_t started;                 // 0 until the first scan
static uint8_t last[8];                 // the matrix at the last scan, 1 = down
static uint8_t held = 0xFF;             // the key that repeats, or $FF
static uint8_t held_code;               // its code
static uint8_t repeat;                  // frames to its next repeat
static uint8_t caps_lock;
static uint8_t extend;                  // 1 after EXTEND by itself
static uint8_t extend_alone;            // EXTEND is down with no other key

// Put n bytes in the key queue, all of them or none
static void send(const uint8_t *b, uint8_t n)
{
    if (255 - tn_keyq_avail() < n) return;
    while (n--) tn_keyq_push(*b++);
}

// Send what code stands for
static void emit(uint8_t code)
{
    uint8_t b[3];

    switch (code) {
    case K_NONE:
        return;
    case K_ENTER:
        b[0] = '\r';
        b[1] = '\n';
        send(b, (tn_pi && !tn_term_lnm) ? 1 : 2);
        return;
    case K_UP: case K_DOWN: case K_RIGHT: case K_LEFT:
        b[0] = ESC;
        b[1] = tn_term_ckm ? 'O' : '[';
        b[2] = 'A' + (code - K_UP);
        send(b, 3);
        return;
    case K_CAPS_LOCK:
        caps_lock ^= 1;
        return;
    case K_EDIT:
        tn_escape = 1;
        return;
    }
    send(&code, 1);
}

// The code of key k with the shifts that are down now
static uint8_t decode(uint8_t k, uint8_t caps_down, uint8_t symbol_down)
{
    uint8_t c;

    if (caps_down && symbol_down) {             // EXTEND held: control code
        extend = 0;
        c = plain[k];
        return (c >= 'a' && c <= 'z') ? c & 0x1F : K_NONE;
    }
    if (extend) {
        extend = 0;
        if (symbol_down) return extend_symbol[k];
        c = plain[k];
        return (c >= 'a' && c <= 'z') ? c & 0x1F : K_NONE;
    }
    if (symbol_down) return symbol[k];
    if (caps_down) return caps[k];
    c = plain[k];
    if (caps_lock && c >= 'a' && c <= 'z') c -= 'a' - 'A';
    return c;
}

void tn_keys_scan(void)
{
    uint8_t now[8];
    uint8_t row, bit, k;
    uint8_t caps_down, symbol_down, others;
    uint8_t pressed = 0xFF;
    uint8_t half = 0xFE;                        // $FE, $FD, $FB ... $7F

    for (row = 0; row < 8; row++) {
        now[row] = ~z80_inp((uint16_t)half << 8 | 0xFE) & 0x1F;
        half = (half << 1) | 0x01;
    }

    // Keys down at the start (ENTER, from the command line) are not new
    if (!started) {
        for (row = 0; row < 8; row++) last[row] = now[row];
        started = 1;
        return;
    }

    caps_down = now[ROW_CAPS] & 0x01;
    symbol_down = now[ROW_SYMBOL] & 0x02;

    // The first key that went down since the last scan, shifts left out
    others = 0;
    for (row = 0, k = 0; row < 8; row++) {
        uint8_t down = now[row];
        if (row == ROW_CAPS) down &= ~0x01;
        if (row == ROW_SYMBOL) down &= ~0x02;
        others |= down;
        for (bit = 0x01; bit != 0x20; bit <<= 1, k++)
            if ((down & bit) && !(last[row] & bit) && pressed == 0xFF) pressed = k;
        last[row] = now[row];
    }

    // EXTEND pressed and let go with no other key starts E mode for one
    // key; pressed again, it ends it.
    if (others) {
        extend_alone = 0;
    } else if (caps_down && symbol_down) {
        extend_alone = 1;
    } else if (!caps_down && !symbol_down && extend_alone) {
        extend_alone = 0;
        extend ^= 1;
    }

    if (pressed != 0xFF) {
        held = pressed;
        held_code = decode(pressed, caps_down, symbol_down);
        repeat = REPEAT_DELAY;
        if (tn_escape == 1) {
            tn_escape_key = held_code;
            tn_escape = 2;
            held = 0xFF;                        // no repeat
            return;
        }
        if (tn_escape) return;                  // the main loop has not read it yet
        emit(held_code);
        return;
    }

    if (held == 0xFF) return;
    if (!(now[held / 5] & (1 << (held % 5)))) {
        held = 0xFF;                            // let go
        return;
    }
    if (--repeat) return;
    repeat = REPEAT_RATE;
    if (held_code != K_CAPS_LOCK && held_code != K_EDIT && !tn_escape) emit(held_code);
}

uint8_t tn_keys_down(void)
{
    return (~z80_inp(0x00FE) & 0x1F) != 0;      // all half rows at once
}
