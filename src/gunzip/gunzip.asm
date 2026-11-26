; GUNZIP: custom BASIC error messages, last character with bit 7 set

SECTION rodata_user

PUBLIC _err_break
PUBLIC _err_bad_name
PUBLIC _err_not_gzip
PUBLIC _err_unsupported
PUBLIC _err_header_crc
PUBLIC _err_truncated
PUBLIC _err_data
PUBLIC _err_crc
PUBLIC _err_size
PUBLIC _err_no_memory

_err_break:

   defm "D BREAK - no repea", 't' + 0x80

_err_bad_name:

   defm "Name must end .gz or .tg", 'z' + 0x80

_err_not_gzip:

   defm "Not a gzip fil", 'e' + 0x80

_err_unsupported:

   defm "Unsupported gzip fil", 'e' + 0x80

_err_header_crc:

   defm "Header CRC erro", 'r' + 0x80

_err_truncated:

   defm "Unexpected end of fil", 'e' + 0x80

_err_data:

   defm "Bad compressed dat", 'a' + 0x80

_err_crc:

   defm "CRC erro", 'r' + 0x80

_err_size:

   defm "Size erro", 'r' + 0x80

_err_no_memory:

   defm "4 Out of memor", 'y' + 0x80
