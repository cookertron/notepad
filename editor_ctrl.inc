; ============================================================================
; editor_ctrl.inc - Editor Control (CTYPE_EDITOR) for TUI Framework
;
; Provides paint and key handler for the text editor control.
; Renders gap buffer content into a TUI window, handles keyboard input.
;
; Requires: editor_const.inc, editor_gap.inc included before this file.
; ============================================================================

; --- Scratch variables for paint ---
ed_paint_origdl:   DB 0        ; origin column
ed_paint_origdh:   DB 0        ; origin row
ed_paint_width:    DB 0        ; control width
ed_paint_height:   DB 0        ; control height
ed_paint_focused:  DB 0        ; focused flag
ed_paint_curline:  DB 0        ; 1 if current row is cursor line & focused
ed_paint_line:     DW 0        ; current line number during rendering
ed_numbuf:         DB 0,0,0,0,0,0  ; 6 bytes for decimal number formatting
ed_paint_offset:   DW 0        ; logical offset tracker during paint
ed_sel_start:      DW 0        ; precomputed selection start (min of anchor/cursor)
ed_sel_end:        DW 0        ; precomputed selection end (max of anchor/cursor)
ed_shift_held:     DB 0        ; cached shift state (nonzero if Shift pressed)
ed_mouse_absrow:   DB 0        ; control absolute Y (cached on press for drag)
ed_mouse_abscol:   DB 0        ; control absolute X
ed_mouse_oldcur:   DW 0        ; old cursor offset before mouse press
ed_needs_vbar:     DB 0        ; 1 if vertical scroll bar needed
ed_paint_vcol:     DW 0        ; visual column tracker during paint
ed_cursor_offset:  DW 0        ; precomputed cursor logical offset
ed_clip_digits:    DB 0        ; digit count scratch for clip free
ed_undo_digits:    DB 0        ; digit count scratch for undo free
ed_gap_digits:     DB 0        ; digit count scratch for gap free

; Status bar strings
str_status_ln:     DB ' Ln ', 0
str_status_col:    DB ', Col ', 0
str_status_mod:    DB '  Modified', 0
str_status_clip:   DB 'Clip:', 0
str_status_undo:   DB ' Undo:', 0
str_status_free:   DB ' free', 0

; ----------------------------------------------------------------------------
; _ed_format_number - Format 16-bit unsigned number to decimal string
; Input:  AX = number
; Output: DI = pointer to first digit in ed_numbuf (null-terminated)
; Clobbers: AX
; Preserves: BX, CX, DX
; ----------------------------------------------------------------------------
_ed_format_number: PROC
    PUSH BX
    PUSH CX
    PUSH DX
    MOV DI, ed_numbuf
    ADD DI, 5
    MOV BYTE [DI], 0
    MOV BX, 10
.divide:
    DEC DI
    XOR DX, DX
    DIV BX
    ADD DL, '0'
    MOV [DI], DL
    TEST AX, AX
    JNZ .divide
    POP DX
    POP CX
    POP BX
    RET
ENDP

; ----------------------------------------------------------------------------
; _ed_paint_string - Paint a null-terminated string at screen position
; Input:  DI = string, AH = attribute, DH/DL = screen position
; Output: DL advanced past string, DI past null
; Preserves: AH, BX, CX, SI
; ----------------------------------------------------------------------------
_ed_paint_string: PROC
.loop:
    MOV AL, [DI]
    TEST AL, AL
    JZ .done
    CALL tui_putchar
    INC DL
    INC DI
    JMP .loop
.done:
    RET
ENDP

; ----------------------------------------------------------------------------
; _ed_check_shift - Read BIOS keyboard flags, cache shift state
; Only works in DOSBox-X (INT 16h unsupported in agent86)
; Output: ed_shift_held set to nonzero if Shift held
; ----------------------------------------------------------------------------
_ed_check_shift: PROC
    PUSH AX
    MOV AH, 02h
    XOR AL, AL
    INT 16h
    AND AL, 03h
    MOV [ed_shift_held], AL
    POP AX
    RET
ENDP

; ----------------------------------------------------------------------------
; _ed_phys_to_logical - Convert physical pointer to logical offset
; Input:  SI = editor state, DI = physical pointer (not inside gap)
; Output: AX = logical offset
; Preserves: all except AX
; ----------------------------------------------------------------------------
_ed_phys_to_logical: PROC
    CMP DI, [SI + ED_GAPSTART]
    JBE .before_gap
    ; DI >= GAPEND: offset = (DI - GAPEND) + (GAPSTART - BUF)
    MOV AX, DI
    SUB AX, [SI + ED_GAPEND]
    ADD AX, [SI + ED_GAPSTART]
    SUB AX, [SI + ED_BUF]
    RET
.before_gap:
    MOV AX, DI
    SUB AX, [SI + ED_BUF]
    RET
ENDP

; ----------------------------------------------------------------------------
; _ed_sel_delete - Delete the selected region and clear selection
; Input:  SI = editor state (assumes ED_SELANCHOR != 0FFFFh)
; Preserves: AX
; ----------------------------------------------------------------------------
_ed_sel_delete: PROC
    PUSH AX
    PUSH BX
    PUSH CX
    PUSH DI

    ; Get cursor offset and anchor
    CALL gap_cursor_offset          ; AX = cursor logical offset
    MOV BX, [SI + ED_SELANCHOR]

    CMP AX, BX
    JE .nothing                     ; same position, nothing to delete
    JA .cursor_after

    ; --- cursor < anchor: selected text is after gap ---
    MOV CX, BX
    SUB CX, AX                     ; CX = chars to delete
    ; Count newlines in [GAPEND .. GAPEND+CX)
    MOV DI, [SI + ED_GAPEND]
    PUSH CX
    XOR BX, BX
.count_after:
    TEST CX, CX
    JZ .count_after_done
    CMP BYTE [DI], 0Ah
    JNE .no_nl_after
    INC BX
.no_nl_after:
    INC DI
    DEC CX
    JMP .count_after
.count_after_done:
    POP CX
    ; Delete: advance GAPEND by CX
    ADD [SI + ED_GAPEND], CX
    ; Adjust LINECOUNT
    SUB [SI + ED_LINECOUNT], BX
    JMP .fixup

.cursor_after:
    ; --- cursor > anchor: selected text is before gap ---
    MOV CX, AX
    SUB CX, BX                     ; CX = chars to delete
    ; Count newlines in [GAPSTART-CX .. GAPSTART)
    MOV DI, [SI + ED_GAPSTART]
    SUB DI, CX
    PUSH CX
    XOR BX, BX
.count_before:
    TEST CX, CX
    JZ .count_before_done
    CMP BYTE [DI], 0Ah
    JNE .no_nl_before
    INC BX
.no_nl_before:
    INC DI
    DEC CX
    JMP .count_before
.count_before_done:
    POP CX
    ; Delete: move GAPSTART back by CX
    SUB [SI + ED_GAPSTART], CX
    ; Adjust LINECOUNT
    SUB [SI + ED_LINECOUNT], BX

.fixup:
    ; Recalculate CURLINE and CURCOL
    CALL _gap_recalc_line
    CALL _gap_recalc_col
    ; Clear selection and set modified
    MOV WORD [SI + ED_SELANCHOR], 0FFFFh
    OR BYTE [SI + ED_FLAGS], EDF_MODIFIED

.nothing:
    POP DI
    POP CX
    POP BX
    POP AX
    RET
ENDP

; ----------------------------------------------------------------------------
; _ed_move_to_end - Move cursor (gap) to end of all text
; Input:  SI = editor state
; Preserves: AX, BX, CX, DI
; ----------------------------------------------------------------------------
_ed_move_to_end: PROC
    PUSH AX
    PUSH BX
    PUSH CX
    PUSH DI

    ; CX = bytes after gap
    MOV CX, [SI + ED_BUFEND]
    SUB CX, [SI + ED_GAPEND]
    TEST CX, CX
    JZ .done

    ; Forward copy: [GAPEND..) -> [GAPSTART..)
    MOV BX, [SI + ED_GAPEND]
    MOV DI, [SI + ED_GAPSTART]
.copy_loop:
    MOV AL, [BX]
    MOV [DI], AL
    INC BX
    INC DI
    DEC CX
    JNZ .copy_loop

    ; Update gap pointers
    MOV [SI + ED_GAPSTART], DI
    MOV AX, [SI + ED_BUFEND]
    MOV [SI + ED_GAPEND], AX

    ; Update cursor position
    MOV AX, [SI + ED_LINECOUNT]
    DEC AX
    MOV [SI + ED_CURLINE], AX
    CALL _gap_recalc_col

.done:
    POP DI
    POP CX
    POP BX
    POP AX
    RET
ENDP

; ----------------------------------------------------------------------------
; editor_select_all - Select all text (for menu handler)
; Uses global ed_state
; ----------------------------------------------------------------------------
editor_select_all: PROC
    PUSH SI
    MOV SI, ed_state
    MOV WORD [SI + ED_SELANCHOR], 0
    CALL _ed_move_to_end
    MOV BYTE [fw_state + FW_DIRTY], 1
    POP SI
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_ctrl_draw_editor - Draw all visible rows of the editor
; Input:  BX = control ptr, DH = abs row, DL = abs col, CH = focused flag
; Preserves: BX, DI
; ----------------------------------------------------------------------------
tui_ctrl_draw_editor: PROC
    PUSH SI
    PUSH AX
    PUSH CX
    PUSH DX
    PUSH BP
    PUSH BX
    PUSH DI

    ; Save paint parameters
    MOV [ed_paint_origdl], DL
    MOV [ed_paint_origdh], DH
    MOV AL, [BX + CTRL_W]
    MOV [ed_paint_width], AL
    MOV AL, [BX + CTRL_H]
    MOV [ed_paint_height], AL
    DEC BYTE [ed_paint_height]      ; reserve last row for status bar
    MOV [ed_paint_focused], CH

    ; Get editor state pointer
    MOV SI, [BX + CTRL_ED_STATE]

    ; Check if vertical scroll bar needed: LINECOUNT > text_height
    MOV BYTE [ed_needs_vbar], 0
    XOR AH, AH
    MOV AL, [ed_paint_height]
    CMP [SI + ED_LINECOUNT], AX
    JBE .no_vbar_setup
    MOV BYTE [ed_needs_vbar], 1
    DEC BYTE [ed_paint_width]       ; shrink text area by 1 for vbar column
.no_vbar_setup:
    MOV BP, BX                  ; BP = control pointer (freed BX for temp use)

    ; Precompute selection range
    MOV WORD [ed_sel_start], 0FFFFh
    MOV WORD [ed_sel_end], 0FFFFh
    CMP WORD [SI + ED_SELANCHOR], 0FFFFh
    JE .no_sel_setup
    CALL gap_cursor_offset          ; AX = cursor offset
    MOV BX, [SI + ED_SELANCHOR]
    CMP AX, BX
    JBE .cursor_first
    XCHG AX, BX
.cursor_first:
    MOV [ed_sel_start], AX
    MOV [ed_sel_end], BX
.no_sel_setup:

    ; Precompute cursor logical offset for cursor-check during paint
    CALL gap_cursor_offset
    MOV [ed_cursor_offset], AX

    ; Find physical pointer to first visible line (ED_SCROLLY)
    MOV AX, [SI + ED_SCROLLY]
    CALL gap_find_line_phys     ; DI = physical ptr to line start

    ; Compute initial logical offset for selection tracking
    CALL _ed_phys_to_logical
    MOV [ed_paint_offset], AX

    ; Initialize line number tracker
    MOV AX, [SI + ED_SCROLLY]
    MOV [ed_paint_line], AX

    ; Row loop: CL = row counter (0..height-1)
    XOR CL, CL
.row_loop:
    CMP CL, [ed_paint_height]
    JB .row_cont
    JMP .rows_done
.row_cont:

    ; Set screen position
    MOV DH, [ed_paint_origdh]
    ADD DH, CL
    MOV DL, [ed_paint_origdl]

    ; Get current line number
    MOV AX, [ed_paint_line]

    ; Past end of text? → empty row
    CMP AX, [SI + ED_LINECOUNT]
    JB .have_line
    JMP .empty_row
.have_line:

    ; --- Determine base attribute ---
    MOV AH, CLR_ED_TEXT
    MOV BYTE [ed_paint_curline], 0
    PUSH AX
    MOV AX, [ed_paint_line]
    CMP AX, [SI + ED_CURLINE]
    JNE .not_cur
    CMP BYTE [ed_paint_focused], 0
    JE .not_cur
    MOV BYTE [ed_paint_curline], 1
.not_cur:
    POP AX
    CMP BYTE [ed_paint_curline], 0
    JE .attr_set
    MOV AH, CLR_ED_CURLINE
.attr_set:

    ; --- Fill row with spaces ---
    PUSH AX
    PUSH CX
    MOV AL, 20h
    MOV CL, [ed_paint_width]
    CALL tui_hline
    POP CX
    POP AX

    ; --- Skip to SCROLLX visual column ---
    PUSH CX
    MOV WORD [ed_paint_vcol], 0
    MOV BX, [SI + ED_SCROLLX]
    TEST BX, BX
    JZ .sx_done
.sx_loop:
    CALL .gap_skip
    CMP DI, [SI + ED_BUFEND]
    JAE .sx_done
    CMP BYTE [DI], 0Ah
    JE .sx_done
    ; Advance vcol by char width
    CMP BYTE [DI], 09h
    JNE .sx_regular
    ; TAB: advance to next tab stop
    MOV CX, [ed_paint_vcol]
    ADD CX, ED_TABWIDTH
    AND CX, 0FFF8h
    MOV [ed_paint_vcol], CX
    JMP .sx_check
.sx_regular:
    INC WORD [ed_paint_vcol]
.sx_check:
    INC DI
    INC WORD [ed_paint_offset]
    CMP [ed_paint_vcol], BX
    JB .sx_loop
.sx_done:
    POP CX

    ; --- Render characters up to width ---
    PUSH CX
    PUSH DX
    MOV BL, [ed_paint_width]    ; BL = remaining columns

.ch_loop:
    OR BL, BL
    JZ .ch_width_done
    CALL .gap_skip
    CMP DI, [SI + ED_BUFEND]
    JAE .ch_eol
    CMP BYTE [DI], 0Ah
    JE .ch_eol

    ; Check for TAB character
    CMP BYTE [DI], 09h
    JE .ch_tab

    ; --- Regular character ---
    PUSH AX
    MOV AL, [DI]
    ; Selection highlight check
    PUSH CX
    MOV CX, [ed_paint_offset]
    CMP CX, [ed_sel_start]
    JB .ch_no_sel
    CMP CX, [ed_sel_end]
    JAE .ch_no_sel
    MOV AH, CLR_ED_SELECT
.ch_no_sel:
    POP CX
    ; Check cursor (offset-based)
    CMP BYTE [ed_paint_curline], 0
    JE .ch_no_cur
    PUSH CX
    MOV CX, [ed_paint_offset]
    CMP CX, [ed_cursor_offset]
    POP CX
    JNE .ch_no_cur
    MOV AH, CLR_ED_CURSOR
.ch_no_cur:
    CALL tui_putchar
    POP AX
    INC DL
    INC DI
    INC WORD [ed_paint_offset]
    INC WORD [ed_paint_vcol]
    DEC BL
    JMP .ch_loop

.ch_tab:
    ; --- TAB: expand to spaces ---
    PUSH AX
    ; Compute spaces: next_tab = (vcol + 8) & FFF8h; spaces = next_tab - vcol
    MOV CX, [ed_paint_vcol]
    ADD CX, ED_TABWIDTH
    AND CX, 0FFF8h
    SUB CX, [ed_paint_vcol]    ; CX = spaces to paint
    ; Clamp to remaining width
    XOR CH, CH                 ; CX was small, but ensure CH=0
    CMP CL, BL
    JBE .tab_clamp_ok
    MOV CL, BL
.tab_clamp_ok:
    ; Determine attribute for this TAB
    MOV AH, CLR_ED_TEXT
    CMP BYTE [ed_paint_curline], 0
    JE .tab_attr_base
    MOV AH, CLR_ED_CURLINE
.tab_attr_base:
    ; Selection check
    PUSH DX
    MOV DX, [ed_paint_offset]
    CMP DX, [ed_sel_start]
    JB .tab_no_sel
    CMP DX, [ed_sel_end]
    JAE .tab_no_sel
    MOV AH, CLR_ED_SELECT
.tab_no_sel:
    ; Cursor check (offset-based)
    CMP BYTE [ed_paint_curline], 0
    JE .tab_no_cur
    CMP DX, [ed_cursor_offset]
    JNE .tab_no_cur
    MOV AH, CLR_ED_CURSOR
.tab_no_cur:
    POP DX
    ; Paint CL space characters
    MOV AL, 20h
.tab_paint:
    TEST CL, CL
    JZ .tab_paint_done
    CALL tui_putchar
    INC DL
    DEC BL
    DEC CL
    JMP .tab_paint
.tab_paint_done:
    ; Update vcol to actual next tab stop (not clamped)
    MOV CX, [ed_paint_vcol]
    ADD CX, ED_TABWIDTH
    AND CX, 0FFF8h
    MOV [ed_paint_vcol], CX
    ; Advance past the single TAB byte
    INC DI
    INC WORD [ed_paint_offset]
    POP AX
    JMP .ch_loop

.ch_eol:
    ; At end of line or buffer — check selection and cursor at EOL
    PUSH AX
    XOR AH, AH                     ; default: no highlight
    ; Selection check for EOL position
    PUSH CX
    MOV CX, [ed_paint_offset]
    CMP CX, [ed_sel_start]
    JB .eol_no_sel
    CMP CX, [ed_sel_end]
    JAE .eol_no_sel
    MOV AH, CLR_ED_SELECT
.eol_no_sel:
    POP CX
    ; Cursor check (offset-based)
    CMP BYTE [ed_paint_curline], 0
    JE .eol_no_cur
    PUSH CX
    MOV CX, [ed_paint_offset]
    CMP CX, [ed_cursor_offset]
    POP CX
    JNE .eol_no_cur
    MOV AH, CLR_ED_CURSOR
.eol_no_cur:
    TEST AH, AH
    JZ .eol_no_draw
    MOV AL, 20h
    CALL tui_putchar
.eol_no_draw:
    POP AX
    ; Advance DI past LF (if at LF) and increment paint offset
    CALL .gap_skip
    CMP DI, [SI + ED_BUFEND]
    JAE .ch_line_adv
    CMP BYTE [DI], 0Ah
    JNE .ch_line_adv
    INC DI
    INC WORD [ed_paint_offset]
.ch_line_adv:
    CALL .gap_skip
    POP DX
    POP CX
    JMP .next_row

.ch_width_done:
    ; Width limit reached: advance DI past rest of line + LF
    CALL .advance_past_line
    POP DX
    POP CX
    JMP .next_row

.empty_row:
    ; Past end of text: fill with spaces
    PUSH AX
    PUSH CX
    MOV AH, CLR_ED_TEXT
    MOV AL, 20h
    MOV CL, [ed_paint_width]
    CALL tui_hline
    POP CX
    POP AX

.next_row:
    INC WORD [ed_paint_line]
    INC CL
    JMP .row_loop

.rows_done:
    ; --- Paint status bar on last row ---
    MOV DH, [ed_paint_origdh]
    ADD DH, [ed_paint_height]
    MOV DL, [ed_paint_origdl]
    PUSH CX
    MOV AH, CLR_ED_STATUS
    MOV AL, 20h
    MOV CL, [BP + CTRL_W]          ; full width (include vbar column)
    CALL tui_hline
    POP CX
    MOV DL, [ed_paint_origdl]
    MOV DI, str_status_ln
    MOV AH, CLR_ED_STATUS
    CALL _ed_paint_string
    MOV AX, [SI + ED_CURLINE]
    INC AX
    CALL _ed_format_number
    MOV AH, CLR_ED_STATUS
    CALL _ed_paint_string
    MOV DI, str_status_col
    CALL _ed_paint_string
    MOV AX, [SI + ED_CURCOL]
    INC AX
    CALL _ed_format_number
    MOV AH, CLR_ED_STATUS
    CALL _ed_paint_string
    TEST BYTE [SI + ED_FLAGS], EDF_MODIFIED
    JZ .status_done
    MOV DI, str_status_mod
    CALL _ed_paint_string
.status_done:

    ; --- Right-aligned memory indicators: Clip:NNNN Undo:NNNN NNNN free ---

    ; Phase 1: Count digit widths for all three values
    ; Clip free = ED_BUFSIZE - ed_cliplen
    MOV AX, ED_BUFSIZE
    SUB AX, [ed_cliplen]
    CALL _ed_format_number
    MOV AX, ed_numbuf
    ADD AX, 5
    SUB AX, DI
    MOV [ed_clip_digits], AL

    ; Undo free = UNDO_BUFSIZE - undo_head
    MOV AX, UNDO_BUFSIZE
    SUB AX, [undo_head]
    CALL _ed_format_number
    MOV AX, ed_numbuf
    ADD AX, 5
    SUB AX, DI
    MOV [ed_undo_digits], AL

    ; Gap free = ED_GAPEND - ED_GAPSTART
    MOV AX, [SI + ED_GAPEND]
    SUB AX, [SI + ED_GAPSTART]
    CALL _ed_format_number
    MOV AX, ed_numbuf
    ADD AX, 5
    SUB AX, DI
    MOV [ed_gap_digits], AL

    ; Phase 2: Total width = clip_digits + undo_digits + gap_digits + 17
    ; (5 "Clip:" + 6 " Undo:" + 1 " " + 5 " free" = 17 fixed chars)
    XOR AH, AH
    MOV AL, [ed_clip_digits]
    ADD AL, [ed_undo_digits]
    ADD AL, [ed_gap_digits]
    ADD AL, 17
    MOV CL, AL

    ; Starting column = origdl + CTRL_W - total_width
    MOV DL, [ed_paint_origdl]
    ADD DL, [BP + CTRL_W]
    SUB DL, CL

    ; Re-set DH to status bar row
    MOV DH, [ed_paint_origdh]
    ADD DH, [ed_paint_height]

    ; Phase 3: Paint left to right
    MOV AH, CLR_ED_STATUS

    ; Paint "Clip:"
    MOV DI, str_status_clip
    CALL _ed_paint_string

    ; Paint clip free number
    MOV AX, ED_BUFSIZE
    SUB AX, [ed_cliplen]
    CALL _ed_format_number
    MOV AH, CLR_ED_STATUS
    CALL _ed_paint_string

    ; Paint " Undo:"
    MOV DI, str_status_undo
    CALL _ed_paint_string

    ; Paint undo free number
    MOV AX, UNDO_BUFSIZE
    SUB AX, [undo_head]
    CALL _ed_format_number
    MOV AH, CLR_ED_STATUS
    CALL _ed_paint_string

    ; Paint " " separator before gap free
    MOV AL, ' '
    CALL tui_putchar
    INC DL

    ; Paint gap free number
    MOV AX, [SI + ED_GAPEND]
    SUB AX, [SI + ED_GAPSTART]
    CALL _ed_format_number
    MOV AH, CLR_ED_STATUS
    CALL _ed_paint_string

    ; Paint " free"
    MOV DI, str_status_free
    CALL _ed_paint_string

    ; --- Draw vertical scroll bar if needed ---
    CMP BYTE [ed_needs_vbar], 0
    JNZ .vbar_do_draw
    JMP .vbar_draw_done
.vbar_do_draw:

    ; Column = rightmost column of control (using BP = saved control ptr)
    MOV DL, [ed_paint_origdl]
    ADD DL, [BP + CTRL_W]
    DEC DL
    ; Top row = control origin
    MOV DH, [ed_paint_origdh]
    ; Bar height = text area height
    MOV BL, [ed_paint_height]

    ; Prepare params: AL=scroll, AH=visible, CL=total (all bytes)
    CMP WORD [SI + ED_LINECOUNT], 255
    JA .vbar_draw_scale

    ; --- Direct (linecount <= 255) ---
    MOV AL, [SI + ED_SCROLLY]      ; scroll (low byte, safe since total<=255)
    MOV AH, [ed_paint_height]      ; visible = text area height
    MOV CL, [SI + ED_LINECOUNT]    ; total (low byte)
    JMP .vbar_draw_call

.vbar_draw_scale:
    ; --- Scaled (linecount > 255) ---
    ; scroll_scaled = (ED_SCROLLY * 255) / ED_LINECOUNT
    PUSH DX
    MOV AX, [SI + ED_SCROLLY]
    MOV CX, 255
    MUL CX                         ; DX:AX = scrolly * 255
    DIV WORD [SI + ED_LINECOUNT]   ; AX = scaled scroll
    MOV [ed_needs_vbar], AL        ; temp store scroll_scaled
    ; visible_scaled = (height * 255) / ED_LINECOUNT
    XOR AH, AH
    MOV AL, [ed_paint_height]
    MOV CX, 255
    MUL CX                         ; DX:AX = height * 255
    DIV WORD [SI + ED_LINECOUNT]   ; AX = scaled visible
    OR AL, AL
    JNZ .vbar_vis_ok
    MOV AL, 1                      ; minimum 1
.vbar_vis_ok:
    MOV AH, AL                     ; AH = visible_scaled
    MOV AL, [ed_needs_vbar]        ; AL = scroll_scaled
    MOV CL, 255                    ; total = 255
    POP DX

.vbar_draw_call:
    CALL tui_draw_vscrollbar

.vbar_draw_done:
    POP DI
    POP BX
    POP BP
    POP DX
    POP CX
    POP AX
    POP SI
    RET

; --- Helper: skip gap if DI is at gap start ---
.gap_skip:
    CMP DI, [SI + ED_GAPSTART]
    JNE .gs_done
    MOV DI, [SI + ED_GAPEND]
.gs_done:
    RET

; --- Helper: advance DI past rest of current line and LF ---
.advance_past_line:
    PUSH AX
.apl_loop:
    CALL .gap_skip
    CMP DI, [SI + ED_BUFEND]
    JAE .apl_done
    CMP BYTE [DI], 0Ah
    JE .apl_lf
    INC DI
    INC WORD [ed_paint_offset]
    JMP .apl_loop
.apl_lf:
    INC DI
    INC WORD [ed_paint_offset]
    CALL .gap_skip
.apl_done:
    POP AX
    RET

ENDP

; ----------------------------------------------------------------------------
; tui_ed_handle_key - Handle keyboard input for the editor control
; Input:  BX = control ptr, AH = key type, AL = key/scan code
;         SI = window struct
; Output: CF=0 if handled, CF=1 if not
; ----------------------------------------------------------------------------
tui_ed_handle_key: PROC
    PUSH DX
    PUSH CX

    ; Save SI (window struct) and switch SI to editor state
    PUSH SI
    MOV SI, [BX + CTRL_ED_STATE]

    CMP AH, KTYPE_EXTENDED
    JZ .extended

    ; --- Normal keys ---
    CMP AL, KEY_BACKSPACE
    JZ .backspace
    CMP AL, 0Dh                 ; Enter
    JZ .enter
    CMP AL, 09h                 ; Tab
    JNE .not_tab_key
    JMP .tab_key
.not_tab_key:
    CMP AL, 20h
    JB .check_ctrl
    CMP AL, 7Eh
    JA .not_handled
    ; Printable character (20h-7Eh): delete selection first, then insert
    CMP WORD [SI + ED_SELANCHOR], 0FFFFh
    JE .print_no_sel
    CALL undo_capture_selection
    CALL _ed_sel_delete
    CALL gap_insert_char
    MOV CX, 1
    CALL undo_record_compound
    JMP .handled
.print_no_sel:
    CALL undo_save_cursor
    CALL gap_insert_char
    CALL undo_record_insert1
    JMP .handled

.backspace:
    CMP WORD [SI + ED_SELANCHOR], 0FFFFh
    JE .bs_normal
    CALL undo_capture_selection
    CALL _ed_sel_delete
    CALL undo_record_sel_delete
    JMP .handled
.bs_normal:
    CALL undo_save_cursor
    CALL gap_delete_back
    JC .handled
    CALL undo_record_delete_back
    JMP .handled

.enter:
    CMP WORD [SI + ED_SELANCHOR], 0FFFFh
    JE .enter_no_sel
    CALL undo_capture_selection
    CALL _ed_sel_delete
    MOV AL, 0Ah
    CALL gap_insert_char
    MOV CX, 1
    CALL undo_record_compound
    JMP .handled
.enter_no_sel:
    CALL undo_save_cursor
    MOV AL, 0Ah
    CALL gap_insert_char
    CALL undo_record_insert1
    JMP .handled

.tab_key:
    CMP WORD [SI + ED_SELANCHOR], 0FFFFh
    JE .tab_no_sel
    CALL undo_capture_selection
    CALL _ed_sel_delete
    MOV AL, 09h
    CALL gap_insert_char
    MOV CX, 1
    CALL undo_record_compound
    JMP .handled
.tab_no_sel:
    CALL undo_save_cursor
    MOV AL, 09h
    CALL gap_insert_char
    CALL undo_record_insert1
    JMP .handled

.check_ctrl:
    CMP AL, KEY_CTRL_C
    JE .ctrl_copy
    CMP AL, KEY_CTRL_X
    JE .ctrl_cut
    CMP AL, KEY_CTRL_V
    JE .ctrl_paste
    CMP AL, KEY_CTRL_F
    JE .ctrl_find
    CMP AL, KEY_CTRL_R
    JE .ctrl_replace
    CMP AL, KEY_CTRL_A
    JNE .check_file
    ; Select All: anchor=0, move cursor to end
    MOV WORD [SI + ED_SELANCHOR], 0
    CALL _ed_move_to_end
    JMP .handled
.check_file:
    CMP AL, KEY_CTRL_O
    JE .ctrl_open
    CMP AL, KEY_CTRL_N
    JE .ctrl_new
    CMP AL, KEY_CTRL_S
    JE .ctrl_save
.check_undo:
    CMP AL, KEY_CTRL_Z
    JE .ctrl_undo
    CMP AL, KEY_CTRL_Y
    JE .ctrl_redo
    JMP .not_handled

.ctrl_undo:
    CALL undo_perform_undo
    JMP .handled
.ctrl_redo:
    CALL undo_perform_redo
    JMP .handled
.ctrl_copy:
    CALL editor_clipboard_copy
    JMP .handled
.ctrl_cut:
    CALL editor_clipboard_cut
    JMP .handled
.ctrl_paste:
    CALL editor_clipboard_paste
    JMP .handled
.ctrl_open:
    CALL editor_file_open
    JMP .handled_dlg
.ctrl_new:
    CALL editor_file_new
    JMP .handled_dlg
.ctrl_save:
    CALL editor_file_save
    JMP .handled_dlg
.ctrl_find:
    CALL editor_find
    JMP .handled_dlg
.ctrl_replace:
    CALL editor_replace
    JMP .handled_dlg
.find_next:
    CALL editor_find_next
    JMP .handled_dlg

.extended:
    ; --- Extended keys (AL = scan code) ---
    ; Check for movement keys
    CMP AL, KEY_LEFT
    JE .is_movement
    CMP AL, KEY_RIGHT
    JE .is_movement
    CMP AL, KEY_UP
    JE .is_movement
    CMP AL, KEY_DOWN
    JE .is_movement
    CMP AL, KEY_HOME
    JE .is_movement
    CMP AL, KEY_END
    JE .is_movement
    CMP AL, KEY_PGUP
    JE .is_movement
    CMP AL, KEY_PGDN
    JE .is_movement
    ; Non-movement extended keys
    CMP AL, KEY_DELETE
    JZ .delete
    CMP AL, KEY_F3
    JE .find_next
    JMP .not_handled

.is_movement:
    CALL _ed_check_shift
    ; If shift held and no anchor yet -> set anchor = cursor offset
    CMP BYTE [ed_shift_held], 0
    JZ .move_dispatch
    CMP WORD [SI + ED_SELANCHOR], 0FFFFh
    JNE .move_dispatch
    PUSH AX
    CALL gap_cursor_offset
    MOV [SI + ED_SELANCHOR], AX
    POP AX
.move_dispatch:
    CMP AL, KEY_LEFT
    JE .left
    CMP AL, KEY_RIGHT
    JE .right
    CMP AL, KEY_UP
    JE .up
    CMP AL, KEY_DOWN
    JE .down
    CMP AL, KEY_HOME
    JE .home
    CMP AL, KEY_END
    JE .end_key
    CMP AL, KEY_PGUP
    JE .pgup
    CMP AL, KEY_PGDN
    JE .pgdn
    JMP .not_handled

.left:
    CALL gap_move_left
    JMP .move_done
.right:
    CALL gap_move_right
    JMP .move_done
.up:
    CALL gap_move_up
    JMP .move_done
.down:
    CALL gap_move_down
    JMP .move_done
.home:
    CALL gap_home
    JMP .move_done
.end_key:
    CALL gap_end
    JMP .move_done

.delete:
    CMP WORD [SI + ED_SELANCHOR], 0FFFFh
    JE .del_normal
    CALL undo_capture_selection
    CALL _ed_sel_delete
    CALL undo_record_sel_delete
    JMP .handled
.del_normal:
    CALL undo_save_cursor
    CALL gap_delete_fwd
    JC .handled
    CALL undo_record_delete_fwd
    JMP .handled

.pgup:
    MOV CL, [BX + CTRL_H]
    DEC CL
    XOR CH, CH
.pgup_loop:
    CALL gap_move_up
    JC .move_done
    DEC CX
    JNZ .pgup_loop
    JMP .move_done

.pgdn:
    MOV CL, [BX + CTRL_H]
    DEC CL
    XOR CH, CH
.pgdn_loop:
    CALL gap_move_down
    JC .move_done
    DEC CX
    JNZ .pgdn_loop
    JMP .move_done

.move_done:
    ; If shift not held, clear selection
    CMP BYTE [ed_shift_held], 0
    JNZ .handled
    MOV WORD [SI + ED_SELANCHOR], 0FFFFh
    JMP .handled

.handled_dlg:
    ; Exit path for find/replace: BX may be clobbered by dialog calls,
    ; so skip gap_ensure_visible (find functions handle it themselves).
    POP SI
    POP CX
    POP DX
    CLC
    RET

.handled:
    ; Ensure cursor visible (height - 1 for status bar)
    MOV CL, [BX + CTRL_H]
    DEC CL
    MOV CH, [BX + CTRL_W]
    ; Narrow width if vbar present (linecount > text height)
    PUSH AX
    XOR AH, AH
    MOV AL, CL
    CMP [SI + ED_LINECOUNT], AX
    JBE .ev1_no_vbar
    DEC CH
.ev1_no_vbar:
    POP AX
    CALL gap_ensure_visible
    POP SI
    ; Trigger redraw
    MOV BYTE [fw_state + FW_DIRTY], 1
    POP CX
    POP DX
    CLC
    RET

.not_handled:
    POP SI
    POP CX
    POP DX
    STC
    RET
ENDP

; ----------------------------------------------------------------------------
; _ed_screen_to_offset - Convert screen position to buffer logical offset
; Input:  SI = editor state, DH = mouse row, DL = mouse col
;         ed_mouse_absrow/abscol must be set
; Output: AX = logical offset at that screen position
; Clobbers: AX, BX, CX, DI
; ----------------------------------------------------------------------------
_ed_screen_to_offset: PROC
    PUSH DX

    ; rel_row = DH - absrow, rel_col = DL - abscol
    SUB DH, [ed_mouse_absrow]
    SUB DL, [ed_mouse_abscol]

    ; target_line = ED_SCROLLY + rel_row
    XOR AH, AH
    MOV AL, DH
    ADD AX, [SI + ED_SCROLLY]
    ; Clamp to LINECOUNT-1
    MOV BX, [SI + ED_LINECOUNT]
    DEC BX
    CMP AX, BX
    JBE .line_ok
    MOV AX, BX
.line_ok:

    ; target_vcol = ED_SCROLLX + rel_col (visual column)
    XOR CH, CH
    MOV CL, DL
    ADD CX, [SI + ED_SCROLLX]      ; CX = target visual column

    ; Find physical start of target_line
    CALL gap_find_line_phys         ; DI = physical start of line

    ; Walk forward tracking visual column, stop when vcol >= target
    XOR BX, BX                     ; BX = current visual column
    TEST CX, CX
    JZ .col_done
.col_walk:
    CMP DI, [SI + ED_GAPSTART]
    JNE .cw_no_gap
    MOV DI, [SI + ED_GAPEND]
.cw_no_gap:
    CMP DI, [SI + ED_BUFEND]
    JAE .col_done
    CMP BYTE [DI], 0Ah
    JE .col_done
    ; Advance vcol by character width
    CMP BYTE [DI], 09h
    JNE .cw_regular
    ; TAB: compute next tab stop
    PUSH AX
    MOV AX, BX
    ADD AX, ED_TABWIDTH
    AND AX, 0FFF8h
    MOV BX, AX
    POP AX
    JMP .cw_advance
.cw_regular:
    INC BX
.cw_advance:
    INC DI
    CMP BX, CX
    JB .col_walk
.col_done:
    ; Final gap skip (DI may have landed on GAPSTART after last INC)
    CMP DI, [SI + ED_GAPSTART]
    JNE .no_final_gap
    MOV DI, [SI + ED_GAPEND]
.no_final_gap:

    ; Convert physical pointer to logical offset
    CALL _ed_phys_to_logical        ; AX = logical offset

    POP DX
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_ed_mouse_press - Handle mouse click on editor control
; Input:  BX = control ptr, SI = window struct, DH = mouse row, DL = mouse col
; Called from tui_mouse_on_press after focus is set.
; ----------------------------------------------------------------------------
tui_ed_mouse_press: PROC
    PUSH AX
    PUSH CX
    PUSH DI

    ; Compute and cache control absolute origin
    MOV AL, [SI + WIN_Y]
    TEST BYTE [SI + WIN_FLAGS], WINF_BORDER
    JZ .no_bdr_y
    INC AL
.no_bdr_y:
    ADD AL, [BX + CTRL_Y]
    MOV [ed_mouse_absrow], AL

    MOV AL, [SI + WIN_X]
    TEST BYTE [SI + WIN_FLAGS], WINF_BORDER
    JZ .no_bdr_x
    INC AL
.no_bdr_x:
    ADD AL, [BX + CTRL_X]
    MOV [ed_mouse_abscol], AL

    ; Clamp mouse row to text area (exclude status bar)
    ; max screen row = absrow + CTRL_H - 2
    MOV AL, [ed_mouse_absrow]
    ADD AL, [BX + CTRL_H]
    SUB AL, 2
    CMP DH, AL
    JBE .row_ok
    MOV DH, AL
.row_ok:

    ; --- Check for vertical scroll bar click ---
    PUSH SI                         ; save window ptr temporarily
    MOV SI, [BX + CTRL_ED_STATE]
    ; Vbar present when LINECOUNT > text height (CTRL_H - 1)
    MOV AL, [BX + CTRL_H]
    DEC AL
    XOR AH, AH
    CMP [SI + ED_LINECOUNT], AX
    POP SI                          ; restore window ptr
    JBE .no_vbar_click

    ; Vbar present. Check if click is on vbar column.
    MOV AL, [ed_mouse_abscol]
    ADD AL, [BX + CTRL_W]
    DEC AL                          ; AL = vbar column (absolute)
    CMP DL, AL
    JNE .no_vbar_click

    ; Click IS on scroll bar. Compute relative row.
    MOV AL, DH
    SUB AL, [ed_mouse_absrow]       ; AL = relative row (0-based)
    ; Check within vbar height (not on status bar)
    MOV AH, [BX + CTRL_H]
    DEC AH                          ; AH = vbar height = text area height
    CMP AL, AH
    JAE .no_vbar_click

    ; AL = rel_row (0..height-1), AH = bar_height
    ; Get editor state for scroll operations
    PUSH SI
    MOV SI, [BX + CTRL_ED_STATE]

    ; Dispatch: row 0 = up arrow, row == height-1 = down arrow
    TEST AL, AL
    JZ .vbar_up

    MOV CL, AH
    DEC CL                          ; CL = bar_height - 1
    CMP AL, CL
    JZ .vbar_down

    ; --- Track/thumb click: detect thumb position ---
    ; AL = rel_row, AH = bar_height
    ; click_track_pos = rel_row - 1 (0-based within track)
    DEC AL                           ; AL = click_track_pos
    ; Save click_track_pos and bar_height
    ; sb_scratch: [0]=click_pos, [1]=bar_height, [2]=thumb_size, [3]=thumb_pos
    MOV [sb_scratch], AL             ; [0] = click_track_pos
    MOV [sb_scratch + 1], AH         ; [1] = bar_height

    ; track_len = bar_height - 2
    PUSH DX
    XOR AH, AH
    MOV AL, [sb_scratch + 1]
    SUB AL, 2                        ; AL = track_len

    ; thumb_size = max(1, (visible * track_len) / total)
    MOV CL, AL                       ; save track_len in CL
    XOR AH, AH
    MOV AL, [BX + CTRL_H]
    DEC AL                            ; AL = visible (text height)
    XOR CH, CH                        ; CX = track_len
    MUL CX                            ; AX = visible * track_len
    XOR DX, DX
    DIV WORD [SI + ED_LINECOUNT]      ; AX = thumb_size
    OR AL, AL
    JNZ .vbar_ts_ok
    MOV AL, 1
.vbar_ts_ok:
    MOV [sb_scratch + 2], AL          ; [2] = thumb_size

    ; range = track_len - thumb_size
    XOR AH, AH
    MOV AL, CL                        ; AL = track_len
    SUB AL, [sb_scratch + 2]          ; AL = range
    OR AL, AL
    JZ .vbar_range_zero

    ; thumb_pos = (scrolly * range) / max_scroll
    ; max_scroll = linecount - visible
    XOR AH, AH
    MUL WORD [SI + ED_SCROLLY]        ; DX:AX = range * scrolly
    ; Divide by max_scroll
    MOV CX, [SI + ED_LINECOUNT]
    PUSH AX
    XOR AH, AH
    MOV AL, [BX + CTRL_H]
    DEC AL
    SUB CX, AX                        ; CX = max_scroll
    POP AX
    OR CX, CX
    JZ .vbar_range_zero
    DIV CX                            ; AX = thumb_pos
    MOV [sb_scratch + 3], AL          ; [3] = thumb_pos
    POP DX

    ; Compare click_track_pos against thumb region
    MOV AL, [sb_scratch]              ; click_track_pos
    CMP AL, [sb_scratch + 3]
    JB .vbar_page_up                  ; click above thumb

    MOV AH, [sb_scratch + 3]
    ADD AH, [sb_scratch + 2]         ; AH = thumb_pos + thumb_size
    CMP AL, AH
    JAE .vbar_page_down              ; click below thumb

    ; --- Click ON the thumb: start drag ---
    SUB AL, [sb_scratch + 3]         ; AL = offset within thumb
    MOV [mouse_state + MS_SB_OFFSET], AL
    MOV [mouse_state + MS_SB_CTRL], BX
    MOV BYTE [mouse_state + MS_SB_DIR], 0  ; vertical
    OR BYTE [mouse_state + MS_FLAGS], MSF_SB_DRAG
    JMP .vbar_click_done

.vbar_range_zero:
    POP DX
    JMP .vbar_click_done

.vbar_up:
    ; Scroll up by 1
    CMP WORD [SI + ED_SCROLLY], 0
    JE .vbar_click_done
    DEC WORD [SI + ED_SCROLLY]
    JMP .vbar_click_done

.vbar_down:
    ; Scroll down by 1: max_scroll = linecount - text_height
    PUSH AX
    XOR AH, AH
    MOV AL, [BX + CTRL_H]
    DEC AL                           ; visible height
    MOV CX, [SI + ED_LINECOUNT]
    SUB CX, AX                      ; CX = max_scroll
    CMP [SI + ED_SCROLLY], CX
    JAE .vbar_down_done
    INC WORD [SI + ED_SCROLLY]
.vbar_down_done:
    POP AX
    JMP .vbar_click_done

.vbar_page_up:
    ; ED_SCROLLY -= text_height, clamp 0
    XOR AH, AH
    MOV AL, [BX + CTRL_H]
    DEC AL
    CMP [SI + ED_SCROLLY], AX
    JAE .vbar_pgu_sub
    MOV WORD [SI + ED_SCROLLY], 0
    JMP .vbar_click_done
.vbar_pgu_sub:
    SUB [SI + ED_SCROLLY], AX
    JMP .vbar_click_done

.vbar_page_down:
    ; ED_SCROLLY += text_height, clamp max
    PUSH DX
    XOR AH, AH
    MOV AL, [BX + CTRL_H]
    DEC AL                           ; AL = visible
    MOV DX, [SI + ED_LINECOUNT]
    SUB DX, AX                      ; DX = max_scroll
    ADD AX, [SI + ED_SCROLLY]       ; AX = scroll + visible
    CMP AX, DX
    JBE .vbar_pgd_ok
    MOV AX, DX
.vbar_pgd_ok:
    MOV [SI + ED_SCROLLY], AX
    POP DX
    JMP .vbar_click_done

.vbar_click_done:
    POP SI                           ; restore window ptr
    MOV BYTE [fw_state + FW_DIRTY], 1
    POP DI
    POP CX
    POP AX
    RET

.no_vbar_click:

    ; Switch SI to editor state
    PUSH SI
    MOV SI, [BX + CTRL_ED_STATE]

    ; Save old cursor offset (for shift-click anchor)
    CALL gap_cursor_offset          ; AX = old cursor offset
    MOV [ed_mouse_oldcur], AX

    ; Convert screen position to buffer offset
    PUSH BX                             ; save control ptr (clobbered by _ed_screen_to_offset)
    CALL _ed_screen_to_offset       ; AX = target offset
    POP BX                              ; restore control ptr

    ; Move cursor to target
    CALL gap_goto_offset

    ; Check shift state
    CALL _ed_check_shift
    CMP BYTE [ed_shift_held], 0
    JZ .no_shift

    ; Shift held: if no anchor yet, set anchor = old cursor pos
    CMP WORD [SI + ED_SELANCHOR], 0FFFFh
    JNE .set_drag
    MOV AX, [ed_mouse_oldcur]
    MOV [SI + ED_SELANCHOR], AX
    JMP .set_drag

.no_shift:
    ; No shift: set anchor = new cursor pos (for potential drag)
    CALL gap_cursor_offset
    MOV [SI + ED_SELANCHOR], AX

.set_drag:
    ; Enable control drag tracking
    OR BYTE [mouse_state + MS_FLAGS], MSF_CTRL_DRAG
    MOV [mouse_state + MS_PRESSED], BX

    ; Ensure cursor visible (height-1 for status bar)
    MOV CL, [BX + CTRL_H]
    DEC CL
    MOV CH, [BX + CTRL_W]
    ; Narrow width if vbar present
    PUSH AX
    XOR AH, AH
    MOV AL, CL
    CMP [SI + ED_LINECOUNT], AX
    JBE .ev2_no_vbar
    DEC CH
.ev2_no_vbar:
    POP AX
    CALL gap_ensure_visible

    POP SI
    MOV BYTE [fw_state + FW_DIRTY], 1

    POP DI
    POP CX
    POP AX
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_ed_mouse_drag - Handle mouse drag on editor control
; Input:  BX = control ptr (from MS_PRESSED)
; Called each frame while MSF_CTRL_DRAG is active and button held.
; ----------------------------------------------------------------------------
tui_ed_mouse_drag: PROC
    PUSH AX
    PUSH CX
    PUSH DX
    PUSH SI

    MOV SI, [BX + CTRL_ED_STATE]

    ; Read current mouse position
    MOV DH, [mouse_state + MS_ROW]
    MOV DL, [mouse_state + MS_COL]

    ; Compute last text row: absrow + CTRL_H - 2 (exclude status bar)
    MOV AL, [ed_mouse_absrow]
    ADD AL, [BX + CTRL_H]
    SUB AL, 2                       ; AL = last text row (screen coords)

    ; Auto-scroll: if mouse above control, scroll up
    CMP DH, [ed_mouse_absrow]
    JAE .not_above
    CMP WORD [SI + ED_SCROLLY], 0
    JE .clamp_top
    DEC WORD [SI + ED_SCROLLY]
.clamp_top:
    MOV DH, [ed_mouse_absrow]      ; position cursor at first row
    JMP .row_done

.not_above:
    ; Auto-scroll: if mouse below text area, scroll down
    CMP DH, AL
    JBE .row_done
    PUSH CX
    MOV CL, [BX + CTRL_H]
    DEC CL                          ; CL = visible text height
    XOR CH, CH
    PUSH AX
    MOV AX, [SI + ED_LINECOUNT]
    CMP AX, CX
    JBE .no_scroll_dn               ; all text fits, no scroll needed
    SUB AX, CX                     ; AX = max scroll
    CMP [SI + ED_SCROLLY], AX
    JAE .no_scroll_dn
    INC WORD [SI + ED_SCROLLY]
.no_scroll_dn:
    POP AX
    POP CX
    MOV DH, AL                     ; position cursor at last text row
.row_done:

    ; Clamp col to [abscol, abscol + CTRL_W - 1]
    CMP DL, [ed_mouse_abscol]
    JAE .col_not_left
    MOV DL, [ed_mouse_abscol]
.col_not_left:
    MOV AL, [ed_mouse_abscol]
    ADD AL, [BX + CTRL_W]
    DEC AL
    CMP DL, AL
    JBE .col_not_right
    MOV DL, AL
.col_not_right:

    ; Convert to buffer offset
    PUSH BX                             ; save control ptr (clobbered by _ed_screen_to_offset)
    CALL _ed_screen_to_offset       ; AX = target offset
    POP BX                              ; restore control ptr

    ; Move cursor (anchor stays → selection extends)
    CALL gap_goto_offset

    ; Ensure cursor visible
    MOV CL, [BX + CTRL_H]
    DEC CL
    MOV CH, [BX + CTRL_W]
    ; Narrow width if vbar present
    PUSH AX
    XOR AH, AH
    MOV AL, CL
    CMP [SI + ED_LINECOUNT], AX
    JBE .ev3_no_vbar
    DEC CH
.ev3_no_vbar:
    POP AX
    CALL gap_ensure_visible

    MOV BYTE [fw_state + FW_DIRTY], 1

    POP SI
    POP DX
    POP CX
    POP AX
    RET
ENDP
