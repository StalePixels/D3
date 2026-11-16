//
// GUNZIP: read gzip members (RFC 1952) and run the DEFLATE decoder on each
//

#include <arch/zxn/esxdos.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gunzip.h"
#include "inflate.h"

// Header flags
#define FHCRC       0x02
#define FEXTRA      0x04
#define FNAME       0x08
#define FCOMMENT    0x10
#define FRESERVED   0xe0

// The decoder reads its input in blocks of this size into $E000
#define INPUT_BLOCK 0x1000
#define INPUT_BASE  0xe000

static struct inflate_job job;
static struct esx_stat st;
static unsigned char pages;

static unsigned char buf[256];
static uint16_t buf_len, buf_pos;
static uint32_t buf_at;             // file position of buf[0]

static uint8_t flags;
static uint32_t hcrc;

uint32_t out_total;

// Stops with the esxDOS error in errno, if there is one
static void check_errno(void)
{
    if (errno) exit(errno);
}

static void seek(uint32_t pos)
{
    errno = 0;
    esx_f_seek(fin, pos, ESX_SEEK_SET);
    check_errno();
    buf_at = pos;
    buf_len = buf_pos = 0;
}

static uint32_t get32(const unsigned char *p)
{
    return p[0] | ((uint16_t)p[1] << 8) | ((uint32_t)(p[2] | ((uint16_t)p[3] << 8)) << 16);
}

static void crc_byte(unsigned char b)
{
    uint8_t k;

    hcrc ^= b;
    for (k = 0; k != 8; ++k)
        hcrc = (hcrc & 1) ? (hcrc >> 1) ^ 0xedb88320UL : hcrc >> 1;
}

// The next header byte; with FHCRC set, it is added to the header CRC
static unsigned char get_byte(void)
{
    unsigned char b;

    if (buf_pos == buf_len) {
        buf_at += buf_len;
        buf_pos = 0;
        errno = 0;
        buf_len = esx_f_read(fin, buf, sizeof(buf));
        check_errno();
        if (!buf_len) exit((int)err_truncated);
    }
    b = buf[buf_pos++];
    if (flags & FHCRC) crc_byte(b);
    return b;
}

static void skip_string(void)
{
    while (get_byte()) ;
}

// Reads the header of the member at pos. Returns the position of its
// compressed data.
static uint32_t read_header(uint32_t pos)
{
    uint16_t len;
    uint8_t i;
    unsigned char hdr[10];

    seek(pos);
    flags = 0;
    hcrc = 0xffffffffUL;
    for (i = 0; i != sizeof(hdr); ++i) {
        hdr[i] = get_byte();
        if (i == 3) {
            flags = hdr[3];
            if (flags & FHCRC) {
                // the four bytes before FLG are in the CRC too
                hcrc = 0xffffffffUL;
                crc_byte(hdr[0]); crc_byte(hdr[1]); crc_byte(hdr[2]); crc_byte(hdr[3]);
            }
        }
        if (i == 1 && (hdr[0] != 0x1f || hdr[1] != 0x8b)) exit((int)err_not_gzip);
    }
    if (hdr[2] != 8 || (flags & FRESERVED)) exit((int)err_unsupported);

    if (flags & FEXTRA) {
        len = get_byte();
        len |= (uint16_t)get_byte() << 8;
        while (len--) get_byte();
    }
    if (flags & FNAME) skip_string();
    if (flags & FCOMMENT) skip_string();
    if (flags & FHCRC) {
        len = (uint16_t)~hcrc;
        flags = 0;
        if (get_byte() != (uint8_t)len || get_byte() != (uint8_t)(len >> 8))
            exit((int)err_header_crc);
    }
    return buf_at + buf_pos;
}

// Decompresses the member whose data starts at data. Returns the position
// of its 8-byte trailer.
static uint32_t inflate_member(uint32_t data)
{
    uint32_t got, end;
    unsigned char r;

    seek(data);
    job.in_handle = fin;
    job.out_handle = fout;
    job.method = 8;
    // The decoder checks its output against crc and usize. The trailer is
    // found only after decoding, so the check is done here: usize can never
    // match, and a complete decode returns UNZIP_E_CHECK.
    job.crc = 0;
    job.usize = 0xffffffffUL;
    job.diag[INFLATE_DIAG_ESXERR] = 0;

    r = inflate_call(&job);

    if (r == UNZIP_E_ABORT) exit((int)err_break);
    if (r == UNZIP_E_READ || r == UNZIP_E_WRITE)
        exit(job.diag[INFLATE_DIAG_ESXERR] ? job.diag[INFLATE_DIAG_ESXERR] : ESX_EIO);

    errno = 0;
    got = esx_f_fgetpos(fin) - data;
    check_errno();

    // The decoder stops with UNZIP_E_DATA when a read gets no bytes
    if (r == UNZIP_E_DATA && data + got >= st.size) exit((int)err_truncated);
    if (r != UNZIP_E_CHECK) exit((int)err_data);

    // Every block the decoder read was full, apart from a last one at the
    // end of the file. The used part of the last block ends at J_INPOS.
    end = data + got - ((got - 1) & (INPUT_BLOCK - 1)) - 1
        + (*(uint16_t *)&job.diag[INFLATE_DIAG_INPOS] - INPUT_BASE + 1);
    return end;
}

void gzip_exit(void)
{
    unzip_pages_free(&job, pages);
    pages = 0;
}

void gzip_decompress(void)
{
    uint32_t pos = 0, end;

    errno = 0;
    esx_f_fstat(fin, &st);
    check_errno();

    if ((pages = unzip_pages_alloc(&job)) < UNZIP_PAGES) exit((int)err_no_memory);

    for (;;) {
        end = inflate_member(read_header(pos));

        if (end + 8 > st.size) exit((int)err_truncated);
        seek(end);
        errno = 0;
        if (esx_f_read(fin, buf, 8) != 8) exit(errno ? errno : (int)err_truncated);
        if (get32(buf) != job.crc_out) exit((int)err_crc);
        if (get32(buf + 4) != job.out_count) exit((int)err_size);
        out_total += job.out_count;

        // Another member, or the end of the file
        pos = end + 8;
        if (pos == st.size) return;
        errno = 0;
        if (esx_f_read(fin, buf, 2) != 2 || buf[0] != 0x1f || buf[1] != 0x8b) {
            check_errno();
            printf("Data after the end ignored\n");
            return;
        }
    }
}
