//
// TELNET: what the shell gives the terminal and the telnet code.
// telnet.inc says the same for asm.
//
// The shell takes over the machine, sets up the screen, the interrupts and
// the UART, and runs the main loop. At exit it puts back everything it
// changed, also after an error, and returns to NextZXOS.
//
// Memory while the session runs:
//
//   $0000-$1FFF  ROM and esxDOS, as NextZXOS left them
//   $2000-$3FFF  the first 8K of the dot command: the CRT and C library
//   $4000-$4FFF  tile definitions, 512 tiles of 8 bytes        (bank 5)
//   $5000-$63FF  the tilemap, 32 rows of 80 cells               (bank 5)
//   $6400-$7FFF  free for the terminal                          (bank 5)
//   $8000-$BFFF  code, data, and the stack below $C000
//   $C000-$DFFF  MMU6: free for the terminal to page anything in
//   $E000-$FFFF  MMU7: the UART receive ring. Never page it out.
//
// The dotn loader gives the command all of $8000-$FFFF, in pages of its
// own; MMU6 and MMU7 hold logical pages 0 and 1 of the main bank. The
// shell copies all of bank 5 into two more pages at the start and copies
// it back at exit, so the terminal can write anywhere in $4000-$7FFF. It
// also puts back the page in MMU6 at exit.
//
// Main loop shape (telnet.c):
//
//   while (!tn_quit) {
//       while (tn_uart_avail()) <terminal takes tn_uart_get()>;
//       while (tn_keyq_avail()) tn_uart_put(tn_keyq_pop());
//   }
//

#ifndef D3_TELNET_H
#define D3_TELNET_H

#include <stdint.h>

// ---------------------------------------------------------------- screen

// The tilemap is 80x32 text mode with 512 tiles. A 60 Hz display shows
// only rows 1 to 30.
#define TN_COLS             80
#define TN_ROWS             32

// Tiles 0-255 are the CP437 font (0-31 are blank). Tiles 256-511 are the
// same glyphs inverted, for reverse video and the cursor.
#define TN_TILES            ((uint8_t *)0x4000)

// A cell is the character, then the attribute. The cell of row r, column c
// is TN_MAP[r * TN_COLS + c].
struct tn_cell {
    uint8_t ch;
    uint8_t attr;
};

#define TN_MAP              ((struct tn_cell *)0x5000)

// Attribute bits:
//   bit 7     bold: the ink is the bright colour
//   bits 6-4  paper, colour 0-7
//   bits 3-1  ink, colour 0-7
//   bit 0     reverse: the tile comes from 256-511, the inverted glyphs
// Colours are in ANSI order: 0 black, 1 red, 2 green, 3 yellow, 4 blue,
// 5 magenta, 6 cyan, 7 white. The palette (tilemap palette 0) has a pair
// of entries for each attribute: paper at index attr & $FE, ink at the
// next index.
#define TN_ATTR(ink, paper, bold)   (((bold) << 7) | ((paper) << 4) | ((ink) << 1))
#define TN_ATTR_REVERSE     0x01
#define TN_ATTR_DEFAULT     TN_ATTR(7, 0, 0)

// The shell sets NextReg $31 (tilemap Y offset) to 0 and puts it back at
// exit. The terminal can use it to scroll, with the map rows as a ring.

// ---------------------------------------------------------------- UART

// The UART in use (ESP, or the Pi with -pi) runs at 115,200 baud, 8N1.
// The interrupts move its received bytes into an 8K ring: the frame
// interrupt every frame, and the "Rx near full" interrupt when the UART's
// own 512-byte buffer is 3/4 full.

// Number of bytes in the ring, 0-8191
extern uint16_t tn_uart_avail(void);

// The next byte from the ring. Call it only when tn_uart_avail() is not 0.
extern uint8_t tn_uart_get(void);

// Send a byte. Waits while the UART's transmit buffer is full.
extern void tn_uart_put(uint8_t c) __z88dk_fastcall;

// Set when the ring was full and a received byte was lost
extern uint8_t tn_rx_overflow;

// ---------------------------------------------------------------- key queue

// Bytes waiting to go to the UART: keys from the keyboard hook, and the
// terminal's replies (DSR, DA). The main loop sends them. Push is safe
// from the interrupt and from the main program.

// Add c to the queue. Returns 1, or 0 when the queue (255 bytes) is full.
extern uint8_t tn_keyq_push(uint8_t c) __z88dk_fastcall;

// Number of bytes in the queue
extern uint8_t tn_keyq_avail(void);

// The next byte. Call it only when tn_keyq_avail() is not 0.
extern uint8_t tn_keyq_pop(void);

// ---------------------------------------------------------------- terminal

// term.asm: a VT100 with the ECMA-48 colours. It is 80 columns by 24
// rows, the size a telnet server assumes without NAWS, on display rows
// 4 to 27: the tilemap clip window hides the other map rows. The map rows
// are a ring, so terminal row r is not always map row r + 4: term.asm
// says how.
#define TN_TERM_ROWS        24

// Reset the terminal and clear the screen. Call it after the takeover.
extern void tn_term_init(void);

// The status bar: one row on display row 28, below the terminal. The
// shell writes its cells in tn_status, then calls tn_term_status to show
// them. The terminal keeps the bar on display row 28 through its scrolls
// and resets (term.asm).
extern struct tn_cell tn_status[TN_COLS];
extern void tn_term_status(void);

// Act on one byte from the remote end
extern void tn_term_put(uint8_t c) __z88dk_fastcall;

// The cursor blink, for tn_hook_cursor
extern void tn_term_blink(void);

// Modes the keyboard needs:
extern uint8_t tn_term_ckm;     // DECCKM: 1 = cursor keys send ESC O x, 0 = ESC [ x
extern uint8_t tn_term_lnm;     // LNM: 1 = Return sends CR LF, 0 = CR

// ---------------------------------------------------------------- frame hooks

// The frame interrupt calls these, in this order, after it has emptied the
// UART into the ring. They run with interrupts off, on the main stack.
// The interrupt saves every register and the NextReg select port
// ($243B), so a hook can be C and can read NextRegs. Keep them short, and
// do not call a library routine there that keeps state between calls.
//
// hooks.c holds them.
extern void tn_hook_cursor(void);           // cursor blink (the terminal)
extern void tn_hook_keys(void);             // frame count, keyboard scan (keys.c)

// ---------------------------------------------------------------- session

// Set to end the session: the main loop returns and the shell puts the
// machine back.
extern volatile uint8_t tn_quit;

// From the command line
extern uint8_t tn_pi;                       // 1 with -pi
extern const char *tn_host;                 // host, or 0 with -pi
extern uint16_t tn_port;                    // port, 23 if not given

#endif // D3_TELNET_H
