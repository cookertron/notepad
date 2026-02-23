; ============================================================================
; tui_event.inc - Input and Event Loop
; ============================================================================

; ----------------------------------------------------------------------------
; tui_poll_key - Non-blocking keyboard poll using INT 21h/06h
; Output: If no key:        ZF=1
;         If normal key:    ZF=0, AH=KTYPE_NORMAL (0), AL=char
;         If extended key:  ZF=0, AH=KTYPE_EXTENDED (1), AL=scan code
; ----------------------------------------------------------------------------
tui_poll_key: PROC
    MOV AH, 06h
    MOV DL, 0FFh
    INT 21h
    JZ .no_key              ; ZF=1 means no char available

    ; Got a byte in AL - check if it's an extended key prefix (00h or E0h)
    OR AL, AL
    JZ .extended
    CMP AL, 0E0h            ; enhanced keyboard prefix
    JZ .extended

    ; Normal key: AH=0, AL=char, clear ZF
    MOV AH, KTYPE_NORMAL
    OR AL, AL              ; clear ZF (AL is nonzero since we checked above)
    RET

.extended:
    ; Read second byte (scan code)
    MOV AH, 06h
    MOV DL, 0FFh
    INT 21h
    ; AL = scan code now
    MOV AH, KTYPE_EXTENDED
    OR AH, AH              ; clear ZF (AH=1, nonzero)
    RET

.no_key:
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_dispatch_key - Handle a keypress
; Input:  AH = key type (KTYPE_NORMAL or KTYPE_EXTENDED), AL = char/scan code
; Output: May set FW_RUNNING to 0, may call movement/cycling routines
; ----------------------------------------------------------------------------
tui_dispatch_key: PROC
    ; Check for modal dialog - skip normal dispatch
    CMP BYTE [fw_state + FW_MODAL_WIN], 0FFh
    JZ .normal_dispatch
    JMP tui_modal_dispatch
.normal_dispatch:
    ; Menu intercept first - when active, consumes ALL keys
    CALL tui_menu_handle_key
    JC .not_handled_by_menu
    RET
.not_handled_by_menu:
    CALL tui_ctrl_handle_key
    JC .not_handled_by_ctrl
    RET
.not_handled_by_ctrl:
    CMP AH, KTYPE_EXTENDED
    JNZ .normal_keys
    JMP .extended
.normal_keys:

    ; --- Normal key ---
    CMP AL, KEY_TAB
    JZ .tab
    CMP AL, KEY_ENTER
    JZ .activate
    CMP AL, KEY_SPACE
    JZ .activate
    RET

.tab:
    ; Try control focus cycling first; fall back to window cycling
    PUSH SI
    MOV AL, [fw_state + FW_TOPWIN]
    CMP AL, 0FFh
    JZ .tab_no_win

    ; Get active window struct
    MOV BL, WIN_SIZE
    MUL BL                  ; AX = topwin * WIN_SIZE
    MOV SI, AX
    ADD SI, win_table

    ; Check if window has controls
    CMP WORD [SI + WIN_FIRST], 0
    JZ .tab_cycle_win

    CALL tui_ctrl_focus_next
    JNC .tab_done           ; CF=0: focus changed, done

    ; CF=1: no focusable or only one — cycle windows
.tab_cycle_win:
    CALL tui_zorder_cycle

.tab_done:
    POP SI
    RET

.tab_no_win:
    POP SI
    RET

.activate:
    ; Activate focused control in active window
    PUSH SI
    MOV AL, [fw_state + FW_TOPWIN]
    CMP AL, 0FFh
    JZ .activate_done

    ; Get active window struct
    MOV BL, WIN_SIZE
    MUL BL
    MOV SI, AX
    ADD SI, win_table
    CALL tui_ctrl_activate

.activate_done:
    POP SI
    RET

.extended:
    ; --- Extended key (AL = scan code) ---
    CMP AL, KEY_CTRL_UP
    JZ .ctrl_up
    CMP AL, KEY_CTRL_DOWN
    JZ .ctrl_down
    CMP AL, KEY_CTRL_LEFT
    JZ .ctrl_left
    CMP AL, KEY_CTRL_RIGHT
    JZ .ctrl_right
    CMP AL, KEY_F6
    JZ .f6
    RET

.ctrl_up:
    MOV AL, KEY_UP
    CALL tui_win_move
    RET

.ctrl_down:
    MOV AL, KEY_DOWN
    CALL tui_win_move
    RET

.ctrl_left:
    MOV AL, KEY_LEFT
    CALL tui_win_move
    RET

.ctrl_right:
    MOV AL, KEY_RIGHT
    CALL tui_win_move
    RET

.f6:
    CALL tui_zorder_cycle
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_modal_dispatch - Key dispatch when a modal dialog is active
; Restricts input to dialog only: no menu, no window cycling, no quit via q
; Called via JMP from tui_dispatch_key; returns via RET to dispatch_key caller
; ----------------------------------------------------------------------------
tui_modal_dispatch: PROC
    ; Try control key handling first (textbox in dialog handles own keys)
    CALL tui_ctrl_handle_key
    JNC .mdl_done               ; CF=0: handled by control

    ; Not handled by control - check modal-specific keys
    CMP AH, KTYPE_EXTENDED
    JZ .mdl_ext

    ; --- Normal keys ---
    CMP AL, 1Bh                ; Escape = cancel/dismiss
    JZ .mdl_cancel
    CMP AL, KEY_TAB
    JZ .mdl_tab
    CMP AL, KEY_ENTER
    JZ .mdl_activate
    CMP AL, KEY_SPACE
    JZ .mdl_activate
.mdl_done:
    RET                         ; ignore all other keys (q/Q etc.)

.mdl_ext:
    RET                         ; ignore all extended keys in modal

.mdl_cancel:
    MOV BYTE [fw_state + FW_DLG_RESULT], DLG_CANCEL
    MOV AL, [fw_state + FW_MODAL_WIN]
    CALL tui_win_close
    MOV BYTE [fw_state + FW_RUNNING], 0
    RET

.mdl_tab:
    ; Focus cycle within top window only (no window cycling)
    PUSH SI
    MOV AL, [fw_state + FW_TOPWIN]
    CMP AL, 0FFh
    JZ .mdl_tab_done
    XOR AH, AH
    MOV BL, WIN_SIZE
    MUL BL
    MOV SI, AX
    ADD SI, win_table
    CMP WORD [SI + WIN_FIRST], 0
    JZ .mdl_tab_done
    CALL tui_ctrl_focus_next
.mdl_tab_done:
    POP SI
    RET

.mdl_activate:
    ; Activate focused control in top window
    PUSH SI
    MOV AL, [fw_state + FW_TOPWIN]
    CMP AL, 0FFh
    JZ .mdl_act_done
    XOR AH, AH
    MOV BL, WIN_SIZE
    MUL BL
    MOV SI, AX
    ADD SI, win_table
    CALL tui_ctrl_activate
.mdl_act_done:
    POP SI
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_run - Main event loop
; Polls for input, dispatches keys, redraws when dirty.
; Loops until FW_RUNNING becomes 0.
; ----------------------------------------------------------------------------
tui_run: PROC
    ; Initial draw
    CALL tui_compose
    CALL tui_blit

.loop:
    ; Check if still running
    CMP BYTE [fw_state + FW_RUNNING], 0
    JZ .exit

    ; Poll for key
    CALL tui_poll_key
    JZ .no_key

    ; Got a key in AH:AL - dispatch it
    CALL tui_dispatch_key

.no_key:
    ; Poll mouse
    CALL tui_mouse_poll
    TEST AL, 3              ; any change?
    JZ .no_mouse
    CALL tui_mouse_dispatch
.no_mouse:

    ; If dirty, redraw
    CMP BYTE [fw_state + FW_DIRTY], 0
    JZ .loop

    CALL tui_compose
    CALL tui_blit
    JMP .loop

.exit:
    RET
ENDP
