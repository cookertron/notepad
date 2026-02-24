; ============================================================================
; tui_dialog.inc - Standard Dialog Functions (msgbox, confirm, input)
; ============================================================================

; ----------------------------------------------------------------------------
; tui_dlg_strlen - Measure null-terminated string length
; Input:  SI = string ptr
; Output: CX = length (< 256)
; Preserves all other registers
; ----------------------------------------------------------------------------
tui_dlg_strlen: PROC
    PUSH SI
    XOR CX, CX
.loop:
    CMP BYTE [SI], 0
    JZ .done
    INC SI
    INC CX
    JMP .loop
.done:
    POP SI
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_dlg_run_modal - Create dialog window, run inner event loop, return result
; Input:  dlg_tmpl filled, control structs built in dlg_scratch
; Output: AL = FW_DLG_RESULT
; ----------------------------------------------------------------------------
tui_dlg_run_modal: PROC
    PUSH BX
    PUSH CX
    PUSH DX
    PUSH SI
    PUSH DI

    ; Save current framework state
    MOV AL, [fw_state + FW_RUNNING]
    PUSH AX
    MOV AL, [fw_state + FW_TOPWIN]
    PUSH AX

    ; Create the dialog window from template
    MOV SI, dlg_tmpl
    CALL tui_win_create         ; AL = slot index, or FFh if full
    CMP AL, 0FFh
    JZ .fail

    ; Set modal window
    MOV [fw_state + FW_MODAL_WIN], AL

    ; Default result = cancel (in case of unexpected exit)
    MOV BYTE [fw_state + FW_DLG_RESULT], DLG_CANCEL

    ; Start nested event loop
    MOV BYTE [fw_state + FW_RUNNING], 1
    MOV BYTE [fw_state + FW_DIRTY], 1
    CALL tui_run

    ; Nested loop exited - dialog has been closed by handler or Esc
    MOV BYTE [fw_state + FW_MODAL_WIN], 0FFh

    ; Save result before restoring state
    MOV BL, [fw_state + FW_DLG_RESULT]

    ; Restore previous framework state
    POP AX
    MOV [fw_state + FW_TOPWIN], AL
    POP AX
    MOV [fw_state + FW_RUNNING], AL

    ; Trigger redraw of parent windows
    MOV BYTE [fw_state + FW_DIRTY], 1

    MOV AL, BL                  ; AL = dialog result
    JMP .ret

.fail:
    ; Window table full - discard saved state, return cancel
    POP AX
    POP AX
    MOV AL, DLG_CANCEL

.ret:
    POP DI
    POP SI
    POP DX
    POP CX
    POP BX
    RET
ENDP

; ============================================================================
; Dialog button handlers (called via RET-trampoline: SI=ctrl, DI=window)
; ============================================================================

dlg_handler_ok:
    MOV BYTE [fw_state + FW_DLG_RESULT], DLG_OK
    MOV AL, [DI + WIN_ID]
    CALL tui_win_close
    MOV BYTE [fw_state + FW_RUNNING], 0
    RET

dlg_handler_cancel:
    MOV BYTE [fw_state + FW_DLG_RESULT], DLG_CANCEL
    MOV AL, [DI + WIN_ID]
    CALL tui_win_close
    MOV BYTE [fw_state + FW_RUNNING], 0
    RET

dlg_handler_yes:
    MOV BYTE [fw_state + FW_DLG_RESULT], DLG_YES
    MOV AL, [DI + WIN_ID]
    CALL tui_win_close
    MOV BYTE [fw_state + FW_RUNNING], 0
    RET

dlg_handler_no:
    MOV BYTE [fw_state + FW_DLG_RESULT], DLG_NO
    MOV AL, [DI + WIN_ID]
    CALL tui_win_close
    MOV BYTE [fw_state + FW_RUNNING], 0
    RET

; ============================================================================
; tui_dlg_msgbox - Message box with OK button
; Input:  SI = message string ptr, DI = title string ptr
; Output: (none - blocks until OK or Esc)
; ============================================================================
tui_dlg_msgbox: PROC
    PushAll
    PUSH DI                     ; save title ptr

    ; Measure message length
    CALL tui_dlg_strlen         ; CX = msg_len, SI preserved

    ; Compute interior width = max(msg_len+4, 12, 20)
    MOV AL, CL
    ADD AL, DLG_PAD
    CMP AL, DLG_BTN_OK_W + DLG_PAD
    JAE .w1
    MOV AL, DLG_BTN_OK_W + DLG_PAD
.w1:
    CMP AL, DLG_MIN_W
    JAE .w2
    MOV AL, DLG_MIN_W
.w2:
    MOV BL, AL                  ; BL = interior width
    ADD AL, 2
    MOV BH, AL                  ; BH = total width (with borders)

    ; Center dialog on screen
    MOV AL, SCR_W
    SUB AL, BH
    SHR AL, 1
    MOV DL, AL                  ; DL = dialog X
    MOV DH, (SCR_H - 6) / 2    ; DH = dialog Y (centered)

    ; ---- Build message label at dlg_label ----
    ; Center msg: label_x = (interior_w - msg_len) / 2
    MOV AL, BL
    SUB AL, CL
    SHR AL, 1

    MOV DI, dlg_label
    MOV BYTE [DI + CTRL_TYPE], CTYPE_LABEL
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE
    MOV [DI + CTRL_X], AL
    MOV BYTE [DI + CTRL_Y], 1
    MOV [DI + CTRL_W], CL
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], dlg_btn1
    MOV [DI + CTRL_TEXT], SI
    MOV BYTE [DI + CTRL_ATTR], CLR_LABEL
    MOV BYTE [DI + CTRL_FATTR], CLR_LABEL
    MOV WORD [DI + CTRL_HANDLER], 0

    ; ---- Build OK button at dlg_btn1 ----
    ; Center: btn_x = (interior_w - 8) / 2
    MOV AL, BL
    SUB AL, DLG_BTN_OK_W
    SHR AL, 1

    MOV DI, dlg_btn1
    MOV BYTE [DI + CTRL_TYPE], CTYPE_BUTTON
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    MOV [DI + CTRL_X], AL
    MOV BYTE [DI + CTRL_Y], 3
    MOV BYTE [DI + CTRL_W], DLG_BTN_OK_W
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], 0
    MOV WORD [DI + CTRL_TEXT], dlg_str_ok
    MOV BYTE [DI + CTRL_ATTR], CLR_BTN
    MOV BYTE [DI + CTRL_FATTR], CLR_BTN_FOCUS
    MOV WORD [DI + CTRL_HANDLER], dlg_handler_ok

    ; ---- Build window template at dlg_tmpl ----
    POP AX                      ; AX = title ptr
    MOV DI, dlg_tmpl
    MOV BYTE [DI + WIN_FLAGS], WINF_VISIBLE + WINF_BORDER + WINF_TITLE + WINF_SHADOW
    MOV [DI + WIN_X], DL
    MOV [DI + WIN_Y], DH
    MOV [DI + WIN_W], BH
    MOV BYTE [DI + WIN_H], 6
    MOV BYTE [DI + WIN_ATTR], CLR_WIN_BG
    MOV BYTE [DI + WIN_BATTR], CLR_WIN_BDR
    MOV BYTE [DI + WIN_TATTR], CLR_WIN_TTL
    MOV [DI + WIN_TITLE], AX
    MOV WORD [DI + WIN_FIRST], dlg_label
    MOV WORD [DI + WIN_FOCUS], dlg_btn1
    MOV WORD [DI + WIN_HANDLER], 0
    MOV BYTE [DI + WIN_ZORDER], 0
    MOV BYTE [DI + WIN_ID], 0
    MOV BYTE [DI + WIN_SCROLLX], 0
    MOV BYTE [DI + WIN_SCROLLY], 0

    CALL tui_dlg_run_modal

    PopAll
    RET
ENDP

; ============================================================================
; tui_dlg_confirm - Yes/No confirmation dialog
; Input:  SI = message string ptr, DI = title string ptr
; Output: AL = DLG_YES (1) or DLG_NO (0)
; ============================================================================
tui_dlg_confirm: PROC
    PushAll
    PUSH DI                     ; save title ptr

    ; Measure message length
    CALL tui_dlg_strlen         ; CX = msg_len

    ; Interior width = max(msg_len+4, 22, 20)
    ; where 22 = 8+8+2+4 (two buttons + gap + padding)
    MOV AL, CL
    ADD AL, DLG_PAD
    CMP AL, DLG_BTN_YN_W + DLG_BTN_YN_W + DLG_BTN_GAP + DLG_PAD
    JAE .w1
    MOV AL, DLG_BTN_YN_W + DLG_BTN_YN_W + DLG_BTN_GAP + DLG_PAD
.w1:
    CMP AL, DLG_MIN_W
    JAE .w2
    MOV AL, DLG_MIN_W
.w2:
    MOV BL, AL                  ; BL = interior width
    ADD AL, 2
    MOV BH, AL                  ; BH = total width

    ; Center on screen
    MOV AL, SCR_W
    SUB AL, BH
    SHR AL, 1
    MOV DL, AL                  ; DL = dialog X
    MOV DH, (SCR_H - 6) / 2    ; DH = dialog Y (centered)

    ; ---- Build message label ----
    MOV AL, BL
    SUB AL, CL
    SHR AL, 1

    MOV DI, dlg_label
    MOV BYTE [DI + CTRL_TYPE], CTYPE_LABEL
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE
    MOV [DI + CTRL_X], AL
    MOV BYTE [DI + CTRL_Y], 1
    MOV [DI + CTRL_W], CL
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], dlg_btn1
    MOV [DI + CTRL_TEXT], SI
    MOV BYTE [DI + CTRL_ATTR], CLR_LABEL
    MOV BYTE [DI + CTRL_FATTR], CLR_LABEL
    MOV WORD [DI + CTRL_HANDLER], 0

    ; ---- Build Yes and No buttons ----
    ; Center both: total_btn_w = 8+2+8 = 18
    ; btn_start_x = (interior_w - 18) / 2
    MOV AL, BL
    SUB AL, DLG_BTN_YN_W + DLG_BTN_GAP + DLG_BTN_YN_W
    SHR AL, 1
    MOV CH, AL                  ; CH = yes_x

    MOV DI, dlg_btn1
    MOV BYTE [DI + CTRL_TYPE], CTYPE_BUTTON
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    MOV [DI + CTRL_X], CH
    MOV BYTE [DI + CTRL_Y], 3
    MOV BYTE [DI + CTRL_W], DLG_BTN_YN_W
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], dlg_btn2
    MOV WORD [DI + CTRL_TEXT], dlg_str_yes
    MOV BYTE [DI + CTRL_ATTR], CLR_BTN
    MOV BYTE [DI + CTRL_FATTR], CLR_BTN_FOCUS
    MOV WORD [DI + CTRL_HANDLER], dlg_handler_yes

    ; No button: x = yes_x + 8 + 2
    MOV AL, CH
    ADD AL, DLG_BTN_YN_W + DLG_BTN_GAP

    MOV DI, dlg_btn2
    MOV BYTE [DI + CTRL_TYPE], CTYPE_BUTTON
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    MOV [DI + CTRL_X], AL
    MOV BYTE [DI + CTRL_Y], 3
    MOV BYTE [DI + CTRL_W], DLG_BTN_YN_W
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], 0
    MOV WORD [DI + CTRL_TEXT], dlg_str_no
    MOV BYTE [DI + CTRL_ATTR], CLR_BTN
    MOV BYTE [DI + CTRL_FATTR], CLR_BTN_FOCUS
    MOV WORD [DI + CTRL_HANDLER], dlg_handler_no

    ; ---- Build window template ----
    POP AX                      ; AX = title ptr
    MOV DI, dlg_tmpl
    MOV BYTE [DI + WIN_FLAGS], WINF_VISIBLE + WINF_BORDER + WINF_TITLE + WINF_SHADOW
    MOV [DI + WIN_X], DL
    MOV [DI + WIN_Y], DH
    MOV [DI + WIN_W], BH
    MOV BYTE [DI + WIN_H], 6
    MOV BYTE [DI + WIN_ATTR], CLR_WIN_BG
    MOV BYTE [DI + WIN_BATTR], CLR_WIN_BDR
    MOV BYTE [DI + WIN_TATTR], CLR_WIN_TTL
    MOV [DI + WIN_TITLE], AX
    MOV WORD [DI + WIN_FIRST], dlg_label
    MOV WORD [DI + WIN_FOCUS], dlg_btn1
    MOV WORD [DI + WIN_HANDLER], 0
    MOV BYTE [DI + WIN_ZORDER], 0
    MOV BYTE [DI + WIN_ID], 0
    MOV BYTE [DI + WIN_SCROLLX], 0
    MOV BYTE [DI + WIN_SCROLLY], 0

    CALL tui_dlg_run_modal

    PopAll
    MOV AL, [fw_state + FW_DLG_RESULT]
    RET
ENDP

; ============================================================================
; tui_dlg_input - Text input dialog
; Input:  SI = prompt string ptr, DI = title string ptr,
;         BX = text buffer ptr, CL = max length
; Output: AL = DLG_OK (1) or DLG_CANCEL (0)
;         Buffer at BX contains entered text (valid only if AL=DLG_OK)
; ============================================================================
tui_dlg_input: PROC
    PushAll

    ; Save caller inputs to temp area
    MOV [dlg_save_title], DI
    MOV [dlg_save_buf], BX
    MOV [dlg_save_maxlen], CL

    ; Measure prompt length
    CALL tui_dlg_strlen         ; CX = prompt_len, SI preserved

    ; Interior width = max(prompt_len+4, 24, 20)
    ; where 24 = 8+10+2+4 (OK + Cancel + gap + padding)
    MOV AL, CL
    ADD AL, DLG_PAD
    CMP AL, DLG_BTN_OK_W + DLG_BTN_CAN_W + DLG_BTN_GAP + DLG_PAD
    JAE .w1
    MOV AL, DLG_BTN_OK_W + DLG_BTN_CAN_W + DLG_BTN_GAP + DLG_PAD
.w1:
    CMP AL, DLG_MIN_W
    JAE .w2
    MOV AL, DLG_MIN_W
.w2:
    MOV BL, AL                  ; BL = interior width
    ADD AL, 2
    MOV BH, AL                  ; BH = total width

    ; Center on screen (height = 7)
    MOV AL, SCR_W
    SUB AL, BH
    SHR AL, 1
    MOV DL, AL                  ; DL = dialog X
    MOV DH, (SCR_H - 7) / 2    ; DH = dialog Y (centered)

    ; ---- Build prompt label at row 1, left-aligned (x=1) ----
    MOV DI, dlg_label
    MOV BYTE [DI + CTRL_TYPE], CTYPE_LABEL
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE
    MOV BYTE [DI + CTRL_X], 1
    MOV BYTE [DI + CTRL_Y], 1
    MOV [DI + CTRL_W], CL
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], dlg_textbox
    MOV [DI + CTRL_TEXT], SI
    MOV BYTE [DI + CTRL_ATTR], CLR_LABEL
    MOV BYTE [DI + CTRL_FATTR], CLR_LABEL
    MOV WORD [DI + CTRL_HANDLER], 0

    ; ---- Build textbox at row 2, x=1, width = interior_w - 2 ----
    MOV AL, BL
    SUB AL, 2                   ; textbox visible width

    MOV DI, dlg_textbox
    MOV BYTE [DI + CTRL_TYPE], CTYPE_TEXTBOX
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    MOV BYTE [DI + CTRL_X], 1
    MOV BYTE [DI + CTRL_Y], 2
    MOV [DI + CTRL_W], AL
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], dlg_btn1
    PUSH AX
    MOV AX, [dlg_save_buf]
    MOV [DI + CTRL_TEXT], AX
    POP AX
    MOV BYTE [DI + CTRL_ATTR], CLR_TEXTBOX
    MOV BYTE [DI + CTRL_FATTR], CLR_TB_FOCUS
    MOV WORD [DI + CTRL_HANDLER], 0
    PUSH AX
    MOV AL, [dlg_save_maxlen]
    MOV [DI + CTRL_TB_MAXLEN], AL
    POP AX
    MOV BYTE [DI + CTRL_TB_CURPOS], 0
    MOV BYTE [DI + CTRL_TB_SCROLL], 0
    ; Compute initial text length from buffer
    PUSH SI
    PUSH CX
    MOV SI, [dlg_save_buf]
    XOR CX, CX
.tb_len:
    CMP BYTE [SI], 0
    JZ .tb_len_done
    INC SI
    INC CL
    JMP .tb_len
.tb_len_done:
    MOV [DI + CTRL_TB_LEN], CL
    MOV [DI + CTRL_TB_CURPOS], CL   ; cursor at end of existing text
    POP CX
    POP SI

    ; ---- Build OK button at row 4 ----
    ; Center: total_btn_w = 8+2+10 = 20, start_x = (interior_w - 20) / 2
    MOV AL, BL
    SUB AL, DLG_BTN_OK_W + DLG_BTN_GAP + DLG_BTN_CAN_W
    SHR AL, 1
    MOV CH, AL                  ; CH = ok_x

    MOV DI, dlg_btn1
    MOV BYTE [DI + CTRL_TYPE], CTYPE_BUTTON
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    MOV [DI + CTRL_X], CH
    MOV BYTE [DI + CTRL_Y], 4
    MOV BYTE [DI + CTRL_W], DLG_BTN_OK_W
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], dlg_btn2
    MOV WORD [DI + CTRL_TEXT], dlg_str_ok
    MOV BYTE [DI + CTRL_ATTR], CLR_BTN
    MOV BYTE [DI + CTRL_FATTR], CLR_BTN_FOCUS
    MOV WORD [DI + CTRL_HANDLER], dlg_handler_ok

    ; ---- Build Cancel button ----
    MOV AL, CH
    ADD AL, DLG_BTN_OK_W + DLG_BTN_GAP

    MOV DI, dlg_btn2
    MOV BYTE [DI + CTRL_TYPE], CTYPE_BUTTON
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    MOV [DI + CTRL_X], AL
    MOV BYTE [DI + CTRL_Y], 4
    MOV BYTE [DI + CTRL_W], DLG_BTN_CAN_W
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], 0
    MOV WORD [DI + CTRL_TEXT], dlg_str_cancel
    MOV BYTE [DI + CTRL_ATTR], CLR_BTN
    MOV BYTE [DI + CTRL_FATTR], CLR_BTN_FOCUS
    MOV WORD [DI + CTRL_HANDLER], dlg_handler_cancel

    ; ---- Build window template ----
    MOV DI, dlg_tmpl
    MOV BYTE [DI + WIN_FLAGS], WINF_VISIBLE + WINF_BORDER + WINF_TITLE + WINF_SHADOW
    MOV [DI + WIN_X], DL
    MOV [DI + WIN_Y], DH
    MOV [DI + WIN_W], BH
    MOV BYTE [DI + WIN_H], 7
    MOV BYTE [DI + WIN_ATTR], CLR_WIN_BG
    MOV BYTE [DI + WIN_BATTR], CLR_WIN_BDR
    MOV BYTE [DI + WIN_TATTR], CLR_WIN_TTL
    PUSH AX
    MOV AX, [dlg_save_title]
    MOV [DI + WIN_TITLE], AX
    POP AX
    MOV WORD [DI + WIN_FIRST], dlg_label
    MOV WORD [DI + WIN_FOCUS], dlg_textbox
    MOV WORD [DI + WIN_HANDLER], 0
    MOV BYTE [DI + WIN_ZORDER], 0
    MOV BYTE [DI + WIN_ID], 0
    MOV BYTE [DI + WIN_SCROLLX], 0
    MOV BYTE [DI + WIN_SCROLLY], 0

    CALL tui_dlg_run_modal

    PopAll
    MOV AL, [fw_state + FW_DLG_RESULT]
    RET
ENDP

; ============================================================================
; tui_dlg_input2 - Dual text input dialog (two labeled fields + OK/Cancel)
; Caller pre-sets memory vars before calling:
;   [dlg_save_prompt2]  = second prompt string pointer
;   [dlg_save_buf2]     = second buffer pointer
;   [dlg_save_maxlen2]  = second max length
; Input:  SI = prompt1 string, DI = title string,
;         BX = buffer1 ptr, CL = maxlen1
; Output: AL = DLG_OK (1) or DLG_CANCEL (0)
;         Buffers contain entered text (valid only if AL=DLG_OK)
;
; Layout (interior rows 0-6, WIN_H=9 with border):
;   Row 0: Label 1       Row 1: Textbox 1
;   Row 2: (separator)   Row 3: Label 2
;   Row 4: Textbox 2     Row 5: (separator)
;   Row 6: [ OK ]  [ Cancel ]
; ============================================================================
tui_dlg_input2: PROC
    PushAll

    ; Save caller inputs to temp area
    MOV [dlg_save_title], DI
    MOV [dlg_save_buf], BX
    MOV [dlg_save_maxlen], CL

    ; Measure prompt1 length
    CALL tui_dlg_strlen         ; CX = prompt1_len, SI preserved
    MOV CH, CL                  ; CH = prompt1_len

    ; Measure prompt2 length
    PUSH SI
    MOV SI, [dlg_save_prompt2]
    CALL tui_dlg_strlen         ; CL = prompt2_len
    POP SI

    ; CH = prompt1_len, CL = prompt2_len → CH = max
    CMP CH, CL
    JAE .p1_longer
    MOV CH, CL
.p1_longer:

    ; Interior width = max(max_prompt_len+4, btn_row_w=24, DLG_MIN_W=20)
    MOV AL, CH
    ADD AL, DLG_PAD
    CMP AL, DLG_BTN_OK_W + DLG_BTN_CAN_W + DLG_BTN_GAP + DLG_PAD
    JAE .w1
    MOV AL, DLG_BTN_OK_W + DLG_BTN_CAN_W + DLG_BTN_GAP + DLG_PAD
.w1:
    CMP AL, DLG_MIN_W
    JAE .w2
    MOV AL, DLG_MIN_W
.w2:
    MOV BL, AL                  ; BL = interior width
    ADD AL, 2
    MOV BH, AL                  ; BH = total width (with borders)

    ; Center on screen (height = 9)
    MOV AL, SCR_W
    SUB AL, BH
    SHR AL, 1
    MOV DL, AL                  ; DL = dialog X
    MOV DH, (SCR_H - 9) / 2    ; DH = dialog Y (centered)

    ; ---- Build prompt1 label at row 0, x=1 ----
    PUSH CX
    CALL tui_dlg_strlen         ; CX = prompt1_len (SI still = prompt1)

    MOV DI, dlg_label
    MOV BYTE [DI + CTRL_TYPE], CTYPE_LABEL
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE
    MOV BYTE [DI + CTRL_X], 1
    MOV BYTE [DI + CTRL_Y], 0
    MOV [DI + CTRL_W], CL
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], dlg_textbox
    MOV [DI + CTRL_TEXT], SI
    MOV BYTE [DI + CTRL_ATTR], CLR_LABEL
    MOV BYTE [DI + CTRL_FATTR], CLR_LABEL
    MOV WORD [DI + CTRL_HANDLER], 0
    POP CX

    ; ---- Build textbox1 at row 1, x=1, width = interior_w - 2 ----
    MOV AL, BL
    SUB AL, 2                   ; textbox visible width

    MOV DI, dlg_textbox
    MOV BYTE [DI + CTRL_TYPE], CTYPE_TEXTBOX
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    MOV BYTE [DI + CTRL_X], 1
    MOV BYTE [DI + CTRL_Y], 1
    MOV [DI + CTRL_W], AL
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], dlg_label2
    PUSH AX
    MOV AX, [dlg_save_buf]
    MOV [DI + CTRL_TEXT], AX
    POP AX
    MOV BYTE [DI + CTRL_ATTR], CLR_TEXTBOX
    MOV BYTE [DI + CTRL_FATTR], CLR_TB_FOCUS
    MOV WORD [DI + CTRL_HANDLER], 0
    PUSH AX
    MOV AL, [dlg_save_maxlen]
    MOV [DI + CTRL_TB_MAXLEN], AL
    POP AX
    MOV BYTE [DI + CTRL_TB_CURPOS], 0
    MOV BYTE [DI + CTRL_TB_SCROLL], 0
    ; Compute initial text length from buffer1
    PUSH SI
    PUSH CX
    MOV SI, [dlg_save_buf]
    XOR CX, CX
.tb1_len:
    CMP BYTE [SI], 0
    JZ .tb1_len_done
    INC SI
    INC CL
    JMP .tb1_len
.tb1_len_done:
    MOV [DI + CTRL_TB_LEN], CL
    MOV [DI + CTRL_TB_CURPOS], CL   ; cursor at end of existing text
    POP CX
    POP SI

    ; ---- Build prompt2 label at row 3, x=1 ----
    PUSH SI
    PUSH CX
    MOV SI, [dlg_save_prompt2]
    CALL tui_dlg_strlen         ; CX = prompt2_len

    MOV DI, dlg_label2
    MOV BYTE [DI + CTRL_TYPE], CTYPE_LABEL
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE
    MOV BYTE [DI + CTRL_X], 1
    MOV BYTE [DI + CTRL_Y], 3
    MOV [DI + CTRL_W], CL
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], dlg_textbox2
    MOV [DI + CTRL_TEXT], SI
    MOV BYTE [DI + CTRL_ATTR], CLR_LABEL
    MOV BYTE [DI + CTRL_FATTR], CLR_LABEL
    MOV WORD [DI + CTRL_HANDLER], 0
    POP CX
    POP SI

    ; ---- Build textbox2 at row 4, x=1, width = interior_w - 2 ----
    MOV AL, BL
    SUB AL, 2                   ; textbox visible width

    MOV DI, dlg_textbox2
    MOV BYTE [DI + CTRL_TYPE], CTYPE_TEXTBOX
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    MOV BYTE [DI + CTRL_X], 1
    MOV BYTE [DI + CTRL_Y], 4
    MOV [DI + CTRL_W], AL
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], dlg_btn1
    PUSH AX
    MOV AX, [dlg_save_buf2]
    MOV [DI + CTRL_TEXT], AX
    POP AX
    MOV BYTE [DI + CTRL_ATTR], CLR_TEXTBOX
    MOV BYTE [DI + CTRL_FATTR], CLR_TB_FOCUS
    MOV WORD [DI + CTRL_HANDLER], 0
    PUSH AX
    MOV AL, [dlg_save_maxlen2]
    MOV [DI + CTRL_TB_MAXLEN], AL
    POP AX
    MOV BYTE [DI + CTRL_TB_CURPOS], 0
    MOV BYTE [DI + CTRL_TB_SCROLL], 0
    ; Compute initial text length from buffer2
    PUSH SI
    PUSH CX
    MOV SI, [dlg_save_buf2]
    XOR CX, CX
.tb2_len:
    CMP BYTE [SI], 0
    JZ .tb2_len_done
    INC SI
    INC CL
    JMP .tb2_len
.tb2_len_done:
    MOV [DI + CTRL_TB_LEN], CL
    MOV [DI + CTRL_TB_CURPOS], CL   ; cursor at end of existing text
    POP CX
    POP SI

    ; ---- Build OK button at row 6 ----
    MOV AL, BL
    SUB AL, DLG_BTN_OK_W + DLG_BTN_GAP + DLG_BTN_CAN_W
    SHR AL, 1
    MOV CH, AL                  ; CH = ok_x

    MOV DI, dlg_btn1
    MOV BYTE [DI + CTRL_TYPE], CTYPE_BUTTON
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    MOV [DI + CTRL_X], CH
    MOV BYTE [DI + CTRL_Y], 6
    MOV BYTE [DI + CTRL_W], DLG_BTN_OK_W
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], dlg_btn2
    MOV WORD [DI + CTRL_TEXT], dlg_str_ok
    MOV BYTE [DI + CTRL_ATTR], CLR_BTN
    MOV BYTE [DI + CTRL_FATTR], CLR_BTN_FOCUS
    MOV WORD [DI + CTRL_HANDLER], dlg_handler_ok

    ; ---- Build Cancel button at row 6 ----
    MOV AL, CH
    ADD AL, DLG_BTN_OK_W + DLG_BTN_GAP

    MOV DI, dlg_btn2
    MOV BYTE [DI + CTRL_TYPE], CTYPE_BUTTON
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    MOV [DI + CTRL_X], AL
    MOV BYTE [DI + CTRL_Y], 6
    MOV BYTE [DI + CTRL_W], DLG_BTN_CAN_W
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], 0
    MOV WORD [DI + CTRL_TEXT], dlg_str_cancel
    MOV BYTE [DI + CTRL_ATTR], CLR_BTN
    MOV BYTE [DI + CTRL_FATTR], CLR_BTN_FOCUS
    MOV WORD [DI + CTRL_HANDLER], dlg_handler_cancel

    ; ---- Build window template ----
    MOV DI, dlg_tmpl
    MOV BYTE [DI + WIN_FLAGS], WINF_VISIBLE + WINF_BORDER + WINF_TITLE + WINF_SHADOW
    MOV [DI + WIN_X], DL
    MOV [DI + WIN_Y], DH
    MOV [DI + WIN_W], BH
    MOV BYTE [DI + WIN_H], 9
    MOV BYTE [DI + WIN_ATTR], CLR_WIN_BG
    MOV BYTE [DI + WIN_BATTR], CLR_WIN_BDR
    MOV BYTE [DI + WIN_TATTR], CLR_WIN_TTL
    PUSH AX
    MOV AX, [dlg_save_title]
    MOV [DI + WIN_TITLE], AX
    POP AX
    MOV WORD [DI + WIN_FIRST], dlg_label
    MOV WORD [DI + WIN_FOCUS], dlg_textbox
    MOV WORD [DI + WIN_HANDLER], 0
    MOV BYTE [DI + WIN_ZORDER], 0
    MOV BYTE [DI + WIN_ID], 0
    MOV BYTE [DI + WIN_SCROLLX], 0
    MOV BYTE [DI + WIN_SCROLLY], 0

    CALL tui_dlg_run_modal

    PopAll
    MOV AL, [fw_state + FW_DLG_RESULT]
    RET
ENDP

; ============================================================================
; File Selector Dialog
; ============================================================================

; ----------------------------------------------------------------------------
; tui_file_get_path - Get current working directory into dlg_file_path
; Format: "\DIRNAME" or "\" for root
; ----------------------------------------------------------------------------
tui_file_get_path: PROC
    PUSH AX
    PUSH DX
    PUSH SI
    MOV BYTE [dlg_file_path], '\'
    MOV AH, 47h
    MOV DL, 0                       ; current drive
    MOV SI, dlg_file_path + 1       ; buffer (after leading \)
    INT 21h
    POP SI
    POP DX
    POP AX
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_file_enumerate - List current directory into dlg_file_* buffers
; Fills dlg_file_count, dlg_file_items[], dlg_file_names[]
; Always adds ".." as first entry for parent navigation
; ----------------------------------------------------------------------------
tui_file_enumerate: PROC
    PUSH AX
    PUSH BX
    PUSH CX
    PUSH DX
    PUSH SI
    PUSH DI
    PUSH BP

    ; Set DTA
    MOV AH, 1Ah
    MOV DX, dlg_file_dta
    INT 21h

    ; Init pointers
    MOV BP, dlg_file_names          ; BP = next name slot
    MOV BX, dlg_file_items          ; BX = next item ptr slot
    MOV BYTE [dlg_file_count], 0

    ; Entry 0: "..\0" (parent directory)
    MOV WORD [BX], BP
    MOV DI, BP
    MOV BYTE [DI], '.'
    MOV BYTE [DI + 1], '.'
    MOV BYTE [DI + 2], '\'
    MOV BYTE [DI + 3], 0
    ADD BP, DLG_FILE_NAMELEN
    ADD BX, 2
    MOV BYTE [dlg_file_count], 1

    ; FindFirst *.* with directories included
    MOV AH, 4Eh
    MOV CX, 10h                     ; include directories
    MOV DX, dlg_file_pattern
    INT 21h
    JC .done

.process:
    ; Skip "." and ".." from FindFirst/FindNext results
    CMP BYTE [dlg_file_dta + 1Eh], '.'
    JNZ .not_dot
    CMP BYTE [dlg_file_dta + 1Fh], 0
    JZ .find_next                   ; skip "."
    CMP BYTE [dlg_file_dta + 1Fh], '.'
    JNZ .not_dot
    CMP BYTE [dlg_file_dta + 20h], 0
    JZ .find_next                   ; skip ".."

.not_dot:
    ; Store item pointer: items[count] = name slot
    MOV WORD [BX], BP

    ; Copy filename from DTA to name slot
    MOV SI, dlg_file_dta + 1Eh
    MOV DI, BP
.copy_name:
    MOV AL, [SI]
    MOV [DI], AL
    OR AL, AL
    JZ .name_done
    INC SI
    INC DI
    JMP .copy_name

.name_done:
    ; If directory attribute set, append '\'
    TEST BYTE [dlg_file_dta + 15h], 10h
    JZ .not_dir
    MOV BYTE [DI], '\'
    MOV BYTE [DI + 1], 0
.not_dir:
    ADD BP, DLG_FILE_NAMELEN
    ADD BX, 2
    INC BYTE [dlg_file_count]
    CMP BYTE [dlg_file_count], DLG_FILE_MAXENT
    JAE .done

.find_next:
    MOV AH, 4Fh
    INT 21h
    JNC .process

.done:
    POP BP
    POP DI
    POP SI
    POP DX
    POP CX
    POP BX
    POP AX
    RET
ENDP

; ----------------------------------------------------------------------------
; dlg_file_handle_sel - Shared logic for listbox/OK button: process selection
; Input:  SI = listbox control (must point to the file listbox)
; If directory: chdir into it, re-enumerate, update listbox + path
; If file: copy name to result buffer, close dialog with DLG_OK
; ----------------------------------------------------------------------------
dlg_file_handle_sel: PROC
    PUSH AX
    PUSH BX
    PUSH CX
    PUSH DX
    PUSH DI

    ; Look up selected item name
    MOV AL, [SI + CTRL_LB_SEL]
    XOR AH, AH
    SHL AX, 1
    MOV BX, [SI + CTRL_LB_ITEMS]
    ADD BX, AX
    MOV BX, [BX]                    ; BX = item string ptr

    ; Find end of string, check for trailing '\'
    MOV DX, BX                      ; DX = string start
.find_end:
    CMP BYTE [BX], 0
    JZ .check_type
    INC BX
    JMP .find_end
.check_type:
    CMP BX, DX                      ; empty string?
    JZ .ret
    DEC BX                          ; BX -> last char before null
    CMP BYTE [BX], '\'
    JZ .is_dir

    ; --- FILE SELECTED: copy to result buffer, close dialog ---
    MOV BX, DX                      ; BX = source name start
    MOV DI, [dlg_save_buf]
    MOV CL, [dlg_save_maxlen]
.copy_file:
    MOV AL, [BX]
    MOV [DI], AL
    OR AL, AL
    JZ .close_ok
    INC BX
    INC DI
    DEC CL
    JNZ .copy_file
    MOV BYTE [DI], 0               ; null-terminate if truncated
.close_ok:
    MOV BYTE [fw_state + FW_DLG_RESULT], DLG_OK
    MOV AL, [fw_state + FW_MODAL_WIN]
    CALL tui_win_close
    MOV BYTE [fw_state + FW_RUNNING], 0
    JMP .ret

.is_dir:
    ; Strip trailing '\' for chdir
    MOV BYTE [BX], 0
    ; DX = directory name start
    MOV AH, 3Bh
    INT 21h                         ; chdir
    JC .ret                         ; failed, do nothing
    ; Re-enumerate directory
    CALL tui_file_enumerate
    ; Update listbox control
    MOV AL, [dlg_file_count]
    MOV [SI + CTRL_LB_COUNT], AL
    MOV BYTE [SI + CTRL_LB_SEL], 0
    MOV BYTE [SI + CTRL_LB_SCROLL], 0
    ; Update path display
    CALL tui_file_get_path
    MOV BYTE [fw_state + FW_DIRTY], 1

.ret:
    POP DI
    POP DX
    POP CX
    POP BX
    POP AX
    RET
ENDP

; Listbox handler (Enter on listbox or mouse click on item)
; SI = listbox control, DI = window
dlg_file_lb_handler:
    JMP dlg_file_handle_sel

; OK button handler - reads selection from listbox, applies same logic
; SI = button control (ignored), DI = window
dlg_file_ok_handler:
    PUSH SI
    MOV SI, dlg_listbox
    CALL dlg_file_handle_sel
    POP SI
    RET

; ----------------------------------------------------------------------------
; tui_file_detect_drives - Probe drives A-Z using IOCTL, fill dlg_drive_*
; Output: dlg_drive_count, dlg_drive_items[], dlg_drive_names[] populated
; ----------------------------------------------------------------------------
tui_file_detect_drives: PROC
    PUSH AX
    PUSH BX
    PUSH CX
    PUSH DX
    PUSH DI

    MOV BYTE [dlg_drive_count], 0
    MOV DI, dlg_drive_names          ; DI = next name slot
    MOV CX, dlg_drive_items          ; CX = next item pointer slot
    MOV BL, 1                        ; BL = drive number (1=A, 2=B, ...)

.detect_loop:
    CMP BL, 27
    JAE .detect_done

    ; IOCTL: check if block device
    MOV AX, 4409h
    INT 21h
    JC .detect_next                  ; CF=1 means drive not available

    ; Drive exists — store "X:" string
    MOV AL, BL
    ADD AL, 'A' - 1                  ; convert 1-based to letter
    MOV [DI], AL
    MOV BYTE [DI + 1], ':'
    MOV BYTE [DI + 2], 0

    ; Store pointer in items array
    PUSH BX
    MOV BX, CX
    MOV [BX], DI
    POP BX
    ADD CX, 2                        ; advance item pointer
    ADD DI, 3                        ; advance name slot
    INC BYTE [dlg_drive_count]

.detect_next:
    INC BL
    JMP .detect_loop

.detect_done:
    POP DI
    POP DX
    POP CX
    POP BX
    POP AX
    RET
ENDP

; ----------------------------------------------------------------------------
; _dlg_file_find_cur_drive - Find index of current drive in dlg_drive_items
; Output: AL = index into dlg_drive_items (0-based), or 0 if not found
; ----------------------------------------------------------------------------
_dlg_file_find_cur_drive: PROC
    PUSH BX
    PUSH CX
    PUSH SI

    ; Get current drive letter
    MOV AH, 19h
    INT 21h                          ; AL = 0=A, 1=B, ...
    ADD AL, 'A'                      ; AL = drive letter
    MOV AH, AL                       ; AH = target letter

    ; Linear search dlg_drive_items
    MOV SI, dlg_drive_items
    MOV CL, [dlg_drive_count]
    XOR BL, BL                       ; BL = index

.find_loop:
    OR CL, CL
    JZ .find_notfound
    PUSH BX
    MOV BX, [SI]                     ; BX = ptr to "X:" string
    CMP [BX], AH                     ; compare first char
    POP BX
    JZ .find_found
    ADD SI, 2
    INC BL
    DEC CL
    JMP .find_loop

.find_found:
    MOV AL, BL
    JMP .find_ret

.find_notfound:
    XOR AL, AL                       ; default to index 0

.find_ret:
    POP SI
    POP CX
    POP BX
    RET
ENDP

; ----------------------------------------------------------------------------
; dlg_file_drive_handler - Dropdown commit handler for drive selection
; Input: SI = dropdown control, DI = dialog window
; Switches to selected drive, chdir root, re-enumerates files
; ----------------------------------------------------------------------------
dlg_file_drive_handler: PROC
    PUSH AX
    PUSH BX
    PUSH DX

    ; Get selected drive letter from items[SEL]
    MOV AL, [SI + CTRL_DD_SEL]
    XOR AH, AH
    SHL AX, 1
    MOV BX, [SI + CTRL_DD_ITEMS]
    ADD BX, AX
    MOV BX, [BX]                     ; BX = ptr to "X:" string
    MOV AL, [BX]                     ; AL = drive letter

    ; Select disk: AH=0Eh, DL = drive (0-based)
    SUB AL, 'A'
    MOV DL, AL
    MOV AH, 0Eh
    INT 21h

    ; Chdir to root: AH=3Bh, DX = "\"
    MOV AH, 3Bh
    MOV DX, dlg_file_rootdir
    INT 21h

    ; Re-enumerate files and update path
    CALL tui_file_enumerate
    CALL tui_file_get_path

    ; Update listbox control
    PUSH SI
    MOV SI, dlg_listbox
    MOV AL, [dlg_file_count]
    MOV [SI + CTRL_LB_COUNT], AL
    MOV BYTE [SI + CTRL_LB_SEL], 0
    MOV BYTE [SI + CTRL_LB_SCROLL], 0
    POP SI

    MOV BYTE [fw_state + FW_DIRTY], 1

    POP DX
    POP BX
    POP AX
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_dlg_file - File selector dialog
; Input:  DI = title string ptr, BX = result buffer ptr, CL = max filename len
; Output: AL = DLG_OK (1) or DLG_CANCEL (0)
;         Result buffer contains selected filename if DLG_OK
;         CWD left at the directory where the file was selected
; ----------------------------------------------------------------------------
tui_dlg_file: PROC
    PushAll

    ; Save caller params
    MOV [dlg_save_title], DI
    MOV [dlg_save_buf], BX
    MOV [dlg_save_maxlen], CL

    ; Detect available drives and find current drive index
    CALL tui_file_detect_drives
    CALL _dlg_file_find_cur_drive    ; AL = current drive index
    PUSH AX                          ; save drive index for dropdown SEL

    ; Populate path display and file list
    CALL tui_file_get_path
    CALL tui_file_enumerate

    ; --- Build "Drive:" label (dlg_label2) at row 0, col 0 ---
    POP AX                           ; AL = current drive index
    PUSH AX                          ; re-save for dropdown below
    MOV DI, dlg_label2
    MOV BYTE [DI + CTRL_TYPE], CTYPE_LABEL
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE
    MOV BYTE [DI + CTRL_X], 0
    MOV BYTE [DI + CTRL_Y], 0
    MOV BYTE [DI + CTRL_W], 6
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], dlg_drive_dd
    MOV WORD [DI + CTRL_TEXT], dlg_str_drive
    MOV BYTE [DI + CTRL_ATTR], CLR_LABEL
    MOV BYTE [DI + CTRL_FATTR], CLR_LABEL
    MOV WORD [DI + CTRL_HANDLER], 0

    ; --- Build drive dropdown (dlg_drive_dd) at row 0, col 7, width 5 ---
    POP AX                           ; AL = current drive index
    MOV DI, dlg_drive_dd
    MOV BYTE [DI + CTRL_TYPE], CTYPE_DROPDOWN
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    MOV BYTE [DI + CTRL_X], 7
    MOV BYTE [DI + CTRL_Y], 0
    MOV BYTE [DI + CTRL_W], 5
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], dlg_label
    MOV WORD [DI + CTRL_TEXT], 0
    MOV BYTE [DI + CTRL_ATTR], CLR_DD_NORMAL
    MOV BYTE [DI + CTRL_FATTR], CLR_DD_FOCUS
    MOV WORD [DI + CTRL_HANDLER], dlg_file_drive_handler
    MOV WORD [DI + CTRL_DD_ITEMS], dlg_drive_items
    PUSH AX
    MOV AL, [dlg_drive_count]
    MOV [DI + CTRL_DD_COUNT], AL
    POP AX
    MOV [DI + CTRL_DD_SEL], AL
    MOV [DI + CTRL_DD_TMPSEL], AL
    MOV BYTE [DI + CTRL_DD_SCROLL], 0
    MOV BYTE [DI + CTRL_DD_MAXVIS], 8

    ; --- Build path label (dlg_label) at row 0, col 13, width 19 ---
    MOV DI, dlg_label
    MOV BYTE [DI + CTRL_TYPE], CTYPE_LABEL
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE
    MOV BYTE [DI + CTRL_X], 13
    MOV BYTE [DI + CTRL_Y], 0
    MOV BYTE [DI + CTRL_W], 19
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], dlg_listbox
    MOV WORD [DI + CTRL_TEXT], dlg_file_path
    MOV BYTE [DI + CTRL_ATTR], CLR_LABEL
    MOV BYTE [DI + CTRL_FATTR], CLR_LABEL
    MOV WORD [DI + CTRL_HANDLER], 0

    ; --- Build listbox (dlg_listbox) ---
    MOV DI, dlg_listbox
    MOV BYTE [DI + CTRL_TYPE], CTYPE_LISTBOX
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    MOV BYTE [DI + CTRL_X], 0
    MOV BYTE [DI + CTRL_Y], 1
    MOV BYTE [DI + CTRL_W], 32
    MOV BYTE [DI + CTRL_H], 8
    MOV WORD [DI + CTRL_NEXT], dlg_btn1
    MOV WORD [DI + CTRL_TEXT], 0
    MOV BYTE [DI + CTRL_ATTR], CLR_LB_NORMAL
    MOV BYTE [DI + CTRL_FATTR], CLR_LB_FOCUS
    MOV WORD [DI + CTRL_HANDLER], dlg_file_lb_handler
    MOV WORD [DI + CTRL_LB_ITEMS], dlg_file_items
    MOV AL, [dlg_file_count]
    MOV [DI + CTRL_LB_COUNT], AL
    MOV BYTE [DI + CTRL_LB_SEL], 0
    MOV BYTE [DI + CTRL_LB_SCROLL], 0

    ; --- Build OK button (dlg_btn1) ---
    MOV DI, dlg_btn1
    MOV BYTE [DI + CTRL_TYPE], CTYPE_BUTTON
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    MOV BYTE [DI + CTRL_X], 8
    MOV BYTE [DI + CTRL_Y], 10
    MOV BYTE [DI + CTRL_W], DLG_BTN_OK_W
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], dlg_btn2
    MOV WORD [DI + CTRL_TEXT], dlg_str_ok
    MOV BYTE [DI + CTRL_ATTR], CLR_BTN
    MOV BYTE [DI + CTRL_FATTR], CLR_BTN_FOCUS
    MOV WORD [DI + CTRL_HANDLER], dlg_file_ok_handler

    ; --- Build Cancel button (dlg_btn2) ---
    MOV DI, dlg_btn2
    MOV BYTE [DI + CTRL_TYPE], CTYPE_BUTTON
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    MOV BYTE [DI + CTRL_X], 18
    MOV BYTE [DI + CTRL_Y], 10
    MOV BYTE [DI + CTRL_W], DLG_BTN_CAN_W
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], 0
    MOV WORD [DI + CTRL_TEXT], dlg_str_cancel
    MOV BYTE [DI + CTRL_ATTR], CLR_BTN
    MOV BYTE [DI + CTRL_FATTR], CLR_BTN_FOCUS
    MOV WORD [DI + CTRL_HANDLER], dlg_handler_cancel

    ; --- Build window template ---
    MOV DI, dlg_tmpl
    MOV BYTE [DI + WIN_FLAGS], WINF_VISIBLE + WINF_BORDER + WINF_TITLE + WINF_SHADOW
    MOV BYTE [DI + WIN_X], (SCR_W - 34) / 2
    MOV BYTE [DI + WIN_Y], (SCR_H - 14) / 2
    MOV BYTE [DI + WIN_W], 34
    MOV BYTE [DI + WIN_H], 14
    MOV BYTE [DI + WIN_ATTR], CLR_WIN_BG
    MOV BYTE [DI + WIN_BATTR], CLR_WIN_BDR
    MOV BYTE [DI + WIN_TATTR], CLR_WIN_TTL
    PUSH AX
    MOV AX, [dlg_save_title]
    MOV [DI + WIN_TITLE], AX
    POP AX
    MOV WORD [DI + WIN_FIRST], dlg_label2
    MOV WORD [DI + WIN_FOCUS], dlg_listbox
    MOV WORD [DI + WIN_HANDLER], 0
    MOV BYTE [DI + WIN_ZORDER], 0
    MOV BYTE [DI + WIN_ID], 0
    MOV BYTE [DI + WIN_SCROLLX], 0
    MOV BYTE [DI + WIN_SCROLLY], 0

    CALL tui_dlg_run_modal

    PopAll
    MOV AL, [fw_state + FW_DLG_RESULT]
    RET
ENDP
