; ============================================================================
; editor_undo.inc - Undo/Redo for DOS Notepad
;
; Linear undo stack with variable-length records. Each edit is recorded as:
;   [REC_SIZE][CHANGE_OFS][CURSOR_POS][DATA_LEN][INS_LEN][DATA...][REC_SIZE2]
;
; Undo pops from undo stack, applies reverse, pushes to redo stack.
; Redo pops from redo stack, applies reverse, pushes to undo stack.
; Any new edit clears the redo stack (unless undo_from_redo is set).
;
; Requires: editor_const.inc, editor_gap.inc included first.
; ============================================================================

; --- Scratch for _undo_apply ---
undo_apply_change:   DW 0
undo_apply_cursor:   DW 0
undo_apply_datalen:  DW 0
undo_apply_inslen:   DW 0
undo_apply_dataptr:  DW 0
undo_apply_dir:      DB 0
undo_del_tmp:        DB 0

; ----------------------------------------------------------------------------
; undo_init - Reset undo and redo stacks
; ----------------------------------------------------------------------------
undo_init: PROC
    MOV WORD [undo_head], 0
    MOV WORD [undo_tail], 0
    MOV WORD [undo_count], 0
    MOV WORD [redo_head], 0
    MOV WORD [redo_count], 0
    MOV BYTE [undo_from_redo], 0
    RET
ENDP

; ----------------------------------------------------------------------------
; _undo_clear_redo - Zero redo stack (called on new edits)
; ----------------------------------------------------------------------------
_undo_clear_redo: PROC
    CMP BYTE [undo_from_redo], 0
    JNE .skip
    MOV WORD [redo_head], 0
    MOV WORD [redo_count], 0
.skip:
    RET
ENDP

; ----------------------------------------------------------------------------
; undo_save_cursor - Save current cursor offset to scratch (before edit)
; Input: SI = editor state
; ----------------------------------------------------------------------------
undo_save_cursor: PROC
    PUSH AX
    CALL gap_cursor_offset
    MOV [undo_scratch_cursor], AX
    POP AX
    RET
ENDP

; ----------------------------------------------------------------------------
; _undo_ensure_space - Ensure CX bytes available in undo buffer
; Discards oldest records and compacts if needed.
; Input: CX = bytes needed
; Clobbers: AX, BX
; ----------------------------------------------------------------------------
_undo_ensure_space: PROC
    PUSH DX

    MOV AX, UNDO_BUFSIZE
    SUB AX, [undo_head]
    CMP AX, CX
    JAE .done

.discard_loop:
    CMP WORD [undo_count], 0
    JE .compact
    ; Read REC_SIZE at tail
    MOV BX, [undo_tail]
    ADD BX, undo_buf               ; BX = pointer to record
    MOV AX, [BX]                   ; AX = REC_SIZE
    ADD [undo_tail], AX
    DEC WORD [undo_count]
    ; Check total available
    MOV AX, UNDO_BUFSIZE
    SUB AX, [undo_head]
    ADD AX, [undo_tail]
    CMP AX, CX
    JAE .compact
    JMP .discard_loop

.compact:
    MOV BX, [undo_tail]
    TEST BX, BX
    JZ .done

    MOV DX, [undo_head]
    SUB DX, BX                     ; DX = bytes to copy
    TEST DX, DX
    JZ .compact_zero

    PUSH SI
    PUSH DI
    PUSH CX
    MOV SI, undo_buf
    ADD SI, BX
    MOV DI, undo_buf
    MOV CX, DX
    CLD
    PUSH ES
    PUSH DS
    POP ES
    REP MOVSB
    POP ES
    POP CX
    POP DI
    POP SI

    MOV [undo_head], DX
    JMP .compact_done

.compact_zero:
    MOV WORD [undo_head], 0

.compact_done:
    MOV WORD [undo_tail], 0

.done:
    POP DX
    RET
ENDP

; ----------------------------------------------------------------------------
; _undo_write_buf - Write header, data, trailer to a buffer
; Input: BX = pointer to write position in buffer
;        DI = pointer to data bytes (DATA_LEN bytes, may be 0)
; Sets undo_scratch fields must be set before call.
; Output: AX = total record size written
; Clobbers: AX, CX
; ----------------------------------------------------------------------------
_undo_write_buf: PROC
    PUSH DX
    PUSH SI
    PUSH DI

    ; Total size
    MOV AX, [undo_scratch_datalen]
    ADD AX, UREC_HDR

    ; Write header
    MOV [BX + UREC_SIZE], AX
    MOV DX, [undo_scratch_change]
    MOV [BX + UREC_CHANGE], DX
    MOV DX, [undo_scratch_cursor]
    MOV [BX + UREC_CURSOR], DX
    MOV DX, [undo_scratch_datalen]
    MOV [BX + UREC_DATALEN], DX
    MOV DX, [undo_scratch_inslen]
    MOV [BX + UREC_INSLEN], DX

    ; Copy DATA bytes
    MOV CX, [undo_scratch_datalen]
    TEST CX, CX
    JZ .no_data
    MOV SI, DI                      ; SI = source data
    MOV DI, BX
    ADD DI, UREC_DATA               ; DI = dest in buffer
    PUSH ES
    PUSH DS
    POP ES
    CLD
    REP MOVSB
    POP ES
    JMP .trail

.no_data:
    MOV DI, BX
    ADD DI, UREC_DATA

.trail:
    ; Write trailing REC_SIZE2
    MOV AX, [undo_scratch_datalen]
    ADD AX, UREC_HDR
    MOV [DI], AX

    POP DI
    POP SI
    POP DX
    RET
ENDP

; ----------------------------------------------------------------------------
; _undo_push_record - Write a complete undo record to undo buffer
; Input: undo_scratch_change, _cursor, _datalen, _inslen set
;        DI = pointer to data bytes (DATA_LEN bytes, may be 0)
; Clobbers: AX, BX, CX
; ----------------------------------------------------------------------------
_undo_push_record: PROC
    PUSH DI

    ; Total record size
    MOV CX, [undo_scratch_datalen]
    ADD CX, UREC_HDR

    ; Too big for buffer?
    CMP CX, UNDO_BUFSIZE
    JA .too_big

    CALL _undo_ensure_space

    ; Compute write pointer
    MOV BX, undo_buf
    ADD BX, [undo_head]

    CALL _undo_write_buf            ; AX = total size

    ; Update head and count
    ADD [undo_head], AX
    INC WORD [undo_count]

    CALL _undo_clear_redo

.too_big:
    POP DI
    RET
ENDP

; ----------------------------------------------------------------------------
; _redo_push_record - Write a complete record to redo buffer
; Input: undo_scratch_change, _cursor, _datalen, _inslen set
;        DI = pointer to data bytes
; Clobbers: AX, BX, CX
; ----------------------------------------------------------------------------
_redo_push_record: PROC
    PUSH DI

    MOV CX, [undo_scratch_datalen]
    ADD CX, UREC_HDR

    ; Check if fits
    MOV AX, REDO_BUFSIZE
    SUB AX, [redo_head]
    CMP AX, CX
    JB .overflow

    MOV BX, redo_buf
    ADD BX, [redo_head]

    CALL _undo_write_buf            ; AX = total size

    ADD [redo_head], AX
    INC WORD [redo_count]
    JMP .done_r

.overflow:
    MOV WORD [redo_head], 0
    MOV WORD [redo_count], 0

.done_r:
    POP DI
    RET
ENDP

; ============================================================================
; Public recording functions
; ============================================================================

; ----------------------------------------------------------------------------
; undo_record_insert1 - Record single char insert (no selection)
; Call undo_save_cursor BEFORE gap_insert_char, then this AFTER.
; Input: SI = editor state
; ----------------------------------------------------------------------------
undo_record_insert1: PROC
    PUSH AX
    PUSH BX
    PUSH CX
    PUSH DI

    MOV AX, [undo_scratch_cursor]
    MOV [undo_scratch_change], AX
    MOV WORD [undo_scratch_datalen], 0
    MOV WORD [undo_scratch_inslen], 1
    XOR DI, DI
    CALL _undo_push_record

    POP DI
    POP CX
    POP BX
    POP AX
    RET
ENDP

; ----------------------------------------------------------------------------
; undo_record_insert_n - Record N-char insert (no selection)
; Call undo_save_cursor BEFORE paste loop, then this AFTER.
; Input: SI = editor state, CX = count inserted
; ----------------------------------------------------------------------------
undo_record_insert_n: PROC
    PUSH AX
    PUSH BX
    PUSH DI

    MOV AX, [undo_scratch_cursor]
    MOV [undo_scratch_change], AX
    MOV WORD [undo_scratch_datalen], 0
    MOV [undo_scratch_inslen], CX
    XOR DI, DI
    CALL _undo_push_record

    POP DI
    POP BX
    POP AX
    RET
ENDP

; ----------------------------------------------------------------------------
; undo_record_delete_back - Record single backspace (no selection)
; Call undo_save_cursor BEFORE, then this AFTER gap_delete_back.
; Input: SI = editor state, AL = deleted char
; ----------------------------------------------------------------------------
undo_record_delete_back: PROC
    PUSH BX
    PUSH CX
    PUSH DI

    MOV [undo_del_tmp], AL          ; save deleted char before gap_cursor_offset clobbers AL
    CALL gap_cursor_offset
    MOV [undo_scratch_change], AX
    MOV WORD [undo_scratch_datalen], 1
    MOV WORD [undo_scratch_inslen], 0
    MOV DI, undo_del_tmp
    CALL _undo_push_record

    POP DI
    POP CX
    POP BX
    RET
ENDP

; ----------------------------------------------------------------------------
; undo_record_delete_fwd - Record single forward delete (no selection)
; Call undo_save_cursor BEFORE, then this AFTER gap_delete_fwd.
; Input: SI = editor state, AL = deleted char
; ----------------------------------------------------------------------------
undo_record_delete_fwd: PROC
    PUSH BX
    PUSH CX
    PUSH DI

    MOV [undo_del_tmp], AL          ; save deleted char before gap_cursor_offset clobbers AL
    CALL gap_cursor_offset
    MOV [undo_scratch_change], AX
    MOV WORD [undo_scratch_datalen], 1
    MOV WORD [undo_scratch_inslen], 0
    MOV DI, undo_del_tmp
    CALL _undo_push_record

    POP DI
    POP CX
    POP BX
    RET
ENDP

; ----------------------------------------------------------------------------
; undo_capture_selection - Capture selected text before _ed_sel_delete
; Input: SI = editor state (ED_SELANCHOR != FFFFh)
; Saves cursor, copies selected bytes to undo_sel_tmp, sets scratch fields.
; ----------------------------------------------------------------------------
undo_capture_selection: PROC
    PUSH AX
    PUSH BX
    PUSH CX
    PUSH DI

    CALL gap_cursor_offset
    MOV [undo_scratch_cursor], AX

    MOV BX, [SI + ED_SELANCHOR]
    CMP AX, BX
    JBE .cursor_first
    XCHG AX, BX
.cursor_first:
    ; AX = sel_start, BX = sel_end
    MOV [undo_scratch_change], AX
    MOV CX, BX
    SUB CX, AX
    MOV [undo_scratch_datalen], CX

    MOV BX, AX
    MOV DI, undo_sel_tmp
    TEST CX, CX
    JZ .copy_done
.copy_loop:
    CALL gap_get_char
    MOV [DI], AL
    INC BX
    INC DI
    DEC CX
    JNZ .copy_loop
.copy_done:

    POP DI
    POP CX
    POP BX
    POP AX
    RET
ENDP

; ----------------------------------------------------------------------------
; undo_record_sel_delete - Finalize record after standalone sel_delete
; Input: SI = editor state
; ----------------------------------------------------------------------------
undo_record_sel_delete: PROC
    PUSH BX
    PUSH DI
    MOV WORD [undo_scratch_inslen], 0
    MOV DI, undo_sel_tmp
    CALL _undo_push_record
    POP DI
    POP BX
    RET
ENDP

; ----------------------------------------------------------------------------
; undo_record_compound - Finalize record after sel_delete + insert
; Input: SI = editor state, CX = number of chars inserted
; ----------------------------------------------------------------------------
undo_record_compound: PROC
    PUSH BX
    PUSH DI
    MOV [undo_scratch_inslen], CX
    MOV DI, undo_sel_tmp
    CALL _undo_push_record
    POP DI
    POP BX
    RET
ENDP

; ============================================================================
; Undo/Redo execution
; ============================================================================

; ----------------------------------------------------------------------------
; _undo_read_record - Read record fields from buffer pointer into apply vars
; Input: BX = pointer to record in memory
; Output: undo_apply_* fields set
; Clobbers: AX
; ----------------------------------------------------------------------------
_undo_read_record: PROC
    MOV AX, [BX + UREC_CHANGE]
    MOV [undo_apply_change], AX
    MOV AX, [BX + UREC_CURSOR]
    MOV [undo_apply_cursor], AX
    MOV AX, [BX + UREC_DATALEN]
    MOV [undo_apply_datalen], AX
    MOV AX, [BX + UREC_INSLEN]
    MOV [undo_apply_inslen], AX
    MOV AX, BX
    ADD AX, UREC_DATA
    MOV [undo_apply_dataptr], AX
    RET
ENDP

; ----------------------------------------------------------------------------
; undo_perform_undo - Execute undo (Ctrl+Z)
; Input: SI = editor state
; ----------------------------------------------------------------------------
undo_perform_undo: PROC
    CMP WORD [undo_count], 0
    JE .nothing

    PUSH AX
    PUSH BX
    PUSH CX
    PUSH DI

    ; Pop top record: read trailing REC_SIZE2 at head-2
    MOV BX, [undo_head]
    ADD BX, undo_buf
    SUB BX, 2
    MOV AX, [BX]                   ; AX = REC_SIZE
    ; Record start = undo_buf + head - REC_SIZE
    MOV BX, [undo_head]
    SUB BX, AX                     ; BX = record offset from undo_buf start
    SUB [undo_head], AX
    DEC WORD [undo_count]

    ; BX = offset, convert to pointer
    ADD BX, undo_buf
    CALL _undo_read_record

    MOV BYTE [undo_apply_dir], 0
    CALL _undo_apply

    POP DI
    POP CX
    POP BX
    POP AX
.nothing:
    RET
ENDP

; ----------------------------------------------------------------------------
; undo_perform_redo - Execute redo (Ctrl+Y)
; Input: SI = editor state
; ----------------------------------------------------------------------------
undo_perform_redo: PROC
    CMP WORD [redo_count], 0
    JE .nothing

    PUSH AX
    PUSH BX
    PUSH CX
    PUSH DI

    ; Pop top record from redo stack
    MOV BX, [redo_head]
    ADD BX, redo_buf
    SUB BX, 2
    MOV AX, [BX]                   ; AX = REC_SIZE
    MOV BX, [redo_head]
    SUB BX, AX
    SUB [redo_head], AX
    DEC WORD [redo_count]

    ADD BX, redo_buf
    CALL _undo_read_record

    MOV BYTE [undo_apply_dir], 1
    CALL _undo_apply

    POP DI
    POP CX
    POP BX
    POP AX
.nothing:
    RET
ENDP

; ----------------------------------------------------------------------------
; _undo_apply - Apply a popped record: delete INS_LEN, insert DATA_LEN,
;               push reverse record to opposite stack.
; Input: undo_apply_* fields set, SI = editor state
; Clobbers: AX, BX, CX, DI
; ----------------------------------------------------------------------------
_undo_apply: PROC
    ; Save current cursor for reverse record's CURSOR_POS
    CALL gap_cursor_offset
    MOV [undo_scratch_cursor], AX

    ; Move to change site
    MOV AX, [undo_apply_change]
    CALL gap_goto_offset

    ; --- Delete INS_LEN chars forward, saving to undo_sel_tmp ---
    MOV CX, [undo_apply_inslen]
    MOV DI, undo_sel_tmp
    TEST CX, CX
    JZ .del_done
.del_loop:
    CALL gap_delete_fwd
    JC .del_done
    MOV [DI], AL
    INC DI
    DEC CX
    JNZ .del_loop
.del_done:

    ; --- Insert DATA_LEN chars from record's DATA ---
    MOV CX, [undo_apply_datalen]
    TEST CX, CX
    JZ .ins_done
    MOV BX, [undo_apply_dataptr]
.ins_loop:
    MOV AL, [BX]
    CALL gap_insert_char
    JC .ins_done
    INC BX
    DEC CX
    JNZ .ins_loop
.ins_done:

    ; --- Build and push reverse record ---
    MOV AX, [undo_apply_change]
    MOV [undo_scratch_change], AX
    ; Reverse: DATA_LEN = old INS_LEN (chars we deleted, now in undo_sel_tmp)
    MOV AX, [undo_apply_inslen]
    MOV [undo_scratch_datalen], AX
    ; Reverse: INS_LEN = old DATA_LEN (chars we inserted)
    MOV AX, [undo_apply_datalen]
    MOV [undo_scratch_inslen], AX

    MOV DI, undo_sel_tmp
    CMP BYTE [undo_apply_dir], 0
    JNE .push_undo
    ; Undo direction: push reverse to redo
    CALL _redo_push_record
    JMP .restore
.push_undo:
    ; Redo direction: push reverse to undo (with from_redo flag)
    MOV BYTE [undo_from_redo], 1
    CALL _undo_push_record
    MOV BYTE [undo_from_redo], 0

.restore:
    ; Restore cursor to popped record's CURSOR_POS
    MOV AX, [undo_apply_cursor]
    CALL gap_goto_offset

    OR BYTE [SI + ED_FLAGS], EDF_MODIFIED
    RET
ENDP

; undo_sel_tmp - defined as EQU in BSS block (notepad.asm)
