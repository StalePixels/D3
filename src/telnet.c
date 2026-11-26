//
// TELNET: a telnet client and ANSI terminal, through the ESP WiFi module,
// or a terminal on the Pi's UART with -pi.
// The shell: the command line, the takeover, the main loop and the exit.
// telnet/telnet.h has the memory map and what the shell gives the rest.
//

#include <arch/zxn.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/d3.h"
#include "telnet/telnet.h"
#include "telnet/shell.h"

// The stack is below $C000: the shell pages MMU6, and the receive ring is
// in MMU7.
#pragma output REGISTER_SP = 0xC000

// The loader maps logical pages 10 and 11 (bank 5) in MMU2 and MMU3, so
// its page table must reach page 11.
#pragma output DOTN_LAST_PAGE = 11

// Two pages the loader allocates and frees, for the copy of bank 5
// (telnet/takeover.c)
#pragma output DOTN_NUM_EXTRA = 2

uint8_t tn_pi;
const char *tn_host;
uint16_t tn_port = 23;
volatile uint8_t tn_quit;

// The status bar (telnet.h): the connection and the menu key, or the
// menu after EDIT
static const char menu_text[] = " Q: leave    any other key: close the menu";
static char where[TN_COLS + 1];
static uint8_t menu_up;

static void status_idle(void)
{
    if (tn_pi) strcpy(where, " Pi UART");
    else snprintf(where, sizeof(where), " %s port %u", tn_host, tn_port);
    tn_status_show(where, "EDIT menu ");
}

// After EDIT (keys.c): show the menu; when the next key comes, act on it
// and close the menu. The session goes on meanwhile.
static void menu(void)
{
    if (tn_escape == 1) {
        if (!menu_up) {
            tn_status_show(menu_text, "");
            menu_up = 1;
        }
        return;
    }
    if (tn_escape_key == 'q' || tn_escape_key == 'Q') tn_quit = 1;
    tn_escape = 0;
    menu_up = 0;
    status_idle();
}

static const char *const help_text[] = {
    "\nTELNET",
      "======",
    "\n Usage",
    ".TELNET host [port]",
    ".TELNET -pi",
    "\nhost: name or IP address",
    "port: 1-65535, 23 if not given",
    "-pi:  a terminal on the Pi's",
    "      UART (GPIO 14, 15)",
    "\nEDIT, then Q leaves.",
    "BREAK sends ESC.",
    "EXTEND with a letter sends its",
    "control code."
};

// Help, then exit with err (0, or a custom error message)
static void usage(const char *err)
{
    d3_logo(); printf(" TELNET\n");
    for (uint8_t i = 0; i < sizeof(help_text) / sizeof(help_text[0]); i++)
        printf("%s\n", help_text[i]);
    exit((int)err);
}

static void parse(int argc, char **argv)
{
    if (argc < 2) usage(0);

    if (argv[1][0] == '-') {
        if (argc != 2 || stricmp(argv[1], "-pi")) usage(err_bad_option);
        tn_pi = 1;
        return;
    }

    if (argc > 3) usage(err_bad_option);
    tn_host = argv[1];

    if (argc == 3) {
        const char *p = argv[2];
        uint32_t n = 0;

        for (; isdigit(*p) && n <= 65535; ++p) n = n * 10 + (*p - '0');
        if (p == argv[2] || *p || !n || n > 65535) usage(err_bad_port);
        tn_port = n;
    }
}

int main(int argc, char **argv)
{
    parse(argc, argv);

    atexit(tn_putback);
    tn_takeover();
    tn_term_init();
    status_idle();

    if (!tn_pi) {
        const char *err = tn_esp_open();
        if (err) exit((int)err);
    }

    // Bytes from the remote end go to the screen, keys to the remote end.
    // With -pi there is no telnet protocol: every byte is data.
    while (!tn_quit) {
        if (tn_escape) menu();
        while (tn_uart_avail()) {
            if (tn_pi) tn_term_put(tn_uart_get());
            else tn_telnet_rx(tn_uart_get());
        }
        while (tn_keyq_avail()) tn_uart_put(tn_keyq_pop());
    }

    if (!tn_pi) tn_esp_close();

    // Keys still down at the return to BASIC would reach the BASIC editor
    while (tn_keys_down()) ;

    tn_putback();
    if (!tn_pi) printf("Connection closed\n");
    return 0;
}
