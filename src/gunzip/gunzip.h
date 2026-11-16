//
// GUNZIP: decompress a gzip file
//

#ifndef D3_GUNZIP_H
#define D3_GUNZIP_H

#include <stdint.h>

// The decoder's result codes (inflate_job.inc)
#define UNZIP_OK            0
#define UNZIP_E_NOMEM       1
#define UNZIP_E_READ        2
#define UNZIP_E_FORMAT      3
#define UNZIP_E_UNSUPPORTED 4
#define UNZIP_E_PATH        5
#define UNZIP_E_CREATE      6
#define UNZIP_E_WRITE       7
#define UNZIP_E_DATA        8
#define UNZIP_E_CHECK       9
#define UNZIP_E_ABORT       10

// Handles open during a run (0xff when closed), for the exit cleanup
extern unsigned char fin, fout;

// Bytes written to the output file
extern uint32_t out_total;

// Custom error messages (gunzip.asm)
extern const char err_break[];
extern const char err_bad_name[];
extern const char err_not_gzip[];
extern const char err_unsupported[];
extern const char err_header_crc[];
extern const char err_truncated[];
extern const char err_data[];
extern const char err_crc[];
extern const char err_size[];
extern const char err_no_memory[];

// gzip.c: decompress every member of the gzip file open in fin into fout
void gzip_decompress(void);

// gzip.c: free the decoder's pages
void gzip_exit(void);

#endif //D3_GUNZIP_H
