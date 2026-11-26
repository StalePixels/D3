//
// TELNET: take over the machine, and put it back.
//
// tn_takeover keeps everything the session changes, then sets up the
// screen, the UART and the interrupts. tn_putback writes it all back. It
// runs at every exit (atexit), also after an error, and does nothing
// when nothing was taken.
//

#include <arch/zxn.h>
#include <intrinsic.h>
#include <string.h>

#include "telnet.h"
#include "shell.h"

// The extra pages that the dotn loader allocates (DOTN_NUM_EXTRA in
// telnet.c), and frees when the command ends
extern uint8_t _z_extra_table[];

#define PAGE_BANK5_LO       (_z_extra_table[0])     // copy of $4000-$5FFF
#define PAGE_BANK5_HI       (_z_extra_table[1])     // copy of $6000-$7FFF

#define BANK5               ((uint8_t *)0x4000)
#define WINDOW              ((uint8_t *)0xC000)     // MMU6
#define PAGE_SIZE           0x2000

#define SYSVAR_BORDCR       (*(volatile uint8_t *)23624)

// NextRegs that the session changes, kept at the start and written back
// at exit in this order, after bank 5 and the palettes
static const uint8_t kept_regs[] = {
    0x07,           // CPU speed
    0x14,           // global transparency colour
    0x15,           // sprites and layers: bit 0 shows the sprites
    0x2F, 0x30,     // tilemap X scroll
    0x31,           // tilemap Y scroll
    0x43,           // palette control
    0x40,           // palette index
    0x4A,           // fallback colour
    0x4C,           // tilemap transparency index
    0x56, 0x57,     // MMU6, MMU7
    0x6B,           // tilemap control
    0x6C,           // default tilemap attribute
    0x6E,           // tilemap base address
    0x6F,           // tile definitions base address
    0xA0,           // Pi peripheral enable (the Pi's UART on GPIO 14, 15)
    0x68            // ULA control: the ULA comes back on last
};

#define N_KEPT              sizeof(kept_regs)
#define KEPT_15             2           // index of $15 in kept_regs
#define KEPT_43             6           // index of $43 in kept_regs

static uint8_t kept[N_KEPT];
static uint8_t kept_clip[4];            // $1B: X1, X2, Y1, Y2
static uint8_t kept_border;
static uint8_t kept_layer2;             // NextReg $69 bit 7: Layer 2 shown
static uint8_t kept_ula_palette[512];
static uint8_t kept_tilemap_palette[512];

uint8_t tn_nr_read(uint8_t reg) __z88dk_fastcall
{
    IO_NEXTREG_REG = reg;
    return IO_NEXTREG_DAT;
}

void tn_nr_write(uint8_t reg, uint8_t val)
{
    IO_NEXTREG_REG = reg;
    IO_NEXTREG_DAT = val;
}

// Select palette (TN_PALETTE_ULA or TN_PALETTE_TILEMAP) for reads and
// writes through $40, $41 and $44, with auto-increment on. Keeps the
// bits of $43 that pick the palettes on show.
void tn_palette_edit(uint8_t palette) __z88dk_fastcall
{
    tn_nr_write(0x43, (kept[KEPT_43] & 0x0F) | palette);
}

static void palette_keep(uint8_t palette, uint8_t *dst)
{
    uint8_t i = 0;

    tn_palette_edit(palette);
    do {
        tn_nr_write(0x40, i);
        *dst++ = tn_nr_read(0x41);      // RRRGGGBB
        *dst++ = tn_nr_read(0x44);      // bit 0: the low bit of blue
    } while (++i);
}

static void palette_putback(uint8_t palette, const uint8_t *src)
{
    uint8_t i = 0;

    tn_palette_edit(palette);
    tn_nr_write(0x40, 0);
    do {
        tn_nr_write(0x44, *src++);
        tn_nr_write(0x44, *src++);
    } while (++i);
}

// The tilemap clip window. Reads of $1B do not move on to the next
// coordinate, so each one is written back to move on.
static void clip_keep(void)
{
    tn_nr_write(0x1C, 0x08);            // first coordinate of the tilemap clip
    for (uint8_t i = 0; i < 4; i++) {
        kept_clip[i] = tn_nr_read(0x1B);
        tn_nr_write(0x1B, kept_clip[i]);
    }
}

static void clip_set(const uint8_t *clip)
{
    tn_nr_write(0x1C, 0x08);
    for (uint8_t i = 0; i < 4; i++) tn_nr_write(0x1B, clip[i]);
}

// Bank 5 is two pages. Copy each to or from its page through MMU6.
static void bank5_copy(uint8_t keep)
{
    tn_nr_write(0x56, PAGE_BANK5_LO);
    if (keep) memcpy(WINDOW, BANK5, PAGE_SIZE);
    else memcpy(BANK5, WINDOW, PAGE_SIZE);

    tn_nr_write(0x56, PAGE_BANK5_HI);
    if (keep) memcpy(WINDOW, BANK5 + PAGE_SIZE, PAGE_SIZE);
    else memcpy(BANK5 + PAGE_SIZE, WINDOW, PAGE_SIZE);
}

void tn_takeover(void)
{
    uint8_t i;

    tn_irq_save();                      // interrupts off from here

    for (i = 0; i < N_KEPT; i++) kept[i] = tn_nr_read(kept_regs[i]);
    clip_keep();
    kept_border = (SYSVAR_BORDCR >> 3) & 7;
    kept_layer2 = tn_nr_read(0x69) & 0x80;
    palette_keep(TN_PALETTE_ULA, kept_ula_palette);
    palette_keep(TN_PALETTE_TILEMAP, kept_tilemap_palette);
    bank5_copy(1);

    tn_nr_write(0x07, 3);               // 28 MHz
    tn_nr_write(0x6B, 0);               // tilemap off while it is built
    tn_nr_write(0x15, kept[KEPT_15] & 0xFE);       // sprites off

    // Layer 2 off. The other bits of $69 write port $7FFD bit 3 and
    // port $FF: they keep their values.
    tn_nr_write(0x69, tn_nr_read(0x69) & 0x7F);

    // ULA off. Bit 4 clear: the Next keyboard's extra keys come into the
    // key matrix (keys.c).
    tn_nr_write(0x68, (kept[N_KEPT - 1] | 0x80) & 0xEF);
    IO_FE = 0;                          // black border

    tn_screen_setup();
    tn_rx_reset();

    tn_irq_start(tn_uart_open());       // interrupts on
}

void tn_putback(void)
{
    uint8_t i;

    if (!tn_taken) return;

    intrinsic_di();
    tn_uart_close();
    tn_nr_write(0x6B, 0);               // tilemap off before bank 5 changes

    bank5_copy(0);
    palette_putback(TN_PALETTE_ULA, kept_ula_palette);
    palette_putback(TN_PALETTE_TILEMAP, kept_tilemap_palette);
    clip_set(kept_clip);
    tn_nr_write(0x69, (tn_nr_read(0x69) & 0x7F) | kept_layer2);
    for (i = 0; i < N_KEPT; i++) {
        uint8_t v = kept[i];
        if (kept_regs[i] == 0x07) v &= 0x03;    // bits 5-4 read the actual speed
        tn_nr_write(kept_regs[i], v);
    }
    IO_FE = kept_border;

    tn_irq_restore();                   // interrupts as they were
}
