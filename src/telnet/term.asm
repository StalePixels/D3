; TELNET: the terminal. A VT100 with the ECMA-48 colours, on the 80x32
; tilemap. telnet.h lists what it does and what it gives the rest.
;
; The screen
;
; The terminal is 80 columns by 24 rows (ROWS), the size a telnet server
; assumes when the client refuses NAWS. It sits in the middle of the
; tilemap's 32 display rows, at display rows 4 to 27 (TOP_ROW is the
; first), inside what a 60 Hz display shows. The shell's status bar is on
; the display row below it, 28 (STATUS_ROW). The tilemap clip window shows
; only those rows, pixel lines TOP_ROW * 8 to (STATUS_ROW + 1) * 8 - 1, so
; the spare map rows never show.
;
; The 32 map rows are a ring. term_base is the map row of terminal row 0,
; and terminal row r is map row (term_base + r) mod 32. NextReg $31, the
; tilemap Y offset, is (term_base - TOP_ROW) * 8, so map row term_base
; shows on display row TOP_ROW (set_y_offset). A scroll of the whole
; screen adds 1 to term_base and moves the offset by 8 lines: no row moves
; in memory, and only the new bottom row is cleared. A scroll region
; smaller than the screen copies its rows.
;
; The status bar is the map row after the terminal's last row, map row
; (term_base + ROWS) mod 32, so it shows on display row STATUS_ROW
; whatever the offset. The shell keeps its cells in tn_status. When
; term_base moves, and after a reset, status_draw copies them to the new
; map row.
;
; How a byte is handled
;
; term_state holds the address of the handler for the next byte. In the
; ground state, a printable byte goes straight to the screen (st_ground).
; ESC, ESC [ and the designators each have their own handler, which
; collects the sequence and acts on its last byte. The control codes act
; in every state, as on a VT100: an ESC or CSI sequence can have a CR or
; LF in the middle.
;
; The cursor
;
; The cursor is the reverse bit (attribute bit 0) of the cell under it.
; Only the frame interrupt sets it, in tn_term_blink. tn_term_put takes it
; off before it changes anything, so the cell under the cursor never has
; the bit left on. term_flags tells the interrupt what the terminal is
; doing:
;
;   bit 0  busy: tn_term_put is running, so the interrupt does nothing
;   bit 1  output: a byte came since the last frame interrupt
;
; The interrupt shows the cursor only after a frame with no output, and
; then turns it on and off every BLINK_FRAMES frames. So the cursor does
; not show while text comes in.
;
; Wrap
;
; A character in the last column leaves the cursor there, with
; wrap_pending set (when DECAWM is on). The next printable character goes
; to the start of the next line. Any move of the cursor clears it.

INCLUDE "telnet.inc"

SECTION code_user

PUBLIC _tn_term_init, _tn_term_put, _tn_term_blink, _tn_term_status
PUBLIC _tn_term_ckm, _tn_term_lnm

EXTERN _tn_screen_palette
EXTERN _tn_status

defc ROWS               = TN_TERM_ROWS
defc TOP_ROW            = (TN_ROWS - ROWS) / 2  ; display row of terminal row 0
defc LAST_ROW           = ROWS - 1
defc STATUS_ROW         = TOP_ROW + ROWS        ; display row of the status bar
defc LAST_COL           = TN_COLS - 1
defc ROW_BYTES          = TN_COLS * 2
defc MAP_ROWS_MASK      = TN_ROWS - 1   ; map rows wrap at 32

defc MAX_PARAMS         = 16            ; more parameters are ignored
defc BLINK_FRAMES       = 25            ; half a second at 50 Hz

defc FLAG_BUSY          = $01
defc FLAG_OUTPUT        = $02

; Character sets, for term_g0 and term_g1
defc CS_ASCII           = 0
defc CS_UK              = 1
defc CS_LINES           = 2             ; DEC special graphics: line drawing

defc NR_CLIP_TILEMAP    = $1b
defc NR_CLIP_CONTROL    = $1c
defc NR_TILEMAP_Y       = $31

; ------------------------------------------------------------ entry points

; void tn_term_init(void)
; Resets the terminal, as ESC c does. Call it once the screen is set up,
; and before tn_term_put: term_state is not set until then.

_tn_term_init:

   ld a,FLAG_BUSY | FLAG_OUTPUT
   ld (term_flags),a
   ld a,(cursor_shown)
   or a
   call nz, cursor_hide
   call term_reset
   ld a,FLAG_OUTPUT
   ld (term_flags),a
   ret

; void tn_term_put(uint8_t c) __z88dk_fastcall
; L = a byte from the remote end.

_tn_term_put:

   ld a,FLAG_BUSY | FLAG_OUTPUT
   ld (term_flags),a
   ld a,(cursor_shown)
   or a
   call nz, cursor_hide        ; keeps L
   ld a,l
   ld hl,(term_state)
   call jump_hl                ; the handler, with the byte in A
   ld a,FLAG_OUTPUT
   ld (term_flags),a
   ret

jump_hl:

   jp (hl)

; void tn_term_blink(void)
; The cursor blink. The frame interrupt calls it (tn_hook_cursor), with
; interrupts off.

_tn_term_blink:

   ld a,(term_flags)
   rrca                        ; bit 0: busy
   ret c
   rrca                        ; bit 1: output since the last frame
   jr nc, blink_quiet
   xor a
   ld (term_flags),a
   inc a
   ld (blink_count),a          ; show it at the first quiet frame
   ret

blink_quiet:

   ld hl,blink_count
   dec (hl)
   ret nz
   ld (hl),BLINK_FRAMES
   ld a,(cursor_shown)
   or a
   jr nz, cursor_hide
   ld a,(mode_tcem)
   or a
   ret z
   ld hl,(cur_ptr)
   ld (cursor_cell),hl
   inc hl
   ld a,(hl)
   xor TN_ATTR_REVERSE
   ld (hl),a
   ld a,1
   ld (cursor_shown),a
   ret

; Take the cursor off the screen. Uses af, de.

cursor_hide:

   xor a
   ld (cursor_shown),a
   ld de,(cursor_cell)
   inc de
   ld a,(de)
   xor TN_ATTR_REVERSE
   ld (de),a
   ret

; ------------------------------------------------------------ ground state

; Printable bytes go on the screen. Bytes 32-126 are ASCII, or another
; character set (translate); 128-255 show as their CP437 glyph. DEL is
; ignored.

st_ground:

   cp ' '
   jr c, do_control
   cp $7f
   ret z

   ld e,a
   ld a,(cs_active)
   or a
   call nz, translate
   ld a,(wrap_pending)
   or a
   jr z, print_cell
   push de
   call wrap_line
   pop de

print_cell:

   ld hl,(cur_ptr)
   ld (hl),e
   inc hl
   ld a,(cur_attr)
   ld (hl),a
   ld a,(cur_col)
   cp LAST_COL
   jr z, print_edge
   inc a
   ld (cur_col),a
   inc hl
   ld (cur_ptr),hl
   ret

print_edge:

   ld a,(mode_awm)             ; the cursor stays in the last column
   ld (wrap_pending),a
   ret

; The deferred wrap: the cursor goes to the start of the next line.

wrap_line:

   xor a
   ld (cur_col),a
   call index
   jp cursor_update

; E = a byte of 32-126 -> E = the glyph in the active character set.
; A = cs_active, not CS_ASCII.

translate:

   cp CS_LINES
   jr z, translate_lines
   ld a,e                      ; the UK set has a pound sign for #
   cp '#'
   ret nz
   ld e,$9c
   ret

translate_lines:

   ld a,e
   sub $5f
   ret c
   cp 32
   ret nc
   ld hl,line_glyphs
   add hl,a
   ld e,(hl)
   ret

; ------------------------------------------------------------ control codes

; A = 0-31. The control codes act in every state.

do_control:

   ld hl,control_table
   add a,a
   add hl,a
   ld a,(hl)
   inc hl
   ld h,(hl)
   ld l,a
   jp (hl)

ctl_bs:

   ld a,(cur_col)
   or a
   jr z, ctl_bs_set
   dec a

ctl_bs_set:

   ld (cur_col),a
   jp cursor_update

; HT: to the next tab stop, or the last column

ctl_ht:

   ld a,(cur_col)
   ld hl,tab_stops
   add hl,a

ctl_ht_next:

   cp LAST_COL
   jr z, ctl_ht_set
   inc a
   inc hl
   bit 0,(hl)
   jr z, ctl_ht_next

ctl_ht_set:

   ld (cur_col),a
   jp cursor_update

; LF, VT and FF: a new line. With LNM set, also a carriage return.

ctl_lf:

   ld a,(mode_lnm)
   or a
   jr z, ctl_lf_index
   xor a
   ld (cur_col),a

ctl_lf_index:

   call index
   jp cursor_update

ctl_cr:

   xor a
   ld (cur_col),a
   jp cursor_update

ctl_so:

   ld a,1                      ; G1
   jr set_gl

ctl_si:

   xor a                       ; G0

set_gl:

   ld (cur_gl),a

; Make cs_active the character set that GL shows

charset_update:

   ld hl,cur_g0
   ld a,(cur_gl)
   add hl,a
   ld a,(hl)
   ld (cs_active),a
   ret

; CAN and SUB end a sequence. ESC starts one.

ctl_can:

   ld hl,st_ground
   ld (term_state),hl
   ret

ctl_esc:

   ld hl,st_esc
   ld (term_state),hl
   ret

ctl_none:

   ret

; ------------------------------------------------------------ cursor and rows

; Set cur_ptr from cur_row and cur_col, and clear wrap_pending. Every
; move of the cursor ends here.

cursor_update:

   xor a
   ld (wrap_pending),a
   ld a,(cur_row)
   call row_addr
   ld a,(cur_col)
   add a,a
   add hl,a
   ld (cur_ptr),hl
   ret

; A = terminal row, 0-31 -> HL = address of its first cell.
; Uses af, de.

row_addr:

   ld hl,term_base
   add a,(hl)
   and MAP_ROWS_MASK
   ld d,a
   ld e,ROW_BYTES
   mul d,e
   ld hl,TN_MAP
   add hl,de
   ret

; IND: the cursor goes down a row. At the bottom margin the scroll region
; scrolls up instead; below it, the cursor stops at the last row.
; The caller sets cur_ptr after (cursor_update).

index:

   ld a,(cur_row)
   ld hl,margin_bot
   cp (hl)
   jr z, scroll_up
   cp LAST_ROW
   ret z
   inc a
   ld (cur_row),a
   ret

; RI: the cursor goes up a row, or the scroll region scrolls down at the
; top margin

reverse_index:

   ld a,(cur_row)
   ld hl,margin_top
   cp (hl)
   jr z, scroll_down
   or a
   ret z
   dec a
   ld (cur_row),a
   ret

; Scroll the region up a row, and clear its bottom row

scroll_up:

   call region_is_screen
   jr nz, scroll_up_copy

   ld a,(term_base)            ; the whole screen: move the ring
   inc a
   and MAP_ROWS_MASK
   ld (term_base),a
   call status_draw            ; in the hidden row below the old status bar
   call set_y_offset
   ld a,LAST_ROW
   jp erase_row                ; the old status bar's row

scroll_up_copy:

   ld a,(margin_top)

scroll_up_row:

   ld hl,margin_bot
   cp (hl)
   jp z, erase_row
   ld c,a
   call row_addr               ; the row
   push hl
   ld a,c
   inc a
   call row_addr               ; the row below it
   pop de
   push bc
   ld bc,ROW_BYTES
   ldir
   pop bc
   ld a,c
   inc a
   jr scroll_up_row

; Scroll the region down a row, and clear its top row

scroll_down:

   call region_is_screen
   jr nz, scroll_down_copy

   ld a,(term_base)
   dec a
   and MAP_ROWS_MASK
   ld (term_base),a
   call status_draw            ; over the old last row, which scrolls off
   call set_y_offset
   xor a
   jp erase_row

scroll_down_copy:

   ld a,(margin_bot)

scroll_down_row:

   ld hl,margin_top
   cp (hl)
   jp z, erase_row
   ld c,a
   call row_addr
   push hl
   ld a,c
   dec a
   call row_addr               ; the row above it
   pop de
   push bc
   ld bc,ROW_BYTES
   ldir
   pop bc
   ld a,c
   dec a
   jr scroll_down_row

; Z when the scroll region is the whole screen

region_is_screen:

   ld a,(margin_top)
   or a
   ret nz
   ld a,(margin_bot)
   cp LAST_ROW
   ret

; void tn_term_status(void)
; status_draw: copies tn_status to the status bar's map row,
; (term_base + ROWS) mod 32. Uses af, bc, de, hl.

_tn_term_status:
status_draw:

   ld a,ROWS
   call row_addr
   ex de,hl
   ld hl,_tn_status
   ld bc,ROW_BYTES
   ldir
   ret

; NextReg $31 from term_base: (term_base - TOP_ROW) * 8, so map row
; term_base shows on display row TOP_ROW

set_y_offset:

   ld a,(term_base)
   sub TOP_ROW
   and MAP_ROWS_MASK
   add a,a
   add a,a
   add a,a                     ; 8 lines a row
   nextreg NR_TILEMAP_Y,a
   ret

; ------------------------------------------------------------ erase

; Erased cells get a space, with the current ink and paper and reverse
; off.

; A = row: erase all of it

erase_row:

   call row_addr
   ld b,TN_COLS

; HL = first cell, B = number of cells (1-80)

erase_cells:

   ld a,(cur_attr)
   and $ff - TN_ATTR_REVERSE
   ld c,a
   ld a,' '

erase_next:

   ld (hl),a
   inc hl
   ld (hl),c
   inc hl
   djnz erase_next
   ret

; Erase rows A to C - 1

erase_rows:

   cp c
   ret nc
   push af
   push bc
   call erase_row
   pop bc
   pop af
   inc a
   jp erase_rows

; ------------------------------------------------------------ escape

st_esc:

   cp ' '
   jp c, do_control
   cp $7f
   ret z
   ld hl,st_ground             ; most sequences end here
   ld (term_state),hl
   cp $30
   jr c, esc_intermediate
   ld hl,esc_finals
   jp find_handler

; ESC followed by 20-2F: what comes next depends on it

esc_intermediate:

   ld hl,st_g0
   cp '('
   jr z, esc_next_state
   ld hl,st_g1
   cp ')'
   jr z, esc_next_state
   ld hl,st_hash
   cp '#'
   jr z, esc_next_state
   ld hl,st_esc_ignore         ; not one of ours: skip to its last byte

esc_next_state:

   ld (term_state),hl
   ret

st_esc_ignore:

   cp ' '
   jp c, do_control
   cp $30
   ret c
   cp $7f
   ret z
   jp ctl_can                  ; its last byte

; ESC ( and ESC ): the character set for G0 or G1

st_g0:

   ld hl,cur_g0
   jr designate

st_g1:

   ld hl,cur_g1

designate:

   cp ' '
   jp c, do_control
   ld c,CS_ASCII
   cp 'B'
   jr z, designate_set
   ld c,CS_UK
   cp 'A'
   jr z, designate_set
   ld c,CS_LINES
   cp '0'
   jr nz, designate_done

designate_set:

   ld (hl),c
   call charset_update

designate_done:

   jp ctl_can

; ESC #: only ESC # 8 (DECALN)

st_hash:

   cp ' '
   jp c, do_control
   cp '8'
   call z, esc_decaln
   jp ctl_can

; ESC D: IND

esc_ind:

   call index
   jp cursor_update

; ESC M: RI

esc_ri:

   call reverse_index
   jp cursor_update

; ESC E: NEL, a carriage return and a line feed

esc_nel:

   xor a
   ld (cur_col),a
   jr esc_ind

; ESC H: HTS, a tab stop at the cursor column

esc_hts:

   ld a,(cur_col)
   ld hl,tab_stops
   add hl,a
   ld (hl),1
   ret

; ESC 7: DECSC, keep the cursor, the rendition, the character sets and
; origin mode

esc_decsc:

   ld hl,saved_from
   ld de,saved
   ld bc,SAVED_SIZE
   ldir
   ret

; ESC 8: DECRC, put them back

esc_decrc:

   ld hl,saved
   ld de,saved_from
   ld bc,SAVED_SIZE
   ldir
   call charset_update
   jp cursor_update

; ESC c: RIS, reset the terminal

esc_ris:

   jp term_reset

; ESC # 8: DECALN, the screen full of E, the margins reset, the cursor home

esc_decaln:

   call margins_reset
   xor a

decaln_row:

   push af
   call row_addr
   ld b,TN_COLS
   ld c,'E'

decaln_cell:

   ld (hl),c
   inc hl
   ld (hl),TN_ATTR_DEFAULT
   inc hl
   djnz decaln_cell
   pop af
   inc a
   cp ROWS
   jr c, decaln_row

   xor a
   ld (cur_row),a
   ld (cur_col),a
   jp cursor_update

; ------------------------------------------------------------ CSI

; ESC [: clear the parameters and collect a sequence

esc_csi:

   xor a
   ld (csi_private),a
   ld (param_index),a
   ld hl,params
   ld b,MAX_PARAMS

esc_csi_clear:

   ld (hl),a
   inc hl
   djnz esc_csi_clear
   ld hl,st_csi_entry
   ld (term_state),hl
   ret

; The first byte after ESC [ can be a private marker, 3C-3F

st_csi_entry:

   cp $3c
   jr c, st_csi_param
   cp $40
   jr nc, st_csi_param
   ld (csi_private),a
   ld hl,st_csi_param
   ld (term_state),hl
   ret

; Parameters: numbers 0-255 (larger ones count as 255), split by ;

st_csi_param:

   cp ' '
   jp c, do_control
   cp $30
   jr c, csi_ignore            ; intermediates: none of ours has one
   cp ':'
   jr c, csi_digit
   jr z, csi_ignore            ; sub-parameters: not VT100
   cp ';'
   jr z, csi_next_param
   cp $40
   jr c, csi_ignore            ; a private marker after a parameter
   cp $7f
   ret z
   ld hl,st_ground             ; the last byte
   ld (term_state),hl
   jr csi_dispatch

csi_ignore:

   ld hl,st_csi_ignore
   ld (term_state),hl
   ret

csi_digit:

   sub '0'
   ld c,a
   ld a,(param_index)
   cp MAX_PARAMS
   ret nc
   ld hl,params
   add hl,a
   ld d,(hl)
   ld e,10
   mul d,e
   ld a,c
   add de,a
   ld a,d
   or a
   jr z, csi_digit_set
   ld e,255

csi_digit_set:

   ld (hl),e
   ret

csi_next_param:

   ld hl,param_index
   ld a,(hl)
   cp MAX_PARAMS
   ret nc
   inc (hl)
   ret

; Skip to the last byte of a sequence that is not ours

st_csi_ignore:

   cp ' '
   jp c, do_control
   cp $40
   ret c
   cp $7f
   ret z
   jp ctl_can

; A = the last byte of the sequence

csi_dispatch:

   ld c,a
   ld a,(csi_private)
   or a
   jr nz, csi_dispatch_private
   ld a,c
   ld hl,csi_finals
   jr find_handler

csi_dispatch_private:

   cp '?'                      ; only DECSET and DECRST
   ret nz
   ld a,c
   cp 'h'
   jp z, csi_decset
   cp 'l'
   jp z, csi_decrst
   ret

; A = a byte, HL = a table of entries: the byte, then the handler's
; address, and 0 at the end. Jumps to the handler for A.

find_handler:

   ld c,a

find_next:

   ld a,(hl)
   or a
   ret z
   inc hl
   cp c
   jr z, find_found
   inc hl
   inc hl
   jr find_next

find_found:

   ld a,(hl)
   inc hl
   ld h,(hl)
   ld l,a
   jp (hl)

; A = parameter number -> A = its value, 0 when not given

param:

   ld hl,params
   add hl,a
   ld a,(hl)
   ret

; A = parameter number -> A = its value, or 1 for 0 and not given

param_or_1:

   call param
   or a
   ret nz
   inc a
   ret

; -> B = the number of parameters, 1-MAX_PARAMS

param_count:

   ld a,(param_index)
   inc a
   cp MAX_PARAMS + 1
   jr c, param_count_set
   ld a,MAX_PARAMS

param_count_set:

   ld b,a
   ret

; CUU: up, stopping at the top margin, or at row 0 above it

csi_cuu:

   xor a
   call param_or_1
   ld c,a
   ld a,(margin_top)
   ld b,a
   ld a,(cur_row)
   cp b
   jr nc, cuu_move
   ld b,0

cuu_move:

   sub c
   jr c, cuu_limit
   cp b
   jr nc, set_row

cuu_limit:

   ld a,b

set_row:

   ld (cur_row),a
   jp cursor_update

; CUD: down, stopping at the bottom margin, or at the last row below it

csi_cud:

   xor a
   call param_or_1
   ld c,a
   ld a,(margin_bot)
   ld b,a
   ld a,(cur_row)
   cp b
   jr c, cud_move
   jr z, cud_move
   ld b,LAST_ROW

cud_move:

   add a,c
   jr c, cud_limit
   cp b
   jr c, set_row

cud_limit:

   ld a,b
   jr set_row

; CUF: right, stopping at the last column

csi_cuf:

   xor a
   call param_or_1
   ld c,a
   ld a,(cur_col)
   add a,c
   jr c, cuf_limit
   cp LAST_COL + 1
   jr c, set_col

cuf_limit:

   ld a,LAST_COL

set_col:

   ld (cur_col),a
   jp cursor_update

; CUB: left, stopping at column 0

csi_cub:

   xor a
   call param_or_1
   ld c,a
   ld a,(cur_col)
   sub c
   jr nc, set_col
   xor a
   jr set_col

; CUP and HVP: row;column, from 1. With origin mode, the row counts from
; the top margin and stops at the bottom margin.

csi_cup:

   xor a
   call param_or_1
   dec a
   cp ROWS
   jr c, cup_row
   ld a,LAST_ROW

cup_row:

   ld c,a
   ld a,(mode_om)
   or a
   ld a,c
   jr z, cup_col
   ld hl,margin_top
   add a,(hl)
   ld hl,margin_bot
   cp (hl)
   jr c, cup_col
   ld a,(hl)

cup_col:

   ld (cur_row),a
   ld a,1
   call param_or_1
   dec a
   cp TN_COLS
   jr c, set_col
   ld a,LAST_COL
   jr set_col

; Home: row 0, or the top margin in origin mode, and column 0

cursor_home:

   xor a
   ld (cur_col),a
   ld a,(mode_om)
   or a
   jp z, set_row
   ld a,(margin_top)
   jp set_row

; ED: 0 cursor to the end of the screen, 1 start of the screen to the
; cursor, 2 all the screen

csi_ed:

   xor a
   call param
   or a
   jr z, ed_below
   dec a
   jr z, ed_above
   dec a
   ret nz
   ld c,ROWS
   jp erase_rows               ; A = 0

ed_below:

   call el_right
   ld a,(cur_row)
   inc a
   ld c,ROWS
   jp erase_rows

ed_above:

   ld a,(cur_row)
   ld c,a
   xor a
   call erase_rows
   jr el_left

; EL: 0 cursor to the end of the line, 1 start of the line to the cursor,
; 2 all the line

csi_el:

   xor a
   call param
   or a
   jr z, el_right
   dec a
   jr z, el_left
   dec a
   ret nz
   ld a,(cur_row)
   jp erase_row

el_right:

   ld hl,(cur_ptr)
   ld a,(cur_col)
   neg
   add a,TN_COLS
   ld b,a
   jp erase_cells

el_left:

   ld a,(cur_row)
   call row_addr
   ld a,(cur_col)
   inc a
   ld b,a
   jp erase_cells

; SGR: the rendition

csi_sgr:

   call param_count
   ld hl,params

sgr_next:                      ; B = parameters left, HL = the next one

   ld a,b
   or a
   ret z
   dec b
   ld a,(hl)
   inc hl
   cp 38
   jr z, sgr_extended
   cp 48
   jr z, sgr_extended
   push hl
   push bc
   call sgr_one
   pop bc
   pop hl
   jr sgr_next

; 38 and 48 set colours a VT100 does not have: 5;n or 2;r;g;b. Skip them.

sgr_extended:

   ld a,b
   or a
   ret z
   dec b
   ld a,(hl)
   inc hl
   ld c,1
   cp 5
   jr z, sgr_skip
   ld c,3
   cp 2
   jr nz, sgr_next

sgr_skip:

   ld a,b
   or a
   ret z
   dec b
   inc hl
   dec c
   jr nz, sgr_skip
   jr sgr_next

; A = one SGR parameter

sgr_one:

   ld hl,cur_attr
   or a
   jr z, sgr_reset
   cp 1
   jr z, sgr_bold
   cp 7
   jr z, sgr_reverse
   cp 22
   jr z, sgr_normal
   cp 27
   jr z, sgr_positive
   cp 39
   jr z, sgr_ink_default
   cp 49
   jr z, sgr_paper_default
   cp 30
   ret c
   cp 38
   jr c, sgr_ink
   cp 40
   ret c
   cp 48
   ret nc

   sub 40                      ; 40-47: paper
   swapnib
   ld c,a
   ld a,(hl)
   and $8f
   or c
   ld (hl),a
   ret

sgr_ink:

   sub 30                      ; 30-37: ink
   add a,a
   ld c,a
   ld a,(hl)
   and $f1
   or c
   ld (hl),a
   ret

sgr_reset:

   ld (hl),TN_ATTR_DEFAULT
   ret

sgr_bold:

   set 7,(hl)
   ret

sgr_normal:

   res 7,(hl)
   ret

sgr_reverse:

   set 0,(hl)
   ret

sgr_positive:

   res 0,(hl)
   ret

sgr_ink_default:

   ld a,(hl)
   or $0e                      ; white
   ld (hl),a
   ret

sgr_paper_default:

   ld a,(hl)
   and $8f                     ; black
   ld (hl),a
   ret

; DECSTBM: the scroll region, top;bottom from 1. The cursor goes home.

csi_stbm:

   xor a
   call param_or_1
   dec a
   ld c,a                      ; top
   ld a,1
   call param
   or a
   jr z, stbm_bottom
   cp ROWS + 1
   jr c, stbm_set

stbm_bottom:

   ld a,ROWS

stbm_set:

   dec a                       ; bottom
   cp c
   ret c                       ; the region needs at least two rows
   ret z
   ld (margin_bot),a
   ld a,c
   ld (margin_top),a
   jp cursor_home

margins_reset:

   xor a
   ld (margin_top),a
   ld a,LAST_ROW
   ld (margin_bot),a
   ret

; TBC: 0 clear the tab stop at the cursor, 3 clear them all

csi_tbc:

   xor a
   call param
   or a
   jr z, tbc_here
   cp 3
   ret nz
   ld hl,tab_stops
   ld b,TN_COLS

tbc_all:

   ld (hl),0
   inc hl
   djnz tbc_all
   ret

tbc_here:

   ld a,(cur_col)
   ld hl,tab_stops
   add hl,a
   ld (hl),0
   ret

; SM and RM: only 20, LNM

csi_sm:

   ld e,1
   jr sm_rm

csi_rm:

   ld e,0

sm_rm:

   call param_count
   ld hl,params

sm_rm_next:

   ld a,(hl)
   inc hl
   cp 20
   jr nz, sm_rm_skip
   ld a,e
   ld (mode_lnm),a

sm_rm_skip:

   djnz sm_rm_next
   ret

; DECSET and DECRST (ESC [ ? ... h and l)

csi_decset:

   ld e,1
   jr decset_decrst

csi_decrst:

   ld e,0

decset_decrst:

   call param_count
   ld hl,params

decset_next:

   ld a,(hl)
   inc hl
   push hl
   push bc
   push de
   call decset_one
   pop de
   pop bc
   pop hl
   djnz decset_next
   ret

; A = mode, E = 1 to set or 0 to reset

decset_one:

   cp 1
   jr z, set_ckm
   cp 5
   jr z, set_scnm
   cp 6
   jr z, set_om
   cp 7
   jr z, set_awm
   cp 25
   ret nz
   ld a,e
   ld (mode_tcem),a
   ret

set_ckm:

   ld a,e
   ld (_tn_term_ckm),a
   ret

set_awm:

   ld a,e
   ld (mode_awm),a
   ret

set_om:

   ld a,e
   ld (mode_om),a
   jp cursor_home

; DECSCNM: the whole screen in reverse, by swapping ink and paper in the
; palette

set_scnm:

   ld a,(mode_scnm)
   cp e
   ret z
   ld a,e
   ld (mode_scnm),a
   ld l,a
   jp _tn_screen_palette

; DSR: 5 the terminal is fine, 6 where the cursor is

csi_dsr:

   xor a
   call param
   cp 5
   jr z, dsr_status
   cp 6
   ret nz

   ld a,(cur_row)              ; ESC [ row ; column R, from 1
   ld c,a
   ld a,(mode_om)
   or a
   jr z, dsr_row
   ld a,(margin_top)

dsr_row:

   neg
   add a,c
   inc a
   push af
   call send_csi
   pop af
   call send_number
   ld a,';'
   call send_byte
   ld a,(cur_col)
   inc a
   call send_number
   ld a,'R'
   jr send_byte

dsr_status:

   ld bc,reply_ok
   jr send_string

; DA: what the terminal is: a VT100 with no options

csi_da:

   xor a
   call param
   or a
   ret nz
   ld bc,reply_da
   jr send_string

; Replies go into the key queue, which the main loop sends to the remote
; end.

send_csi:

   ld a,$1b
   call send_byte
   ld a,'['

; A = byte

send_byte:

   push bc
   ld l,a
   call _tn_keyq_push
   pop bc
   ret

; A = 0-99, in decimal

send_number:

   ld b,'0'

send_tens:

   cp 10
   jr c, send_ones
   sub 10
   inc b
   jr send_tens

send_ones:

   ld c,a
   ld a,b
   cp '0'
   call nz, send_byte
   ld a,c
   add a,'0'
   jr send_byte

; BC = a string that ends with 0

send_string:

   ld a,(bc)
   or a
   ret z
   call send_byte
   inc bc
   jr send_string

; ------------------------------------------------------------ reset

; The power-on state: the screen clear, every mode at its default, tab
; stops every 8 columns.

term_reset:

   ld hl,st_ground
   ld (term_state),hl

   xor a
   ld (cur_g0),a
   ld (cur_g1),a
   ld (cur_gl),a
   ld (cs_active),a
   ld (mode_om),a
   ld (mode_lnm),a
   ld (_tn_term_ckm),a
   ld a,TN_ATTR_DEFAULT
   ld (cur_attr),a
   ld a,1
   ld (mode_awm),a
   ld (mode_tcem),a
   ld (blink_count),a

   ld e,0                      ; DECSCNM off
   call set_scnm
   call margins_reset

   ld hl,tab_stops
   ld b,TN_COLS
   ld c,0

reset_tab:

   ld a,c
   and 7
   ld a,0
   jr nz, reset_tab_set
   inc a

reset_tab_set:

   ld (hl),a
   inc hl
   inc c
   djnz reset_tab
   ld hl,tab_stops             ; no stop at column 0
   ld (hl),0

   ld a,TOP_ROW                ; terminal row 0 in map row TOP_ROW
   ld (term_base),a
   call set_y_offset

   nextreg NR_CLIP_CONTROL,$08 ; the clip window shows the terminal rows
   nextreg NR_CLIP_TILEMAP,0   ; and the status bar only
   nextreg NR_CLIP_TILEMAP,159
   nextreg NR_CLIP_TILEMAP,TOP_ROW * 8
   nextreg NR_CLIP_TILEMAP,(STATUS_ROW + 1) * 8 - 1

   xor a
   ld c,TN_ROWS
   call erase_rows             ; the hidden rows too
   call status_draw

   xor a
   ld (cur_row),a
   ld (cur_col),a
   call cursor_update
   jp esc_decsc                ; ESC 8 with nothing kept goes home

; ------------------------------------------------------------ tables

SECTION rodata_user

; Control codes 0-31

control_table:

   defw ctl_none, ctl_none, ctl_none, ctl_none     ; NUL SOH STX ETX
   defw ctl_none, ctl_none, ctl_none, ctl_none     ; EOT ENQ ACK BEL
   defw ctl_bs,   ctl_ht,   ctl_lf,   ctl_lf       ; BS  HT  LF  VT
   defw ctl_lf,   ctl_cr,   ctl_so,   ctl_si       ; FF  CR  SO  SI
   defw ctl_none, ctl_none, ctl_none, ctl_none     ; DLE DC1 DC2 DC3
   defw ctl_none, ctl_none, ctl_none, ctl_none     ; DC4 NAK SYN ETB
   defw ctl_can,  ctl_none, ctl_can,  ctl_esc      ; CAN EM  SUB ESC
   defw ctl_none, ctl_none, ctl_none, ctl_none     ; FS  GS  RS  US

; ESC and a last byte

esc_finals:

   defb 'D'
   defw esc_ind
   defb 'M'
   defw esc_ri
   defb 'E'
   defw esc_nel
   defb 'H'
   defw esc_hts
   defb '7'
   defw esc_decsc
   defb '8'
   defw esc_decrc
   defb 'c'
   defw esc_ris
   defb '['
   defw esc_csi
   defb 0

; ESC [ and a last byte

csi_finals:

   defb 'A'
   defw csi_cuu
   defb 'B'
   defw csi_cud
   defb 'C'
   defw csi_cuf
   defb 'D'
   defw csi_cub
   defb 'H'
   defw csi_cup
   defb 'f'
   defw csi_cup
   defb 'J'
   defw csi_ed
   defb 'K'
   defw csi_el
   defb 'm'
   defw csi_sgr
   defb 'r'
   defw csi_stbm
   defb 'g'
   defw csi_tbc
   defb 'h'
   defw csi_sm
   defb 'l'
   defw csi_rm
   defb 'n'
   defw csi_dsr
   defb 'c'
   defw csi_da
   defb 0

; DEC special graphics, bytes $5F-$7E, as CP437 glyphs. Where CP437 has no
; such glyph the byte shows as itself: the control pictures (b c d e h i)
; and not-equal (|). The scan lines 1, 3, 7 and 9 show as scan line 5, a
; horizontal line, and the diamond as a small square.

line_glyphs:

   defb $20, $fe, $b1, 'b', 'c', 'd', 'e', $f8     ; _ diamond ▒ HT FF CR LF °
   defb $f1, 'h', 'i', $d9, $bf, $da, $c0, $c5     ; ± NL VT ┘ ┐ ┌ └ ┼
   defb $c4, $c4, $c4, $c4, $c4, $c3, $b4, $c1     ; scan 1-9 ├ ┤ ┴
   defb $c2, $b3, $f3, $f2, $e3, '|', $9c, $fa     ; ┬ │ ≤ ≥ π ≠ £ ·

reply_ok:

   defb $1b, "[0n", 0

reply_da:

   defb $1b, "[?1;0c", 0

; ------------------------------------------------------------ state

SECTION bss_user

term_state:         defw 0      ; the handler for the next byte
_tn_term_ckm:       defb 0      ; DECCKM: 1 when cursor keys send ESC O x
_tn_term_lnm:                   ; LNM: 1 when LF also returns the carriage
mode_lnm:           defb 0

term_flags:         defb 0      ; FLAG_BUSY, FLAG_OUTPUT
blink_count:        defb 0      ; frames to the next blink
cursor_shown:       defb 0      ; 1 while the cursor is on the screen
cursor_cell:        defw 0      ; the cell it is on

term_base:          defb 0      ; the map row of terminal row 0
cur_ptr:            defw 0      ; the cell under the cursor
wrap_pending:       defb 0
cs_active:          defb 0      ; the character set GL shows

margin_top:         defb 0      ; the scroll region, rows 0-23
margin_bot:         defb 0

mode_awm:           defb 0      ; DECAWM: wrap at the last column
mode_tcem:          defb 0      ; DECTCEM: the cursor shows
mode_scnm:          defb 0      ; DECSCNM: the screen in reverse

; What DECSC keeps, in this order

saved_from:
cur_row:            defb 0
cur_col:            defb 0
cur_attr:           defb 0      ; the attribute for new characters
cur_g0:             defb 0      ; CS_ASCII, CS_UK or CS_LINES
cur_g1:             defb 0
cur_gl:             defb 0      ; 0 G0, 1 G1
mode_om:            defb 0      ; DECOM: rows count from the top margin
defc SAVED_SIZE     = ASMPC - saved_from

saved:              defs SAVED_SIZE

csi_private:        defb 0      ; the private marker, or 0
param_index:        defb 0      ; the parameter being read
params:             defs MAX_PARAMS

tab_stops:          defs TN_COLS ; 1 at a column with a tab stop
