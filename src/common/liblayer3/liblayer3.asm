; I have intentionally left the ULA memory intact, so I can use it later for a help SCR file ;-)

PUBLIC _tilemap
defc _tilemap = 0x6C00      ; 80x32 words

PUBLIC _tiles
defc _tiles = 0x5C00        ; 124 4bit, 496 1bit  @  0x6C00

SECTION BANK_41

PUBLIC _font
_font:
    BINARY "specp437_32-255.zx7"