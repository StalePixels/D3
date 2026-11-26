; TELNET: interrupts.
;
; The session runs in hardware IM2 mode (NextReg $C0 bit 0): each source
; of interrupts jumps through its own entry in the vector table below.
; Two sources are on:
;
; - The frame (ULA) interrupt empties the UART into the ring, then calls
;   the cursor and keyboard hooks.
; - The UART "Rx near full" interrupt empties the UART into the ring when
;   its 512-byte buffer is 3/4 full. At 115,200 baud the buffer fills in
;   about two frames, so this covers a frame interrupt that comes late.
;
; tn_irq_save keeps the interrupt state of NextZXOS and tn_irq_restore
; puts it back: IFF, I, and NextRegs $C0, $C4, $C5 and $C6. The interrupt
; mode cannot be read back, so the restore sets IM 1: NextZXOS runs a dot
; command in IM 1, and the dotn CRT sets IM 1 again when the command ends.

INCLUDE "telnet.inc"

SECTION code_user

EXTERN rx_drain

PUBLIC _tn_irq_save, _tn_irq_start, _tn_irq_restore

defc NR_SELECT          = $243b
defc NR_INT_CONTROL     = $c0
defc NR_INT_EN_0        = $c4
defc NR_INT_EN_1        = $c5
defc NR_INT_EN_2        = $c6
defc NR_INT_STATUS_0    = $c8
defc NR_INT_STATUS_1    = $c9
defc NR_INT_STATUS_2    = $ca

; void tn_irq_save(void)
; Turns interrupts off and keeps the interrupt state. Sets tn_taken: from
; here on, the exit must put the machine back.

_tn_irq_save:

   ld a,i                      ; P/V = IFF2
   jp pe, save_on
   ld a,i                      ; an interrupt taken during the first read
   jp pe, save_on              ; clears P/V, so read it again
   xor a
   jr save_iff

save_on:

   ld a,1

save_iff:

   di
   ld (irq_old_iff),a
   ld a,1
   ld (_tn_taken),a
   ld a,i
   ld (irq_old_i),a

   ld hl,irq_old_regs
   ld a,NR_INT_CONTROL
   call nr_read
   ld (hl),a
   inc hl
   ld a,NR_INT_EN_0
   call nr_read
   ld (hl),a
   inc hl
   ld a,NR_INT_EN_1
   call nr_read
   ld (hl),a
   inc hl
   ld a,NR_INT_EN_2
   call nr_read
   ld (hl),a
   ret

; a = NextReg -> a = its value. Uses bc.

nr_read:

   ld bc,NR_SELECT
   out (c),a
   inc b
   in a,(c)
   ret

; void tn_irq_start(uint8_t uart_bits) __z88dk_fastcall
; L = the bits to set in NextReg $C6: the Rx near full bit of the UART in
; use. Starts IM2 with the frame interrupt and that one.

_tn_irq_start:

   di
   ld a,l
   nextreg NR_INT_EN_2,a
   nextreg NR_INT_EN_1,0       ; no CTC
   nextreg NR_INT_EN_0,$01     ; the frame; no line, no expansion bus
   nextreg NR_INT_STATUS_0,$03 ; clear anything still pending
   nextreg NR_INT_STATUS_1,$ff
   nextreg NR_INT_STATUS_2,$77

   ; The table must start on a 32-byte boundary. The linker does not
   ; place a section on one, so copy the table to the first boundary in
   ; irq_table_space.
   ld hl,irq_table_space + 31
   ld a,l
   and $e0
   ld l,a
   ld e,l
   ld d,h
   push hl
   ld hl,irq_vectors
   ld bc,32
   ldir
   pop hl

   ld a,h
   ld i,a
   ld a,(irq_old_regs)         ; keep the stackless NMI bit
   and $08
   or l                        ; bits 7-5 of the table's low byte
   or $01                      ; hardware IM2 mode
   nextreg NR_INT_CONTROL,a
   im 2
   ei
   ret

; void tn_irq_restore(void)
; Puts back the interrupt state that tn_irq_save kept, and clears
; tn_taken. The last thing the exit does.

_tn_irq_restore:

   di
   ld hl,irq_old_regs + 3
   ld a,(hl)
   nextreg NR_INT_EN_2,a
   dec hl
   ld a,(hl)
   nextreg NR_INT_EN_1,a
   dec hl
   ld a,(hl)
   nextreg NR_INT_EN_0,a
   dec hl
   ld a,(hl)
   and $e9                     ; bits 2-1 are read only, bit 4 is reserved
   nextreg NR_INT_CONTROL,a

   ld a,(irq_old_i)
   ld i,a
   im 1

   xor a
   ld (_tn_taken),a
   ld a,(irq_old_iff)
   or a
   ret z
   ei
   ret

; The handlers. Each ends with RETI, which tells the Next's interrupt
; controller that the handler is done.

; The frame interrupt. Saves every register and the NextReg select port,
; so that the hooks can be C and can use NextRegs.

irq_frame:

   push af
   push bc
   push de
   push hl
   ex af,af'
   exx
   push af
   push bc
   push de
   push hl
   push ix
   push iy

   ld bc,NR_SELECT
   in a,(c)
   push af                     ; the main program's NextReg select

   call rx_drain
   call _tn_hook_cursor
   call _tn_hook_keys

   pop af
   ld bc,NR_SELECT
   out (c),a

   pop iy
   pop ix
   pop hl
   pop de
   pop bc
   pop af
   exx
   ex af,af'
   pop hl
   pop de
   pop bc
   pop af
   ei
   reti

; UART Rx near full

irq_uart:

   push af
   push bc
   push de
   push hl
   call rx_drain
   pop hl
   pop de
   pop bc
   pop af
   ei
   reti

; Every other source. None is enabled.

irq_none:

   ei
   reti

SECTION data_user

; One entry for each source, in the order of NextReg $C0's vector numbers.
; tn_irq_start copies it to a 32-byte boundary in irq_table_space: I holds
; the high byte of that address, and NextReg $C0 bits 7-5 the top three
; bits of the low byte.

irq_vectors:

   defw irq_none               ; 0  line
   defw irq_uart               ; 1  UART 0 (ESP) Rx
   defw irq_uart               ; 2  UART 1 (Pi) Rx
   defw irq_none               ; 3  CTC 0
   defw irq_none               ; 4  CTC 1
   defw irq_none               ; 5  CTC 2
   defw irq_none               ; 6  CTC 3
   defw irq_none               ; 7  CTC 4
   defw irq_none               ; 8  CTC 5
   defw irq_none               ; 9  CTC 6
   defw irq_none               ; 10 CTC 7
   defw irq_frame              ; 11 frame (ULA)
   defw irq_none               ; 12 UART 0 Tx
   defw irq_none               ; 13 UART 1 Tx
   defw irq_none               ; 14 not used
   defw irq_none               ; 15 not used

SECTION bss_user

PUBLIC _tn_taken

; Non-zero from tn_irq_save to the end of tn_irq_restore: the machine is
; taken over, and the exit must put it back.
_tn_taken:          defb 0

irq_old_iff:        defb 0      ; 1 when interrupts were on
irq_old_i:          defb 0
irq_old_regs:       defs 4      ; NextRegs $C0, $C4, $C5, $C6

irq_table_space:    defs 32 + 31    ; the vector table, on the first 32-byte boundary
