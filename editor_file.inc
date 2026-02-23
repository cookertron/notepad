; ============================================================================
; editor_file.inc - File I/O for DOS Notepad
;
; Provides New/Open/Save/Save As operations using DOS INT 21h file services
; and TUI framework dialogs.
;
; Requires: editor_const.inc, editor_gap.inc, editor_ctrl.inc included first.
; ============================================================================

; --- Data ---
ed_win_slot:        DB 0            ; window slot index from tui_win_create
ed_file_handle:     DW 0            ; temp file handle during I/O

; --- Dialog string constants ---
str_save_prompt:    DB 'Save changes?', 0
str_save_dlg_ttl:  DB 'Save', 0
str_open_dlg_ttl:  DB 'Open', 0
str_saveas_dlg_ttl: DB 'Save As', 0
str_saveas_prompt:  DB 'Filename:', 0
str_err_open:       DB 'Cannot open file', 0
str_err_save:       DB 'Cannot save file', 0
str_err_title:      DB 'Error', 0

; ----------------------------------------------------------------------------
; _editor_update_title - Update window title from ED_FILENAME or "Untitled"
; Uses: ed_win_slot, ed_state globals
; Clobbers: AX, BX
; ----------------------------------------------------------------------------
_editor_update_title: PROC
    PUSH AX
    PUSH BX
    ; Compute win_table entry: slot * WIN_SIZE + win_table
    MOV AL, [ed_win_slot]
    MOV BL, WIN_SIZE
    MUL BL
    ADD AX, win_table
    MOV BX, AX
    ; Set WIN_TITLE based on ED_FILENAME
    MOV AX, [ed_state + ED_FILENAME]
    TEST AX, AX
    JZ .use_untitled
    MOV WORD [BX + WIN_TITLE], AX
    JMP .done
.use_untitled:
    MOV WORD [BX + WIN_TITLE], str_untitled
.done:
    POP BX
    POP AX
    RET
ENDP

; ----------------------------------------------------------------------------
; _editor_check_save - If buffer modified, ask user to save
; If user answers Yes, calls editor_file_save.
; Clobbers: AX (and everything if dialog shown)
; ----------------------------------------------------------------------------
_editor_check_save: PROC
    TEST BYTE [ed_state + ED_FLAGS], EDF_MODIFIED
    JZ .done
    MOV SI, str_save_prompt
    MOV DI, str_save_dlg_ttl
    CALL tui_dlg_confirm
    CMP AL, DLG_YES
    JNE .done
    CALL editor_file_save
.done:
    RET
ENDP

; ----------------------------------------------------------------------------
; _editor_load_file - Load a file into the gap buffer
; Input:  DX = pointer to filename string
; Output: CF=1 on error (msgbox already shown)
; Clobbers: AX, BX, CX, DX, DI
; ----------------------------------------------------------------------------
_editor_load_file: PROC
    PUSH SI

    ; --- Open file (read-only) ---
    MOV AX, 3D00h
    INT 21h
    JC .open_err
    MOV [ed_file_handle], AX

    ; --- Re-init gap buffer (resets all state) ---
    MOV SI, ed_state
    MOV AX, ed_buffer
    MOV CX, ED_BUFSIZE
    CALL gap_init

    ; --- Read file content into buffer ---
    MOV BX, [ed_file_handle]
    MOV DX, [SI + ED_BUF]
    MOV CX, ED_BUFSIZE
    MOV AH, 3Fh
    INT 21h
    JC .read_err
    ; AX = bytes actually read
    MOV CX, AX

    ; --- Close file ---
    PUSH CX
    MOV BX, [ed_file_handle]
    MOV AH, 3Eh
    INT 21h
    POP CX

    ; --- Compact CRs (0Dh) in-place ---
    ; DI = read pointer, BX = write pointer, CX = bytes to process
    MOV DI, [SI + ED_BUF]
    MOV BX, DI
.compact:
    TEST CX, CX
    JZ .compact_done
    MOV AL, [DI]
    CMP AL, 0Dh
    JE .skip_cr
    MOV [BX], AL
    INC BX
.skip_cr:
    INC DI
    DEC CX
    JMP .compact
.compact_done:
    ; BX = pointer past last compacted byte
    ; Text sits at [ED_BUF .. BX).  We need cursor at start (position 0),
    ; so relocate text to the END of the buffer:
    ;   GAPSTART = BUF (cursor at 0), GAPEND = BUFEND - text_size

    ; --- Count LFs for LINECOUNT ---
    MOV DI, [SI + ED_BUF]
    XOR CX, CX
.count_lf:
    CMP DI, BX
    JAE .count_done
    CMP BYTE [DI], 0Ah
    JNE .not_lf
    INC CX
.not_lf:
    INC DI
    JMP .count_lf
.count_done:
    INC CX
    MOV [SI + ED_LINECOUNT], CX

    ; --- Relocate text to end of buffer ---
    MOV CX, BX
    SUB CX, [SI + ED_BUF]          ; CX = text_size
    TEST CX, CX
    JZ .no_relocate                 ; empty file: gap_init state is correct

    ; GAPSTART stays at BUF (already set by gap_init)
    ; Set GAPEND = BUFEND - text_size
    MOV AX, [SI + ED_BUFEND]
    SUB AX, CX
    MOV [SI + ED_GAPEND], AX

    ; Backward copy: text from BUF..BX → (BUFEND-text_size)..BUFEND
    ; (backward because dest > src and regions may overlap)
    MOV DI, [SI + ED_BUFEND]
    DEC DI                          ; DI = last byte of dest
    PUSH SI                         ; save ed_state
    MOV SI, BX
    DEC SI                          ; SI = last byte of source
    STD
    REP MOVSB
    CLD
    POP SI                          ; restore ed_state

.no_relocate:
    ; --- Cursor at start of file ---
    MOV WORD [SI + ED_CURLINE], 0
    MOV WORD [SI + ED_CURCOL], 0
    MOV WORD [SI + ED_SCROLLY], 0
    MOV WORD [SI + ED_SCROLLX], 0
    AND BYTE [SI + ED_FLAGS], 0FEh

    POP SI
    CLC
    RET

.read_err:
    ; Close file handle before reporting error
    MOV BX, [ed_file_handle]
    MOV AH, 3Eh
    INT 21h
.open_err:
    POP SI
    MOV SI, str_err_open
    MOV DI, str_err_title
    CALL tui_dlg_msgbox
    STC
    RET
ENDP

; ----------------------------------------------------------------------------
; _editor_save_to_file - Write gap buffer content to a file
; Input:  DX = pointer to filename string
; Output: CF=1 on error (msgbox already shown)
; Clobbers: AX, BX, CX, DX
; ----------------------------------------------------------------------------
_editor_save_to_file: PROC
    PUSH SI
    MOV SI, ed_state

    ; --- Create/truncate file ---
    MOV AH, 3Ch
    XOR CX, CX
    INT 21h
    JC .create_err
    MOV [ed_file_handle], AX

    ; --- Write before-gap region: ED_BUF to ED_GAPSTART ---
    MOV BX, AX
    MOV DX, [SI + ED_BUF]
    MOV CX, [SI + ED_GAPSTART]
    SUB CX, [SI + ED_BUF]
    TEST CX, CX
    JZ .write_after
    MOV AH, 40h
    INT 21h
    JC .write_err

.write_after:
    ; --- Write after-gap region: ED_GAPEND to ED_BUFEND ---
    MOV BX, [ed_file_handle]
    MOV DX, [SI + ED_GAPEND]
    MOV CX, [SI + ED_BUFEND]
    SUB CX, [SI + ED_GAPEND]
    TEST CX, CX
    JZ .close
    MOV AH, 40h
    INT 21h
    JC .write_err

.close:
    MOV BX, [ed_file_handle]
    MOV AH, 3Eh
    INT 21h
    ; Clear modified flag
    AND BYTE [SI + ED_FLAGS], 0FEh

    POP SI
    CLC
    RET

.write_err:
    MOV BX, [ed_file_handle]
    MOV AH, 3Eh
    INT 21h
.create_err:
    POP SI
    MOV SI, str_err_save
    MOV DI, str_err_title
    CALL tui_dlg_msgbox
    STC
    RET
ENDP

; ----------------------------------------------------------------------------
; editor_file_new - New file: prompt save, reset buffer, update title
; ----------------------------------------------------------------------------
editor_file_new: PROC
    CALL _editor_check_save
    ; Re-init gap buffer (clears everything including ED_FILENAME)
    MOV SI, ed_state
    MOV AX, ed_buffer
    MOV CX, ED_BUFSIZE
    CALL gap_init
    CALL undo_init
    CALL _editor_update_title
    MOV BYTE [fw_state + FW_DIRTY], 1
    RET
ENDP

; ----------------------------------------------------------------------------
; editor_file_open - Open file: prompt save, file dialog, load file
; ----------------------------------------------------------------------------
editor_file_open: PROC
    CALL _editor_check_save
    ; Show file browser dialog
    MOV DI, str_open_dlg_ttl
    MOV BX, ed_filedlg_buf
    MOV CL, 63
    CALL tui_dlg_file
    CMP AL, DLG_OK
    JNE .cancelled
    ; Load the selected file
    MOV DX, ed_filedlg_buf
    CALL _editor_load_file
    JC .cancelled
    CALL undo_init
    ; Copy filename: ed_filedlg_buf -> ed_filename_buf
    PUSH SI
    MOV SI, ed_filedlg_buf
    MOV DI, ed_filename_buf
.copy:
    MOV AL, [SI]
    MOV [DI], AL
    TEST AL, AL
    JZ .copy_done
    INC SI
    INC DI
    JMP .copy
.copy_done:
    POP SI
    ; Set ED_FILENAME pointer and update title
    MOV WORD [ed_state + ED_FILENAME], ed_filename_buf
    CALL _editor_update_title
    MOV BYTE [fw_state + FW_DIRTY], 1
.cancelled:
    RET
ENDP

; ----------------------------------------------------------------------------
; editor_file_save - Save: if no filename, do Save As; else write file
; ----------------------------------------------------------------------------
editor_file_save: PROC
    CMP WORD [ed_state + ED_FILENAME], 0
    JZ editor_file_save_as
    MOV DX, [ed_state + ED_FILENAME]
    CALL _editor_save_to_file
    JC .done
    MOV BYTE [fw_state + FW_DIRTY], 1
.done:
    RET
ENDP

; ----------------------------------------------------------------------------
; editor_file_save_as - Save As: input dialog for filename, then save
; ----------------------------------------------------------------------------
editor_file_save_as: PROC
    ; Pre-fill dialog buffer with current filename (if any)
    PUSH SI
    MOV AX, [ed_state + ED_FILENAME]
    TEST AX, AX
    JZ .clear_buf
    MOV SI, AX
    MOV DI, ed_filedlg_buf
.prefill:
    MOV AL, [SI]
    MOV [DI], AL
    TEST AL, AL
    JZ .prefill_done
    INC SI
    INC DI
    JMP .prefill
.clear_buf:
    MOV BYTE [ed_filedlg_buf], 0
.prefill_done:
    POP SI
    ; Show input dialog for filename
    MOV SI, str_saveas_prompt
    MOV DI, str_saveas_dlg_ttl
    MOV BX, ed_filedlg_buf
    MOV CL, 63
    CALL tui_dlg_input
    CMP AL, DLG_OK
    JNE .cancelled
    ; Reject empty filename
    CMP BYTE [ed_filedlg_buf], 0
    JE .cancelled
    ; Save the file
    MOV DX, ed_filedlg_buf
    CALL _editor_save_to_file
    JC .cancelled
    ; Copy filename: ed_filedlg_buf -> ed_filename_buf
    PUSH SI
    MOV SI, ed_filedlg_buf
    MOV DI, ed_filename_buf
.copy:
    MOV AL, [SI]
    MOV [DI], AL
    TEST AL, AL
    JZ .copy_done
    INC SI
    INC DI
    JMP .copy
.copy_done:
    POP SI
    MOV WORD [ed_state + ED_FILENAME], ed_filename_buf
    CALL _editor_update_title
    MOV BYTE [fw_state + FW_DIRTY], 1
.cancelled:
    RET
ENDP
