//
// TELNET: the UART. The ESP's UART, or with -pi the Pi's UART on GPIO 14
// and 15, at 115,200 baud, 8 bits, no parity, one stop bit.
//
// Both UARTs use the same ports; port $153B picks one. The lower 14 bits
// of a UART's prescaler cannot be read back, so at exit the UART used
// stays at 115,200 baud. Everything else goes back as it was.
//

#include <arch/zxn.h>

#include "telnet.h"
#include "shell.h"

// bit 6: 1 for the Pi's UART. bit 4: 1 to write bits 2-0, the top 3 bits
// of the 17-bit prescaler.
__sfr __banked __at 0x153b IO_UART_SELECT;

// The frame format of the selected UART
__sfr __banked __at 0x163b IO_UART_FRAME;

#define SELECT_PI           0x40
#define SELECT_SET_TOP      0x10
#define FRAME_8N1           0x18

#define BAUD                115200UL

// The system clock for each video timing (NextReg $11)
static const uint32_t clocks[8] = {
    CLK_28_0, CLK_28_1, CLK_28_2, CLK_28_3, CLK_28_4, CLK_28_5, CLK_28_6, CLK_28_7
};

static uint8_t select;              // the UART used
static uint8_t kept_select;         // port $153B as it was
static uint8_t kept_top;            // top 3 bits of the prescaler of the UART used
static uint8_t kept_frame;          // its frame format

uint8_t tn_uart_open(void)
{
    uint32_t prescaler;

    kept_select = IO_UART_SELECT;
    select = 0;
    if (tn_pi) {
        // NextReg $A0 bit 5: the UART on GPIO 14 and 15, bit 4: Rx on 14
        // and Tx on 15, the way round to talk to the Pi. takeover.c keeps
        // $A0 and puts it back.
        tn_nr_write(0xA0, tn_nr_read(0xA0) | 0x30);
        select = SELECT_PI;
    }

    IO_UART_SELECT = select;
    kept_top = IO_UART_SELECT & 0x07;
    kept_frame = IO_UART_FRAME;
    IO_UART_FRAME = FRAME_8N1;

    prescaler = clocks[tn_nr_read(0x11) & 7] / BAUD;
    IO_UART_SELECT = select | SELECT_SET_TOP | ((prescaler >> 14) & 0x07);
    IO_UART_BAUD_RATE = prescaler & 0x7F;
    IO_UART_BAUD_RATE = ((prescaler >> 7) & 0x7F) | 0x80;

    // NextReg $C6: Rx near full of UART 1 (Pi) or UART 0 (ESP)
    return tn_pi ? 0x20 : 0x02;
}

void tn_uart_close(void)
{
    IO_UART_FRAME = kept_frame;
    IO_UART_SELECT = select | SELECT_SET_TOP | kept_top;
    IO_UART_SELECT = (kept_select & 0x47) | SELECT_SET_TOP;
}
