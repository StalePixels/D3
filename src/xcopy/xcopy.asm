; XCOPY: custom BASIC error messages, last character with bit 7 set.
; In the bank (bank.asm): the exit cleanup pages it in for the CRT, which
; copies the message.

SECTION PAGE_12

PUBLIC _err_break
PUBLIC _err_self_copy
PUBLIC _err_dst_in_src
PUBLIC _err_invalid_option
PUBLIC _err_need_z
PUBLIC _err_need_url
PUBLIC _err_two_urls
PUBLIC _err_no_memory
PUBLIC _err_line_too_long

_err_break:

   defm "D BREAK - no repea", 't' + 0x80

_err_self_copy:

   defm "Cannot copy a file onto itsel", 'f' + 0x80

_err_dst_in_src:

   defm "DST is inside SR", 'C' + 0x80

_err_invalid_option:

   defm "Invalid optio", 'n' + 0x80

_err_need_z:

   defm "Use -z for nbn:// path", 's' + 0x80

_err_need_url:

   defm "-z needs nbn://host/pat", 'h' + 0x80

_err_two_urls:

   defm "SRC and DST both nbn:/", '/' + 0x80

_err_no_memory:

   defm "4 Out of memor", 'y' + 0x80

_err_line_too_long:

   defm "Command line too lon", 'g' + 0x80

; "Mounted as drive X:", with the drive letter written in at run time

SECTION data_user

PUBLIC _err_mounted
PUBLIC _err_mounted_drive

_err_mounted:

   defm "Mounted as drive "

_err_mounted_drive:

   defm "?", ':' + 0x80


; F_OPENDIR and F_READDIR with a wildcard pattern in DE, which the z88dk
; library calls do not pass. Parameters in _wd_dir, _wd_pat, _wd_buf.

SECTION code_user

PUBLIC _wd_opendir
PUBLIC _wd_readdir

EXTERN _wd_dir, _wd_pat, _wd_buf, _wd_handle

; uint8_t wd_opendir(void): 0, or an esxDOS error code
; Mode $30: long names, only entries that match the pattern

_wd_opendir:

   push ix
   push iy
   ld hl,(_wd_dir)
   ld de,(_wd_pat)
   ld b,$30
   ld a,'*'
   rst $08
   defb $a3                    ; F_OPENDIR
   pop iy
   pop ix
   jr c, opendir_fail
   ld (_wd_handle),a
   ld l,0
   ret

opendir_fail:

   ld l,a
   ret

; uint8_t wd_readdir(void): 1 with an entry in *_wd_buf, else 0

_wd_readdir:

   push ix
   push iy
   ld a,(_wd_handle)
   ld hl,(_wd_buf)
   ld de,(_wd_pat)
   rst $08
   defb $a4                    ; F_READDIR
   pop iy
   pop ix
   ld l,a
   ret nc
   ld l,0
   ret


; DISK_FILEMAP ($85) for the handle in _fm_handle, up to 257 entries into
; the buffer at _fm_buf (must be >= $4000). On success _fm_end holds the
; address after the last entry and _fm_flags the card flags.

PUBLIC _fm_filemap

EXTERN _fm_handle, _fm_buf, _fm_end, _fm_flags

; uint8_t fm_filemap(void): 0, or an esxDOS error code

_fm_filemap:

   push ix
   push iy
   ld a,(_fm_handle)
   ld hl,(_fm_buf)
   ld de,257
   rst $08
   defb $85                    ; DISK_FILEMAP
   pop iy
   pop ix
   jr c, filemap_fail
   ld (_fm_end),hl
   ld (_fm_flags),a
   ld l,0
   ret

filemap_fail:

   ld l,a
   ret


; IDE_DOS_MAPPING ($00F7) through M_P3DOS, for drive letter _dm_drive,
; with the 256-byte description ($ff at the end) into the buffer at
; _dm_buf (must be in $4000-$BFE0)

PUBLIC _dm_mapping

EXTERN _dm_drive, _dm_buf

; uint8_t dm_mapping(void): the unit ($ff for a disk image file), or 0
; when the drive is not mapped or the call fails

_dm_mapping:

   push ix
   push iy
   ld a,(_dm_drive)
   or $80                      ; $80: the long description
   ld l,a
   ld bc,(_dm_buf)
   exx
   ld de,$00f7                 ; IDE_DOS_MAPPING
   ld c,7
   rst $08
   defb $94                    ; M_P3DOS
   pop iy
   pop ix
   ld l,0
   ret nc                      ; error
   ret z                       ; not mapped
   ld l,a
   ret
