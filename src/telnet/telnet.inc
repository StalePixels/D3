; TELNET: what the shell gives the terminal and the telnet code, for asm.
; telnet.h says the same for C, with the memory map and the main loop.
;
; Calls follow z88dk's C rules: a byte argument in L (fastcall), a byte
; result in L, a word result in HL. Any register can change, except IX
; and IY.

; Screen: 80x32 text mode tilemap, 512 tiles, in bank 5

defc TN_COLS            = 80
defc TN_ROWS            = 32            ; a 60 Hz display shows rows 1-30
defc TN_TERM_ROWS       = 24            ; the terminal, in the middle rows 4-27
                                        ; the status bar below it, row 28

defc TN_TILES           = $4000         ; tiles 0-255 font, 256-511 inverted
defc TN_MAP             = $5000         ; cells of 2 bytes: character, attribute
defc TN_FREE            = $6400         ; $6400-$7FFF free for the terminal

; Attribute: bit 7 bold, bits 6-4 paper, bits 3-1 ink, bit 0 reverse.
; Colours in ANSI order (0 black ... 7 white).

defc TN_ATTR_REVERSE    = $01
defc TN_ATTR_DEFAULT    = $0e           ; white ink on black paper

; The UART receive ring, 8K, in MMU7

defc TN_RING            = $e000

; Routines and variables (telnet.h has the details)

EXTERN _tn_uart_avail                   ; -> HL = bytes in the ring
EXTERN _tn_uart_get                     ; -> L = next byte
EXTERN _tn_uart_put                     ; L = byte to send
EXTERN _tn_rx_overflow                  ; byte: non-zero when the ring lost a byte

EXTERN _tn_keyq_push                    ; L = byte -> L = 1, or 0 when full
EXTERN _tn_keyq_avail                   ; -> L = bytes in the queue
EXTERN _tn_keyq_pop                     ; -> L = next byte

EXTERN _tn_term_init                    ; reset the terminal
EXTERN _tn_term_put                     ; L = a byte from the remote end
EXTERN _tn_term_blink                   ; the cursor blink
EXTERN _tn_term_ckm                     ; byte: DECCKM
EXTERN _tn_term_lnm                     ; byte: LNM

EXTERN _tn_hook_cursor                  ; called each frame, interrupts off
EXTERN _tn_hook_keys                    ; called each frame, after the cursor

EXTERN _tn_quit                         ; byte: set to end the session
