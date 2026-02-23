; ============================================================================
; tui_window.inc - Window Management
; ============================================================================

; ----------------------------------------------------------------------------
; tui_win_create - Create a new window in the next available slot
; Input:  SI = pointer to a temporary 20-byte window struct (filled by caller)
; Output: AL = window slot index (0-15), or FFh if table full
; ----------------------------------------------------------------------------
tui_win_create: PROC
    PUSH BX
    PUSH CX
    PUSH DX
    PUSH SI
    PUSH DI

    ; Find free slot (FLAGS byte == 0 means free)
    MOV DI, win_table
    XOR CX, CX
.find_slot:
    CMP CX, MAX_WINDOWS
    JZ .full
    TEST BYTE [DI + WIN_FLAGS], 0FFh
    JZ .found
    ADD DI, WIN_SIZE
    INC CX
    JMP .find_slot

.full:
    MOV AL, 0FFh
    JMP .ret

.found:
    ; CX = slot index, DI = pointer into win_table
    MOV DX, CX             ; save slot index in DX
    PUSH DI                 ; save dest pointer

    ; Copy WIN_SIZE bytes from [SI] to [DI]
    MOV CX, WIN_SIZE
.copy:
    MOV AL, [SI]
    MOV [DI], AL
    INC SI
    INC DI
    LOOP .copy

    POP DI                  ; DI = start of slot
    MOV CX, DX             ; CX = slot index

    ; Set the window ID to its slot index
    MOV [DI + WIN_ID], CL

    ; Add to z_order: place at top
    MOV BL, [fw_state + FW_NUMWIN]
    XOR BH, BH
    MOV [BX + z_order], CL

    ; Update state
    MOV [fw_state + FW_TOPWIN], CL
    INC BYTE [fw_state + FW_NUMWIN]
    MOV BYTE [fw_state + FW_DIRTY], 1

    MOV AL, CL             ; return slot index in AL

.ret:
    POP DI
    POP SI
    POP DX
    POP CX
    POP BX
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_win_draw - Draw a single window into shadow_buf
; Input:  SI = pointer to window struct in win_table
; ----------------------------------------------------------------------------
tui_win_draw: PROC
    PushAll

    ; Check if visible - use inverted jump for range safety
    TEST BYTE [SI + WIN_FLAGS], WINF_VISIBLE
    JNZ .visible
    JMP .done
.visible:

    ; Load window geometry
    MOV DL, [SI + WIN_X]
    MOV DH, [SI + WIN_Y]
    MOV BL, [SI + WIN_W]
    MOV BH, [SI + WIN_H]

    ; ---- SHADOW (if enabled) ----
    TEST BYTE [SI + WIN_FLAGS], WINF_SHADOW
    JZ .no_shadow

    PUSH DX                 ; save original x,y

    ; Right shadow column: col=x+w, rows y+1..y+h
    MOV DL, [SI + WIN_X]
    ADD DL, BL              ; col = x + w
    MOV DH, [SI + WIN_Y]
    INC DH                  ; row = y + 1
    MOV CL, BH              ; h cells
.shadow_r:
    CALL tui_darken_cell
    INC DH
    DEC CL
    JNZ .shadow_r

    ; Bottom shadow row: row=y+h, cols x+1..x+w
    MOV DL, [SI + WIN_X]
    INC DL                  ; col = x + 1
    MOV DH, [SI + WIN_Y]
    ADD DH, BH              ; row = y + h
    MOV CL, BL              ; w cells
    CALL tui_darken_hline

    POP DX                  ; restore original x,y

.no_shadow:
    ; ---- INTERIOR FILL ----
    TEST BYTE [SI + WIN_FLAGS], WINF_BORDER
    JNZ .has_border
    JMP .no_border
.has_border:

    ; Bordered: fill interior at (x+1, y+1) size (w-2, h-2)
    PUSH DX
    PUSH BX
    INC DL
    INC DH
    SUB BL, 2
    SUB BH, 2
    MOV AL, 20h
    MOV AH, [SI + WIN_ATTR]
    CALL tui_fill_rect
    POP BX
    POP DX

    ; ---- SELECT BORDER STYLE (active vs inactive) ----
    MOV AL, [SI + WIN_ID]
    CMP AL, [fw_state + FW_TOPWIN]
    JNZ .inactive_bdr

    ; Active window: double-line border, active color
    MOV BYTE [bdr_scratch + BDR_TL], DBOX_TL
    MOV BYTE [bdr_scratch + BDR_TR], DBOX_TR
    MOV BYTE [bdr_scratch + BDR_BL], DBOX_BL
    MOV BYTE [bdr_scratch + BDR_BR], DBOX_BR
    MOV BYTE [bdr_scratch + BDR_H], DBOX_H
    MOV BYTE [bdr_scratch + BDR_V], DBOX_V
    MOV BYTE [bdr_scratch + BDR_ATTR], CLR_WIN_BDR_ACT
    JMP .draw_border

.inactive_bdr:
    ; Inactive window: single-line border, normal color
    MOV BYTE [bdr_scratch + BDR_TL], BOX_TL
    MOV BYTE [bdr_scratch + BDR_TR], BOX_TR
    MOV BYTE [bdr_scratch + BDR_BL], BOX_BL
    MOV BYTE [bdr_scratch + BDR_BR], BOX_BR
    MOV BYTE [bdr_scratch + BDR_H], BOX_H
    MOV BYTE [bdr_scratch + BDR_V], BOX_V
    MOV AL, [SI + WIN_BATTR]
    MOV [bdr_scratch + BDR_ATTR], AL

.draw_border:
    ; ---- BORDER ----
    MOV AH, [bdr_scratch + BDR_ATTR]

    ; Top-left corner
    MOV AL, [bdr_scratch + BDR_TL]
    CALL tui_putchar

    ; Top horizontal line
    PUSH DX
    INC DL
    MOV AL, [bdr_scratch + BDR_H]
    MOV CL, BL
    SUB CL, 2
    CALL tui_hline
    POP DX

    ; Top-right corner
    PUSH DX
    ADD DL, BL
    DEC DL
    MOV AL, [bdr_scratch + BDR_TR]
    CALL tui_putchar
    POP DX

    ; Left and right vertical bars
    PUSH DX
    INC DH
    MOV CL, BH
    SUB CL, 2
.vert:
    MOV AL, [bdr_scratch + BDR_V]
    MOV AH, [bdr_scratch + BDR_ATTR]
    CALL tui_putchar
    PUSH DX
    ADD DL, BL
    DEC DL
    CALL tui_putchar
    POP DX
    INC DH
    DEC CL
    JNZ .vert
    POP DX

    ; Bottom-left corner
    PUSH DX
    ADD DH, BH
    DEC DH
    MOV AL, [bdr_scratch + BDR_BL]
    MOV AH, [bdr_scratch + BDR_ATTR]
    CALL tui_putchar
    POP DX

    ; Bottom horizontal line
    PUSH DX
    ADD DH, BH
    DEC DH
    INC DL
    MOV AL, [bdr_scratch + BDR_H]
    MOV AH, [bdr_scratch + BDR_ATTR]
    MOV CL, BL
    SUB CL, 2
    CALL tui_hline
    POP DX

    ; Bottom-right corner
    PUSH DX
    ADD DH, BH
    DEC DH
    ADD DL, BL
    DEC DL
    MOV AL, [bdr_scratch + BDR_BR]
    ; If resizable, use resize indicator char
    TEST BYTE [SI + WIN_FLAGS], WINF_RESIZABLE
    JZ .br_normal
    MOV AL, 12h                 ; CP437 up-down arrow (resize indicator)
.br_normal:
    MOV AH, [bdr_scratch + BDR_ATTR]
    CALL tui_putchar
    POP DX

    JMP .draw_title

.no_border:
    ; No border: fill entire area
    MOV AL, 20h
    MOV AH, [SI + WIN_ATTR]
    CALL tui_fill_rect

.draw_title:
    ; ---- TITLE (if enabled) ----
    TEST BYTE [SI + WIN_FLAGS], WINF_TITLE
    JZ .draw_controls

    MOV BX, [SI + WIN_TITLE]
    OR BX, BX
    JZ .draw_controls

    ; Measure title length
    MOV DI, BX
    XOR CX, CX
.tlen:
    CMP BYTE [DI], 0
    JZ .tmeasured
    INC DI
    INC CX
    JMP .tlen
.tmeasured:
    ; Center title: col = x + (w - len) / 2
    MOV AL, [SI + WIN_W]
    SUB AL, CL
    SHR AL, 1
    ADD AL, [SI + WIN_X]
    MOV DL, AL
    MOV DH, [SI + WIN_Y]
    MOV AH, [SI + WIN_TATTR]

    ; Load title string pointer into SI for tui_putstr
    PUSH SI                 ; save window struct pointer
    MOV SI, BX
    CALL tui_putstr
    POP SI                  ; restore window struct pointer

.draw_controls:
    ; ---- CONTROLS (if any) ----
    CALL tui_ctrl_draw_all

.done:
    PopAll
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_compose - Redraw all windows in z-order into shadow_buf
; Clears shadow to desktop, then draws each visible window bottom to top.
; ----------------------------------------------------------------------------
tui_compose: PROC
    PushAll

    ; Clear to desktop background
    CALL tui_clear_shadow

    ; Draw windows in z-order (index 0 = bottom)
    MOV CL, [fw_state + FW_NUMWIN]
    OR CL, CL
    JZ .done
    XOR CH, CH
    XOR BX, BX
.draw_loop:
    CMP BL, CL
    JZ .done

    PUSH BX
    PUSH CX

    ; Get window slot from z_order[BX]
    MOV AL, [BX + z_order]
    XOR AH, AH

    ; Calculate SI = win_table + slot * WIN_SIZE (20)
    MOV BL, WIN_SIZE
    MUL BL                  ; AX = slot * 20
    MOV SI, AX
    ADD SI, win_table

    CALL tui_win_draw

    POP CX
    POP BX
    INC BL
    JMP .draw_loop

.done:
    ; Draw menu bar + dropdown on top of everything
    CALL tui_menu_draw_bar
    CALL tui_menu_draw_dropdown
    CALL tui_dd_draw_popup

    MOV BYTE [fw_state + FW_DIRTY], 0
    PopAll
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_win_close - Close a window by slot index
; Input: AL = window slot index to close
; ----------------------------------------------------------------------------
tui_win_close: PROC
    PushAll

    ; Clear the window's flags (mark slot as free)
    XOR AH, AH
    MOV BL, WIN_SIZE
    MUL BL                  ; AX = slot * 20
    MOV SI, AX
    ADD SI, win_table
    MOV BYTE [SI + WIN_FLAGS], 0

    ; Reload slot index (MUL clobbered AX)
    MOV AL, [SI + WIN_ID]

    ; Remove from z_order: find the slot
    MOV CL, [fw_state + FW_NUMWIN]
    XOR CH, CH
    XOR BX, BX
.find_in_z:
    CMP BX, CX
    JZ .not_found
    CMP [BX + z_order], AL
    JZ .found_z
    INC BX
    JMP .find_in_z

.found_z:
    ; Shift remaining entries down: z_order[i] = z_order[i+1]
    MOV DI, BX              ; DI = position to fill
.shift:
    MOV DX, DI
    INC DX
    CMP DX, CX              ; past end?
    JZ .shift_done
    ; Copy next entry down
    PUSH BX
    MOV BX, DX
    MOV AH, [BX + z_order]
    POP BX
    PUSH BX
    MOV BX, DI
    MOV [BX + z_order], AH
    POP BX
    INC DI
    JMP .shift

.shift_done:
    DEC BYTE [fw_state + FW_NUMWIN]
    MOV BYTE [fw_state + FW_DIRTY], 1

    ; Update topwin to new top (if any windows remain)
    MOV CL, [fw_state + FW_NUMWIN]
    OR CL, CL
    JZ .no_top
    MOV BL, CL
    DEC BL
    XOR BH, BH
    MOV AL, [BX + z_order]
    MOV [fw_state + FW_TOPWIN], AL
    JMP .not_found

.no_top:
    MOV BYTE [fw_state + FW_TOPWIN], 0FFh

.not_found:
    PopAll
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_zorder_cycle - Rotate top window to bottom (Tab key)
; The current topmost window moves to z_order[0], all others shift up.
; ----------------------------------------------------------------------------
tui_zorder_cycle: PROC
    PushAll

    ; Close any open dropdown popup
    MOV WORD [fw_state + FW_OPENDD], 0

    MOV CL, [fw_state + FW_NUMWIN]
    CMP CL, 2
    JB .done                    ; need at least 2 windows

    ; Save current top: z_order[numwin-1]
    MOV BL, CL
    DEC BL
    XOR BH, BH
    MOV AL, [BX + z_order]     ; AL = old top window ID
    MOV DL, AL                 ; save in DL

    ; Shift all entries right: z_order[i] = z_order[i-1], from top down to 1
    ; BX = numwin-1 (already set), use SI for source index
.shift:
    CMP BL, 0
    JZ .shift_done
    MOV SI, BX
    DEC SI                      ; SI = i-1
    MOV AL, [SI + z_order]
    MOV [BX + z_order], AL
    DEC BX
    JMP .shift

.shift_done:
    ; Place old top at z_order[0] (bottom)
    MOV [z_order], DL

    ; Update FW_TOPWIN to new top: z_order[numwin-1]
    MOV BL, CL
    DEC BL
    XOR BH, BH
    MOV AL, [BX + z_order]
    MOV [fw_state + FW_TOPWIN], AL
    MOV BYTE [fw_state + FW_DIRTY], 1

.done:
    PopAll
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_win_move - Move active window by arrow key
; Input: AL = scan code (KEY_UP/DOWN/LEFT/RIGHT)
; ----------------------------------------------------------------------------
tui_win_move: PROC
    PushAll

    ; Close any open dropdown popup
    MOV WORD [fw_state + FW_OPENDD], 0

    ; Save scan code in DL before MUL clobbers AX
    MOV DL, AL

    ; Look up active window struct: SI = win_table + topwin * WIN_SIZE
    MOV AL, [fw_state + FW_TOPWIN]
    MOV BL, WIN_SIZE
    MUL BL                      ; AX = topwin * 20
    MOV SI, AX
    ADD SI, win_table

    ; Check MOVABLE flag
    TEST BYTE [SI + WIN_FLAGS], WINF_MOVABLE
    JZ .done

    ; Dispatch on scan code in DL
    CMP DL, KEY_UP
    JZ .move_up
    CMP DL, KEY_DOWN
    JZ .move_down
    CMP DL, KEY_LEFT
    JZ .move_left
    CMP DL, KEY_RIGHT
    JZ .move_right
    JMP .done

.move_up:
    ; If menu bar exists, minimum y = 1
    CMP WORD [fw_state + FW_MENUBAR], 0
    JZ .move_up_no_menu
    CMP BYTE [SI + WIN_Y], 1
    JBE .done
    JMP .move_up_do
.move_up_no_menu:
    CMP BYTE [SI + WIN_Y], 0
    JZ .done
.move_up_do:
    DEC BYTE [SI + WIN_Y]
    JMP .moved

.move_down:
    ; Allow window to go partially off bottom; keep top row visible
    CMP BYTE [SI + WIN_Y], SCR_H - 1
    JAE .done
    INC BYTE [SI + WIN_Y]
    JMP .moved

.move_left:
    CMP BYTE [SI + WIN_X], 0
    JZ .done
    DEC BYTE [SI + WIN_X]
    JMP .moved

.move_right:
    ; Shadow clips at right edge; window body stops at edge
    MOV AL, [SI + WIN_X]
    ADD AL, [SI + WIN_W]
    CMP AL, SCR_W
    JAE .done
    INC BYTE [SI + WIN_X]
    JMP .moved

.moved:
    MOV BYTE [fw_state + FW_DIRTY], 1

.done:
    PopAll
    RET
ENDP
