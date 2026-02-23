; ============================================================================
; tui_video.inc - Drawing Primitives (all write to shadow_buf)
; ============================================================================

; ----------------------------------------------------------------------------
; tui_init - Initialize the TUI framework
; Clears shadow buffer, zeroes state, sets FW_RUNNING=1
; ----------------------------------------------------------------------------
tui_init: PROC
    PushAll

    ; Hide text cursor
    MOV AH, 01h
    MOV CH, 20h             ; start line > max = hidden
    INT 10h

    CLD

    ; Zero framework state (16 bytes)
    MOV DI, fw_state
    XOR AL, AL
    MOV CX, 16
    REP STOSB

    ; Zero window table (320 bytes)
    MOV DI, win_table
    XOR AL, AL
    MOV CX, 320
    REP STOSB

    ; Zero z_order (16 bytes)
    MOV DI, z_order
    XOR AL, AL
    MOV CX, 16
    REP STOSB

    ; Initialize modal window to "none"
    MOV BYTE [fw_state + FW_MODAL_WIN], 0FFh

    ; Set running and dirty flags
    MOV BYTE [fw_state + FW_RUNNING], 1
    MOV BYTE [fw_state + FW_DIRTY], 1

    ; Clear shadow buffer to desktop
    CALL tui_clear_shadow

    PopAll
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_clear_shadow - Fill shadow_buf with desktop background
; Space (20h) + CLR_DESKTOP attribute
; ----------------------------------------------------------------------------
tui_clear_shadow: PROC
    PUSH AX
    PUSH CX
    PUSH DI

    MOV DI, shadow_buf
    MOV AX, CLR_DESKTOP * 256 + 20h  ; AH=attr, AL=space
    MOV CX, SCR_CELLS
    CLD
    REP STOSW

    POP DI
    POP CX
    POP AX
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_blit - Copy shadow_buf to VRAM (B800:0000)
; Saves/restores ES. Uses REP MOVSW for speed.
; ----------------------------------------------------------------------------
tui_blit: PROC
    PUSH AX
    PUSH CX
    PUSH SI
    PUSH DI
    PUSH ES

    MOV AX, VRAM_SEG
    MOV ES, AX
    MOV SI, shadow_buf
    XOR DI, DI
    MOV CX, SCR_CELLS
    CLD
    REP MOVSW

    POP ES
    POP DI
    POP SI
    POP CX
    POP AX
    RET
ENDP

; ----------------------------------------------------------------------------
; calc_vram_offset - Calculate absolute address in shadow_buf
; Input:  DH = row, DL = col
; Output: DI = absolute address in shadow_buf
; Clobbers: none (BX saved/restored)
; ----------------------------------------------------------------------------
calc_vram_offset: PROC
    PUSH BX
    MOV BL, DH
    XOR BH, BH
    SHL BX, 1
    MOV DI, [BX + row_offsets]
    MOV BL, DL
    XOR BH, BH
    SHL BX, 1
    ADD DI, BX
    ADD DI, shadow_buf
    POP BX
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_putchar - Write a single char+attr to shadow_buf
; Input:  AL = character, AH = attribute, DH = row, DL = col
; ----------------------------------------------------------------------------
tui_putchar: PROC
    CMP DH, SCR_H
    JAE .skip
    CMP DL, SCR_W
    JAE .skip
    PUSH DI
    PUSH BX
    CALL calc_vram_offset
    MOV [DI], AL
    MOV [DI + 1], AH
    POP BX
    POP DI
.skip:
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_putstr - Write null-terminated string to shadow_buf
; Input:  SI = pointer to string, AH = attribute, DH = row, DL = col
; Note:   SI is advanced past the string on return
; ----------------------------------------------------------------------------
tui_putstr: PROC
    CMP DH, SCR_H
    JAE .skip
    PUSH DI
    PUSH BX
    CALL calc_vram_offset
    MOV BL, DL              ; BL tracks current column
.next_char:
    CMP BL, SCR_W
    JAE .done
    MOV AL, [SI]
    OR AL, AL
    JZ .done
    MOV [DI], AL
    MOV [DI + 1], AH
    INC SI
    ADD DI, 2
    INC BL
    JMP .next_char
.done:
    POP BX
    POP DI
.skip:
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_hline - Draw horizontal line of a single char+attr
; Input:  AL = character, AH = attribute, DH = row, DL = col, CL = width
; ----------------------------------------------------------------------------
tui_hline: PROC
    CMP DH, SCR_H
    JAE .skip
    CMP DL, SCR_W
    JAE .skip
    PUSH DI
    PUSH BX
    PUSH CX
    ; Clip width to right screen edge
    MOV CH, SCR_W
    SUB CH, DL               ; CH = max chars that fit
    CMP CL, CH
    JBE .no_clip
    MOV CL, CH
.no_clip:
    CALL calc_vram_offset
    XOR CH, CH
.fill:
    OR CL, CL
    JZ .done
    MOV [DI], AL
    MOV [DI + 1], AH
    ADD DI, 2
    DEC CL
    JMP .fill
.done:
    POP CX
    POP BX
    POP DI
.skip:
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_fill_rect - Fill rectangle with char+attr
; Input:  AL = char, AH = attr, DH = row, DL = col, BH = height, BL = width
; ----------------------------------------------------------------------------
tui_fill_rect: PROC
    PushAll
    ; Clip: skip if starting row off-screen
    CMP DH, SCR_H
    JAE .done
    ; Clip row count to bottom screen edge
    MOV CH, SCR_H
    SUB CH, DH               ; CH = max rows that fit
    CMP BH, CH
    JBE .rows_ok
    MOV BH, CH
.rows_ok:
    MOV CL, BL               ; CL = width for hline
.row_loop:
    OR BH, BH
    JZ .done
    CALL tui_hline            ; hline clips horizontally
    INC DH
    DEC BH
    JMP .row_loop
.done:
    PopAll
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_darken_cell - Darken a single cell's attribute (keep existing char)
; Input:  DH = row, DL = col
; Effect: Sets attr to CLR_SHADOW (08h), preserves character
; ----------------------------------------------------------------------------
tui_darken_cell: PROC
    CMP DH, SCR_H
    JAE .skip
    CMP DL, SCR_W
    JAE .skip
    PUSH DI
    PUSH BX
    CALL calc_vram_offset
    MOV BYTE [DI + 1], CLR_SHADOW
    POP BX
    POP DI
.skip:
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_darken_hline - Darken a horizontal line of cells (keep existing chars)
; Input:  DH = row, DL = col, CL = width
; Effect: Sets all attrs to CLR_SHADOW, preserves characters
; ----------------------------------------------------------------------------
tui_darken_hline: PROC
    CMP DH, SCR_H
    JAE .skip
    CMP DL, SCR_W
    JAE .skip
    PUSH DI
    PUSH BX
    PUSH CX
    ; Clip width to right screen edge
    MOV CH, SCR_W
    SUB CH, DL
    CMP CL, CH
    JBE .no_clip
    MOV CL, CH
.no_clip:
    CALL calc_vram_offset
    XOR CH, CH
.fill:
    OR CL, CL
    JZ .done
    MOV BYTE [DI + 1], CLR_SHADOW
    ADD DI, 2
    DEC CL
    JMP .fill
.done:
    POP CX
    POP BX
    POP DI
.skip:
    RET
ENDP
