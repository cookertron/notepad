; ============================================================================
; editor_find.inc - Find/Replace Operations for DOS Notepad
;
; Provides Find (Ctrl+F), Find Next (F3), and Replace (Ctrl+R).
; Forward search from cursor, case-sensitive, no wrap-around.
;
; Requires: editor_const.inc, editor_gap.inc, editor_ctrl.inc,
;           editor_clip.inc included first.
; ============================================================================

; --- Scratch variables ---
ed_find_scanpos:   DW 0            ; current scan position during search
ed_find_textlen:   DW 0            ; cached text length during search

; --- String constants ---
str_find_dlg_ttl:  DB 'Find', 0
str_find_prompt:   DB 'Find what:', 0
str_repl_dlg_ttl:  DB 'Replace', 0
str_repl_prompt2:  DB 'Replace with:', 0
str_not_found:     DB 'Not found', 0
str_nf_title:      DB 'Find', 0

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

; ----------------------------------------------------------------------------
; editor_replace - Show Replace dialog (tui_dlg_input2)
; If current selection matches search term: replace it, then find next.
; Otherwise: just find next.
; Uses global ed_state.
; ----------------------------------------------------------------------------
editor_replace: PROC
    PUSH SI
    PUSH BX
    PUSH CX

    ; Set up second input for tui_dlg_input2
    MOV WORD [dlg_save_prompt2], str_repl_prompt2
    MOV WORD [dlg_save_buf2], ed_replace_buf
    MOV BYTE [dlg_save_maxlen2], 63

    ; Show Replace dialog
    MOV SI, str_find_prompt
    MOV DI, str_repl_dlg_ttl
    MOV BX, ed_find_buf
    MOV CL, 63
    CALL tui_dlg_input2
    CMP AL, DLG_OK
    JNE .cancelled

    ; Compute search term length
    CALL _ed_compute_find_len
    CMP WORD [ed_find_len], 0
    JE .cancelled

    MOV SI, ed_state

    ; Check if current selection matches search term
    CALL _ed_check_sel_matches
    JC .just_find                   ; no match or no selection → just find

    ; Selection matches: replace it
    CALL undo_capture_selection
    CALL _ed_sel_delete
    ; Insert replacement text
    CALL _ed_insert_replace_buf
    ; Record undo (compound: sel_delete + insert)
    CALL _ed_replace_buf_len        ; CX = replacement length
    CALL undo_record_compound

    ; Find next occurrence
    CALL _ed_find_forward
    JC .replace_not_found

    CALL _ed_find_ensure_visible
    JMP .cancelled

.just_find:
    CALL _ed_find_forward
    JC .replace_not_found

    CALL _ed_find_ensure_visible
    JMP .cancelled

.replace_not_found:
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
