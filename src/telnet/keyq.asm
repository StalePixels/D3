; TELNET: the key queue. Bytes waiting to go to the UART: keys from the
; keyboard hook, and the terminal's replies.
;
; 256 bytes on a 256-byte boundary, so a one-byte index is the low byte of
; the address and wraps by itself. keyq_head is where the next byte goes,
; keyq_tail the next byte to send. It holds 255 bytes: full is
; keyq_head + 1 = keyq_tail.
;
; Push can come from the frame interrupt or from the main program, so it
; runs with interrupts off. Only the main program pops.

INCLUDE "telnet.inc"

SECTION code_user

PUBLIC _tn_keyq_push, _tn_keyq_avail, _tn_keyq_pop

; uint8_t tn_keyq_push(uint8_t c) __z88dk_fastcall

_tn_keyq_push:

   ld a,i                      ; P/V = IFF2: were interrupts on?
   jp pe, push_keep
   ld a,i                      ; an interrupt taken during the first read
                               ; clears P/V, so read it again
push_keep:

   push af
   di

   ld a,(keyq_head)
   ld e,a
   inc a
   ld d,a                      ; d = keyq_head + 1
   ld a,(keyq_tail)
   cp d
   jr z, push_full

   ld a,l
   ld hl,keyq_buf
   ld l,e
   ld (hl),a
   ld a,d
   ld (keyq_head),a
   ld l,1
   jr push_done

push_full:

   ld l,0

push_done:

   pop af
   ret po                      ; interrupts were off
   ei
   ret

; uint8_t tn_keyq_avail(void)

_tn_keyq_avail:

   ld a,(keyq_tail)
   ld l,a
   ld a,(keyq_head)
   sub l
   ld l,a
   ret

; uint8_t tn_keyq_pop(void)

_tn_keyq_pop:

   ld a,(keyq_tail)
   ld hl,keyq_buf
   ld l,a
   inc a
   ld (keyq_tail),a
   ld l,(hl)
   ret

SECTION bss_align_256

keyq_buf:           defs 256

SECTION bss_user

keyq_head:          defb 0
keyq_tail:          defb 0
