//
// TELNET: the parts of the shell, for its own files. telnet.h is what the
// shell gives the rest of the program.
//

#ifndef D3_TELNET_SHELL_H
#define D3_TELNET_SHELL_H

#include <stdint.h>

// takeover.c
void tn_takeover(void);
void tn_putback(void);
uint8_t tn_nr_read(uint8_t reg) __z88dk_fastcall;
void tn_nr_write(uint8_t reg, uint8_t val);
void tn_palette_edit(uint8_t palette) __z88dk_fastcall;

// Palettes, for tn_palette_edit (NextReg $43 bits 6-4)
#define TN_PALETTE_ULA          0x00
#define TN_PALETTE_TILEMAP      0x30

// screen.c. tn_status_show puts left at the start of the status bar and
// right at its end, and shows it.
void tn_screen_setup(void);
void tn_screen_palette(uint8_t reverse) __z88dk_fastcall;
void tn_status_show(const char *left, const char *right);

// uart.c: tn_uart_open returns the bit of NextReg $C6 for the UART's
// "Rx near full" interrupt
uint8_t tn_uart_open(void);
void tn_uart_close(void);

// irq.asm
void tn_irq_save(void);
void tn_irq_start(uint8_t uart_bits) __z88dk_fastcall;
void tn_irq_restore(void);
extern uint8_t tn_taken;

// rx.asm
void tn_rx_reset(void);

// hooks.c: frames since the start of the session
extern volatile uint16_t tn_frames;

// keys.c: tn_keys_scan runs each frame. tn_keys_down is non-zero while
// any key is down. EDIT sets tn_escape to 1; the next key sets it to 2,
// with that key's code in tn_escape_key ('q' or 'Q' for Q). The main loop
// sets it back to 0.
void tn_keys_scan(void);
uint8_t tn_keys_down(void);
extern volatile uint8_t tn_escape;
extern volatile uint8_t tn_escape_key;

// protocol.c: one byte from the remote end, through the telnet protocol
void tn_telnet_rx(uint8_t c) __z88dk_fastcall;

// esp.c: tn_esp_open returns 0, or the error for BASIC
const char *tn_esp_open(void);
void tn_esp_close(void);

// data.asm
extern const uint8_t tn_font_zx7[];
extern const char err_bad_port[];
extern const char err_bad_option[];

#endif // D3_TELNET_SHELL_H
