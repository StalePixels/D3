; TELNET: the UART receive ring, and sending.
;
; The ring is one 8K page in MMU7, $E000-$FFFF. The interrupts write it
; (rx_drain), and the main program reads it (tn_uart_get). rx_head is
; where the next byte goes and rx_tail is the next byte to read; each
; side writes only its own pointer, so neither needs interrupts off. The
; ring is full with one byte free: rx_head + 1 = rx_tail.
;
; Why 8K: the UART's own buffer is 512 bytes, about two frames at
; 115,200 baud (11,520 bytes a second), and the interrupts empty it
; every frame. The ring holds what arrives while the terminal is busy
; with something long, such as a clear of the whole screen. 8K is 0.7 s
; (35 frames) of data at the full rate, and it is one page, so a pointer
; wraps with a test of its high byte.

INCLUDE "telnet.inc"

defc IO_UART_STATUS     = $133b         ; read: status, write: send a byte
defc IO_UART_RX         = $143b

SECTION code_user

PUBLIC rx_drain
PUBLIC _tn_uart_avail, _tn_uart_get, _tn_uart_put, _tn_rx_reset

; Move every byte in the UART's buffer into the ring. A byte that does not
; fit is lost, and tn_rx_overflow is set. The UART is emptied either way,
; so that its interrupt stops.
; Interrupts must be off. Uses af, bc, de, hl.

rx_drain:

   ld hl,(rx_head)
   ld de,(rx_tail)
   ld bc,IO_UART_STATUS

drain_next:

   in a,(c)
   rrca                        ; bit 0: the buffer has bytes
   jr nc, drain_done

   inc b                       ; IO_UART_RX
   in a,(c)
   dec b                       ; IO_UART_STATUS

   ld (hl),a                   ; the slot at rx_head is always free
   inc l
   jr nz, drain_check
   inc h
   jr nz, drain_check
   ld h,TN_RING / 256          ; past $FFFF: back to the start

drain_check:

   ld a,l
   cp e
   jr nz, drain_next
   ld a,h
   cp d
   jr nz, drain_next

   ; rx_head has caught up with rx_tail: step it back, so the byte is lost

   ld a,1
   ld (_tn_rx_overflow),a
   dec hl
   ld a,h
   cp TN_RING / 256
   jr nc, drain_next
   ld hl,$ffff
   jr drain_next

drain_done:

   ld (rx_head),hl
   ret

; uint16_t tn_uart_avail(void)

_tn_uart_avail:

   ld hl,(rx_head)
   ld de,(rx_tail)
   or a
   sbc hl,de
   ld a,h
   and $1f                     ; modulo 8K
   ld h,a
   ret

; uint8_t tn_uart_get(void)

_tn_uart_get:

   ld hl,(rx_tail)
   ld a,(hl)
   inc l
   jr nz, get_done
   inc h
   jr nz, get_done
   ld h,TN_RING / 256

get_done:

   ld (rx_tail),hl
   ld l,a
   ret

; void tn_uart_put(uint8_t c) __z88dk_fastcall

_tn_uart_put:

   ld bc,IO_UART_STATUS

put_wait:

   in a,(c)
   and $02                     ; bit 1: the transmit buffer is full
   jr nz, put_wait
   out (c),l
   ret

; void tn_rx_reset(void)
; Empty the ring. For the shell, before the interrupts start.

_tn_rx_reset:

   ld hl,TN_RING
   ld (rx_head),hl
   ld (rx_tail),hl
   xor a
   ld (_tn_rx_overflow),a
   ret

SECTION bss_user

PUBLIC _tn_rx_overflow

rx_head:            defw 0
rx_tail:            defw 0
_tn_rx_overflow:    defb 0
