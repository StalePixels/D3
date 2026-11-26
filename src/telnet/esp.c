//
// TELNET: the link through the ESP WiFi module, with its AT commands.
//
// tn_esp_open checks that the ESP answers, opens a TCP link and puts the
// ESP in transparent mode: AT+CIPMODE=1, AT+CIPSTART, then AT+CIPSEND,
// which the ESP answers with '>'. From then on every byte sent to the
// UART goes to the remote end, and every byte from the remote end comes
// out of the UART, with nothing added.
//
// The remote end can send data as soon as the link opens, before the '>'.
// Those bytes go to the telnet code like all the others. Some firmware
// sends them bare and some puts each block after "+IPD,<length>:"; both
// work.
//
// tn_esp_close takes the ESP out of transparent mode ("+++" with a second
// of quiet before and after it), back to AT+CIPMODE=0, and closes the
// link.
//
// Each wait for an answer has a time limit, counted in frames.
//

#include <stdlib.h>                     // atoi
#include <string.h>

#include "telnet.h"
#include "shell.h"

// What an answer ends with
#define WAIT_OK             0           // OK
#define WAIT_PROMPT         1           // '>'

// What a wait gives
#define GOT_NOTHING         0           // the answer has not ended yet
#define GOT_OK              1
#define GOT_ERROR           2
#define GOT_TIMEOUT         3

// The longest wait for the answer to a command. A busy ESP can take
// seconds to answer even AT; a shorter wait lets its late answer be
// taken as the answer to the next command.
#define ANSWER_SECONDS      10

// The error for BASIC: the ESP's message, last character with bit 7 set
char tn_esp_error[33];

static uint8_t fps;                     // frames a second: 50 or 60

static char line[48];                   // the line from the ESP so far
static uint8_t line_len;
static uint16_t ipd_left;               // bytes left of a "+IPD" block
static uint8_t blank_lines;             // empty lines held back (WAIT_PROMPT)
static char message[33];                // the ESP's last line that was not OK or ERROR

static uint16_t frames(void)
{
    uint16_t f;

    do f = tn_frames; while (f != tn_frames);   // the interrupt changes it
    return f;
}

static void put_str(const char *s)
{
    while (*s) tn_uart_put(*s++);
}

static void put_number(uint16_t n)
{
    char digits[5];
    uint8_t i = 0;

    do {
        digits[i++] = '0' + n % 10;
        n /= 10;
    } while (n);
    while (i) tn_uart_put(digits[--i]);
}

// Is the line (without its CR) s?
static uint8_t line_is(const char *s, uint8_t n)
{
    return n == strlen(s) && !memcmp(line, s, n);
}

// Hand the line to the telnet code as data, with the empty lines before it
static void line_to_telnet(void)
{
    uint8_t i;

    for (; blank_lines; blank_lines--) {
        tn_telnet_rx('\r');
        tn_telnet_rx('\n');
    }
    for (i = 0; i < line_len; i++) tn_telnet_rx(line[i]);
    line_len = 0;
}

// A whole line has come (its LF not stored)
static uint8_t end_line(uint8_t wait)
{
    uint8_t n = line_len;

    if (n && line[n - 1] == '\r') n--;

    if (!n) {
        // In WAIT_PROMPT an empty line may be data: hold it until the
        // next line says what it is.
        if (wait == WAIT_PROMPT) blank_lines++;
        line_len = 0;
        return GOT_NOTHING;
    }
    if (line_is("OK", n)) {
        blank_lines = 0;
        line_len = 0;
        return (wait == WAIT_OK) ? GOT_OK : GOT_NOTHING;
    }
    if (line_is("ERROR", n) || line_is("FAIL", n)) {
        line_len = 0;
        return GOT_ERROR;
    }
    if (n >= 2 && line[0] == 'A' && line[1] == 'T') {
        line_len = 0;                           // the ESP's echo of a command
        return GOT_NOTHING;
    }
    if (wait == WAIT_PROMPT) {
        line[line_len++] = '\n';
        line_to_telnet();
        return GOT_NOTHING;
    }
    if (n >= sizeof(message)) n = sizeof(message) - 1;
    memcpy(message, line, n);
    message[n] = 0;
    line_len = 0;
    return GOT_NOTHING;
}

// One byte from the ESP
static uint8_t take(uint8_t c, uint8_t wait)
{
    if (ipd_left) {
        ipd_left--;
        tn_telnet_rx(c);
        return GOT_NOTHING;
    }

    if (c == '\n') return end_line(wait);

    if (wait == WAIT_PROMPT && line_len == 0 && c == '>') {
        blank_lines = 0;
        return GOT_OK;
    }

    if (line_len == sizeof(line) - 1) {
        // Too long for an ESP line: data, or a line not worth keeping
        if (wait == WAIT_PROMPT) line_to_telnet();
        else line_len = 0;
    }
    line[line_len++] = c;

    // "+IPD,<length>:" starts a block of data
    if (c == ':' && line_len > 5 && !memcmp(line, "+IPD,", 5)) {
        line[line_len - 1] = 0;
        ipd_left = atoi(line + 5);
        line_len = 0;
        blank_lines = 0;                        // the ESP's CR LF before it
    }
    return GOT_NOTHING;
}

// Wait for the end of an answer, at most the given seconds
static uint8_t wait_for(uint8_t wait, uint8_t seconds)
{
    uint16_t start = frames();
    uint16_t limit = seconds * fps;
    uint8_t got;

    message[0] = 0;
    for (;;) {
        while (tn_uart_avail()) {
            got = take(tn_uart_get(), wait);
            if (got) return got;
        }
        if (frames() - start >= limit) return GOT_TIMEOUT;
    }
}

// Send an AT command and wait for its answer
static uint8_t command(const char *cmd, uint8_t wait, uint8_t seconds)
{
    put_str(cmd);
    put_str("\r\n");
    return wait_for(wait, seconds);
}

// Nothing sent for the given seconds. Data from the remote end still goes
// to the screen.
static void quiet(uint8_t seconds)
{
    uint16_t start = frames();
    uint16_t limit = seconds * fps + 1;

    while (frames() - start < limit)
        while (tn_uart_avail()) take(tn_uart_get(), WAIT_OK);
}

// The error for the answer got: the ESP's message, "ERROR", or "no answer"
static const char *error(uint8_t got)
{
    const char *s = message;
    uint8_t n;

    if (got == GOT_TIMEOUT) s = "no answer";
    else if (!*s) s = "ERROR";
    n = strlen(s);
    memcpy(tn_esp_error, s, n);
    tn_esp_error[n - 1] |= 0x80;
    return tn_esp_error;
}

const char *tn_esp_open(void)
{
    uint8_t got;
    uint8_t tries = 3;

    fps = (tn_nr_read(0x05) & 0x04) ? 60 : 50;

    // The first AT can meet the end of something else in the ESP's
    // buffer, and get ERROR: then it goes again. An ESP that does not
    // answer is busy, and another AT only adds to its work, so a timeout
    // ends the tries.
    do got = command("AT", WAIT_OK, ANSWER_SECONDS); while (got == GOT_ERROR && --tries);
    if (got != GOT_OK) return error(got);

    got = command("AT+CIPMODE=1", WAIT_OK, ANSWER_SECONDS);
    if (got != GOT_OK) return error(got);

    put_str("AT+CIPSTART=\"TCP\",\"");
    put_str(tn_host);
    put_str("\",");
    put_number(tn_port);
    got = command("", WAIT_OK, 20);
    if (got != GOT_OK) return error(got);

    got = command("AT+CIPSEND", WAIT_PROMPT, ANSWER_SECONDS);
    if (got != GOT_OK) return error(got);

    return 0;
}

void tn_esp_close(void)
{
    quiet(1);
    put_str("+++");
    quiet(1);
    command("AT+CIPMODE=0", WAIT_OK, ANSWER_SECONDS);
    command("AT+CIPCLOSE", WAIT_OK, ANSWER_SECONDS);         // CLOSED, then OK
}
