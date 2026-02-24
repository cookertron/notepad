; ============================================================================
; tui_menu.inc - Menu Bar + Dropdown Menus
; ============================================================================

; ----------------------------------------------------------------------------
; _mi_index_to_offset - Convert menu item index to byte offset
; Input:  AL = item index
; Output: AX = index * MI_SIZE
; Clobbers: AH
; ----------------------------------------------------------------------------
_mi_index_to_offset: PROC
    XOR AH, AH
    PUSH CX
    MOV CL, MI_SIZE
    MUL CL
    POP CX
    RET
ENDP

; ----------------------------------------------------------------------------
; _mde_index_to_offset - Convert dropdown entry index to byte offset
; Input:  AL = entry index
; Output: AX = index * MDE_SIZE
; Clobbers: AH
; ----------------------------------------------------------------------------
_mde_index_to_offset: PROC
    XOR AH, AH
    PUSH CX
    MOV CL, MDE_SIZE
    MUL CL
    POP CX
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_menu_draw_bar - Draw the menu bar on row 0
; Input: none (reads FW_MENUBAR from fw_state)
; ----------------------------------------------------------------------------
tui_menu_draw_bar: PROC
    PushAll

    ; Check if menu bar exists
    MOV DI, [fw_state + FW_MENUBAR]
    OR DI, DI
    JNZ .has_menu
    JMP .done
.has_menu:

    ; Fill row 0 with spaces + normal bar attr
    MOV DH, 0
    MOV DL, 0
    MOV AL, 20h
    MOV AH, [DI + MBAR_ATTR]
    MOV CL, SCR_W
    CALL tui_hline

    ; Loop through menu items
    MOV BX, [DI + MBAR_ITEMS]  ; BX = items array base
    XOR CX, CX                 ; CX = item index
.item_loop:
    CMP CL, [DI + MBAR_COUNT]
    JAE .hotkey_pass

    ; Calculate item pointer: BX + CX * MI_SIZE
    PUSH CX
    PUSH BX                     ; save items base
    MOV AL, CL
    CALL _mi_index_to_offset    ; AX = index * MI_SIZE
    ADD BX, AX                 ; BX = pointer to this menu item

    ; Select attribute: highlighted if selected
    MOV AH, [DI + MBAR_ATTR]
    CMP CL, [DI + MBAR_SEL]
    JNZ .item_normal
    MOV AH, [DI + MBAR_SELATTR]
.item_normal:

    ; Fill MI_W chars at (0, MI_X) with attr
    MOV DH, 0
    MOV DL, [BX + MI_X]
    MOV AL, 20h
    PUSH CX
    MOV CL, [BX + MI_W]
    CALL tui_hline
    POP CX

    ; Draw text at (0, MI_X + 1)
    PUSH SI
    MOV DL, [BX + MI_X]
    INC DL                      ; pad 1 space
    MOV SI, [BX + MI_TEXT]
    CALL tui_putstr
    POP SI

    POP BX                      ; restore items base
    POP CX                      ; restore index
    INC CL
    JMP .item_loop

    ; --- Hotkey highlight pass ---
    ; Overwrites one char per item with CLR_MENU_HOTKEY attr
    ; Skips selected items (selection color takes priority)
.hotkey_pass:
    MOV BX, [DI + MBAR_ITEMS]
    XOR CX, CX
.hk_loop:
    CMP CL, [DI + MBAR_COUNT]
    JAE .done

    PUSH CX
    PUSH BX
    MOV AL, CL
    CALL _mi_index_to_offset
    ADD BX, AX                 ; BX = menu item ptr

    ; Skip if this item is selected (selection color wins)
    CMP CL, [DI + MBAR_SEL]
    JZ .hk_skip

    ; Skip if MI_HOTIDX == FFh (no hotkey)
    CMP BYTE [BX + MI_HOTIDX], 0FFh
    JZ .hk_skip

    ; Compute screen col: MI_X + 1 + MI_HOTIDX
    MOV DH, 0                  ; row 0
    MOV DL, [BX + MI_X]
    INC DL                      ; +1 for pad
    ADD DL, [BX + MI_HOTIDX]

    ; Read the character from MI_TEXT[MI_HOTIDX]
    PUSH SI
    MOV SI, [BX + MI_TEXT]
    MOV AL, [BX + MI_HOTIDX]
    XOR AH, AH
    ADD SI, AX
    MOV AL, [SI]                ; AL = hotkey char
    POP SI

    MOV AH, CLR_MENU_HOTKEY
    CALL tui_putchar

.hk_skip:
    POP BX
    POP CX
    INC CL
    JMP .hk_loop

.done:
    PopAll
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_menu_draw_dropdown - Draw the open dropdown menu
; Input: none (reads FW_MENUBAR from fw_state)
; ----------------------------------------------------------------------------
tui_menu_draw_dropdown: PROC
    PushAll

    MOV DI, [fw_state + FW_MENUBAR]
    OR DI, DI
    JNZ .dd_has_menu
    JMP .dd_done
.dd_has_menu:

    CMP BYTE [DI + MBAR_OPEN], 1
    JZ .dd_is_open
    JMP .dd_done
.dd_is_open:

    ; Get selected menu item
    MOV AL, [DI + MBAR_SEL]
    CALL _mi_index_to_offset    ; AX = sel * MI_SIZE
    MOV BX, [DI + MBAR_ITEMS]
    ADD BX, AX                 ; BX = selected menu item

    ; Dropdown position: col = MI_X, starting row = 1
    MOV DL, [BX + MI_X]        ; left col
    MOV DH, 1                  ; top row

    ; Dimensions
    MOV CH, [BX + MI_ECOUNT]   ; entry count
    MOV CL, [BX + MI_DDW]      ; dropdown inner width

    ; --- Draw box border ---
    ; Top-left corner
    PUSH DX
    MOV AL, BOX_TL
    MOV AH, CLR_MENU_DD
    CALL tui_putchar

    ; Top horizontal line
    INC DL
    MOV AL, BOX_H
    PUSH CX
    CALL tui_hline              ; CL = ddw already
    POP CX
    POP DX

    ; Top-right corner
    PUSH DX
    ADD DL, CL
    INC DL                      ; col = x + ddw + 1
    MOV AL, BOX_TR
    MOV AH, CLR_MENU_DD
    CALL tui_putchar
    POP DX

    ; Left/right borders + interior fill
    PUSH DX
    INC DH                      ; row = 2
    PUSH CX                     ; save entry count / ddw
    MOV AL, CH                  ; AL = entry count
    XOR AH, AH
    MOV CH, AL                  ; CH = rows to draw
.dd_border_loop:
    OR CH, CH
    JZ .dd_border_done

    ; Left border
    MOV AL, BOX_V
    MOV AH, CLR_MENU_DD
    CALL tui_putchar

    ; Interior: fill ddw spaces
    PUSH DX
    INC DL
    MOV AL, 20h
    MOV AH, CLR_MENU_DD
    PUSH CX
    CALL tui_hline              ; CL still = ddw
    POP CX
    POP DX

    ; Right border
    PUSH DX
    ADD DL, CL
    INC DL                      ; col = x + ddw + 1
    MOV AL, BOX_V
    MOV AH, CLR_MENU_DD
    CALL tui_putchar
    POP DX

    INC DH
    DEC CH
    JMP .dd_border_loop

.dd_border_done:
    POP CX                      ; restore ddw/ecount
    ; Bottom-left corner
    MOV AL, BOX_BL
    MOV AH, CLR_MENU_DD
    CALL tui_putchar

    ; Bottom horizontal
    PUSH DX
    INC DL
    MOV AL, BOX_H
    PUSH CX
    CALL tui_hline
    POP CX
    POP DX

    ; Bottom-right corner
    PUSH DX
    ADD DL, CL
    INC DL
    MOV AL, BOX_BR
    MOV AH, CLR_MENU_DD
    CALL tui_putchar
    POP DX

    POP DX                      ; restore original DX

    ; --- Draw entries ---
    PUSH DX
    MOV DH, 2                  ; first entry row
    MOV SI, [BX + MI_ENTRIES]  ; SI = entries array base
    XOR CH, CH                 ; CH = entry index
.dd_entry_loop:
    CMP CH, [BX + MI_ECOUNT]
    JAE .dd_entries_done

    ; Select attr: highlighted if selected
    MOV AH, CLR_MENU_DD
    CMP CH, [DI + MBAR_DDSEL]
    JNZ .dd_entry_normal
    MOV AH, CLR_MENU_DDSEL
.dd_entry_normal:

    ; Fill ddw chars at (row, x+1) with attr
    PUSH DX
    INC DL                      ; col = x + 1
    MOV AL, 20h
    PUSH CX
    MOV CL, [BX + MI_DDW]
    CALL tui_hline
    POP CX
    POP DX

    ; Draw entry text
    PUSH SI
    PUSH DX
    INC DL                      ; col = x + 1

    ; Calculate entry pointer: entries_base + index * MDE_SIZE
    PUSH AX
    MOV AL, CH
    CALL _mde_index_to_offset  ; AX = index * MDE_SIZE
    PUSH BX
    MOV BX, [BX + MI_ENTRIES]
    ADD BX, AX                 ; BX = entry pointer
    MOV SI, [BX + MDE_TEXT]    ; SI = entry text
    POP BX
    POP AX

    ; AH still has correct attr
    CALL tui_putstr
    POP DX
    POP SI

    INC DH                      ; next row
    INC CH                      ; next entry
    JMP .dd_entry_loop

.dd_entries_done:
    POP DX                      ; restore original position

    ; --- Accelerator text pass ---
    ; Draw right-aligned accel text (e.g., "Ctrl+S") for each entry
    PUSH DX
    XOR CH, CH                 ; CH = entry index
.dd_acc_loop:
    CMP CH, [BX + MI_ECOUNT]
    JAE .dd_acc_done

    PUSH AX
    MOV AL, CH
    CALL _mde_index_to_offset  ; AX = index * MDE_SIZE

    ; Save MI_X and MI_DDW before reassigning BX
    MOV DL, [BX + MI_X]
    MOV CL, [BX + MI_DDW]

    PUSH BX                    ; save menu item ptr
    MOV BX, [BX + MI_ENTRIES]
    ADD BX, AX                 ; BX = entry ptr

    ; Skip if MDE_ACCEL == 0
    CMP WORD [BX + MDE_ACCEL], 0
    JZ .dd_acc_skip

    ; Get accel string length
    PUSH SI
    PUSH CX
    MOV SI, [BX + MDE_ACCEL]
    XOR CX, CX
.dd_acc_strlen:
    CMP BYTE [SI], 0
    JZ .dd_acc_strlen_done
    INC SI
    INC CX
    JMP .dd_acc_strlen
.dd_acc_strlen_done:
    ; CX = accel string length (CL = len, CH = 0)
    ; Compute column: MI_X + 1 + MI_DDW - accel_len
    POP AX                     ; AX = saved CX (AL=MI_DDW, AH=entry_index)
    MOV CH, AH                 ; restore entry index (clobbered by strlen)
    PUSH CX                    ; save CL=accel_len, CH=entry_index
    MOV DH, 2
    ADD DH, CH                 ; row = 2 + entry index
    ; DL = MI_X already
    ADD DL, AL                 ; DL = MI_X + MI_DDW
    INC DL                     ; DL = MI_X + MI_DDW + 1
    SUB DL, CL                ; DL = MI_X + MI_DDW + 1 - accel_len

    ; Select attr: highlighted if selected
    MOV AH, CLR_MENU_DD
    CMP CH, [DI + MBAR_DDSEL]
    JNZ .dd_acc_attr_ok
    MOV AH, CLR_MENU_DDSEL
.dd_acc_attr_ok:

    ; Draw the accel string char by char
    MOV SI, [BX + MDE_ACCEL]
    POP CX                     ; CX = accel_len
.dd_acc_draw:
    OR CL, CL
    JZ .dd_acc_draw_done
    MOV AL, [SI]
    CALL tui_putchar
    INC DL
    INC SI
    DEC CL
    JMP .dd_acc_draw
.dd_acc_draw_done:
    POP SI                     ; restore SI
    JMP .dd_acc_next

.dd_acc_skip:
    POP BX                     ; restore menu item ptr
    POP AX
    INC CH
    JMP .dd_acc_loop

.dd_acc_next:
    POP BX                     ; restore menu item ptr
    POP AX
    INC CH
    JMP .dd_acc_loop

.dd_acc_done:
    POP DX

    ; --- Hotkey highlight pass ---
    ; Overwrite one char per entry with CLR_MENU_DD_HOTKEY attr
    PUSH DX
    XOR CH, CH                 ; CH = entry index
.dd_hk_loop:
    CMP CH, [BX + MI_ECOUNT]
    JAE .dd_hk_done

    PUSH AX
    MOV AL, CH
    CALL _mde_index_to_offset  ; AX = index * MDE_SIZE

    ; Save MI_X before reassigning BX
    MOV DL, [BX + MI_X]

    PUSH BX                    ; save menu item ptr
    MOV BX, [BX + MI_ENTRIES]
    ADD BX, AX                 ; BX = entry ptr

    ; Skip if MDE_HOTIDX == FFh
    CMP BYTE [BX + MDE_HOTIDX], 0FFh
    JZ .dd_hk_skip

    ; Skip if this is the selected entry (selection color wins)
    CMP CH, [DI + MBAR_DDSEL]
    JZ .dd_hk_skip

    ; Compute screen position: row = 2 + index, col = MI_X + 1 + MDE_HOTIDX
    MOV DH, 2
    ADD DH, CH
    ; DL already = MI_X
    INC DL
    ADD DL, [BX + MDE_HOTIDX]

    ; Read the hotkey char from the entry text
    PUSH SI
    MOV SI, [BX + MDE_TEXT]
    MOV AL, [BX + MDE_HOTIDX]
    XOR AH, AH
    ADD SI, AX
    MOV AL, [SI]               ; AL = hotkey char
    POP SI

    MOV AH, CLR_MENU_DD_HOTKEY
    CALL tui_putchar

.dd_hk_skip:
    POP BX                     ; restore menu item ptr
    POP AX
    INC CH
    JMP .dd_hk_loop
.dd_hk_done:
    POP DX

    ; --- Draw shadow ---
    ; Right shadow column: col = x + ddw + 2, rows 2..2+ecount-1
    PUSH DX
    MOV DL, [BX + MI_X]
    ADD DL, [BX + MI_DDW]
    ADD DL, 2                   ; col = x + ddw + 2
    MOV DH, 2                  ; start row
    MOV CH, [BX + MI_ECOUNT]
    INC CH                      ; include bottom border row
.dd_shadow_r:
    OR CH, CH
    JZ .dd_shadow_r_done
    CALL tui_darken_cell
    INC DH
    DEC CH
    JMP .dd_shadow_r
.dd_shadow_r_done:
    POP DX

    ; Bottom shadow row: row = 2 + ecount + 1, cols x+1..x+ddw+2
    PUSH DX
    MOV DH, [BX + MI_ECOUNT]
    ADD DH, 3                   ; row = 2 + ecount + 1
    MOV DL, [BX + MI_X]
    INC DL                      ; col = x + 1
    MOV CL, [BX + MI_DDW]
    ADD CL, 2                   ; width = ddw + 2 (body + right border)
    CALL tui_darken_hline
    POP DX

.dd_done:
    PopAll
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_menu_handle_key - Key handler for menu state machine
; Input: AH = key type, AL = char/scan
; Output: CF=0 if handled (consumed), CF=1 if not handled
; ----------------------------------------------------------------------------
tui_menu_handle_key: PROC
    PUSH BX
    PUSH CX
    PUSH DX
    PUSH SI
    PUSH DI

    MOV DI, [fw_state + FW_MENUBAR]
    OR DI, DI
    JNZ .mhk_has_menu
    JMP .mhk_not_handled
.mhk_has_menu:

    ; Check menu state
    CMP BYTE [DI + MBAR_SEL], 0FFh
    JNZ .mhk_active
    JMP .mhk_inactive

; ---- INACTIVE: only intercept F10 ----
.mhk_inactive:
    CMP AH, KTYPE_EXTENDED
    JZ .mhk_inactive_ext
    JMP .mhk_not_handled
.mhk_inactive_ext:
    CMP AL, KEY_F10
    JZ .mhk_inactive_f10
    ; Try Alt+key match (opens menu dropdown)
    CALL _mhk_check_alt_key
    JNC .mhk_inactive_alt_matched
    ; Try global accelerator match (calls handler directly)
    CALL _mhk_check_accel
    JC .mhk_not_handled
    ; Accel handler was called, key consumed
    CLC
    JMP .mhk_ret
.mhk_inactive_alt_matched:
    ; Match: BL = item index → activate + open dropdown
    MOV [DI + MBAR_SEL], BL
    MOV BYTE [DI + MBAR_OPEN], 1
    MOV BYTE [DI + MBAR_DDSEL], 0
    MOV BYTE [fw_state + FW_DIRTY], 1
    CLC
    JMP .mhk_ret
.mhk_inactive_f10:

    ; F10: activate menu bar
    MOV BYTE [DI + MBAR_SEL], 0
    MOV BYTE [DI + MBAR_OPEN], 0
    MOV BYTE [DI + MBAR_DDSEL], 0FFh
    MOV BYTE [fw_state + FW_DIRTY], 1
    CLC
    JMP .mhk_ret

; ---- ACTIVE: bar highlighted or dropdown open ----
.mhk_active:
    CMP BYTE [DI + MBAR_OPEN], 1
    JNZ .mhk_bar_only
    JMP .mhk_dd_open

; ---- BAR ONLY (no dropdown) ----
.mhk_bar_only:
    CMP AH, KTYPE_EXTENDED
    JZ .mhk_bar_ext

    ; Normal keys while bar active
    CMP AL, KEY_ENTER
    JZ .mhk_bar_open_dd
    CMP AL, KEY_ESCAPE
    JZ .mhk_deactivate
    ; All other normal keys: consumed
    CLC
    JMP .mhk_ret

.mhk_bar_ext:
    CMP AL, KEY_LEFT
    JZ .mhk_bar_left
    CMP AL, KEY_RIGHT
    JZ .mhk_bar_right
    CMP AL, KEY_DOWN
    JZ .mhk_bar_open_dd
    CMP AL, KEY_F10
    JZ .mhk_deactivate
    CMP AL, KEY_F6
    JNZ .mhk_not_f6
    JMP .mhk_not_handled
.mhk_not_f6:
    ; Try Alt+key match
    CALL _mhk_check_alt_key
    JC .mhk_bar_other
    ; Match: switch to that item + open dropdown
    MOV [DI + MBAR_SEL], BL
    MOV BYTE [DI + MBAR_OPEN], 1
    MOV BYTE [DI + MBAR_DDSEL], 0
    MOV BYTE [fw_state + FW_DIRTY], 1
    CLC
    JMP .mhk_ret
.mhk_bar_other:
    ; Other extended keys: consumed
    CLC
    JMP .mhk_ret

.mhk_bar_left:
    MOV AL, [DI + MBAR_SEL]
    OR AL, AL
    JNZ .mhk_bar_left_dec
    ; Wrap to last item
    MOV AL, [DI + MBAR_COUNT]
    DEC AL
    MOV [DI + MBAR_SEL], AL
    JMP .mhk_bar_moved
.mhk_bar_left_dec:
    DEC BYTE [DI + MBAR_SEL]
.mhk_bar_moved:
    MOV BYTE [fw_state + FW_DIRTY], 1
    CLC
    JMP .mhk_ret

.mhk_bar_right:
    MOV AL, [DI + MBAR_SEL]
    INC AL
    CMP AL, [DI + MBAR_COUNT]
    JB .mhk_bar_right_set
    XOR AL, AL                  ; wrap to 0
.mhk_bar_right_set:
    MOV [DI + MBAR_SEL], AL
    JMP .mhk_bar_moved

.mhk_bar_open_dd:
    MOV BYTE [DI + MBAR_OPEN], 1
    MOV BYTE [DI + MBAR_DDSEL], 0
    MOV BYTE [fw_state + FW_DIRTY], 1
    CLC
    JMP .mhk_ret

.mhk_deactivate:
    MOV BYTE [DI + MBAR_SEL], 0FFh
    MOV BYTE [DI + MBAR_OPEN], 0
    MOV BYTE [DI + MBAR_DDSEL], 0FFh
    MOV BYTE [fw_state + FW_DIRTY], 1
    CLC
    JMP .mhk_ret

; ---- DROPDOWN OPEN ----
.mhk_dd_open:
    CMP AH, KTYPE_EXTENDED
    JZ .mhk_dd_ext

    ; Normal keys while dropdown open (relay pattern for long jumps)
    CMP AL, KEY_ENTER
    JZ .mhk_dd_go_activate
    CMP AL, KEY_ESCAPE
    JZ .mhk_dd_go_close
    ; Try dropdown hotkey match
    CALL _mhk_check_dd_hotkey
    JC .mhk_dd_normal_consumed
    ; Match: BL = entry index -> select it and activate
    MOV [DI + MBAR_DDSEL], BL
    CALL tui_menu_activate_entry
    CLC
    JMP .mhk_ret
.mhk_dd_normal_consumed:
    ; All other normal keys: consumed
    CLC
    JMP .mhk_ret

.mhk_dd_go_activate:
    JMP .mhk_dd_activate
.mhk_dd_go_close:
    JMP .mhk_dd_close

.mhk_dd_ext:
    CMP AL, KEY_UP
    JZ .mhk_dd_up
    CMP AL, KEY_DOWN
    JZ .mhk_dd_down
    CMP AL, KEY_LEFT
    JZ .mhk_dd_left
    CMP AL, KEY_RIGHT
    JNZ .mhk_dd_not_right
    JMP .mhk_dd_right
.mhk_dd_not_right:
    CMP AL, KEY_F10
    JNZ .mhk_dd_not_f10
    JMP .mhk_deactivate
.mhk_dd_not_f10:
    ; Try Alt+key match
    CALL _mhk_check_alt_key
    JC .mhk_dd_other
    ; Match: switch to that item + reopen dropdown
    MOV [DI + MBAR_SEL], BL
    MOV BYTE [DI + MBAR_DDSEL], 0
    MOV BYTE [fw_state + FW_DIRTY], 1
    CLC
    JMP .mhk_ret
.mhk_dd_other:
    ; Other extended keys: consumed
    CLC
    JMP .mhk_ret

.mhk_dd_up:
    ; Get current item's entry count
    PUSH AX
    MOV AL, [DI + MBAR_SEL]
    CALL _mi_index_to_offset
    MOV BX, [DI + MBAR_ITEMS]
    ADD BX, AX                 ; BX = current menu item
    POP AX

    MOV AL, [DI + MBAR_DDSEL]
    OR AL, AL
    JNZ .mhk_dd_up_dec
    ; Wrap to last entry
    MOV AL, [BX + MI_ECOUNT]
    DEC AL
    MOV [DI + MBAR_DDSEL], AL
    JMP .mhk_dd_moved
.mhk_dd_up_dec:
    DEC BYTE [DI + MBAR_DDSEL]
.mhk_dd_moved:
    MOV BYTE [fw_state + FW_DIRTY], 1
    CLC
    JMP .mhk_ret

.mhk_dd_down:
    ; Get current item's entry count
    PUSH AX
    MOV AL, [DI + MBAR_SEL]
    CALL _mi_index_to_offset
    MOV BX, [DI + MBAR_ITEMS]
    ADD BX, AX
    POP AX

    MOV AL, [DI + MBAR_DDSEL]
    INC AL
    CMP AL, [BX + MI_ECOUNT]
    JB .mhk_dd_down_set
    XOR AL, AL                  ; wrap to 0
.mhk_dd_down_set:
    MOV [DI + MBAR_DDSEL], AL
    JMP .mhk_dd_moved

.mhk_dd_left:
    ; Close dropdown, move to prev item, reopen
    MOV AL, [DI + MBAR_SEL]
    OR AL, AL
    JNZ .mhk_dd_left_dec
    MOV AL, [DI + MBAR_COUNT]
    DEC AL
    MOV [DI + MBAR_SEL], AL
    JMP .mhk_dd_reopen
.mhk_dd_left_dec:
    DEC BYTE [DI + MBAR_SEL]
.mhk_dd_reopen:
    MOV BYTE [DI + MBAR_DDSEL], 0
    MOV BYTE [fw_state + FW_DIRTY], 1
    CLC
    JMP .mhk_ret

.mhk_dd_right:
    ; Close dropdown, move to next item, reopen
    MOV AL, [DI + MBAR_SEL]
    INC AL
    CMP AL, [DI + MBAR_COUNT]
    JB .mhk_dd_right_set
    XOR AL, AL
.mhk_dd_right_set:
    MOV [DI + MBAR_SEL], AL
    JMP .mhk_dd_reopen

.mhk_dd_activate:
    CALL tui_menu_activate_entry
    CLC
    JMP .mhk_ret

.mhk_dd_close:
    ; Close dropdown only, stay on bar
    MOV BYTE [DI + MBAR_OPEN], 0
    MOV BYTE [DI + MBAR_DDSEL], 0FFh
    MOV BYTE [fw_state + FW_DIRTY], 1
    CLC
    JMP .mhk_ret

.mhk_not_handled:
    STC

.mhk_ret:
    POP DI
    POP SI
    POP DX
    POP CX
    POP BX
    RET
ENDP

; ----------------------------------------------------------------------------
; _mhk_check_alt_key - Match scan code against menu item Alt keys
; Input:  AL = scan code, DI = menu bar ptr
; Output: CF=0 + BL=item index on match, CF=1 if no match
; Clobbers: BX, AH
; ----------------------------------------------------------------------------
_mhk_check_alt_key: PROC
    PUSH CX
    PUSH SI
    PUSH DX

    MOV SI, [DI + MBAR_ITEMS]
    XOR BX, BX                 ; BL = index
.cak_loop:
    CMP BL, [DI + MBAR_COUNT]
    JAE .cak_no_match

    ; Get item's MI_ALTKEY
    PUSH AX
    MOV AL, BL
    CALL _mi_index_to_offset    ; AX = offset
    PUSH BX                     ; save index
    MOV BX, SI
    ADD BX, AX                  ; BX = item ptr
    MOV DL, [BX + MI_ALTKEY]   ; DL = alt scan code
    POP BX                      ; restore index
    POP AX                      ; restore AL = input scan code

    ; Compare
    OR DL, DL
    JZ .cak_next                ; 0 = no alt key
    CMP AL, DL
    JZ .cak_found

.cak_next:
    INC BL
    JMP .cak_loop

.cak_found:
    ; BL = matching index
    CLC
    POP DX
    POP SI
    POP CX
    RET

.cak_no_match:
    STC
    POP DX
    POP SI
    POP CX
    RET
ENDP

; ----------------------------------------------------------------------------
; _mhk_check_dd_hotkey - Match ASCII char against dropdown entry hotkeys
; Input:  AL = ASCII char (case-insensitive), DI = menu bar ptr
; Output: CF=0 + BL=entry index on match, CF=1 if no match
; Clobbers: BX, AH
; ----------------------------------------------------------------------------
_mhk_check_dd_hotkey: PROC
    PUSH CX
    PUSH SI
    PUSH DX

    ; Convert AL to uppercase for case-insensitive match
    CMP AL, 'a'
    JB .cdk_no_upper
    CMP AL, 'z'
    JA .cdk_no_upper
    SUB AL, 20h
.cdk_no_upper:

    ; Get current menu item's entries
    PUSH AX
    MOV AL, [DI + MBAR_SEL]
    CALL _mi_index_to_offset
    MOV SI, [DI + MBAR_ITEMS]
    ADD SI, AX                 ; SI = current menu item
    MOV CL, [SI + MI_ECOUNT]
    MOV SI, [SI + MI_ENTRIES]  ; SI = entries array base
    POP AX

    ; Loop entries
    XOR BX, BX
.cdk_loop:
    CMP BL, CL
    JAE .cdk_no_match

    PUSH AX
    MOV AL, BL
    CALL _mde_index_to_offset
    PUSH BX
    MOV BX, SI
    ADD BX, AX
    MOV DL, [BX + MDE_HOTKEY]
    POP BX
    POP AX

    OR DL, DL
    JZ .cdk_next

    ; Uppercase DL too
    CMP DL, 'a'
    JB .cdk_cmp
    CMP DL, 'z'
    JA .cdk_cmp
    SUB DL, 20h
.cdk_cmp:
    CMP AL, DL
    JZ .cdk_found

.cdk_next:
    INC BL
    JMP .cdk_loop

.cdk_found:
    CLC
    POP DX
    POP SI
    POP CX
    RET

.cdk_no_match:
    STC
    POP DX
    POP SI
    POP CX
    RET
ENDP

; ----------------------------------------------------------------------------
; _mhk_check_accel - Match scan code against all MDE_ACCELKEY fields
; Input:  AL = scan code (extended key), DI = menu bar ptr
; Output: CF=0 if matched (handler called via trampoline), CF=1 if no match
; Clobbers: BX, AH, CX, DX, SI
; ----------------------------------------------------------------------------
_mhk_check_accel: PROC
    PUSH CX
    PUSH SI
    PUSH DX

    ; Save scan code to memory (AL clobbered by MUL in index_to_offset)
    MOV [.ca_scancode], AL

    MOV SI, [DI + MBAR_ITEMS]  ; SI = menu items array base
    XOR DL, DL                 ; DL = menu item index

.ca_mi_loop:
    CMP DL, [DI + MBAR_COUNT]
    JAE .ca_no_match

    ; Get menu item pointer: SI + DL * MI_SIZE
    PUSH AX
    MOV AL, DL
    CALL _mi_index_to_offset   ; AX = item offset
    MOV BX, SI
    ADD BX, AX                 ; BX = menu item ptr
    MOV CL, [BX + MI_ECOUNT]  ; CL = entry count
    MOV WORD [.ca_entries_tmp], BX  ; save MI ptr (need MI_ENTRIES from it)
    POP AX

    ; Loop entries for this menu item
    XOR DH, DH                 ; DH = entry index
.ca_mde_loop:
    CMP DH, CL
    JAE .ca_next_mi

    ; Get entry pointer
    PUSH AX
    MOV AL, DH
    CALL _mde_index_to_offset  ; AX = entry offset
    MOV BX, [.ca_entries_tmp]
    MOV BX, [BX + MI_ENTRIES] ; BX = entries array base
    ADD BX, AX                 ; BX = entry ptr
    POP AX

    ; Check MDE_ACCELKEY against saved scan code
    MOV AL, [.ca_scancode]
    CMP [BX + MDE_ACCELKEY], AL
    JZ .ca_found

    INC DH
    JMP .ca_mde_loop

.ca_next_mi:
    INC DL
    JMP .ca_mi_loop

.ca_found:
    ; BX = matching entry ptr, get handler
    MOV AX, [BX + MDE_HANDLER]
    OR AX, AX
    JZ .ca_no_match            ; no handler → treat as no match

    ; Call handler via RET-trampoline
    MOV WORD [ctrl_dispatch_addr], AX
    POP DX
    POP SI
    POP CX
    ; Push return address, then handler address, then RET
    MOV AX, .ca_after_call
    PUSH AX
    MOV AX, [ctrl_dispatch_addr]
    PUSH AX
    RET                         ; jumps to handler

.ca_after_call:
    CLC
    RET

.ca_no_match:
    STC
    POP DX
    POP SI
    POP CX
    RET

.ca_scancode:    DB 0
.ca_entries_tmp: DW 0
ENDP
; Input: DI = menu bar ptr (reads MBAR_SEL, MBAR_DDSEL)
; ----------------------------------------------------------------------------
tui_menu_activate_entry: PROC
    PUSH AX
    PUSH BX
    PUSH SI

    ; Get selected menu item: items + sel * MI_SIZE
    MOV AL, [DI + MBAR_SEL]
    CALL _mi_index_to_offset
    MOV BX, [DI + MBAR_ITEMS]
    ADD BX, AX                 ; BX = menu item

    ; Get entry: entries + ddsel * MDE_SIZE
    MOV AL, [DI + MBAR_DDSEL]
    CALL _mde_index_to_offset
    MOV SI, [BX + MI_ENTRIES]
    ADD SI, AX                 ; SI = dropdown entry

    ; Close menu
    MOV BYTE [DI + MBAR_SEL], 0FFh
    MOV BYTE [DI + MBAR_OPEN], 0
    MOV BYTE [DI + MBAR_DDSEL], 0FFh
    MOV BYTE [fw_state + FW_DIRTY], 1

    ; Get handler
    MOV AX, [SI + MDE_HANDLER]
    OR AX, AX
    JZ .no_handler

    ; Call via RET-trampoline
    MOV WORD [ctrl_dispatch_addr], AX
    MOV AX, .after_call
    PUSH AX                     ; return address
    MOV AX, [ctrl_dispatch_addr]
    PUSH AX                     ; handler address
    RET                         ; jumps to handler
.after_call:

.no_handler:
    POP SI
    POP BX
    POP AX
    RET
ENDP
