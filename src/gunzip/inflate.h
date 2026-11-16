#ifndef _INFLATE_H
#define _INFLATE_H

#include <stdbool.h>
#include <stdint.h>

// Layout is fixed: inflate_job.inc holds the same offsets for the assembler.
// first and blob_a to len_c are unused since the dotn loader loads the images.
struct inflate_job {
    uint8_t page[7];
    uint8_t first;
    uint8_t in_handle;
    uint8_t out_handle;
    uint8_t method;
    uint32_t csize;
    uint32_t usize;
    uint32_t crc;
    uint8_t host_mmu[8];
    uint8_t host_e3;
    uint8_t reg_06;
    uint8_t reg_b8;
    uint8_t reg_b9;
    uint8_t reg_bb;
    uint8_t clear_b8;
    uint8_t clear_bb;
    void *self;
    void *leave;
    const void *blob_a;
    uint16_t len_a;
    const void *blob_c;
    uint16_t len_c;
    uint8_t result;
    uint32_t out_count;
    uint32_t crc_out;
    uint8_t iff_io;
    uint8_t diag[32];
};

// The 16K bank that holds the two images (inflate_pages.asm.m4). Bank 33 exists
// on a 1MB Next and is below banks 34-47, which NextPiUI uses.
#ifndef UNZIP_BANK
#define UNZIP_BANK 33
#endif
#define INFLATE_PAGE_A (UNZIP_BANK * 2)
#define INFLATE_PAGE_C (UNZIP_BANK * 2 + 1)

// A dotn loader puts each page where it can: its table gives the physical page.
// In a NEX the page number is the physical page.
#ifdef dotn
extern unsigned char _z_page_table[];
#define inflate_page(page) _z_page_table[page]
#else
#define inflate_page(page) (page)
#endif

// diag[29] is J_ESXERR in inflate_job.inc
#define INFLATE_DIAG_ESXERR 29

// diag[0] and diag[1] are J_INPOS: the address in the input buffer ($E000-$EFFF)
// of the last byte the decoder used (D3 .gunzip)
#define INFLATE_DIAG_INPOS 0

#define UNZIP_PAGES 5

bool unzip_pages_keep(void);
void unzip_pages_release(void);
unsigned char unzip_pages_alloc(struct inflate_job *job);
void unzip_pages_free(struct inflate_job *job, unsigned char pages);

extern unsigned char inflate_call(struct inflate_job *job) __z88dk_fastcall;

#endif
