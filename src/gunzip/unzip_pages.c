#include <arch/zxn/esxdos.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>

#include "inflate.h"

// B and W0-W3. The program's loader has already loaded pages A and C
static const unsigned char slot[UNZIP_PAGES] = { 1, 3, 4, 5, 6 };

static bool kept;
static unsigned char keptPage[UNZIP_PAGES];

static unsigned char pages_alloc(unsigned char *page) {
    unsigned char pages;

    for (pages = 0; pages < UNZIP_PAGES; pages++) {
        errno = 0;
        page[pages] = esx_ide_bank_alloc(ESX_BANKTYPE_RAM);
        if (errno) break;
    }
    return pages;
}

static void pages_free(unsigned char *page, unsigned char pages) {
    while (pages) esx_ide_bank_free(ESX_BANKTYPE_RAM, page[--pages]);
}

// For a program that must make no M_P3DOS call during an unzip: the pages are allocated once,
// here, and every unzip uses them until unzip_pages_release. False, with nothing allocated, if they are not free
bool unzip_pages_keep(void) {
    unsigned char pages = pages_alloc(keptPage);

    if (pages == UNZIP_PAGES) kept = true;
    else pages_free(keptPage, pages);
    return kept;
}

void unzip_pages_release(void) {
    if (kept) pages_free(keptPage, UNZIP_PAGES);
    kept = false;
}

// The M_P3DOS calls, apart from unzip.c, so that a program can keep them in main
// memory and still build unzip.c into a bank. Returns the number of pages allocated
unsigned char unzip_pages_alloc(struct inflate_job *job) {
    unsigned char page[UNZIP_PAGES];
    unsigned char pages = UNZIP_PAGES;
    unsigned char index;

    if (kept) memcpy(page, keptPage, UNZIP_PAGES);
    else pages = pages_alloc(page);
    for (index = 0; index < pages; index++) job->page[slot[index]] = page[index];
    job->page[0] = inflate_page(INFLATE_PAGE_A);
    job->page[2] = inflate_page(INFLATE_PAGE_C);
    return pages;
}

void unzip_pages_free(struct inflate_job *job, unsigned char pages) {
    if (kept) return;
    while (pages) esx_ide_bank_free(ESX_BANKTYPE_RAM, job->page[slot[--pages]]);
}
