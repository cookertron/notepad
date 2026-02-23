; ============================================================================
; tui_mouse.inc - Mouse Support
; ============================================================================

; ----------------------------------------------------------------------------
; tui_mouse_init - Detect and configure mouse driver
; Sets MSF_ENABLED if INT 33h driver is present.
; Configures horizontal range 0-639, vertical range 0-399.
; ----------------------------------------------------------------------------
tui_mouse_init: PROC
    PushAll

    ; Zero mouse_state
    MOV DI, mouse_state
    XOR AL, AL
    MOV CX, MS_SIZE
    CLD
    REP STOSB

    ; Set defaults
    MOV BYTE [mouse_state + MS_DRAG_WIN], 0FFh

    ; Reset mouse driver
    XOR AX, AX
    INT 33h
    CMP AX, 0FFFFh
    JNZ .no_mouse

    ; Driver present - enable
    OR BYTE [mouse_state + MS_FLAGS], MSF_ENABLED

    ; Set horizontal range: 0-639
    MOV AX, 0007h
    XOR CX, CX             ; min = 0
    MOV DX, 639
    INT 33h

    ; Set vertical range: 0-199
    MOV AX, 0008h
    XOR CX, CX             ; min = 0
    MOV DX, 199
    INT 33h

    ; Show hardware cursor
    MOV AX, 0001h
    INT 33h

.no_mouse:
    PopAll
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_mouse_poll - Read mouse state, detect changes
; Output: AL = change flags (bit 0: position moved, bit 1: button changed)
; ----------------------------------------------------------------------------
tui_mouse_poll: PROC
    PUSH BX
    PUSH CX
    PUSH DX

    ; Check if enabled
    TEST BYTE [mouse_state + MS_FLAGS], MSF_ENABLED
    JNZ .enabled
    XOR AL, AL
    JMP .ret
.enabled:

    ; Save previous position and buttons
    MOV AL, [mouse_state + MS_COL]
    MOV [mouse_state + MS_PCOL], AL
    MOV AL, [mouse_state + MS_ROW]
    MOV [mouse_state + MS_PROW], AL
    MOV AL, [mouse_state + MS_BUTTONS]
    MOV [mouse_state + MS_PREV_BTN], AL

    ; Read current mouse state
    MOV AX, 0003h
    INT 33h
    ; BX = buttons, CX = x pixel, DX = y pixel

    ; Convert pixel to cell: col = CX >> 3, row = DX >> 3
    SHR CX, 1
    SHR CX, 1
    SHR CX, 1              ; CX = col
    SHR DX, 1
    SHR DX, 1
    SHR DX, 1              ; DX = row

    ; Clamp col to 0-79
    CMP CL, SCR_W
    JB .col_ok
    MOV CL, SCR_W - 1
.col_ok:
    ; Clamp row to 0-49
    CMP DL, SCR_H
    JB .row_ok
    MOV DL, SCR_H - 1
.row_ok:

    ; Store new state
    MOV [mouse_state + MS_COL], CL
    MOV [mouse_state + MS_ROW], DL
    MOV [mouse_state + MS_BUTTONS], BL

    ; Detect changes: AL = flags
    XOR AL, AL

    ; Position changed?
    MOV AH, [mouse_state + MS_PCOL]
    CMP CL, AH
    JNZ .pos_changed
    MOV AH, [mouse_state + MS_PROW]
    CMP DL, AH
    JZ .check_btn
.pos_changed:
    OR AL, 1               ; bit 0 = position moved

.check_btn:
    MOV AH, [mouse_state + MS_PREV_BTN]
    CMP BL, AH
    JZ .ret
    OR AL, 2               ; bit 1 = button changed

.ret:
    POP DX
    POP CX
    POP BX
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_mouse_draw_cursor - Overlay cursor on shadow_buf (invert attr)
; Called after compose, before blit.
; ----------------------------------------------------------------------------
tui_mouse_draw_cursor: PROC
    PUSH AX
    PUSH CX
    PUSH DI
    PUSH DX

    TEST BYTE [mouse_state + MS_FLAGS], MSF_ENABLED
    JZ .done

    MOV DH, [mouse_state + MS_ROW]
    MOV DL, [mouse_state + MS_COL]
    CALL calc_vram_offset   ; DI = shadow_buf address

    ; Read attribute byte, swap nibbles (FG <-> BG)
    MOV AL, [DI + 1]
    MOV CL, 4
    ROL AL, CL
    MOV [DI + 1], AL

.done:
    POP DX
    POP DI
    POP CX
    POP AX
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_mouse_dispatch - Handle mouse state changes
; Input: AL = change flags from tui_mouse_poll
; ----------------------------------------------------------------------------
tui_mouse_dispatch: PROC
    PushAll

    ; Always mark dirty for cursor redraw
    MOV BYTE [fw_state + FW_DIRTY], 1

    ; If dragging, handle drag motion
    TEST BYTE [mouse_state + MS_FLAGS], MSF_DRAGGING
    JZ .not_dragging

    ; Check if left button still held
    TEST BYTE [mouse_state + MS_BUTTONS], 1
    JNZ .still_dragging
    ; Button released - end drag
    AND BYTE [mouse_state + MS_FLAGS], 0FFh - MSF_DRAGGING
    MOV BYTE [mouse_state + MS_DRAG_WIN], 0FFh
    JMP .done

.still_dragging:
    CALL tui_mouse_on_drag
    JMP .done

.not_dragging:
    ; If resizing, handle resize motion
    TEST BYTE [mouse_state + MS_FLAGS], MSF_RESIZING
    JZ .not_resizing

    ; Check if left button still held
    TEST BYTE [mouse_state + MS_BUTTONS], 1
    JNZ .still_resizing
    ; Button released - end resize
    AND BYTE [mouse_state + MS_FLAGS], 0FFh - MSF_RESIZING
    MOV BYTE [mouse_state + MS_DRAG_WIN], 0FFh
    JMP .done

.still_resizing:
    CALL tui_mouse_on_resize
    JMP .done

.not_resizing:
    ; If scroll bar dragging, handle SB drag motion
    TEST BYTE [mouse_state + MS_FLAGS], MSF_SB_DRAG
    JZ .not_sb_drag

    ; Check if left button still held
    TEST BYTE [mouse_state + MS_BUTTONS], 1
    JNZ .still_sb_drag
    ; Button released - end scroll bar drag
    AND BYTE [mouse_state + MS_FLAGS], 0FFh - MSF_SB_DRAG
    MOV WORD [mouse_state + MS_SB_CTRL], 0
    JMP .done

.still_sb_drag:
    CALL tui_mouse_on_sb_drag
    JMP .done

.not_sb_drag:
    ; If control drag active, handle it
    TEST BYTE [mouse_state + MS_FLAGS], MSF_CTRL_DRAG
    JZ .not_ctrl_drag
    TEST BYTE [mouse_state + MS_BUTTONS], 1
    JNZ .still_ctrl_drag
    ; Button released - end control drag
    AND BYTE [mouse_state + MS_FLAGS], 0FFh - MSF_CTRL_DRAG
    MOV WORD [mouse_state + MS_PRESSED], 0
    JMP .done
.still_ctrl_drag:
    CALL tui_mouse_on_ctrl_drag
    JMP .done

.not_ctrl_drag:
    ; Check for left button just pressed (was 0, now 1)
    TEST BYTE [mouse_state + MS_PREV_BTN], 1
    JNZ .was_down
    TEST BYTE [mouse_state + MS_BUTTONS], 1
    JZ .no_btn_change
    ; Left button just pressed
    CALL tui_mouse_on_press
    JMP .done

.was_down:
    ; Check for left button just released (was 1, now 0)
    TEST BYTE [mouse_state + MS_BUTTONS], 1
    JNZ .done
    CALL tui_mouse_on_release
    JMP .done

.no_btn_change:
    ; No button event - handle menu hover on move
    CALL tui_mouse_menu_hover

.done:
    PopAll
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_mouse_hit_test - Determine what's under the cursor
; Input:  DH = row, DL = col
; Output: AL = hit type (HIT_*), SI = window struct ptr (if window hit),
;         BX = context (control ptr or item index)
; ----------------------------------------------------------------------------
tui_mouse_hit_test: PROC
    PUSH CX
    PUSH DI

    ; --- 1. Check open combo dropdown popup ---
    MOV BX, [fw_state + FW_OPENDD]
    OR BX, BX
    JZ .ht_no_dd_popup

    ; Compute popup rect: row = FW_OPENDD_ROW, col = FW_OPENDD_COL
    ; Popup has border, so content is at row+1..row+visible, col+1..col+width
    ; Full popup box: row..row+visible+1, col..col+width+1
    MOV AL, [fw_state + FW_OPENDD_ROW]  ; popup top row
    MOV AH, [fw_state + FW_OPENDD_COL]  ; popup left col

    ; Check row >= popup_row and row <= popup_row + visible + 1
    CMP DH, AL
    JB .ht_no_dd_popup
    ; Compute visible = MIN(MAXVIS, COUNT)
    MOV CL, [BX + CTRL_DD_MAXVIS]
    CMP CL, [BX + CTRL_DD_COUNT]
    JBE .ht_dd_vis_ok
    MOV CL, [BX + CTRL_DD_COUNT]
.ht_dd_vis_ok:
    ; Bottom of popup box = popup_row + visible + 1
    MOV CH, AL
    ADD CH, CL
    INC CH
    CMP DH, CH
    JA .ht_no_dd_popup

    ; Check col >= popup_col and col <= popup_col + width + 1
    CMP DL, AH
    JB .ht_no_dd_popup
    MOV CH, AH
    ADD CH, [BX + CTRL_W]
    INC CH
    CMP DL, CH
    JA .ht_no_dd_popup

    ; Inside popup box - is it an item row? (not border)
    ; Item rows: popup_row + 1 .. popup_row + visible
    MOV CH, [fw_state + FW_OPENDD_ROW]
    INC CH                  ; first item row
    CMP DH, CH
    JB .ht_dd_border
    MOV CH, [fw_state + FW_OPENDD_ROW]
    ADD CH, CL              ; CH = popup_row + visible
    CMP DH, CH
    JA .ht_dd_border

    ; Item row - compute index: (row - popup_row - 1) + SCROLL
    MOV AL, DH
    SUB AL, [fw_state + FW_OPENDD_ROW]
    DEC AL
    ADD AL, [BX + CTRL_DD_SCROLL]
    XOR AH, AH
    MOV BX, AX             ; BX = item index
    MOV AL, HIT_DD_POPUP
    JMP .ht_ret

.ht_dd_border:
    MOV AL, HIT_BORDER
    JMP .ht_ret

.ht_no_dd_popup:

    ; --- 2. Check open menu dropdown ---
    MOV DI, [fw_state + FW_MENUBAR]
    OR DI, DI
    JNZ .ht_have_menu
    JMP .ht_no_menu
.ht_have_menu:
    CMP BYTE [DI + MBAR_OPEN], 1
    JNZ .ht_check_menubar

    ; Get selected menu item for dropdown position
    MOV AL, [DI + MBAR_SEL]
    CMP AL, 0FFh
    JZ .ht_check_menubar
    CALL _mi_index_to_offset ; AX = sel * MI_SIZE
    MOV BX, [DI + MBAR_ITEMS]
    ADD BX, AX             ; BX = selected menu item

    ; Dropdown box: row 1..1+ecount+1, col MI_X..MI_X+MI_DDW+1
    CMP DH, 1
    JB .ht_check_menubar
    MOV AL, [BX + MI_ECOUNT]
    ADD AL, 2               ; +2 for top/bottom border
    CMP DH, AL
    JAE .ht_check_menubar

    CMP DL, [BX + MI_X]
    JB .ht_check_menubar
    MOV AL, [BX + MI_X]
    ADD AL, [BX + MI_DDW]
    INC AL                  ; right border col
    CMP DL, AL
    JA .ht_check_menubar

    ; Inside menu dropdown - check if entry row (rows 2..1+ecount)
    CMP DH, 1
    JZ .ht_menudrop_border  ; top border
    MOV AL, [BX + MI_ECOUNT]
    ADD AL, 2               ; AL = 2 + ecount (bottom border row)
    CMP DH, AL
    JAE .ht_menudrop_border

    ; Entry row: index = row - 2
    MOV AL, DH
    SUB AL, 2
    XOR AH, AH
    MOV BX, AX             ; BX = entry index
    MOV AL, HIT_MENUDROP
    JMP .ht_ret

.ht_menudrop_border:
    MOV AL, HIT_BORDER
    JMP .ht_ret

    ; --- 3. Check menu bar (row 0) ---
.ht_check_menubar:
    OR DH, DH
    JNZ .ht_no_menu
    ; DI = menu bar struct (already loaded)
    OR DI, DI
    JZ .ht_no_menu

    ; Walk menu items to find which one (if any)
    MOV BX, [DI + MBAR_ITEMS]
    XOR CX, CX             ; CX = item index
.ht_mbar_loop:
    CMP CL, [DI + MBAR_COUNT]
    JAE .ht_no_menu

    ; Item ptr = items + index * MI_SIZE
    PUSH AX
    MOV AL, CL
    CALL _mi_index_to_offset
    PUSH BX
    ADD BX, AX             ; BX = item struct

    ; Check col >= MI_X and col < MI_X + MI_W
    MOV AL, [BX + MI_X]
    CMP DL, AL
    JB .ht_mbar_next
    ADD AL, [BX + MI_W]
    CMP DL, AL
    JAE .ht_mbar_next

    ; Hit menu bar item
    POP BX
    POP AX
    MOV BX, CX             ; BX = item index
    MOV AL, HIT_MENUBAR
    JMP .ht_ret

.ht_mbar_next:
    POP BX
    POP AX
    INC CL
    JMP .ht_mbar_loop

.ht_no_menu:

    ; --- 4. Check windows from top to bottom ---
    MOV CL, [fw_state + FW_NUMWIN]
    OR CL, CL
    JNZ .ht_have_wins
    JMP .ht_nothing
.ht_have_wins:
    MOV CH, CL
    DEC CH                  ; CH = index into z_order (top = numwin-1)

.ht_win_loop:
    ; Get window slot from z_order[CH]
    PUSH BX
    MOV BL, CH
    XOR BH, BH
    MOV AL, [BX + z_order]
    POP BX

    ; Get window struct: SI = win_table + slot * WIN_SIZE
    PUSH AX
    XOR AH, AH
    MOV BL, WIN_SIZE
    MUL BL
    MOV SI, AX
    ADD SI, win_table
    POP AX

    ; Skip invisible windows
    TEST BYTE [SI + WIN_FLAGS], WINF_VISIBLE
    JNZ .ht_win_visible
    JMP .ht_next_win
.ht_win_visible:

    ; Check if (col, row) is within window bounds
    ; col >= WIN_X?
    CMP DL, [SI + WIN_X]
    JAE .ht_col_ge
    JMP .ht_next_win
.ht_col_ge:
    ; col < WIN_X + WIN_W?
    MOV AL, [SI + WIN_X]
    ADD AL, [SI + WIN_W]
    CMP DL, AL
    JAE .ht_next_win
    ; row >= WIN_Y?
    CMP DH, [SI + WIN_Y]
    JB .ht_next_win
    ; row < WIN_Y + WIN_H?
    MOV AL, [SI + WIN_Y]
    ADD AL, [SI + WIN_H]
    CMP DH, AL
    JAE .ht_next_win

    ; --- Hit this window. Classify the hit. ---
    TEST BYTE [SI + WIN_FLAGS], WINF_BORDER
    JZ .ht_win_interior

    ; Bordered window: check title bar (top row)
    CMP DH, [SI + WIN_Y]
    JNZ .ht_win_not_title

    ; On title bar row - check close button (top-left corner)
    TEST BYTE [SI + WIN_FLAGS], WINF_CLOSEBTN
    JZ .ht_win_titlebar
    CMP DL, [SI + WIN_X]
    JNZ .ht_win_titlebar

    ; Close button hit
    MOV AL, HIT_CLOSEBTN
    JMP .ht_ret

.ht_win_titlebar:
    MOV AL, HIT_TITLEBAR
    JMP .ht_ret

.ht_win_not_title:
    ; Check if on border (first/last col, or last row)
    CMP DL, [SI + WIN_X]
    JZ .ht_win_border_hit

    ; Check last col
    MOV AL, [SI + WIN_X]
    ADD AL, [SI + WIN_W]
    DEC AL
    CMP DL, AL
    JNZ .ht_not_last_col
    ; On last col — check if also last row (bottom-right corner)
    PUSH AX
    MOV AL, [SI + WIN_Y]
    ADD AL, [SI + WIN_H]
    DEC AL
    CMP DH, AL
    POP AX
    JNZ .ht_win_border_hit     ; last col but not last row → border
    ; Bottom-right corner — check WINF_RESIZABLE
    TEST BYTE [SI + WIN_FLAGS], WINF_RESIZABLE
    JZ .ht_win_border_hit      ; not resizable → just border
    MOV AL, HIT_RESIZE
    JMP .ht_ret

.ht_not_last_col:
    ; Check last row (bottom border)
    MOV AL, [SI + WIN_Y]
    ADD AL, [SI + WIN_H]
    DEC AL
    CMP DH, AL
    JZ .ht_win_border_hit

.ht_win_interior:
    MOV AL, HIT_INTERIOR
    JMP .ht_ret

.ht_win_border_hit:
    MOV AL, HIT_BORDER
    JMP .ht_ret

.ht_next_win:
    OR CH, CH
    JZ .ht_nothing
    DEC CH
    JMP .ht_win_loop

.ht_nothing:
    MOV AL, HIT_NONE
    XOR SI, SI

.ht_ret:
    POP DI
    POP CX
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_mouse_on_press - Handle left-button-down
; ----------------------------------------------------------------------------
tui_mouse_on_press: PROC
    PUSH AX
    PUSH BX
    PUSH CX
    PUSH DX
    PUSH SI
    PUSH DI

    MOV DH, [mouse_state + MS_ROW]
    MOV DL, [mouse_state + MS_COL]
    CALL tui_mouse_hit_test ; AL=type, SI=window, BX=context

    ; Close open combo dropdown popup if clicking outside it
    PUSH AX
    CMP AL, HIT_DD_POPUP
    JZ .press_no_dd_dismiss
    CMP WORD [fw_state + FW_OPENDD], 0
    JZ .press_no_dd_dismiss
    PUSH BX
    MOV BX, [fw_state + FW_OPENDD]
    CALL tui_dd_close_cancel
    POP BX
.press_no_dd_dismiss:
    POP AX

    ; --- Modal enforcement: block clicks outside modal dialog ---
    CMP BYTE [fw_state + FW_MODAL_WIN], 0FFh
    JZ .press_no_modal
    CMP AL, HIT_NONE
    JNZ .modal_not_none
    JMP .press_done             ; desktop click → block
.modal_not_none:
    CMP AL, HIT_MENUBAR
    JNZ .modal_not_mbar
    JMP .press_done             ; menu bar → block during modal
.modal_not_mbar:
    CMP AL, HIT_MENUDROP
    JNZ .modal_not_mdrop
    JMP .press_done             ; menu dropdown → block during modal
.modal_not_mdrop:
    CMP AL, HIT_DD_POPUP
    JZ .press_no_modal          ; combo popup → allow (belongs to dialog)
    ; Window hit: check if it's the modal window
    MOV AH, [SI + WIN_ID]
    CMP AH, [fw_state + FW_MODAL_WIN]
    JZ .press_no_modal          ; click on modal window → allow
    JMP .press_done             ; click on other window → block
.press_no_modal:

    CMP AL, HIT_MENUBAR
    JZ .press_menubar
    CMP AL, HIT_MENUDROP
    JZ .press_menudrop
    CMP AL, HIT_DD_POPUP
    JZ .press_dd_popup

    ; Close menu if clicking outside it
    CALL tui_mouse_close_menu

    ; Dispatch (relay pattern for long jumps)
    CMP AL, HIT_TITLEBAR
    JZ .press_titlebar
    CMP AL, HIT_CLOSEBTN
    JZ .go_press_close
    CMP AL, HIT_INTERIOR
    JZ .go_press_interior
    CMP AL, HIT_BORDER
    JZ .go_press_border
    CMP AL, HIT_RESIZE
    JZ .go_press_resize
    JMP .press_done

.go_press_close:
    JMP .press_close
.go_press_interior:
    JMP .press_interior
.go_press_border:
    JMP .press_border
.go_press_resize:
    JMP .press_resize

.press_menubar:
    ; BX = item index from hit_test
    MOV DI, [fw_state + FW_MENUBAR]
    OR DI, DI
    JNZ .press_menubar_ok
    JMP .press_done
.press_menubar_ok:
    ; Set selected item, open dropdown
    MOV [DI + MBAR_SEL], BL
    MOV BYTE [DI + MBAR_OPEN], 1
    MOV BYTE [DI + MBAR_DDSEL], 0
    MOV BYTE [fw_state + FW_DIRTY], 1
    JMP .press_done

.press_menudrop:
    ; BX = entry index from hit_test
    MOV DI, [fw_state + FW_MENUBAR]
    OR DI, DI
    JNZ .press_menudrop_ok
    JMP .press_done
.press_menudrop_ok:
    ; Set dropdown selection and activate
    MOV [DI + MBAR_DDSEL], BL
    CALL tui_menu_activate_entry
    JMP .press_done

.press_dd_popup:
    ; BX = item index from hit_test
    PUSH BX                 ; save item index
    MOV BX, [fw_state + FW_OPENDD]
    OR BX, BX
    JNZ .press_dd_popup_ok
    POP BX                  ; clean up stack
    JMP .press_done
.press_dd_popup_ok:
    POP AX                  ; AL = item index
    MOV [BX + CTRL_DD_TMPSEL], AL
    CALL tui_dd_close_commit
    ; Call handler if any
    CALL tui_mouse_call_handler
    JMP .press_done

.press_titlebar:
    ; Bring window to front if not already
    MOV AL, [SI + WIN_ID]
    CALL tui_mouse_bring_to_front

    ; Start dragging
    OR BYTE [mouse_state + MS_FLAGS], MSF_DRAGGING
    MOV AL, [SI + WIN_ID]
    MOV [mouse_state + MS_DRAG_WIN], AL

    ; Compute drag offsets: col - WIN_X, row - WIN_Y
    MOV AL, DL
    SUB AL, [SI + WIN_X]
    MOV [mouse_state + MS_DRAG_OX], AL
    MOV AL, DH
    SUB AL, [SI + WIN_Y]
    MOV [mouse_state + MS_DRAG_OY], AL
    JMP .press_done

.press_close:
    ; Close the window
    MOV AL, [SI + WIN_ID]
    CALL tui_win_close
    JMP .press_done

.press_border:
    ; Bring window to front
    MOV AL, [SI + WIN_ID]
    CALL tui_mouse_bring_to_front
    JMP .press_done

.press_resize:
    ; Bring window to front, start resize drag
    MOV AL, [SI + WIN_ID]
    CALL tui_mouse_bring_to_front
    OR BYTE [mouse_state + MS_FLAGS], MSF_RESIZING
    MOV AL, [SI + WIN_ID]
    MOV [mouse_state + MS_DRAG_WIN], AL
    JMP .press_done

.press_interior:
    ; Bring window to front
    MOV AL, [SI + WIN_ID]
    CALL tui_mouse_bring_to_front

    ; Check for control hit
    CALL tui_mouse_hit_test_control  ; BX = control ptr or 0
    OR BX, BX
    JNZ .press_have_ctrl
    JMP .press_done
.press_have_ctrl:

    ; Check if control is focusable + enabled
    TEST BYTE [BX + CTRL_FLAGS], CTRLF_FOCUSABLE
    JZ .press_no_ctrl
    TEST BYTE [BX + CTRL_FLAGS], CTRLF_ENABLED
    JNZ .press_ctrl_ok
.press_no_ctrl:
    JMP .press_done
.press_ctrl_ok:

    ; Set focus to this control
    MOV [SI + WIN_FOCUS], BX
    MOV BYTE [fw_state + FW_DIRTY], 1

    ; Dispatch by control type
    CMP BYTE [BX + CTRL_TYPE], CTYPE_BUTTON
    JZ .press_button
    CMP BYTE [BX + CTRL_TYPE], CTYPE_CHECKBOX
    JZ .press_checkbox
    CMP BYTE [BX + CTRL_TYPE], CTYPE_RADIO
    JZ .press_radio
    CMP BYTE [BX + CTRL_TYPE], CTYPE_TEXTBOX
    JZ .press_textbox
    CMP BYTE [BX + CTRL_TYPE], CTYPE_DROPDOWN
    JZ .press_dropdown
    CMP BYTE [BX + CTRL_TYPE], CTYPE_LISTBOX
    JZ .press_listbox
    CMP BYTE [BX + CTRL_TYPE], CTYPE_TEXTVIEW
    JZ .go_press_textview
    CMP BYTE [BX + CTRL_TYPE], CTYPE_EDITOR
    JZ .go_press_editor
    JMP .press_done

.go_press_textview:
    JMP .press_textview
.go_press_editor:
    JMP .press_editor

.press_button:
    ; Enter pressed state (visual feedback, activate on release)
    MOV [mouse_state + MS_PRESSED], BX
    OR BYTE [mouse_state + MS_FLAGS], MSF_BTN_DOWN
    JMP .press_done

.press_checkbox:
    ; Toggle checkbox state
    MOV AL, [BX + CTRL_CB_STATE]
    XOR AL, 1
    MOV [BX + CTRL_CB_STATE], AL
    ; Call handler if any
    CALL tui_mouse_call_handler
    JMP .press_done

.press_radio:
    ; Select this radio (deselect others in group)
    CMP BYTE [BX + CTRL_CB_STATE], 1
    JNZ .press_radio_do
    JMP .press_done          ; already selected
.press_radio_do:
    CALL tui_radio_group_select
    ; Call handler if any
    CALL tui_mouse_call_handler
    JMP .press_done

.press_textbox:
    ; Position cursor at clicked column
    ; Calculate: ctrl_abs_x = WIN_X + border + CTRL_X
    MOV AL, [SI + WIN_X]
    TEST BYTE [SI + WIN_FLAGS], WINF_BORDER
    JZ .ptb_no_bdr
    INC AL
.ptb_no_bdr:
    ADD AL, [BX + CTRL_X]
    ; cursor_pos = (click_col - ctrl_abs_x) + SCROLL
    MOV AH, DL             ; AH = click col
    SUB AH, AL             ; AH = click col - ctrl_abs_x
    ADD AH, [BX + CTRL_TB_SCROLL]
    ; Clamp to 0..LEN
    CMP AH, [BX + CTRL_TB_LEN]
    JBE .ptb_pos_ok
    MOV AH, [BX + CTRL_TB_LEN]
.ptb_pos_ok:
    MOV [BX + CTRL_TB_CURPOS], AH
    CALL tui_tb_ensure_visible
    JMP .press_done

.press_dropdown:
    ; Open dropdown popup
    CALL tui_dd_open
    JMP .press_done

.press_listbox:
    ; Check if click is on scrollbar column (rightmost col when vbar present)
    ; vbar present when COUNT > CTRL_H
    PUSH CX
    MOV AL, [BX + CTRL_LB_COUNT]
    CMP AL, [BX + CTRL_H]
    JA .plb_no_sb_near
    JMP .plb_no_sb
.plb_no_sb_near:

    ; Has scrollbar. Check if click_col == abs_x + CTRL_W - 1
    MOV AL, [SI + WIN_X]
    TEST BYTE [SI + WIN_FLAGS], WINF_BORDER
    JZ .plb_sb_no_bdr
    INC AL
.plb_sb_no_bdr:
    ADD AL, [BX + CTRL_X]
    ADD AL, [BX + CTRL_W]
    DEC AL                      ; AL = abs col of scrollbar
    CMP DL, AL
    JNZ .plb_no_sb

    ; Click is on scrollbar column
    ; Compute abs top row of control
    MOV AL, [SI + WIN_Y]
    TEST BYTE [SI + WIN_FLAGS], WINF_BORDER
    JZ .plb_sb_no_bdr2
    INC AL
.plb_sb_no_bdr2:
    ADD AL, [BX + CTRL_Y]      ; AL = abs top row

    ; Relative row within bar = click_row - abs_top
    MOV AH, DH
    SUB AH, AL                  ; AH = relative row in bar (0-based)

    ; Dispatch: 0=up arrow, bar_height-1=down arrow, else track/thumb
    CMP AH, 0
    JZ .plb_sb_up
    MOV CL, [BX + CTRL_H]
    DEC CL
    CMP AH, CL
    JZ .plb_sb_down

    ; Track/thumb click - call generic vbar click handler
    ; AH = rel_row (preserved from line 854), set AL/CL/CH
    MOV CL, [BX + CTRL_H]
    MOV CH, [BX + CTRL_LB_COUNT]
    MOV AL, [BX + CTRL_LB_SCROLL]
    ; AH = rel_row still valid
    CALL tui_sb_vbar_click_generic
    ; AL = new scroll value
    MOV [BX + CTRL_LB_SCROLL], AL
    MOV BYTE [fw_state + FW_DIRTY], 1
    POP CX
    JMP .press_done

.plb_sb_up:
    ; Scroll up by 1
    CMP BYTE [BX + CTRL_LB_SCROLL], 0
    JZ .plb_sb_up_done
    DEC BYTE [BX + CTRL_LB_SCROLL]
.plb_sb_up_done:
    MOV BYTE [fw_state + FW_DIRTY], 1
    POP CX
    JMP .press_done

.plb_sb_down:
    ; Scroll down by 1: max_scroll = COUNT - CTRL_H
    MOV AL, [BX + CTRL_LB_COUNT]
    SUB AL, [BX + CTRL_H]
    CMP [BX + CTRL_LB_SCROLL], AL
    JAE .plb_sb_dn_done
    INC BYTE [BX + CTRL_LB_SCROLL]
.plb_sb_dn_done:
    MOV BYTE [fw_state + FW_DIRTY], 1
    POP CX
    JMP .press_done

.plb_no_sb:
    POP CX

    ; item_index = SCROLL + (click_row - ctrl_abs_row)
    MOV AL, [SI + WIN_Y]
    TEST BYTE [SI + WIN_FLAGS], WINF_BORDER
    JZ .plb_no_bdr
    INC AL
.plb_no_bdr:
    ADD AL, [BX + CTRL_Y]       ; AL = abs top row of listbox
    MOV AH, DH                  ; AH = click row
    SUB AH, AL                  ; AH = relative row within listbox
    ADD AH, [BX + CTRL_LB_SCROLL] ; AH = item index
    ; Clamp: if AH >= COUNT, AH = COUNT-1
    CMP AH, [BX + CTRL_LB_COUNT]
    JB .plb_ok
    MOV AH, [BX + CTRL_LB_COUNT]
    DEC AH
.plb_ok:
    MOV [BX + CTRL_LB_SEL], AH
    CALL tui_lb_ensure_visible
    CALL tui_mouse_call_handler
    JMP .press_done

.press_textview:
    ; Check vbar: click on rightmost col when COUNT > eff_h
    PUSH CX

    ; Compute needs_vbar and needs_hbar (same auto-hide logic as draw)
    XOR CH, CH                  ; CH = needs_vbar
    MOV AL, [BX + CTRL_TV_COUNT]
    CMP AL, [BX + CTRL_H]
    JBE .ptv_nvb1
    MOV CH, 1
.ptv_nvb1:
    MOV CL, [BX + CTRL_W]
    SUB CL, CH                  ; CL = eff_w
    XOR AH, AH                  ; AH = needs_hbar
    MOV AL, [BX + CTRL_TV_MAXLEN]
    CMP AL, CL
    JBE .ptv_nhb1
    MOV AH, 1
.ptv_nhb1:
    ; eff_h = CTRL_H - needs_hbar
    MOV AL, [BX + CTRL_H]
    SUB AL, AH
    ; Recheck: if hbar and not vbar, does COUNT > eff_h?
    OR AH, AH
    JZ .ptv_ah_done
    OR CH, CH
    JNZ .ptv_ah_done
    PUSH CX
    MOV CL, [BX + CTRL_TV_COUNT]
    CMP CL, AL
    POP CX
    JBE .ptv_ah_done
    MOV CH, 1
    MOV CL, [BX + CTRL_W]
    DEC CL                      ; eff_w = CTRL_W - 1
.ptv_ah_done:
    ; CH=needs_vbar, AH=needs_hbar, CL=eff_w, AL=eff_h
    ; Save eff_h and eff_w for later
    PUSH AX                     ; AL=eff_h, AH=needs_hbar

    ; Compute abs origin of control
    MOV AL, [SI + WIN_X]
    TEST BYTE [SI + WIN_FLAGS], WINF_BORDER
    JZ .ptv_no_bdr_x
    INC AL
.ptv_no_bdr_x:
    ADD AL, [BX + CTRL_X]      ; AL = abs left col

    ; Check vbar click: col == abs_x + CTRL_W - 1, and needs_vbar
    OR CH, CH
    JNZ .ptv_has_vbar
    JMP .ptv_no_vbar_click
.ptv_has_vbar:
    PUSH AX
    MOV AH, AL
    ADD AH, [BX + CTRL_W]
    DEC AH                      ; AH = abs vbar col
    CMP DL, AH
    POP AX
    JNZ .ptv_no_vbar_click

    ; Click on vbar column - compute relative row
    MOV AL, [SI + WIN_Y]
    TEST BYTE [SI + WIN_FLAGS], WINF_BORDER
    JZ .ptv_vb_no_bdr
    INC AL
.ptv_vb_no_bdr:
    ADD AL, [BX + CTRL_Y]      ; AL = abs top row
    MOV AH, DH
    SUB AH, AL                  ; AH = rel row in vbar
    POP CX                      ; CL = eff_h (was AL pushed earlier), CH = needs_hbar
    PUSH CX

    ; Range check: if rel_row >= eff_h, click is on corner cell, not vbar
    CMP AH, CL
    JAE .ptv_no_vbar_click

    ; 0 = up arrow, eff_h-1 = down arrow
    CMP AH, 0
    JZ .ptv_vb_up
    DEC CL                      ; CL = eff_h - 1
    CMP AH, CL
    JZ .ptv_vb_down

    ; Track/thumb click
    INC CL                      ; restore CL = eff_h
    ; AH = rel_row still valid, set AL/CL/CH
    MOV CH, [BX + CTRL_TV_COUNT]
    MOV AL, [BX + CTRL_TV_SCROLL]
    ; AH = rel_row preserved
    CALL tui_sb_vbar_click_generic
    MOV [BX + CTRL_TV_SCROLL], AL
    MOV BYTE [fw_state + FW_DIRTY], 1
    POP AX
    POP CX
    JMP .press_done

.ptv_vb_up:
    CMP BYTE [BX + CTRL_TV_SCROLL], 0
    JZ .ptv_vb_up_done
    DEC BYTE [BX + CTRL_TV_SCROLL]
.ptv_vb_up_done:
    MOV BYTE [fw_state + FW_DIRTY], 1
    POP AX
    POP CX
    JMP .press_done

.ptv_vb_down:
    MOV AL, [BX + CTRL_TV_COUNT]
    POP CX                      ; CL = eff_h
    PUSH CX
    SUB AL, CL                  ; AL = max_scroll
    CMP [BX + CTRL_TV_SCROLL], AL
    JAE .ptv_vb_dn_done
    INC BYTE [BX + CTRL_TV_SCROLL]
.ptv_vb_dn_done:
    MOV BYTE [fw_state + FW_DIRTY], 1
    POP AX
    POP CX
    JMP .press_done

.ptv_no_vbar_click:
    ; Check hbar click: row == abs_y + CTRL_H - 1, and needs_hbar
    POP AX                      ; AL=eff_h, AH=needs_hbar
    OR AH, AH
    JNZ .ptv_hbar_check
    JMP .ptv_no_hbar_click
.ptv_hbar_check:

    PUSH AX
    MOV AL, [SI + WIN_Y]
    TEST BYTE [SI + WIN_FLAGS], WINF_BORDER
    JZ .ptv_hb_no_bdr
    INC AL
.ptv_hb_no_bdr:
    ADD AL, [BX + CTRL_Y]
    ADD AL, [BX + CTRL_H]
    DEC AL                      ; AL = abs hbar row
    CMP DH, AL
    POP AX
    JNZ .ptv_no_hbar_click

    ; Click on hbar row - compute relative col
    PUSH AX                     ; save eff_h/needs_hbar
    MOV AL, [SI + WIN_X]
    TEST BYTE [SI + WIN_FLAGS], WINF_BORDER
    JZ .ptv_hb_no_bdr2
    INC AL
.ptv_hb_no_bdr2:
    ADD AL, [BX + CTRL_X]      ; AL = abs left col of hbar
    MOV AH, DL
    SUB AH, AL                  ; AH = rel col in hbar
    POP CX                      ; CL = eff_h (not needed), CL reuse

    ; hbar width = eff_w = CTRL_W - needs_vbar
    MOV CL, [BX + CTRL_W]
    SUB CL, CH                  ; CL = hbar_width (eff_w)

    ; Range check: if rel_col >= hbar_width, not on hbar (corner cell)
    CMP AH, CL
    JAE .ptv_no_hbar_click

    ; 0 = left arrow, eff_w-1 = right arrow
    CMP AH, 0
    JZ .ptv_hb_left
    PUSH CX
    DEC CL
    CMP AH, CL
    POP CX
    JZ .ptv_hb_right

    ; Track/thumb click for horizontal
    ; AH = rel_col still valid, set AL/CL/CH
    MOV CH, [BX + CTRL_TV_MAXLEN]  ; total = max_content_w
    MOV AL, [BX + CTRL_TV_SCROLLX]
    ; CL = bar_width (eff_w), AH = rel_col preserved
    CALL tui_sb_hbar_click_generic
    MOV [BX + CTRL_TV_SCROLLX], AL
    MOV BYTE [fw_state + FW_DIRTY], 1
    POP CX
    JMP .press_done

.ptv_hb_left:
    CMP BYTE [BX + CTRL_TV_SCROLLX], 0
    JZ .ptv_hb_l_done
    DEC BYTE [BX + CTRL_TV_SCROLLX]
.ptv_hb_l_done:
    MOV BYTE [fw_state + FW_DIRTY], 1
    POP CX
    JMP .press_done

.ptv_hb_right:
    MOV AL, [BX + CTRL_TV_MAXLEN]
    MOV AH, [BX + CTRL_W]
    SUB AH, CH                  ; AH = eff_w
    SUB AL, AH                  ; AL = max_scroll_x
    CMP [BX + CTRL_TV_SCROLLX], AL
    JAE .ptv_hb_r_done
    INC BYTE [BX + CTRL_TV_SCROLLX]
.ptv_hb_r_done:
    MOV BYTE [fw_state + FW_DIRTY], 1
    POP CX
    JMP .press_done

.ptv_no_hbar_click:
    POP CX

    ; Normal content click: curline = SCROLL + (click_row - ctrl_abs_row)
    MOV AL, [SI + WIN_Y]
    TEST BYTE [SI + WIN_FLAGS], WINF_BORDER
    JZ .ptv_no_bdr
    INC AL
.ptv_no_bdr:
    ADD AL, [BX + CTRL_Y]       ; AL = abs top row of textview
    MOV AH, DH                  ; AH = click row
    SUB AH, AL                  ; AH = relative row within textview
    ADD AH, [BX + CTRL_TV_SCROLL] ; AH = line index
    ; Clamp: if AH >= COUNT, AH = COUNT-1
    CMP AH, [BX + CTRL_TV_COUNT]
    JB .ptv_ok
    MOV AH, [BX + CTRL_TV_COUNT]
    DEC AH
.ptv_ok:
    MOV [BX + CTRL_TV_CURLINE], AH
    CALL tui_tv_ensure_visible
    JMP .press_done

.press_editor:
    CALL tui_ed_mouse_press
    JMP .press_done

.press_done:
    POP DI
    POP SI
    POP DX
    POP CX
    POP BX
    POP AX
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_mouse_on_release - Handle left-button-up
; ----------------------------------------------------------------------------
tui_mouse_on_release: PROC
    PUSH AX
    PUSH BX
    PUSH CX
    PUSH DX
    PUSH SI
    PUSH DI

    ; End drag/resize if active
    AND BYTE [mouse_state + MS_FLAGS], 0FFh - MSF_DRAGGING - MSF_RESIZING
    MOV BYTE [mouse_state + MS_DRAG_WIN], 0FFh

    ; Check if a button was being pressed
    MOV BX, [mouse_state + MS_PRESSED]
    OR BX, BX
    JZ .rel_done

    ; Clear pressed state
    MOV WORD [mouse_state + MS_PRESSED], 0
    AND BYTE [mouse_state + MS_FLAGS], 0FFh - MSF_BTN_DOWN
    MOV BYTE [fw_state + FW_DIRTY], 1

    ; Hit test at current position to see if still over the same button
    MOV DH, [mouse_state + MS_ROW]
    MOV DL, [mouse_state + MS_COL]
    PUSH BX                 ; save original pressed control (BX clobbered by hit_test)
    CALL tui_mouse_hit_test
    CMP AL, HIT_INTERIOR
    JNZ .rel_miss

    ; Check control under cursor
    CALL tui_mouse_hit_test_control
    MOV DI, BX              ; DI = control at release position
    POP BX                  ; BX = originally pressed control

    CMP DI, BX              ; same control?
    JNZ .rel_done

    ; Activate the button: call its handler
    CALL tui_mouse_call_handler
    JMP .rel_done

.rel_miss:
    POP BX                  ; balance stack from PUSH above

.rel_done:
    POP DI
    POP SI
    POP DX
    POP CX
    POP BX
    POP AX
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_mouse_hit_test_control - Find control at (col,row) within window
; Input:  SI = window struct, DH = abs row, DL = abs col
; Output: BX = control ptr (0 if none)
; Clobbers: AX
; ----------------------------------------------------------------------------
tui_mouse_hit_test_control: PROC
    PUSH CX

    ; Calculate interior origin
    MOV AL, [SI + WIN_X]
    MOV AH, [SI + WIN_Y]
    TEST BYTE [SI + WIN_FLAGS], WINF_BORDER
    JZ .htc_origin_set
    INC AL
    INC AH
.htc_origin_set:
    ; AL = interior left col, AH = interior top row

    ; Calculate relative position
    ; rel_col = DL - AL, rel_row = DH - AH
    MOV CL, DL
    SUB CL, AL             ; CL = rel_col
    MOV CH, DH
    SUB CH, AH             ; CH = rel_row

    ; Walk control linked list
    MOV BX, [SI + WIN_FIRST]
.htc_loop:
    OR BX, BX
    JZ .htc_not_found

    ; Check CTRLF_VISIBLE
    TEST BYTE [BX + CTRL_FLAGS], CTRLF_VISIBLE
    JZ .htc_next

    ; Check X range: rel_col >= CTRL_X and rel_col < CTRL_X + CTRL_W
    CMP CL, [BX + CTRL_X]
    JB .htc_next
    MOV AL, [BX + CTRL_X]
    ADD AL, [BX + CTRL_W]
    CMP CL, AL
    JAE .htc_next

    ; Check Y range: rel_row >= CTRL_Y and rel_row < CTRL_Y + CTRL_H
    CMP CH, [BX + CTRL_Y]
    JB .htc_next
    MOV AL, [BX + CTRL_Y]
    ADD AL, [BX + CTRL_H]
    CMP CH, AL
    JAE .htc_next

    ; Found it - BX already has control ptr
    JMP .htc_ret

.htc_next:
    MOV BX, [BX + CTRL_NEXT]
    JMP .htc_loop

.htc_not_found:
    XOR BX, BX

.htc_ret:
    POP CX
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_mouse_call_handler - Call a control's handler via RET-trampoline
; Input: BX = control ptr, SI = window struct ptr
; Note: SI/BX are preserved for caller. Handler gets SI=ctrl, DI=win.
; ----------------------------------------------------------------------------
tui_mouse_call_handler: PROC
    PUSH AX
    PUSH DI

    MOV AX, [BX + CTRL_HANDLER]
    OR AX, AX
    JZ .no_handler

    MOV WORD [ctrl_dispatch_addr], AX
    MOV DI, SI              ; DI = window struct
    PUSH SI
    PUSH BX
    MOV SI, BX              ; SI = control struct (handler convention)
    MOV AX, .after_handler
    PUSH AX                 ; return address
    MOV AX, [ctrl_dispatch_addr]
    PUSH AX                 ; handler address
    RET                     ; jumps to handler
.after_handler:
    POP BX
    POP SI

.no_handler:
    POP DI
    POP AX
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_mouse_on_drag - Update window position during drag
; ----------------------------------------------------------------------------
tui_mouse_on_drag: PROC
    PUSH AX
    PUSH BX
    PUSH SI

    ; Get window being dragged
    MOV AL, [mouse_state + MS_DRAG_WIN]
    CMP AL, 0FFh
    JZ .drag_done

    ; Get window struct: SI = win_table + slot * WIN_SIZE
    XOR AH, AH
    MOV BL, WIN_SIZE
    MUL BL
    MOV SI, AX
    ADD SI, win_table

    ; Check MOVABLE flag
    TEST BYTE [SI + WIN_FLAGS], WINF_MOVABLE
    JZ .drag_done

    ; New X = cursor_col - offset_x
    MOV AL, [mouse_state + MS_COL]
    SUB AL, [mouse_state + MS_DRAG_OX]
    ; Clamp X: if signed negative (underflow), set to 0
    JNC .x_not_neg
    XOR AL, AL
.x_not_neg:
    ; Clamp X: don't let window go off right edge entirely
    ; (allow partial, but X must be < SCR_W)
    CMP AL, SCR_W
    JB .x_ok
    MOV AL, SCR_W - 1
.x_ok:
    MOV [SI + WIN_X], AL

    ; New Y = cursor_row - offset_y
    MOV AL, [mouse_state + MS_ROW]
    SUB AL, [mouse_state + MS_DRAG_OY]
    JNC .y_not_neg
    XOR AL, AL
.y_not_neg:
    ; If menu bar exists, min Y = 1
    CMP WORD [fw_state + FW_MENUBAR], 0
    JZ .y_no_menu
    CMP AL, 1
    JAE .y_menu_ok
    MOV AL, 1
.y_menu_ok:
    JMP .y_ok
.y_no_menu:
    ; No menu bar constraint
.y_ok:
    ; Clamp Y: keep top row visible
    CMP AL, SCR_H - 1
    JBE .y_max_ok
    MOV AL, SCR_H - 1
.y_max_ok:
    MOV [SI + WIN_Y], AL

    MOV BYTE [fw_state + FW_DIRTY], 1

.drag_done:
    POP SI
    POP BX
    POP AX
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_mouse_on_resize - Update window size during resize drag
; New width = cursor_col - WIN_X + 1, new height = cursor_row - WIN_Y + 1
; Clamps to min dimensions and screen bounds.
; ----------------------------------------------------------------------------
tui_mouse_on_resize: PROC
    PUSH AX
    PUSH BX
    PUSH SI

    ; Get window being resized
    MOV AL, [mouse_state + MS_DRAG_WIN]
    CMP AL, 0FFh
    JZ .resize_done

    ; Get window struct: SI = win_table + slot * WIN_SIZE
    XOR AH, AH
    MOV BL, WIN_SIZE
    MUL BL
    MOV SI, AX
    ADD SI, win_table

    ; New width = cursor_col - WIN_X + 1
    MOV AL, [mouse_state + MS_COL]
    SUB AL, [SI + WIN_X]
    INC AL
    ; Clamp min width
    CMP AL, WIN_MINW
    JAE .w_min_ok
    MOV AL, WIN_MINW
.w_min_ok:
    ; Clamp max: WIN_X + width <= SCR_W
    MOV AH, [SI + WIN_X]
    ADD AH, AL
    CMP AH, SCR_W
    JBE .w_max_ok
    MOV AL, SCR_W
    SUB AL, [SI + WIN_X]
.w_max_ok:
    MOV [SI + WIN_W], AL

    ; New height = cursor_row - WIN_Y + 1
    MOV AL, [mouse_state + MS_ROW]
    SUB AL, [SI + WIN_Y]
    INC AL
    ; Clamp min height
    CMP AL, WIN_MINH
    JAE .h_min_ok
    MOV AL, WIN_MINH
.h_min_ok:
    ; Clamp max: WIN_Y + height <= SCR_H
    MOV AH, [SI + WIN_Y]
    ADD AH, AL
    CMP AH, SCR_H
    JBE .h_max_ok
    MOV AL, SCR_H
    SUB AL, [SI + WIN_Y]
.h_max_ok:
    MOV [SI + WIN_H], AL

    MOV BYTE [fw_state + FW_DIRTY], 1

.resize_done:
    POP SI
    POP BX
    POP AX
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_mouse_bring_to_front - Reorder z_order to make window topmost
; Input: AL = window slot index
; ----------------------------------------------------------------------------
tui_mouse_bring_to_front: PROC
    PUSH AX
    PUSH BX
    PUSH CX
    PUSH DX
    PUSH DI

    ; If already top window, nothing to do
    CMP AL, [fw_state + FW_TOPWIN]
    JZ .btf_done

    ; Close any open dropdown popup
    MOV WORD [fw_state + FW_OPENDD], 0

    ; Find window in z_order
    MOV CL, [fw_state + FW_NUMWIN]
    XOR CH, CH
    XOR BX, BX
    MOV DL, AL             ; save window slot in DL
.btf_find:
    CMP BX, CX
    JZ .btf_done            ; not found (shouldn't happen)
    CMP [BX + z_order], DL
    JZ .btf_found
    INC BX
    JMP .btf_find

.btf_found:
    ; BX = position in z_order. Shift entries above it down by one.
    MOV DI, BX
.btf_shift:
    MOV AX, DI
    INC AX
    CMP AX, CX             ; past end?
    JZ .btf_place
    ; z_order[DI] = z_order[DI+1]
    PUSH BX
    MOV BX, AX
    MOV AH, [BX + z_order]
    MOV BX, DI
    MOV [BX + z_order], AH
    POP BX
    INC DI
    JMP .btf_shift

.btf_place:
    ; Place window at z_order[numwin-1] (top)
    MOV BX, CX
    DEC BX
    MOV [BX + z_order], DL

    ; Update FW_TOPWIN
    MOV [fw_state + FW_TOPWIN], DL
    MOV BYTE [fw_state + FW_DIRTY], 1

.btf_done:
    POP DI
    POP DX
    POP CX
    POP BX
    POP AX
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_mouse_close_menu - Close menu bar if active
; Preserves all registers.
; ----------------------------------------------------------------------------
tui_mouse_close_menu: PROC
    PUSH DI
    MOV DI, [fw_state + FW_MENUBAR]
    OR DI, DI
    JZ .cm_done
    CMP BYTE [DI + MBAR_SEL], 0FFh
    JZ .cm_done
    ; Close menu
    MOV BYTE [DI + MBAR_SEL], 0FFh
    MOV BYTE [DI + MBAR_OPEN], 0
    MOV BYTE [DI + MBAR_DDSEL], 0FFh
    MOV BYTE [fw_state + FW_DIRTY], 1
.cm_done:
    POP DI
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_mouse_menu_hover - Handle mouse hover for menu bar/dropdown switching
; Called when position changed with no button pressed.
; If menu is open and cursor moves to different menu item, switch dropdown.
; If cursor moves over dropdown entry, highlight it.
; ----------------------------------------------------------------------------
tui_mouse_menu_hover: PROC
    PUSH AX
    PUSH BX
    PUSH CX
    PUSH DX
    PUSH SI
    PUSH DI

    ; Check if menu is active and open
    MOV DI, [fw_state + FW_MENUBAR]
    OR DI, DI
    JZ .mh_done
    CMP BYTE [DI + MBAR_OPEN], 1
    JNZ .mh_done

    ; Hit test at current cursor position
    MOV DH, [mouse_state + MS_ROW]
    MOV DL, [mouse_state + MS_COL]
    CALL tui_mouse_hit_test

    CMP AL, HIT_MENUBAR
    JZ .mh_bar_hover
    CMP AL, HIT_MENUDROP
    JZ .mh_drop_hover
    JMP .mh_done

.mh_bar_hover:
    ; BX = item index. If different from current MBAR_SEL, switch.
    CMP BL, [DI + MBAR_SEL]
    JZ .mh_done
    MOV [DI + MBAR_SEL], BL
    MOV BYTE [DI + MBAR_DDSEL], 0
    MOV BYTE [fw_state + FW_DIRTY], 1
    JMP .mh_done

.mh_drop_hover:
    ; BX = entry index. Highlight it.
    CMP BL, [DI + MBAR_DDSEL]
    JZ .mh_done
    MOV [DI + MBAR_DDSEL], BL
    MOV BYTE [fw_state + FW_DIRTY], 1

.mh_done:
    POP DI
    POP SI
    POP DX
    POP CX
    POP BX
    POP AX
    RET
ENDP

; ============================================================================
; Scroll Bar Mouse Interaction
; ============================================================================

; ----------------------------------------------------------------------------
; tui_sb_vbar_click_generic - Handle click on vbar track/thumb area
; Input:  AH = relative row within bar (1..bar_height-2, already excluding arrows)
;         AL = current scroll_pos, CL = visible_count (= bar_height),
;         CH = total_count
;         BX = control ptr, SI = window ptr
; Output: AL = new scroll value
; Clobbers: AH, CX, DX (caller must save if needed)
; Uses sb_scratch for thumb math.
; ----------------------------------------------------------------------------
tui_sb_vbar_click_generic: PROC
    PUSH DI
    PUSH BP

    ; Save params
    MOV [sb_scratch], AL        ; [0] = scroll_pos
    MOV [sb_scratch + 1], CL   ; [1] = visible (= bar_height)
    MOV [sb_scratch + 2], CH   ; [2] = total
    ; AH = click_rel_row (1-based: 1 = first track cell)

    ; track_len = bar_height - 2
    MOV DI, CX                 ; save visible(CL)/total(CH) in DI
    PUSH BX                     ; save control ptr before corrupting BL
    MOV BL, CL
    SUB BL, 2                   ; BL = track_len
    MOV AL, [sb_scratch + 1]   ; visible
    MOV BP, AX                  ; save AH (click_rel_row) in BP high byte

    MUL BL                      ; AX = visible * track_len
    MOV CL, [sb_scratch + 2]  ; total
    XOR CH, CH
    XOR DX, DX
    DIV CX                      ; AL = thumb_size
    OR AL, AL
    JNZ .svc_ts_ok
    MOV AL, 1
.svc_ts_ok:
    CMP AL, BL
    JBE .svc_ts_clamp
    MOV AL, BL
.svc_ts_clamp:
    MOV [sb_scratch + 3], AL   ; [3] = thumb_size

    ; thumb_pos = (scroll * (track_len - thumb_size)) / max_scroll
    MOV CL, [sb_scratch + 2]  ; total
    SUB CL, [sb_scratch + 1]  ; CL = max_scroll
    XOR CH, CH
    OR CL, CL
    JZ .svc_thumb_at_zero

    MOV AH, BL
    SUB AH, [sb_scratch + 3]  ; AH = range = track_len - thumb_size
    MOV AL, [sb_scratch]       ; scroll_pos
    MUL AH                      ; AX = scroll * range
    XOR DX, DX
    DIV CX                      ; AL = thumb_pos
    JMP .svc_have_tpos

.svc_thumb_at_zero:
    XOR AL, AL

.svc_have_tpos:
    ; AL = thumb_pos (0-based within track), [sb_scratch+3] = thumb_size
    ; BP = (click_rel_row << 8) | visible_count
    ; Extract click_rel_row from BP high byte without destroying AL
    MOV CX, BP                  ; CL = visible, CH = click_rel_row
    MOV AH, CH                  ; AH = click_rel_row
    DEC AH                      ; AH = click_track_pos (0-based)

    ; Compare with thumb region: thumb_pos..thumb_pos+thumb_size-1
    CMP AH, AL
    JB .svc_page_up
    MOV CL, AL
    ADD CL, [sb_scratch + 3]  ; CL = thumb_pos + thumb_size
    CMP AH, CL
    JAE .svc_page_down

    ; ON the thumb → start drag
    SUB AH, AL                  ; AH = offset within thumb
    MOV [mouse_state + MS_SB_CTRL], BX  ; wait, BX was pushed. Use stack value
    POP BX                      ; restore control ptr
    MOV [mouse_state + MS_SB_CTRL], BX
    MOV BYTE [mouse_state + MS_SB_DIR], 0   ; vertical
    MOV [mouse_state + MS_SB_OFFSET], AH
    OR BYTE [mouse_state + MS_FLAGS], MSF_SB_DRAG
    MOV AL, [sb_scratch]        ; return current scroll (unchanged)
    JMP .svc_ret_no_pop

.svc_page_up:
    ; Click above thumb → page up
    POP BX
    MOV AL, [sb_scratch]        ; current scroll
    MOV AH, [sb_scratch + 1]   ; visible
    CMP AL, AH
    JAE .svc_pgu_sub
    XOR AL, AL                  ; clamp to 0
    JMP .svc_ret_no_pop
.svc_pgu_sub:
    SUB AL, AH
    JMP .svc_ret_no_pop

.svc_page_down:
    ; Click below thumb → page down
    POP BX
    MOV AL, [sb_scratch]        ; current scroll
    ADD AL, [sb_scratch + 1]    ; + visible
    ; Clamp to max_scroll = total - visible
    MOV AH, [sb_scratch + 2]
    SUB AH, [sb_scratch + 1]   ; AH = max_scroll
    CMP AL, AH
    JBE .svc_ret_no_pop
    MOV AL, AH

.svc_ret_no_pop:
    POP BP
    POP DI
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_sb_hbar_click_generic - Handle click on hbar track/thumb area
; Input:  AH = relative col within bar (1..bar_width-2, excluding arrows)
;         AL = current scroll_x, CL = bar_width (= eff_w),
;         CH = total_w (max_content_w)
;         BX = control ptr, SI = window ptr
; Output: AL = new scroll_x value
; Clobbers: AH, CX, DX
; Uses sb_scratch for thumb math.
; ----------------------------------------------------------------------------
tui_sb_hbar_click_generic: PROC
    PUSH DI
    PUSH BP

    MOV [sb_scratch], AL        ; [0] = scroll_x
    MOV [sb_scratch + 1], CL   ; [1] = visible_w (= bar_width)
    MOV [sb_scratch + 2], CH   ; [2] = total_w

    MOV DI, CX
    PUSH BX                     ; save control ptr before corrupting BL
    MOV BL, CL
    SUB BL, 2                   ; BL = track_len

    MOV AL, [sb_scratch + 1]
    MOV BP, AX                  ; save AH (click_rel_col)

    MUL BL
    MOV CL, [sb_scratch + 2]
    XOR CH, CH
    XOR DX, DX
    DIV CX
    OR AL, AL
    JNZ .shc_ts_ok
    MOV AL, 1
.shc_ts_ok:
    CMP AL, BL
    JBE .shc_ts_clamp
    MOV AL, BL
.shc_ts_clamp:
    MOV [sb_scratch + 3], AL   ; thumb_size

    MOV CL, [sb_scratch + 2]
    SUB CL, [sb_scratch + 1]
    XOR CH, CH
    OR CL, CL
    JZ .shc_thumb_at_zero

    MOV AH, BL
    SUB AH, [sb_scratch + 3]
    MOV AL, [sb_scratch]
    MUL AH
    XOR DX, DX
    DIV CX
    JMP .shc_have_tpos

.shc_thumb_at_zero:
    XOR AL, AL

.shc_have_tpos:
    ; AL = thumb_pos, BP = saved AX (low=visible_w, high=click_rel_col)
    MOV CX, BP
    MOV AH, CH                  ; AH = click_rel_col
    DEC AH                      ; 0-based

    CMP AH, AL
    JB .shc_page_left
    MOV CL, AL
    ADD CL, [sb_scratch + 3]
    CMP AH, CL
    JAE .shc_page_right

    ; ON thumb → start drag
    SUB AH, AL
    POP BX
    MOV [mouse_state + MS_SB_CTRL], BX
    MOV BYTE [mouse_state + MS_SB_DIR], 1   ; horizontal
    MOV [mouse_state + MS_SB_OFFSET], AH
    OR BYTE [mouse_state + MS_FLAGS], MSF_SB_DRAG
    MOV AL, [sb_scratch]
    JMP .shc_ret_no_pop

.shc_page_left:
    POP BX
    MOV AL, [sb_scratch]
    MOV AH, [sb_scratch + 1]
    CMP AL, AH
    JAE .shc_pgl_sub
    XOR AL, AL
    JMP .shc_ret_no_pop
.shc_pgl_sub:
    SUB AL, AH
    JMP .shc_ret_no_pop

.shc_page_right:
    POP BX
    MOV AL, [sb_scratch]
    ADD AL, [sb_scratch + 1]
    MOV AH, [sb_scratch + 2]
    SUB AH, [sb_scratch + 1]
    CMP AL, AH
    JBE .shc_ret_no_pop
    MOV AL, AH

.shc_ret_no_pop:
    POP BP
    POP DI
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_mouse_on_sb_drag - Update scroll during scroll bar thumb drag
; Called each frame while MSF_SB_DRAG is active.
; Reads MS_SB_CTRL, MS_SB_DIR, MS_SB_OFFSET to compute new scroll value.
; ----------------------------------------------------------------------------
tui_mouse_on_sb_drag: PROC
    PUSH AX
    PUSH BX
    PUSH CX
    PUSH DX
    PUSH SI
    PUSH BP

    ; Get control being dragged
    MOV BX, [mouse_state + MS_SB_CTRL]
    OR BX, BX
    JNZ .sbd_has_ctrl
    JMP .sbd_done
.sbd_has_ctrl:

    ; Get top window struct
    MOV AL, [fw_state + FW_TOPWIN]
    CMP AL, 0FFh
    JNZ .sbd_has_topwin
    JMP .sbd_done
.sbd_has_topwin:
    XOR AH, AH
    PUSH BX
    MOV BL, WIN_SIZE
    MUL BL
    MOV SI, AX
    ADD SI, win_table
    POP BX

    ; Compute abs origin of control
    MOV DL, [SI + WIN_X]
    MOV DH, [SI + WIN_Y]
    TEST BYTE [SI + WIN_FLAGS], WINF_BORDER
    JZ .sbd_no_bdr
    INC DL
    INC DH
.sbd_no_bdr:
    ADD DL, [BX + CTRL_X]
    ADD DH, [BX + CTRL_Y]
    ; DL = abs left col, DH = abs top row

    CMP BYTE [mouse_state + MS_SB_DIR], 0
    JZ .sbd_vertical
    JMP .sbd_horizontal
.sbd_vertical:

    ; --- Vertical drag ---
    ; bar_top_row = DH, bar_height = depends on control type
    ; For listbox: bar_height = CTRL_H, visible = CTRL_H, total = LB_COUNT
    ; For textview: need eff_h. Simplification: use CTRL_H as bar height,
    ;   but we need the actual bar_height used during drawing.
    ; Since we don't know the effective height easily, use CTRL_H for listbox
    ; and approximate for textview (compute auto-hide).

    ; Determine control type for field offsets
    CMP BYTE [BX + CTRL_TYPE], CTYPE_LISTBOX
    JZ .sbd_v_listbox
    CMP BYTE [BX + CTRL_TYPE], CTYPE_TEXTVIEW
    JNZ .sbd_not_tv
    JMP .sbd_v_textview
.sbd_not_tv:
    JMP .sbd_done

.sbd_v_listbox:
    MOV CL, [BX + CTRL_H]      ; bar_height = visible
    MOV CH, [BX + CTRL_LB_COUNT] ; total

    ; track_len = bar_height - 2
    MOV AL, CL
    SUB AL, 2                   ; AL = track_len

    ; thumb_size (same calc as draw)
    PUSH AX
    PUSH DX
    MOV BP, AX                  ; save track_len in BP low
    MOV AL, CL                  ; visible
    MUL BYTE [BP + 0]           ; Nope, can't address BP like that
    ; Let me use a different approach
    POP DX
    POP AX

    ; Simpler approach: compute new_scroll directly from mouse position
    ; new_scroll = ((mouse_row - bar_top - 1 - offset) * max_scroll) / (track_len - thumb_size)
    ; But computing thumb_size requires the same MUL/DIV...

    ; Use the formula: new_scroll = ((mouse_row - bar_top - 1 - offset) * max_scroll + (range/2)) / range
    ; where range = track_len - thumb_size

    ; First compute thumb_size
    PUSH DX                     ; save drawing coords
    MOV [sb_scratch], CL        ; [0] = visible (= bar_height)
    MOV [sb_scratch + 1], CH    ; [1] = total
    MOV [sb_scratch + 2], AL    ; [2] = track_len

    PUSH AX
    MOV AL, CL                  ; visible
    MUL BYTE [sb_scratch + 2]   ; AX = visible * track_len
    MOV CL, [sb_scratch + 1]   ; total
    XOR CH, CH
    XOR DX, DX
    DIV CX                      ; AL = thumb_size
    OR AL, AL
    JNZ .sbd_vl_ts_ok
    MOV AL, 1
.sbd_vl_ts_ok:
    MOV [sb_scratch + 3], AL   ; [3] = thumb_size
    POP AX                      ; AL = track_len

    ; range = track_len - thumb_size
    SUB AL, [sb_scratch + 3]   ; AL = range
    OR AL, AL
    JZ .sbd_vl_no_range

    ; relative = mouse_row - bar_top - 1 - offset
    POP DX                      ; restore drawing coords
    MOV AH, [mouse_state + MS_ROW]
    SUB AH, DH                  ; AH = mouse_row - bar_top
    DEC AH                      ; -1 (past up arrow)
    SUB AH, [mouse_state + MS_SB_OFFSET] ; -offset

    ; Clamp relative to 0..range
    CMP AH, 80h                 ; check for negative (signed)
    JB .sbd_vl_not_neg
    XOR AH, AH
.sbd_vl_not_neg:
    CMP AH, AL
    JBE .sbd_vl_rel_ok
    MOV AH, AL
.sbd_vl_rel_ok:
    ; new_scroll = (relative * max_scroll) / range
    PUSH AX                     ; save range in AL
    MOV AL, AH                  ; AL = relative
    MOV CL, [sb_scratch + 1]  ; total
    SUB CL, [sb_scratch]       ; CL = max_scroll
    MUL CL                      ; AX = relative * max_scroll
    POP CX                      ; CL = range
    XOR CH, CH
    XOR DX, DX
    DIV CX                      ; AL = new_scroll
    MOV [BX + CTRL_LB_SCROLL], AL
    MOV BYTE [fw_state + FW_DIRTY], 1
    JMP .sbd_done

.sbd_vl_no_range:
    POP DX                      ; balance stack
    JMP .sbd_done

.sbd_v_textview:
    ; For textview: compute eff_h via auto-hide, same as draw
    ; Simplified: check needs_hbar
    PUSH DX

    MOV CL, [BX + CTRL_H]      ; start with CTRL_H
    ; Check needs_hbar: MAXLEN > CTRL_W - 1 (assumes vbar present since we're dragging it)
    MOV AL, [BX + CTRL_TV_MAXLEN]
    MOV AH, [BX + CTRL_W]
    DEC AH                      ; eff_w with vbar
    CMP AL, AH
    JBE .sbd_vt_no_hbar
    DEC CL                      ; eff_h = CTRL_H - 1 (hbar present)
.sbd_vt_no_hbar:
    ; CL = eff_h = bar_height for vbar
    MOV CH, [BX + CTRL_TV_COUNT]

    ; Same math as listbox vbar drag
    MOV [sb_scratch], CL
    MOV [sb_scratch + 1], CH
    MOV AL, CL
    SUB AL, 2                   ; AL = track_len
    MOV [sb_scratch + 2], AL

    PUSH AX
    MOV AL, CL
    MUL BYTE [sb_scratch + 2]
    MOV CL, [sb_scratch + 1]
    XOR CH, CH
    XOR DX, DX
    DIV CX
    OR AL, AL
    JNZ .sbd_vt_ts_ok
    MOV AL, 1
.sbd_vt_ts_ok:
    MOV [sb_scratch + 3], AL
    POP AX

    SUB AL, [sb_scratch + 3]
    OR AL, AL
    JZ .sbd_vt_no_range

    POP DX
    MOV AH, [mouse_state + MS_ROW]
    SUB AH, DH
    DEC AH
    SUB AH, [mouse_state + MS_SB_OFFSET]

    CMP AH, 80h
    JB .sbd_vt_not_neg
    XOR AH, AH
.sbd_vt_not_neg:
    CMP AH, AL
    JBE .sbd_vt_rel_ok
    MOV AH, AL
.sbd_vt_rel_ok:
    PUSH AX
    MOV AL, AH
    MOV CL, [sb_scratch + 1]
    SUB CL, [sb_scratch]
    MUL CL
    POP CX
    XOR CH, CH
    XOR DX, DX
    DIV CX
    MOV [BX + CTRL_TV_SCROLL], AL
    MOV BYTE [fw_state + FW_DIRTY], 1
    JMP .sbd_done

.sbd_vt_no_range:
    POP DX
    JMP .sbd_done

.sbd_horizontal:
    ; --- Horizontal drag (textview only) ---
    ; bar is at bottom row, width = eff_w = CTRL_W - 1 (vbar always present when hbar is)
    PUSH DX

    MOV CL, [BX + CTRL_W]
    DEC CL                      ; CL = eff_w = bar_width
    MOV CH, [BX + CTRL_TV_MAXLEN] ; total_w

    MOV [sb_scratch], CL
    MOV [sb_scratch + 1], CH
    MOV AL, CL
    SUB AL, 2
    MOV [sb_scratch + 2], AL   ; track_len

    PUSH AX
    MOV AL, CL
    MUL BYTE [sb_scratch + 2]
    MOV CL, [sb_scratch + 1]
    XOR CH, CH
    XOR DX, DX
    DIV CX
    OR AL, AL
    JNZ .sbd_h_ts_ok
    MOV AL, 1
.sbd_h_ts_ok:
    MOV [sb_scratch + 3], AL
    POP AX

    SUB AL, [sb_scratch + 3]
    OR AL, AL
    JZ .sbd_h_no_range

    POP DX
    MOV AH, [mouse_state + MS_COL]
    SUB AH, DL
    DEC AH
    SUB AH, [mouse_state + MS_SB_OFFSET]

    CMP AH, 80h
    JB .sbd_h_not_neg
    XOR AH, AH
.sbd_h_not_neg:
    CMP AH, AL
    JBE .sbd_h_rel_ok
    MOV AH, AL
.sbd_h_rel_ok:
    PUSH AX
    MOV AL, AH
    MOV CL, [sb_scratch + 1]
    SUB CL, [sb_scratch]
    MUL CL
    POP CX
    XOR CH, CH
    XOR DX, DX
    DIV CX
    MOV [BX + CTRL_TV_SCROLLX], AL
    MOV BYTE [fw_state + FW_DIRTY], 1
    JMP .sbd_done

.sbd_h_no_range:
    POP DX

.sbd_done:
    POP BP
    POP SI
    POP DX
    POP CX
    POP BX
    POP AX
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_mouse_on_ctrl_drag - Dispatch drag events to control-type handler
; Called each frame while MSF_CTRL_DRAG is active and button held.
; Reads MS_PRESSED for the control being dragged.
; ----------------------------------------------------------------------------
tui_mouse_on_ctrl_drag: PROC
    PUSH AX
    PUSH BX

    MOV BX, [mouse_state + MS_PRESSED]
    OR BX, BX
    JZ .done

    CMP BYTE [BX + CTRL_TYPE], CTYPE_EDITOR
    JNZ .done
    CALL tui_ed_mouse_drag

.done:
    POP BX
    POP AX
    RET
ENDP
