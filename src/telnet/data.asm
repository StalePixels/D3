; TELNET: the font, and the custom BASIC error messages (last character
; with bit 7 set). Both in the main bank.

SECTION rodata_user

PUBLIC _tn_font_zx7

; CP437 glyphs 32-255, 8 bytes each, ZX7 compressed
_tn_font_zx7:

   BINARY "../common/liblayer3/specp437_32-255.zx7"

PUBLIC _err_bad_port
PUBLIC _err_bad_option

_err_bad_port:

   defm "Invalid port numbe", 'r' + 0x80

_err_bad_option:

   defm "Invalid optio", 'n' + 0x80
