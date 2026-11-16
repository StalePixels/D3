; Enter and leave code for the decoder in inflate.asm, and its two code images.
; Must be linked at $4000 or above, outside MMU2: it runs while MMU0 changes.

INCLUDE "inflate_job.inc"

SECTION code_user

PUBLIC _inflate_call

; hl = struct inflate_job *
; Saves the machine state into the job, switches off the automap entry points
; that fire without ROM3 and the NMI buttons, pages the decoder into MMU0 and
; jumps to it. inflate.asm returns to stub_leave with every other slot restored.
_inflate_call:
   push ix
   push iy
   ld a,i
   di
   push af
   ld (stub_sp),sp
   ld (stub_job),hl
   push hl
   pop ix
   ld (ix + J_SELF),l
   ld (ix + J_SELF + 1),h
   ld de,stub_leave
   ld (ix + J_LEAVE),e
   ld (ix + J_LEAVE + 1),d
   ld de,J_HMMU
   add hl,de
   ld e,REG_MMU0
stub_mmu:
   ld a,e
   call stub_getreg
   ld (hl),a
   inc hl
   inc e
   ld a,e
   cp REG_MMU7 + 1
   jr nz,stub_mmu
   in a,(PORT_DIVMMC)
   ld (ix + J_HE3),a
   ld a,REG_PERIPH2
   call stub_getreg
   ld (ix + J_R06),a
   and $E7
   nextreg REG_PERIPH2,a
   ld a,REG_VALID0
   call stub_getreg
   ld (ix + J_RB9),a
   cpl
   ld d,a
   ld a,REG_ENTRY0
   call stub_getreg
   ld (ix + J_RB8),a
   and d
   ld (ix + J_B8C),a
   nextreg REG_ENTRY0,a
   ld a,REG_ENTRY1
   call stub_getreg
   ld (ix + J_RBB),a
   and $FC
   ld (ix + J_BBC),a
   nextreg REG_ENTRY1,a
   ld a,(ix + J_PA)
   nextreg REG_MMU0,a
   xor a
   out (PORT_DIVMMC),a
   ld l,(ix + J_SELF)
   ld h,(ix + J_SELF + 1)
   jp $0000

; a = Next register number
; a <- value
stub_getreg:
   push bc
   ld bc,$243B
   out (c),a
   inc b
   in a,(c)
   pop bc
   ret

stub_leave:
   ld sp,(stub_sp)
   ld ix,(stub_job)
   ld a,(ix + J_HMMU)
   nextreg REG_MMU0,a
   ld a,(ix + J_HE3)
   out (PORT_DIVMMC),a
   ld a,(ix + J_RB8)
   nextreg REG_ENTRY0,a
   ld a,(ix + J_RBB)
   nextreg REG_ENTRY1,a
   ld a,(ix + J_R06)
   nextreg REG_PERIPH2,a
   ld l,(ix + J_RESULT)
   ld h,0
   pop af
   pop iy
   pop ix
   ret po
   ei
   ret

SECTION data_user

stub_sp:  defw 0
stub_job: defw 0
