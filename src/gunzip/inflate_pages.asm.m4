dnl The two decoder images, in the 16K bank that the program picks at build
dnl time with -DUNZIP_BANK for C and -Cm-DUNZIP_BANK for m4. Page A is the
dnl lower 8K; page C is the upper 8K, where its code runs at $F000.
dnl The default must match the one in inflate.h.
ifdef(`UNZIP_BANK',,`define(`UNZIP_BANK',`33')')dnl
SECTION BANK_`'UNZIP_BANK`'_L

   BINARY "inflate_PAGEA.bin"

SECTION BANK_`'UNZIP_BANK`'_H

   defs $1000
   BINARY "inflate_PAGEC.bin"
