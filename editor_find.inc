; ============================================================================
; editor_find.inc - Find/Replace Operations for DOS Notepad
;
; Provides Find (Ctrl+F), Find Next (F3), and Replace (Ctrl+R).
; Forward and backward search, case-sensitive, no wrap-around.
; Custom Replace dialog with Replace, Replace All, direction checkbox.
;
; Requires: editor_const.inc, editor_gap.inc, editor_ctrl.inc,
;           editor_clip.inc included first.
; ============================================================================

; --- Scratch variables ---
ed_find_scanpos:   DW 0            ; current scan position during search
ed_find_textlen:   DW 0            ; cached text length during search
ed_find_up:        DB 0            ; direction checkbox: 0=down, 1=up
ed_repl_count:     DW 0            ; replace all counter
ed_repl_in_sel:    DB 0            ; "In Selection" checkbox: 0=off, 1=on
ed_repl_sel_start: DW 0            ; captured selection start before dialog
ed_repl_sel_end:   DW 0            ; captured selection end before dialog
ed_repl_has_sel:   DB 0            ; 1 if selection was active when dialog opened

; --- String constants ---
str_find_dlg_ttl:  DB 'Find', 0
str_find_prompt:   DB 'Find what:', 0
str_repl_dlg_ttl:  DB 'Replace', 0
str_repl_prompt2:  DB 'Replace with:', 0
str_not_found:     DB 'Not found', 0
str_nf_title:      DB 'Find', 0
str_repl_search_up: DB 'Search Up', 0
str_repl_in_sel:    DB 'In Selection', 0
str_repl_btn:      DB 'Replace', 0
str_repl_all_btn:  DB 'Replace All', 0
str_close_btn:     DB 'Close', 0
str_repl_made:     DB ' replacements made', 0
str_repl_count_msg: RESB 28        ; temp buffer for "N replacements made"

; ----------------------------------------------------------------------------
; _ed_compute_find_len - Measure ed_find_buf, store length in ed_find_len
; Output: CX = length
; Preserves: AX, BX, DI, SI
; ----------------------------------------------------------------------------
_ed_compute_find_len: PROC
    PUSH DI
    MOV DI, ed_find_buf
    XOR CX, CX
.loop:
    CMP BYTE [DI], 0
    JE .done
    INC DI
    INC CX
    JMP .loop
.done:
    MOV [ed_find_len], CX
    POP DI
    RET
ENDP

; ----------------------------------------------------------------------------
; _ed_find_forward - Core search: scan forward from cursor position
; Input:  SI = editor state, ed_find_buf/ed_find_len set
; Output: CF=0 if found (selection set: anchor=match start, cursor=match end)
;         CF=1 if not found
; Clobbers: AX, BX, CX, DI
; ----------------------------------------------------------------------------
_ed_find_forward: PROC
    PUSH DX

    ; Get text length
    CALL gap_text_length
    MOV [ed_find_textlen], AX

    ; Get search term length - if 0, not found
    MOV CX, [ed_find_len]
    TEST CX, CX
    JZ .not_found

    ; Start scanning from cursor position
    CALL gap_cursor_offset
    MOV [ed_find_scanpos], AX

.scan_loop:
    ; Check if scanpos + find_len > textlen
    MOV AX, [ed_find_scanpos]
    ADD AX, [ed_find_len]
    CMP AX, [ed_find_textlen]
    JA .not_found

    ; Compare ed_find_buf[0..find_len-1] against text[scanpos..]
    MOV CX, [ed_find_len]
    MOV DI, ed_find_buf            ; DI = pattern pointer
    MOV BX, [ed_find_scanpos]      ; BX = text logical offset

.compare:
    CALL gap_get_char               ; AL = text[BX]
    CMP AL, [DI]
    JNE .mismatch
    INC BX
    INC DI
    DEC CX
    JNZ .compare

    ; Match found! Set selection: anchor = scanpos, cursor = scanpos + find_len
    MOV AX, [ed_find_scanpos]
    MOV [SI + ED_SELANCHOR], AX
    ADD AX, [ed_find_len]
    CALL gap_goto_offset

    POP DX
    CLC
    RET

.mismatch:
    INC WORD [ed_find_scanpos]
    JMP .scan_loop

.not_found:
    POP DX
    STC
    RET
ENDP

; ----------------------------------------------------------------------------
; _ed_find_backward - Core search: scan backward from cursor position
; Input:  SI = editor state, ed_find_buf/ed_find_len set
; Output: CF=0 if found (selection set: anchor=match start, cursor=match end)
;         CF=1 if not found
; Clobbers: AX, BX, CX, DI
; ----------------------------------------------------------------------------
_ed_find_backward: PROC
    PUSH DX

    ; Get text length
    CALL gap_text_length
    MOV [ed_find_textlen], AX

    ; Get search term length - if 0, not found
    MOV CX, [ed_find_len]
    TEST CX, CX
    JZ .not_found

    ; Start scanning from cursor_offset - 1
    CALL gap_cursor_offset
    TEST AX, AX
    JZ .not_found               ; cursor at start, nothing backward
    DEC AX
    MOV [ed_find_scanpos], AX

.scan_loop:
    ; Check if scanpos + find_len > textlen
    MOV AX, [ed_find_scanpos]
    ADD AX, [ed_find_len]
    CMP AX, [ed_find_textlen]
    JA .dec_pos

    ; Compare ed_find_buf against text[scanpos..]
    MOV CX, [ed_find_len]
    MOV DI, ed_find_buf
    MOV BX, [ed_find_scanpos]

.compare:
    CALL gap_get_char               ; AL = text[BX]
    CMP AL, [DI]
    JNE .mismatch
    INC BX
    INC DI
    DEC CX
    JNZ .compare

    ; Match found! Set selection: anchor = scanpos, cursor = scanpos + find_len
    MOV AX, [ed_find_scanpos]
    MOV [SI + ED_SELANCHOR], AX
    ADD AX, [ed_find_len]
    CALL gap_goto_offset

    POP DX
    CLC
    RET

.mismatch:
.dec_pos:
    MOV AX, [ed_find_scanpos]
    TEST AX, AX
    JZ .not_found
    DEC WORD [ed_find_scanpos]
    JMP .scan_loop

.not_found:
    POP DX
    STC
    RET
ENDP

; ----------------------------------------------------------------------------
; editor_find - Show Find dialog, search forward, "Not found" if miss
; Uses global ed_state. Called from menu handler or Ctrl+F.
; ----------------------------------------------------------------------------
editor_find: PROC
    PUSH SI
    PUSH BX
    PUSH CX

    ; Show Find dialog (single input)
    MOV SI, str_find_prompt
    MOV DI, str_find_dlg_ttl
    MOV BX, ed_find_buf
    MOV CL, 63
    CALL tui_dlg_input
    CMP AL, DLG_OK
    JNE .cancelled

    ; Compute search term length
    CALL _ed_compute_find_len
    CMP WORD [ed_find_len], 0
    JE .cancelled

    ; Search forward from cursor
    MOV SI, ed_state
    CALL _ed_find_forward
    JC .show_not_found

    ; Found: ensure visible and repaint
    CALL _ed_find_ensure_visible
    JMP .cancelled

.show_not_found:
    MOV SI, str_not_found
    MOV DI, str_nf_title
    CALL tui_dlg_msgbox
    MOV BYTE [fw_state + FW_DIRTY], 1

.cancelled:
    POP CX
    POP BX
    POP SI
    RET
ENDP

; ----------------------------------------------------------------------------
; editor_find_next - F3: repeat last search (no dialog)
; If no prior search term, falls through to editor_find.
; Uses global ed_state.
; ----------------------------------------------------------------------------
editor_find_next: PROC
    ; Check if we have a search term
    CMP BYTE [ed_find_buf], 0
    JE editor_find                  ; no term yet, show dialog

    PUSH SI
    PUSH BX
    PUSH CX

    ; Recompute length (in case)
    CALL _ed_compute_find_len

    MOV SI, ed_state
    CALL _ed_find_forward
    JC .show_not_found

    CALL _ed_find_ensure_visible
    JMP .done

.show_not_found:
    MOV SI, str_not_found
    MOV DI, str_nf_title
    CALL tui_dlg_msgbox
    MOV BYTE [fw_state + FW_DIRTY], 1

.done:
    POP CX
    POP BX
    POP SI
    RET
ENDP

; ============================================================================
; Replace Dialog - Custom 3-button dialog with direction checkbox
; ============================================================================

; ----------------------------------------------------------------------------
; _dlg_handler_repl_all - Button handler: Replace All (result = 2)
; Called via RET-trampoline: SI=ctrl, DI=window
; ----------------------------------------------------------------------------
_dlg_handler_repl_all:
    MOV BYTE [fw_state + FW_DLG_RESULT], DLG_REPL_ALL
    MOV AL, [DI + WIN_ID]
    CALL tui_win_close
    MOV BYTE [fw_state + FW_RUNNING], 0
    RET

; ----------------------------------------------------------------------------
; _ed_show_replace_dlg - Build and show custom Replace dialog
; Dialog: 2 textboxes, 1 checkbox, 3 buttons (Replace/Replace All/Close)
; Output: AL = 0 (Close), 1 (Replace), 2 (Replace All)
; ----------------------------------------------------------------------------
_ed_show_replace_dlg: PROC
    PushAll

    ; Fixed dimensions: interior 44 wide, 7 rows (0-6). Total 46x9 with borders.

    ; --- Label 1: "Find what:" at row 0, x=1 ---
    MOV DI, dlg_label
    MOV BYTE [DI + CTRL_TYPE], CTYPE_LABEL
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE
    MOV BYTE [DI + CTRL_X], 1
    MOV BYTE [DI + CTRL_Y], 0
    MOV BYTE [DI + CTRL_W], 10
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], dlg_textbox
    MOV WORD [DI + CTRL_TEXT], str_find_prompt
    MOV BYTE [DI + CTRL_ATTR], CLR_LABEL
    MOV BYTE [DI + CTRL_FATTR], CLR_LABEL
    MOV WORD [DI + CTRL_HANDLER], 0

    ; --- Textbox 1: Find term at row 1, x=1, width=42 ---
    MOV DI, dlg_textbox
    MOV BYTE [DI + CTRL_TYPE], CTYPE_TEXTBOX
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    MOV BYTE [DI + CTRL_X], 1
    MOV BYTE [DI + CTRL_Y], 1
    MOV BYTE [DI + CTRL_W], 42
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], dlg_label2
    MOV WORD [DI + CTRL_TEXT], ed_find_buf
    MOV BYTE [DI + CTRL_ATTR], CLR_TEXTBOX
    MOV BYTE [DI + CTRL_FATTR], CLR_TB_FOCUS
    MOV WORD [DI + CTRL_HANDLER], 0
    MOV BYTE [DI + CTRL_TB_MAXLEN], 63
    MOV BYTE [DI + CTRL_TB_CURPOS], 0
    MOV BYTE [DI + CTRL_TB_SCROLL], 0
    ; Compute initial text length from ed_find_buf
    PUSH SI
    PUSH CX
    MOV SI, ed_find_buf
    XOR CX, CX
.tb1_len:
    CMP BYTE [SI], 0
    JZ .tb1_len_done
    INC SI
    INC CL
    JMP .tb1_len
.tb1_len_done:
    MOV [DI + CTRL_TB_LEN], CL
    MOV [DI + CTRL_TB_CURPOS], CL
    POP CX
    POP SI

    ; --- Label 2: "Replace with:" at row 2, x=1 ---
    MOV DI, dlg_label2
    MOV BYTE [DI + CTRL_TYPE], CTYPE_LABEL
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE
    MOV BYTE [DI + CTRL_X], 1
    MOV BYTE [DI + CTRL_Y], 2
    MOV BYTE [DI + CTRL_W], 13
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], dlg_textbox2
    MOV WORD [DI + CTRL_TEXT], str_repl_prompt2
    MOV BYTE [DI + CTRL_ATTR], CLR_LABEL
    MOV BYTE [DI + CTRL_FATTR], CLR_LABEL
    MOV WORD [DI + CTRL_HANDLER], 0

    ; --- Textbox 2: Replace term at row 3, x=1, width=42 ---
    MOV DI, dlg_textbox2
    MOV BYTE [DI + CTRL_TYPE], CTYPE_TEXTBOX
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    MOV BYTE [DI + CTRL_X], 1
    MOV BYTE [DI + CTRL_Y], 3
    MOV BYTE [DI + CTRL_W], 42
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], dlg_checkbox
    MOV WORD [DI + CTRL_TEXT], ed_replace_buf
    MOV BYTE [DI + CTRL_ATTR], CLR_TEXTBOX
    MOV BYTE [DI + CTRL_FATTR], CLR_TB_FOCUS
    MOV WORD [DI + CTRL_HANDLER], 0
    MOV BYTE [DI + CTRL_TB_MAXLEN], 63
    MOV BYTE [DI + CTRL_TB_CURPOS], 0
    MOV BYTE [DI + CTRL_TB_SCROLL], 0
    ; Compute initial text length from ed_replace_buf
    PUSH SI
    PUSH CX
    MOV SI, ed_replace_buf
    XOR CX, CX
.tb2_len:
    CMP BYTE [SI], 0
    JZ .tb2_len_done
    INC SI
    INC CL
    JMP .tb2_len
.tb2_len_done:
    MOV [DI + CTRL_TB_LEN], CL
    MOV [DI + CTRL_TB_CURPOS], CL
    POP CX
    POP SI

    ; --- Checkbox 1: "Search Up" at row 4, x=1, width=13 ---
    MOV DI, dlg_checkbox
    MOV BYTE [DI + CTRL_TYPE], CTYPE_CHECKBOX
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    MOV BYTE [DI + CTRL_X], 1
    MOV BYTE [DI + CTRL_Y], 4
    MOV BYTE [DI + CTRL_W], 13
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], dlg_checkbox2
    MOV WORD [DI + CTRL_TEXT], str_repl_search_up
    MOV BYTE [DI + CTRL_ATTR], CLR_CHECKBOX
    MOV BYTE [DI + CTRL_FATTR], CLR_CB_FOCUS
    MOV WORD [DI + CTRL_HANDLER], 0
    MOV AL, [ed_find_up]
    MOV [DI + CTRL_CB_STATE], AL

    ; --- Checkbox 2: "In Selection" at row 4, x=20, width=16 ---
    MOV DI, dlg_checkbox2
    MOV BYTE [DI + CTRL_TYPE], CTYPE_CHECKBOX
    MOV BYTE [DI + CTRL_X], 20
    MOV BYTE [DI + CTRL_Y], 4
    MOV BYTE [DI + CTRL_W], 16
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], dlg_btn1
    MOV WORD [DI + CTRL_TEXT], str_repl_in_sel
    MOV BYTE [DI + CTRL_ATTR], CLR_CHECKBOX
    MOV BYTE [DI + CTRL_FATTR], CLR_CB_FOCUS
    MOV WORD [DI + CTRL_HANDLER], 0
    ; Set flags based on whether selection is active
    CMP BYTE [ed_repl_has_sel], 0
    JE .cb2_disabled
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    JMP .cb2_state
.cb2_disabled:
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE
    MOV BYTE [ed_repl_in_sel], 0
.cb2_state:
    MOV AL, [ed_repl_in_sel]
    MOV [DI + CTRL_CB_STATE], AL

    ; --- Replace button at row 6, x=1, width=11 ---
    MOV DI, dlg_btn1
    MOV BYTE [DI + CTRL_TYPE], CTYPE_BUTTON
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    MOV BYTE [DI + CTRL_X], 1
    MOV BYTE [DI + CTRL_Y], 6
    MOV BYTE [DI + CTRL_W], 11
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], dlg_btn2
    MOV WORD [DI + CTRL_TEXT], str_repl_btn
    MOV BYTE [DI + CTRL_ATTR], CLR_BTN
    MOV BYTE [DI + CTRL_FATTR], CLR_BTN_FOCUS
    MOV WORD [DI + CTRL_HANDLER], dlg_handler_ok

    ; --- Replace All button at row 6, x=14, width=15 ---
    MOV DI, dlg_btn2
    MOV BYTE [DI + CTRL_TYPE], CTYPE_BUTTON
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    MOV BYTE [DI + CTRL_X], 14
    MOV BYTE [DI + CTRL_Y], 6
    MOV BYTE [DI + CTRL_W], 15
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], dlg_btn3
    MOV WORD [DI + CTRL_TEXT], str_repl_all_btn
    MOV BYTE [DI + CTRL_ATTR], CLR_BTN
    MOV BYTE [DI + CTRL_FATTR], CLR_BTN_FOCUS
    MOV WORD [DI + CTRL_HANDLER], _dlg_handler_repl_all

    ; --- Close button at row 6, x=34, width=9 ---
    MOV DI, dlg_btn3
    MOV BYTE [DI + CTRL_TYPE], CTYPE_BUTTON
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    MOV BYTE [DI + CTRL_X], 34
    MOV BYTE [DI + CTRL_Y], 6
    MOV BYTE [DI + CTRL_W], 9
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], 0
    MOV WORD [DI + CTRL_TEXT], str_close_btn
    MOV BYTE [DI + CTRL_ATTR], CLR_BTN
    MOV BYTE [DI + CTRL_FATTR], CLR_BTN_FOCUS
    MOV WORD [DI + CTRL_HANDLER], dlg_handler_cancel

    ; --- Window template ---
    MOV DI, dlg_tmpl
    MOV BYTE [DI + WIN_FLAGS], WINF_VISIBLE + WINF_BORDER + WINF_TITLE + WINF_SHADOW
    MOV BYTE [DI + WIN_X], 17           ; (80-46)/2
    MOV BYTE [DI + WIN_Y], 8            ; (25-9)/2
    MOV BYTE [DI + WIN_W], 46
    MOV BYTE [DI + WIN_H], 9
    MOV BYTE [DI + WIN_ATTR], CLR_WIN_BG
    MOV BYTE [DI + WIN_BATTR], CLR_WIN_BDR
    MOV BYTE [DI + WIN_TATTR], CLR_WIN_TTL
    MOV WORD [DI + WIN_TITLE], str_repl_dlg_ttl
    MOV WORD [DI + WIN_FIRST], dlg_label
    MOV WORD [DI + WIN_FOCUS], dlg_textbox
    MOV WORD [DI + WIN_HANDLER], 0
    MOV BYTE [DI + WIN_ZORDER], 0
    MOV BYTE [DI + WIN_ID], 0
    MOV BYTE [DI + WIN_SCROLLX], 0
    MOV BYTE [DI + WIN_SCROLLY], 0

    CALL tui_dlg_run_modal

    ; Save checkbox states back (scratch data still intact)
    MOV AL, [dlg_checkbox + CTRL_CB_STATE]
    MOV [ed_find_up], AL
    MOV AL, [dlg_checkbox2 + CTRL_CB_STATE]
    MOV [ed_repl_in_sel], AL

    PopAll
    MOV AL, [fw_state + FW_DLG_RESULT]
    RET
ENDP

; ----------------------------------------------------------------------------
; _ed_replace_one - Find and replace first occurrence
; Searches forward or backward based on ed_find_up.
; Input:  SI = editor state, ed_find_buf/ed_find_len/ed_replace_buf set
; Output: CF=0 if replaced, CF=1 if not found
; Preserves: SI
; ----------------------------------------------------------------------------
_ed_replace_one: PROC
    PUSH BX
    PUSH CX
    PUSH DI

    ; Search in appropriate direction
    CMP BYTE [ed_find_up], 0
    JNE .search_up
    CALL _ed_find_forward
    JMP .check_found
.search_up:
    CALL _ed_find_backward
.check_found:
    JC .not_found

    ; Found: selection is set (anchor=match start, cursor=match end)
    CALL undo_capture_selection
    CALL _ed_sel_delete
    CALL _ed_insert_replace_buf
    CALL _ed_replace_buf_len        ; CX = replacement length
    CALL undo_record_compound

    POP DI
    POP CX
    POP BX
    CLC
    RET

.not_found:
    POP DI
    POP CX
    POP BX
    STC
    RET
ENDP

; ----------------------------------------------------------------------------
; _ed_format_repl_count - Format "N replacements made" into str_repl_count_msg
; Input:  AX = count
; Clobbers: AX
; Preserves: BX, CX, DX, SI, DI
; ----------------------------------------------------------------------------
_ed_format_repl_count: PROC
    PUSH SI
    PUSH DI

    ; Format number into ed_numbuf
    CALL _ed_format_number          ; DI = first digit in ed_numbuf

    ; Copy digits to str_repl_count_msg
    MOV SI, DI
    MOV DI, str_repl_count_msg
.copy_num:
    MOV AL, [SI]
    TEST AL, AL
    JZ .append_text
    MOV [DI], AL
    INC SI
    INC DI
    JMP .copy_num

.append_text:
    ; Append " replacements made"
    MOV SI, str_repl_made
.copy_text:
    MOV AL, [SI]
    MOV [DI], AL
    TEST AL, AL
    JZ .done
    INC SI
    INC DI
    JMP .copy_text

.done:
    POP DI
    POP SI
    RET
ENDP

; ----------------------------------------------------------------------------
; _ed_replace_all - Replace all occurrences (forward from start)
; Shows count msgbox or "Not found" msgbox.
; Input:  SI = editor state, ed_find_buf/ed_find_len/ed_replace_buf set
; Preserves: SI
; ----------------------------------------------------------------------------
_ed_replace_all: PROC
    PUSH BX
    PUSH CX
    PUSH DI

    ; Move cursor to start of document
    XOR AX, AX
    CALL gap_goto_offset

    ; Clear any selection
    MOV WORD [SI + ED_SELANCHOR], 0FFFFh

    ; Precompute replacement length (constant throughout loop)
    CALL _ed_replace_buf_len        ; CX = replacement length
    PUSH CX                         ; save on stack

    MOV WORD [ed_repl_count], 0

.loop:
    CALL _ed_find_forward
    JC .loop_done

    ; Found: replace this occurrence
    CALL undo_capture_selection
    CALL _ed_sel_delete
    CALL _ed_insert_replace_buf
    POP CX                          ; restore replacement length
    PUSH CX                         ; keep on stack for next iteration
    CALL undo_record_compound

    INC WORD [ed_repl_count]
    JMP .loop

.loop_done:
    POP CX                          ; clean up replacement length

    ; Save SI (editor state) for after msgbox
    PUSH SI

    ; Check count and show appropriate message
    MOV AX, [ed_repl_count]
    TEST AX, AX
    JZ .show_not_found

    ; Format "N replacements made" and show
    CALL _ed_format_repl_count
    MOV SI, str_repl_count_msg
    MOV DI, str_repl_dlg_ttl        ; title = "Replace"
    CALL tui_dlg_msgbox
    JMP .msg_done

.show_not_found:
    MOV SI, str_not_found
    MOV DI, str_nf_title
    CALL tui_dlg_msgbox

.msg_done:
    POP SI                          ; restore editor state pointer
    MOV BYTE [fw_state + FW_DIRTY], 1

    POP DI
    POP CX
    POP BX
    RET
ENDP

; ----------------------------------------------------------------------------
; _ed_replace_all_in_sel - Replace all within captured selection bounds
; Searches forward from ed_repl_sel_start, stops when match would exceed
; ed_repl_sel_end. Adjusts ed_repl_sel_end after each replacement.
; Input:  SI = editor state, ed_find_buf/ed_find_len/ed_replace_buf set,
;         ed_repl_sel_start/ed_repl_sel_end set
; Preserves: SI
; ----------------------------------------------------------------------------
_ed_replace_all_in_sel: PROC
    PUSH BX
    PUSH CX
    PUSH DI
    PUSH DX

    ; Move cursor to selection start
    MOV AX, [ed_repl_sel_start]
    CALL gap_goto_offset

    ; Clear any selection
    MOV WORD [SI + ED_SELANCHOR], 0FFFFh

    ; Precompute replacement length
    CALL _ed_replace_buf_len        ; CX = replacement length
    PUSH CX                         ; save on stack

    MOV WORD [ed_repl_count], 0

.loop:
    CALL _ed_find_forward
    JC .loop_done

    ; Found: check if match end exceeds selection boundary
    ; After _ed_find_forward: anchor = match start, cursor = match end
    CALL gap_cursor_offset          ; AX = match end offset
    CMP AX, [ed_repl_sel_end]
    JA .undo_find

    ; Match is within bounds — replace it
    CALL undo_capture_selection
    CALL _ed_sel_delete
    CALL _ed_insert_replace_buf
    POP CX                          ; restore replacement length
    PUSH CX                         ; keep on stack

    ; Adjust sel_end: += (replace_len - find_len)
    ; DX = replace_len - find_len (signed)
    MOV AX, CX                     ; AX = replace_len
    SUB AX, [ed_find_len]          ; AX = replace_len - find_len (signed)
    ADD [ed_repl_sel_end], AX

    CALL undo_record_compound

    INC WORD [ed_repl_count]
    JMP .loop

.undo_find:
    ; Match was beyond selection — undo the selection set by _ed_find_forward
    MOV WORD [SI + ED_SELANCHOR], 0FFFFh

.loop_done:
    POP CX                          ; clean up replacement length

    ; Save SI for after msgbox
    PUSH SI

    ; Show count or not-found
    MOV AX, [ed_repl_count]
    TEST AX, AX
    JZ .show_not_found

    CALL _ed_format_repl_count
    MOV SI, str_repl_count_msg
    MOV DI, str_repl_dlg_ttl
    CALL tui_dlg_msgbox
    JMP .msg_done

.show_not_found:
    MOV SI, str_not_found
    MOV DI, str_nf_title
    CALL tui_dlg_msgbox

.msg_done:
    POP SI
    MOV BYTE [fw_state + FW_DIRTY], 1

    POP DX
    POP DI
    POP CX
    POP BX
    RET
ENDP

; ----------------------------------------------------------------------------
; editor_replace - Show Replace dialog, execute chosen action
; Uses global ed_state. Called from menu handler or Ctrl+R.
; ----------------------------------------------------------------------------
editor_replace: PROC
    PUSH SI
    PUSH BX
    PUSH CX

    ; Capture selection bounds before dialog
    MOV SI, ed_state
    MOV BYTE [ed_repl_has_sel], 0
    CMP WORD [SI + ED_SELANCHOR], 0FFFFh
    JE .no_sel
    ; Selection active: capture start/end
    MOV BYTE [ed_repl_has_sel], 1
    CALL gap_cursor_offset          ; AX = cursor offset
    MOV BX, [SI + ED_SELANCHOR]
    CMP AX, BX
    JAE .sel_ordered
    XCHG AX, BX
.sel_ordered:
    ; AX = sel_end, BX = sel_start
    MOV [ed_repl_sel_start], BX
    MOV [ed_repl_sel_end], AX
.no_sel:

    ; Show custom replace dialog
    CALL _ed_show_replace_dlg
    ; AL = 0 (Close), 1 (Replace), 2 (Replace All)
    TEST AL, AL
    JZ .cancelled

    ; Save result
    MOV BL, AL

    ; Compute search term length
    CALL _ed_compute_find_len
    CMP WORD [ed_find_len], 0
    JE .cancelled

    MOV SI, ed_state

    CMP BL, DLG_REPL_ALL
    JE .do_replace_all

    ; --- Replace one occurrence ---
    CALL _ed_replace_one
    JC .show_not_found
    CALL _ed_find_ensure_visible
    JMP .cancelled

.do_replace_all:
    ; Check if "In Selection" is checked
    CMP BYTE [ed_repl_in_sel], 0
    JNE .do_replace_all_in_sel
    CALL _ed_replace_all
    CALL _ed_find_ensure_visible
    JMP .cancelled

.do_replace_all_in_sel:
    CALL _ed_replace_all_in_sel
    CALL _ed_find_ensure_visible
    JMP .cancelled

.show_not_found:
    MOV SI, str_not_found
    MOV DI, str_nf_title
    CALL tui_dlg_msgbox
    MOV BYTE [fw_state + FW_DIRTY], 1

.cancelled:
    POP CX
    POP BX
    POP SI
    RET
ENDP

; ----------------------------------------------------------------------------
; _ed_check_sel_matches - Check if current selection equals ed_find_buf
; Input:  SI = editor state, ed_find_buf/ed_find_len set
; Output: CF=0 if selection matches, CF=1 if no match or no selection
; Preserves: SI
; ----------------------------------------------------------------------------
_ed_check_sel_matches: PROC
    PUSH AX
    PUSH BX
    PUSH CX
    PUSH DI

    ; Check for active selection
    CMP WORD [SI + ED_SELANCHOR], 0FFFFh
    JE .no_match

    ; Get selection range
    CALL gap_cursor_offset          ; AX = cursor offset
    MOV BX, [SI + ED_SELANCHOR]
    ; Ensure AX >= BX (sel_start in BX, sel_end in AX)
    CMP AX, BX
    JAE .ordered
    XCHG AX, BX
.ordered:
    ; AX = sel_end, BX = sel_start
    ; Selection length = AX - BX
    MOV CX, AX
    SUB CX, BX                     ; CX = selection length

    ; Must equal search term length
    CMP CX, [ed_find_len]
    JNE .no_match

    ; Compare each character
    MOV DI, ed_find_buf
    ; BX = sel_start (logical offset for gap_get_char)
.cmp_loop:
    TEST CX, CX
    JZ .match
    CALL gap_get_char               ; AL = text[BX]
    CMP AL, [DI]
    JNE .no_match
    INC BX
    INC DI
    DEC CX
    JMP .cmp_loop

.match:
    POP DI
    POP CX
    POP BX
    POP AX
    CLC
    RET

.no_match:
    POP DI
    POP CX
    POP BX
    POP AX
    STC
    RET
ENDP

; ----------------------------------------------------------------------------
; _ed_insert_replace_buf - Insert ed_replace_buf contents at cursor
; Input:  SI = editor state
; Preserves: SI
; ----------------------------------------------------------------------------
_ed_insert_replace_buf: PROC
    PUSH AX
    PUSH DI

    MOV DI, ed_replace_buf
.loop:
    MOV AL, [DI]
    TEST AL, AL
    JZ .done
    CALL gap_insert_char
    JC .done                        ; buffer full
    INC DI
    JMP .loop
.done:
    OR BYTE [SI + ED_FLAGS], EDF_MODIFIED

    POP DI
    POP AX
    RET
ENDP

; ----------------------------------------------------------------------------
; _ed_replace_buf_len - Measure ed_replace_buf string length
; Output: CX = length
; Preserves: AX, BX, DI, SI
; ----------------------------------------------------------------------------
_ed_replace_buf_len: PROC
    PUSH DI
    MOV DI, ed_replace_buf
    XOR CX, CX
.loop:
    CMP BYTE [DI], 0
    JE .done
    INC DI
    INC CX
    JMP .loop
.done:
    POP DI
    RET
ENDP

; ----------------------------------------------------------------------------
; _ed_find_ensure_visible - After a find, ensure cursor visible and repaint
; Input:  SI = editor state
; Preserves: SI
; ----------------------------------------------------------------------------
_ed_find_ensure_visible: PROC
    PUSH CX
    ; Use editor control dimensions for ensure_visible
    MOV CL, [ctrl_editor + CTRL_H]
    DEC CL                          ; subtract status bar row
    MOV CH, [ctrl_editor + CTRL_W]
    CALL gap_ensure_visible
    MOV BYTE [fw_state + FW_DIRTY], 1
    POP CX
    RET
ENDP
