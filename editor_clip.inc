; ============================================================================
; editor_clip.inc - Clipboard Operations (Cut/Copy/Paste)
;
; Internal clipboard buffer for the editor. All functions take SI = editor state.
;
; Requires: editor_const.inc, editor_gap.inc, editor_ctrl.inc included first.
; ============================================================================

ed_paste_had_sel: DB 0              ; flag: paste had selection to delete first

; ----------------------------------------------------------------------------
; editor_clipboard_copy - Copy selected text to clipboard
; Input:  SI = editor state
; If no selection, does nothing.
; Clears selection and sets FW_DIRTY on success.
; ----------------------------------------------------------------------------
editor_clipboard_copy: PROC
    PUSH AX
    PUSH BX
    PUSH CX
    PUSH DI

    ; Check for active selection
    CMP WORD [SI + ED_SELANCHOR], 0FFFFh
    JE .done

    ; Get cursor offset and anchor
    CALL gap_cursor_offset          ; AX = cursor logical offset
    MOV BX, [SI + ED_SELANCHOR]

    ; sel_start = min, sel_end = max
    CMP AX, BX
    JBE .ordered
    XCHG AX, BX
.ordered:
    ; AX = sel_start, BX = sel_end
    MOV CX, BX
    SUB CX, AX                     ; CX = selection length
    TEST CX, CX
    JZ .clear_sel

    ; Copy selected bytes to clipboard: BX=index into buffer, DI=dest index
    MOV BX, AX                     ; BX = source logical offset (sel_start)
    MOV DI, ed_clipboard
.copy_loop:
    CALL gap_get_char               ; AL = char at offset BX
    MOV [DI], AL
    INC BX
    INC DI
    DEC CX
    JNZ .copy_loop

    ; Store clipboard length
    MOV AX, DI
    SUB AX, ed_clipboard
    MOV [ed_cliplen], AX

.clear_sel:
    ; Clear selection
    MOV WORD [SI + ED_SELANCHOR], 0FFFFh
    MOV BYTE [fw_state + FW_DIRTY], 1

.done:
    POP DI
    POP CX
    POP BX
    POP AX
    RET
ENDP

; ----------------------------------------------------------------------------
; editor_clipboard_cut - Cut selected text to clipboard
; Input:  SI = editor state
; If no selection, does nothing.
; Copies text to clipboard, then deletes selection. Sets modified + FW_DIRTY.
; ----------------------------------------------------------------------------
editor_clipboard_cut: PROC
    PUSH AX
    PUSH BX
    PUSH CX
    PUSH DI

    ; Check for active selection
    CMP WORD [SI + ED_SELANCHOR], 0FFFFh
    JE .done

    ; Get cursor offset and anchor
    CALL gap_cursor_offset          ; AX = cursor logical offset
    MOV BX, [SI + ED_SELANCHOR]

    ; sel_start = min, sel_end = max
    CMP AX, BX
    JBE .ordered
    XCHG AX, BX
.ordered:
    ; AX = sel_start, BX = sel_end
    MOV CX, BX
    SUB CX, AX                     ; CX = selection length
    TEST CX, CX
    JZ .clear

    ; Copy selected bytes to clipboard
    MOV BX, AX                     ; BX = source logical offset (sel_start)
    MOV DI, ed_clipboard
.copy_loop:
    CALL gap_get_char               ; AL = char at offset BX
    MOV [DI], AL
    INC BX
    INC DI
    DEC CX
    JNZ .copy_loop

    ; Store clipboard length
    MOV AX, DI
    SUB AX, ed_clipboard
    MOV [ed_cliplen], AX

    ; Delete the selection (anchor is still set)
    CALL undo_capture_selection
    CALL _ed_sel_delete             ; clears anchor, sets modified
    CALL undo_record_sel_delete

    MOV BYTE [fw_state + FW_DIRTY], 1
    JMP .done

.clear:
    MOV WORD [SI + ED_SELANCHOR], 0FFFFh

.done:
    POP DI
    POP CX
    POP BX
    POP AX
    RET
ENDP

; ----------------------------------------------------------------------------
; editor_clipboard_paste - Paste clipboard contents at cursor
; Input:  SI = editor state
; If clipboard is empty, does nothing.
; If selection active, deletes it first. Sets modified + FW_DIRTY.
; ----------------------------------------------------------------------------
editor_clipboard_paste: PROC
    PUSH AX
    PUSH BX
    PUSH CX
    PUSH DI

    ; Check clipboard length
    MOV CX, [ed_cliplen]
    TEST CX, CX
    JZ .done

    ; If selection active, delete it first
    CMP WORD [SI + ED_SELANCHOR], 0FFFFh
    JE .no_sel
    CALL undo_capture_selection
    CALL _ed_sel_delete
    MOV BYTE [ed_paste_had_sel], 1
    JMP .do_paste
.no_sel:
    MOV BYTE [ed_paste_had_sel], 0
    CALL undo_save_cursor

.do_paste:
    ; Insert each byte from clipboard, count actual inserts
    MOV DI, ed_clipboard
    PUSH DX
    XOR DX, DX                     ; DX = chars inserted count
.paste_loop:
    MOV AL, [DI]
    CALL gap_insert_char            ; CF=1 if buffer full
    JC .paste_done
    INC DX
    INC DI
    DEC CX
    JNZ .paste_loop
.paste_done:
    ; Record undo
    MOV CX, DX                     ; CX = chars inserted
    POP DX
    TEST CX, CX
    JZ .skip_undo
    CMP BYTE [ed_paste_had_sel], 0
    JE .paste_no_sel_undo
    CALL undo_record_compound
    JMP .skip_undo
.paste_no_sel_undo:
    CALL undo_record_insert_n
.skip_undo:

    ; Set modified flag
    OR BYTE [SI + ED_FLAGS], EDF_MODIFIED
    MOV BYTE [fw_state + FW_DIRTY], 1

.done:
    POP DI
    POP CX
    POP BX
    POP AX
    RET
ENDP
