; ============================================================================
; editor_gap.inc - Gap Buffer Engine for DOS Notepad
;
; A gap buffer stores text as: [before cursor][gap][after cursor]
; Insert/delete at cursor is O(1). The gap moves with the cursor.
;
; All public functions take SI = pointer to editor state (ED_* struct).
; Requires: editor_const.inc included first.
; ============================================================================

; ----------------------------------------------------------------------------
; _gap_recalc_col - Internal: recalculate cursor column
; Scans backward from gap start to find previous LF or buffer start.
; Input:  SI = editor state
; Output: ED_CURCOL updated
; ----------------------------------------------------------------------------
_gap_recalc_col: PROC
    PUSH BX
    PUSH CX
    MOV BX, [SI + ED_GAPSTART]
    XOR CX, CX
.scan:
    CMP BX, [SI + ED_BUF]
    JE .done
    DEC BX
    CMP BYTE [BX], 0Ah
    JE .done
    INC CX
    JMP .scan
.done:
    MOV [SI + ED_CURCOL], CX
    POP CX
    POP BX
    RET
ENDP

; ----------------------------------------------------------------------------
; _gap_recalc_line - Recount CURLINE by scanning BUF..GAPSTART for newlines
; Input:  SI = editor state
; Output: ED_CURLINE updated
; ----------------------------------------------------------------------------
_gap_recalc_line: PROC
    PUSH DI
    PUSH CX
    MOV DI, [SI + ED_BUF]
    XOR CX, CX
.count:
    CMP DI, [SI + ED_GAPSTART]
    JAE .done
    CMP BYTE [DI], 0Ah
    JNE .no_nl
    INC CX
.no_nl:
    INC DI
    JMP .count
.done:
    MOV [SI + ED_CURLINE], CX
    POP CX
    POP DI
    RET
ENDP

; ----------------------------------------------------------------------------
; gap_init - Initialize a gap buffer
; Input:  SI = editor state, AX = buffer base address, CX = buffer size
; Output: All state fields initialized, gap = entire buffer (empty text)
; ----------------------------------------------------------------------------
gap_init: PROC
    MOV [SI + ED_BUF], AX
    MOV [SI + ED_GAPSTART], AX
    ADD AX, CX
    MOV [SI + ED_BUFEND], AX
    MOV [SI + ED_GAPEND], AX
    MOV WORD [SI + ED_CURLINE], 0
    MOV WORD [SI + ED_CURCOL], 0
    MOV WORD [SI + ED_SCROLLY], 0
    MOV WORD [SI + ED_SCROLLX], 0
    MOV BYTE [SI + ED_FLAGS], 0
    MOV WORD [SI + ED_SELANCHOR], 0FFFFh
    MOV WORD [SI + ED_FILENAME], 0
    MOV WORD [SI + ED_LINECOUNT], 1
    RET
ENDP

; ----------------------------------------------------------------------------
; gap_insert_char - Insert a character at the cursor
; Input:  SI = editor state, AL = character to insert
; Output: CF=1 if buffer full, CF=0 on success
; ----------------------------------------------------------------------------
gap_insert_char: PROC
    PUSH BX
    MOV BX, [SI + ED_GAPSTART]
    CMP BX, [SI + ED_GAPEND]
    JE .full
    MOV [BX], AL
    INC BX
    MOV [SI + ED_GAPSTART], BX
    CMP AL, 0Ah
    JNE .not_nl
    INC WORD [SI + ED_LINECOUNT]
    INC WORD [SI + ED_CURLINE]
    MOV WORD [SI + ED_CURCOL], 0
    JMP .ok
.not_nl:
    INC WORD [SI + ED_CURCOL]
.ok:
    OR BYTE [SI + ED_FLAGS], EDF_MODIFIED
    CLC
    POP BX
    RET
.full:
    STC
    POP BX
    RET
ENDP

; ----------------------------------------------------------------------------
; gap_insert_string - Insert a null-terminated string at cursor
; Input:  SI = editor state, DI = pointer to null-terminated string
; Output: CF=1 if buffer became full mid-insert
; ----------------------------------------------------------------------------
gap_insert_string: PROC
    PUSH AX
.loop:
    MOV AL, [DI]
    TEST AL, AL
    JZ .done
    CALL gap_insert_char
    JC .done
    INC DI
    JMP .loop
.done:
    POP AX
    RET
ENDP

; ----------------------------------------------------------------------------
; gap_delete_back - Delete character before cursor (Backspace)
; Input:  SI = editor state
; Output: AL = deleted character, CF=1 if at buffer start
; ----------------------------------------------------------------------------
gap_delete_back: PROC
    PUSH BX
    MOV BX, [SI + ED_GAPSTART]
    CMP BX, [SI + ED_BUF]
    JE .at_start
    DEC BX
    MOV AL, [BX]
    MOV [SI + ED_GAPSTART], BX
    CMP AL, 0Ah
    JNE .not_nl
    DEC WORD [SI + ED_LINECOUNT]
    DEC WORD [SI + ED_CURLINE]
    CALL _gap_recalc_col
    JMP .ok
.not_nl:
    DEC WORD [SI + ED_CURCOL]
.ok:
    OR BYTE [SI + ED_FLAGS], EDF_MODIFIED
    CLC
    POP BX
    RET
.at_start:
    STC
    POP BX
    RET
ENDP

; ----------------------------------------------------------------------------
; gap_delete_fwd - Delete character after cursor (Delete key)
; Input:  SI = editor state
; Output: AL = deleted character, CF=1 if at buffer end
; ----------------------------------------------------------------------------
gap_delete_fwd: PROC
    PUSH BX
    MOV BX, [SI + ED_GAPEND]
    CMP BX, [SI + ED_BUFEND]
    JE .at_end
    MOV AL, [BX]
    INC WORD [SI + ED_GAPEND]
    CMP AL, 0Ah
    JNE .ok
    DEC WORD [SI + ED_LINECOUNT]
.ok:
    OR BYTE [SI + ED_FLAGS], EDF_MODIFIED
    CLC
    POP BX
    RET
.at_end:
    STC
    POP BX
    RET
ENDP

; ----------------------------------------------------------------------------
; gap_move_left - Move cursor one position left
; Input:  SI = editor state
; Output: AL = character crossed, CF=1 if already at start
; ----------------------------------------------------------------------------
gap_move_left: PROC
    PUSH BX
    PUSH DI
    MOV BX, [SI + ED_GAPSTART]
    CMP BX, [SI + ED_BUF]
    JE .at_start
    DEC BX
    MOV AL, [BX]
    MOV [SI + ED_GAPSTART], BX
    DEC WORD [SI + ED_GAPEND]
    MOV DI, [SI + ED_GAPEND]
    MOV [DI], AL
    CMP AL, 0Ah
    JNE .not_nl
    DEC WORD [SI + ED_CURLINE]
    CALL _gap_recalc_col
    JMP .ok
.not_nl:
    DEC WORD [SI + ED_CURCOL]
.ok:
    CLC
    POP DI
    POP BX
    RET
.at_start:
    STC
    POP DI
    POP BX
    RET
ENDP

; ----------------------------------------------------------------------------
; gap_move_right - Move cursor one position right
; Input:  SI = editor state
; Output: AL = character crossed, CF=1 if already at end
; ----------------------------------------------------------------------------
gap_move_right: PROC
    PUSH BX
    PUSH DI
    MOV BX, [SI + ED_GAPEND]
    CMP BX, [SI + ED_BUFEND]
    JE .at_end
    MOV AL, [BX]
    INC WORD [SI + ED_GAPEND]
    MOV DI, [SI + ED_GAPSTART]
    MOV [DI], AL
    INC WORD [SI + ED_GAPSTART]
    CMP AL, 0Ah
    JNE .not_nl
    INC WORD [SI + ED_CURLINE]
    MOV WORD [SI + ED_CURCOL], 0
    JMP .ok
.not_nl:
    INC WORD [SI + ED_CURCOL]
.ok:
    CLC
    POP DI
    POP BX
    RET
.at_end:
    STC
    POP DI
    POP BX
    RET
ENDP

; ----------------------------------------------------------------------------
; gap_text_length - Get total text length (excluding gap)
; Input:  SI = editor state
; Output: AX = text length
; ----------------------------------------------------------------------------
gap_text_length: PROC
    MOV AX, [SI + ED_GAPSTART]
    SUB AX, [SI + ED_BUF]
    PUSH CX
    MOV CX, [SI + ED_BUFEND]
    SUB CX, [SI + ED_GAPEND]
    ADD AX, CX
    POP CX
    RET
ENDP

; ----------------------------------------------------------------------------
; gap_cursor_offset - Get cursor's logical offset
; Input:  SI = editor state
; Output: AX = offset (number of chars before gap)
; ----------------------------------------------------------------------------
gap_cursor_offset: PROC
    MOV AX, [SI + ED_GAPSTART]
    SUB AX, [SI + ED_BUF]
    RET
ENDP

; ----------------------------------------------------------------------------
; gap_get_char - Get character at a logical offset (gap-transparent)
; Input:  SI = editor state, BX = logical offset (0-based)
; Output: AL = character, CF=1 if offset out of range
; Preserves: BX, SI
; ----------------------------------------------------------------------------
gap_get_char: PROC
    PUSH DI
    PUSH DX
    MOV AX, [SI + ED_GAPSTART]
    SUB AX, [SI + ED_BUF]
    CMP BX, AX
    JAE .after_gap
    MOV DI, [SI + ED_BUF]
    ADD DI, BX
    MOV AL, [DI]
    CLC
    POP DX
    POP DI
    RET
.after_gap:
    MOV DX, BX
    SUB DX, AX
    MOV AX, [SI + ED_BUFEND]
    SUB AX, [SI + ED_GAPEND]
    CMP DX, AX
    JAE .out_of_range
    MOV DI, [SI + ED_GAPEND]
    ADD DI, DX
    MOV AL, [DI]
    CLC
    POP DX
    POP DI
    RET
.out_of_range:
    STC
    POP DX
    POP DI
    RET
ENDP

; ----------------------------------------------------------------------------
; gap_home - Move cursor to start of current line
; Input:  SI = editor state
; ----------------------------------------------------------------------------
gap_home: PROC
    PUSH CX
    MOV CX, [SI + ED_CURCOL]
    TEST CX, CX
    JZ .done
.loop:
    CALL gap_move_left
    DEC CX
    JNZ .loop
.done:
    POP CX
    RET
ENDP

; ----------------------------------------------------------------------------
; gap_end - Move cursor to end of current line
; Input:  SI = editor state
; ----------------------------------------------------------------------------
gap_end: PROC
    PUSH BX
.loop:
    MOV BX, [SI + ED_GAPEND]
    CMP BX, [SI + ED_BUFEND]
    JAE .done
    CMP BYTE [BX], 0Ah
    JE .done
    CALL gap_move_right
    JMP .loop
.done:
    POP BX
    RET
ENDP

; ----------------------------------------------------------------------------
; gap_move_up - Move cursor up one line, preserving column
; Input:  SI = editor state
; Output: CF=1 if already on first line
; ----------------------------------------------------------------------------
gap_move_up: PROC
    CMP WORD [SI + ED_CURLINE], 0
    JE .at_top
    PUSH BX
    PUSH CX
    MOV CX, [SI + ED_CURCOL]
    PUSH CX
    ; Move to start of current line
    CALL gap_home
    ; Move left once to cross newline into previous line
    CALL gap_move_left
    ; Move to start of previous line
    CALL gap_home
    ; Move right to target column or end of line
    POP CX
    TEST CX, CX
    JZ .done
.seek:
    MOV BX, [SI + ED_GAPEND]
    CMP BX, [SI + ED_BUFEND]
    JAE .done
    CMP BYTE [BX], 0Ah
    JE .done
    CALL gap_move_right
    DEC CX
    JNZ .seek
.done:
    POP CX
    POP BX
    CLC
    RET
.at_top:
    STC
    RET
ENDP

; ----------------------------------------------------------------------------
; gap_move_down - Move cursor down one line, preserving column
; Input:  SI = editor state
; Output: CF=1 if already on last line
; ----------------------------------------------------------------------------
gap_move_down: PROC
    PUSH CX
    MOV CX, [SI + ED_CURLINE]
    INC CX
    CMP CX, [SI + ED_LINECOUNT]
    JAE .at_bottom
    PUSH BX
    MOV CX, [SI + ED_CURCOL]
    PUSH CX
    ; Move to end of current line
    CALL gap_end
    ; Move right once to cross newline into next line
    CALL gap_move_right
    ; Move right to target column or end of line
    POP CX
    TEST CX, CX
    JZ .done
.seek:
    MOV BX, [SI + ED_GAPEND]
    CMP BX, [SI + ED_BUFEND]
    JAE .done
    CMP BYTE [BX], 0Ah
    JE .done
    CALL gap_move_right
    DEC CX
    JNZ .seek
.done:
    POP BX
    POP CX
    CLC
    RET
.at_bottom:
    POP CX
    STC
    RET
ENDP

; ----------------------------------------------------------------------------
; gap_ensure_visible - Adjust scroll to keep cursor visible
; Input:  SI = editor state, CL = visible height, CH = visible width
; Output: ED_SCROLLY and ED_SCROLLX updated if needed
; ----------------------------------------------------------------------------
gap_ensure_visible: PROC
    PUSH AX
    PUSH BX
    PUSH DX
    ; CL = visible height, CH = visible width

    ; --- Vertical: CURLINE in [SCROLLY, SCROLLY + height - 1] ---
    MOV AX, [SI + ED_CURLINE]
    CMP AX, [SI + ED_SCROLLY]
    JAE .not_above
    MOV [SI + ED_SCROLLY], AX
    JMP .do_horiz
.not_above:
    MOV BX, [SI + ED_SCROLLY]
    MOV DL, CL
    XOR DH, DH
    ADD BX, DX
    DEC BX
    CMP AX, BX
    JBE .do_horiz
    MOV BX, AX
    SUB BX, DX
    INC BX
    MOV [SI + ED_SCROLLY], BX

.do_horiz:
    ; --- Horizontal: CURCOL in [SCROLLX, SCROLLX + width - 1] ---
    MOV AX, [SI + ED_CURCOL]
    CMP AX, [SI + ED_SCROLLX]
    JAE .not_left
    MOV [SI + ED_SCROLLX], AX
    JMP .vis_done
.not_left:
    MOV BX, [SI + ED_SCROLLX]
    MOV DL, CH
    XOR DH, DH
    ADD BX, DX
    DEC BX
    CMP AX, BX
    JBE .vis_done
    MOV BX, AX
    SUB BX, DX
    INC BX
    MOV [SI + ED_SCROLLX], BX

.vis_done:
    POP DX
    POP BX
    POP AX
    RET
ENDP

; ----------------------------------------------------------------------------
; gap_find_line_phys - Find physical pointer to start of line N
; Walks buffer from start, counting LFs, skipping over the gap.
; Input:  SI = editor state, AX = line number (0-based)
; Output: DI = physical pointer to start of that line
; ----------------------------------------------------------------------------
gap_find_line_phys: PROC
    PUSH CX
    MOV DI, [SI + ED_BUF]
    MOV CX, AX
    TEST CX, CX
    JZ .found
.skip:
    ; Skip gap
    CMP DI, [SI + ED_GAPSTART]
    JNE .no_gap_skip
    MOV DI, [SI + ED_GAPEND]
.no_gap_skip:
    CMP DI, [SI + ED_BUFEND]
    JAE .found
    CMP BYTE [DI], 0Ah
    JNE .not_lf
    DEC CX
    JZ .found_lf
.not_lf:
    INC DI
    JMP .skip
.found_lf:
    INC DI
    ; Skip gap after LF
    CMP DI, [SI + ED_GAPSTART]
    JNE .found
    MOV DI, [SI + ED_GAPEND]
.found:
    POP CX
    RET
ENDP

; ----------------------------------------------------------------------------
; gap_goto_offset - Move cursor (gap) to a target logical offset efficiently
; Bulk-copies bytes between gap regions instead of looping gap_move_left/right.
; Input:  SI = editor state, AX = target logical offset
; Output: Gap repositioned, ED_CURLINE/ED_CURCOL recalculated
; Clobbers: AX
; ----------------------------------------------------------------------------
gap_goto_offset: PROC
    PUSH BX
    PUSH CX
    PUSH DI

    ; Current offset = GAPSTART - BUF
    MOV BX, [SI + ED_GAPSTART]
    SUB BX, [SI + ED_BUF]          ; BX = current offset

    CMP AX, BX
    JE .done
    JA .move_right

    ; --- Move left: target < current ---
    ; Copy bytes from end of pre-gap to end of post-gap (shift gap left)
    MOV CX, BX
    SUB CX, AX                     ; CX = bytes to move
    MOV BX, [SI + ED_GAPSTART]
    SUB BX, CX                     ; BX = source (GAPSTART - CX)
    MOV DI, [SI + ED_GAPEND]
    SUB DI, CX                     ; DI = dest (GAPEND - CX)
    PUSH CX
.copy_left:
    MOV AL, [BX]
    MOV [DI], AL
    INC BX
    INC DI
    DEC CX
    JNZ .copy_left
    POP CX
    SUB [SI + ED_GAPSTART], CX
    SUB [SI + ED_GAPEND], CX
    JMP .recalc

.move_right:
    ; --- Move right: target > current ---
    ; Copy bytes from start of post-gap to end of pre-gap (shift gap right)
    MOV CX, AX
    SUB CX, BX                     ; CX = bytes to move
    MOV BX, [SI + ED_GAPEND]       ; BX = source (GAPEND)
    MOV DI, [SI + ED_GAPSTART]     ; DI = dest (GAPSTART)
    PUSH CX
.copy_right:
    MOV AL, [BX]
    MOV [DI], AL
    INC BX
    INC DI
    DEC CX
    JNZ .copy_right
    POP CX
    ADD [SI + ED_GAPSTART], CX
    ADD [SI + ED_GAPEND], CX

.recalc:
    CALL _gap_recalc_line
    CALL _gap_recalc_col

.done:
    POP DI
    POP CX
    POP BX
    RET
ENDP

; ----------------------------------------------------------------------------
; gap_print_all - Print entire buffer content via INT 21h/02h
; For testing/debugging. Prints before-gap then after-gap regions.
; Input:  SI = editor state
; Preserves: SI, BX, CX
; ----------------------------------------------------------------------------
gap_print_all: PROC
    PUSH BX
    PUSH CX
    MOV BX, [SI + ED_BUF]
    MOV CX, [SI + ED_GAPSTART]
.before:
    CMP BX, CX
    JAE .mid
    MOV DL, [BX]
    MOV AH, 02h
    INT 21h
    INC BX
    JMP .before
.mid:
    MOV BX, [SI + ED_GAPEND]
    MOV CX, [SI + ED_BUFEND]
.after:
    CMP BX, CX
    JAE .done
    MOV DL, [BX]
    MOV AH, 02h
    INT 21h
    INC BX
    JMP .after
.done:
    POP CX
    POP BX
    RET
ENDP
