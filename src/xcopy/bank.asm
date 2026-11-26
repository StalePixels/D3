; XCOPY: calls into the bank, and from it into main memory.
;
; The bank is logical page 12 (section PAGE_12, code at $C000). The dotn
; loader allocates it and loads it. It is paged in at $C000 (MMU6) only
; while one of its functions runs. The rest of the time MMU6 holds the home
; page, logical page 0, where the main bank model puts it.
;
; Each entry below is resident. It pages the bank in, calls the function
; of the same name with "_banked" added, and pages back what MMU6 held
; before. Arguments must be in registers (void, or __z88dk_fastcall), as
; this code puts its own data on the stack.
;
; The entries ending in "_home" go the other way: called from the bank,
; they put the home page back at $C000 for an M_P3DOS call, which pages
; RAM 7 over $C000 while it runs.

SECTION code_user

EXTERN __z_page_table

defc XC_HOME = 0
defc XC_BANK = 12                       ; the PAGE_12 sections
defc MMU6 = $56

; void xc_page_home(void), void xc_page_bank(void): page the home page or
; the bank in at $C000, for the exit cleanup

PUBLIC _xc_page_home, _xc_page_bank

_xc_page_home:
   ld a,XC_HOME
   jr xc_map

_xc_page_bank:
   ld a,XC_BANK
   jr xc_map

; Bank entries

PUBLIC _xc_start, _xc_summary
PUBLIC _ui_print_msg, _mounted_drive, _copy_frag_check
PUBLIC _nbn_close

EXTERN _xc_start_banked, _xc_summary_banked
EXTERN _ui_print_msg_banked, _mounted_drive_banked, _copy_frag_check_banked
EXTERN _nbn_close_banked

_xc_start:
   ld de,_xc_start_banked
   jr xc_bank

_xc_summary:
   ld de,_xc_summary_banked
   jr xc_bank

_ui_print_msg:
   ld de,_ui_print_msg_banked
   jr xc_bank

_mounted_drive:
   ld de,_mounted_drive_banked
   jr xc_bank

_copy_frag_check:
   ld de,_copy_frag_check_banked
   jr xc_bank

_nbn_close:
   ld de,_nbn_close_banked

xc_bank:
   ld a,XC_BANK
   jr xc_call

; Home entries

PUBLIC _NBN_Malloc_home, _dm_mapping_home

EXTERN _NBN_Malloc, _dm_mapping

_NBN_Malloc_home:
   ld de,_NBN_Malloc
   jr xc_home

_dm_mapping_home:
   ld de,_dm_mapping

xc_home:
   ld a,XC_HOME

; a = logical page for MMU6, de = function, hl = its argument
; Returns what the function returns, in hl

xc_call:
   ld c,a
   ld a,(xc_page)
   push af
   ld a,c
   call xc_map
   call xc_jp_de
   pop af

; a = logical page: put it in MMU6. Keeps every register but af.

xc_map:
   ld (xc_page),a
   push hl
   ld hl,__z_page_table
   add hl,a
   ld a,(hl)
   pop hl
   nextreg MMU6,a
   ret

xc_jp_de:
   push de
   ret

SECTION data_user

; The logical page now in MMU6
xc_page:
   defb XC_HOME
