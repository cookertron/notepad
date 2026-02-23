; ============================================================================
; notepad.asm - DOS Notepad Text Editor
; A CGA text-mode editor built on the TUI framework.
; Phase 6: Text Selection (Shift+arrow keys, Ctrl+A, selection delete).
; ============================================================================

ORG 100h

    JMP main

; ============================================================================
; Framework Includes
; ============================================================================

INCLUDE TUI\tui.inc
INCLUDE editor_const.inc
INCLUDE editor_gap.inc
INCLUDE editor_ctrl.inc
INCLUDE editor_file.inc
INCLUDE editor_clip.inc
INCLUDE editor_find.inc
INCLUDE editor_undo.inc

; ============================================================================
; Menu Handlers
; ============================================================================

handler_file_new:
    CALL editor_file_new
    RET

handler_file_open:
    CALL editor_file_open
    RET

handler_file_save:
    CALL editor_file_save
    RET

handler_file_save_as:
    CALL editor_file_save_as
    RET

handler_file_exit: PROC
    ; If modified, ask for confirmation
    TEST BYTE [ed_state + ED_FLAGS], EDF_MODIFIED
    JZ .exit_now
    MOV SI, str_exit_prompt
    MOV DI, str_exit_dlg_ttl
    CALL tui_dlg_confirm
    CMP AL, DLG_YES
    JNE .exit_cancel
.exit_now:
    MOV BYTE [fw_state + FW_RUNNING], 0
.exit_cancel:
    RET
ENDP

handler_edit_undo:
    MOV SI, ed_state
    CALL undo_perform_undo
    MOV BYTE [fw_state + FW_DIRTY], 1
    RET

handler_edit_redo:
    MOV SI, ed_state
    CALL undo_perform_redo
    MOV BYTE [fw_state + FW_DIRTY], 1
    RET

handler_edit_cut:
    MOV SI, ed_state
    CALL editor_clipboard_cut
    RET

handler_edit_copy:
    MOV SI, ed_state
    CALL editor_clipboard_copy
    RET

handler_edit_paste:
    MOV SI, ed_state
    CALL editor_clipboard_paste
    RET

handler_edit_sel_all:
    CALL editor_select_all
    RET

handler_search_find:
    CALL editor_find
    RET

handler_search_find_next:
    CALL editor_find_next
    RET

handler_search_replace:
    CALL editor_replace
    RET

handler_help_about: PROC
    PushAll

    ; --- Build 4 label controls + OK button ---
    ; Interior width = 29, total width = 31
    ; Lines centered within interior

    ; Label 1: "DOS Notepad v1.0" (16 chars) → x = (29-16)/2 = 6
    MOV DI, about_lbl1
    MOV BYTE [DI + CTRL_TYPE], CTYPE_LABEL
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE
    MOV BYTE [DI + CTRL_X], 6
    MOV BYTE [DI + CTRL_Y], 1
    MOV BYTE [DI + CTRL_W], 16
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], about_lbl2
    MOV WORD [DI + CTRL_TEXT], str_about_l1
    MOV BYTE [DI + CTRL_ATTR], CLR_LABEL
    MOV BYTE [DI + CTRL_FATTR], CLR_LABEL
    MOV WORD [DI + CTRL_HANDLER], 0

    ; Label 2: "Code by Claude Opus" (19 chars) → x = (29-19)/2 = 5
    MOV DI, about_lbl2
    MOV BYTE [DI + CTRL_TYPE], CTYPE_LABEL
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE
    MOV BYTE [DI + CTRL_X], 5
    MOV BYTE [DI + CTRL_Y], 3
    MOV BYTE [DI + CTRL_W], 19
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], about_lbl3
    MOV WORD [DI + CTRL_TEXT], str_about_l2
    MOV BYTE [DI + CTRL_ATTR], CLR_LABEL
    MOV BYTE [DI + CTRL_FATTR], CLR_LABEL
    MOV WORD [DI + CTRL_HANDLER], 0

    ; Label 3: "Compiled using Agent86" (22 chars) → x = (29-22)/2 = 3
    MOV DI, about_lbl3
    MOV BYTE [DI + CTRL_TYPE], CTYPE_LABEL
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE
    MOV BYTE [DI + CTRL_X], 3
    MOV BYTE [DI + CTRL_Y], 4
    MOV BYTE [DI + CTRL_W], 22
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], about_lbl4
    MOV WORD [DI + CTRL_TEXT], str_about_l3
    MOV BYTE [DI + CTRL_ATTR], CLR_LABEL
    MOV BYTE [DI + CTRL_FATTR], CLR_LABEL
    MOV WORD [DI + CTRL_HANDLER], 0

    ; Label 4: "github:cookertron/agent86" (25 chars) → x = (29-25)/2 = 2
    MOV DI, about_lbl4
    MOV BYTE [DI + CTRL_TYPE], CTYPE_LABEL
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE
    MOV BYTE [DI + CTRL_X], 2
    MOV BYTE [DI + CTRL_Y], 5
    MOV BYTE [DI + CTRL_W], 25
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], dlg_btn1
    MOV WORD [DI + CTRL_TEXT], str_about_l4
    MOV BYTE [DI + CTRL_ATTR], CLR_LABEL
    MOV BYTE [DI + CTRL_FATTR], CLR_LABEL
    MOV WORD [DI + CTRL_HANDLER], 0

    ; OK button: x = (29-8)/2 = 10, y = 7
    MOV DI, dlg_btn1
    MOV BYTE [DI + CTRL_TYPE], CTYPE_BUTTON
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    MOV BYTE [DI + CTRL_X], 10
    MOV BYTE [DI + CTRL_Y], 7
    MOV BYTE [DI + CTRL_W], DLG_BTN_OK_W
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], 0
    MOV WORD [DI + CTRL_TEXT], dlg_str_ok
    MOV BYTE [DI + CTRL_ATTR], CLR_BTN
    MOV BYTE [DI + CTRL_FATTR], CLR_BTN_FOCUS
    MOV WORD [DI + CTRL_HANDLER], dlg_handler_ok

    ; Window template: width=31, height=10, centered
    MOV DI, dlg_tmpl
    MOV BYTE [DI + WIN_FLAGS], WINF_VISIBLE + WINF_BORDER + WINF_TITLE + WINF_SHADOW
    MOV BYTE [DI + WIN_X], (80 - 31) / 2
    MOV BYTE [DI + WIN_Y], (25 - 10) / 2
    MOV BYTE [DI + WIN_W], 31
    MOV BYTE [DI + WIN_H], 10
    MOV BYTE [DI + WIN_ATTR], CLR_WIN_BG
    MOV BYTE [DI + WIN_BATTR], CLR_WIN_BDR
    MOV BYTE [DI + WIN_TATTR], CLR_WIN_TTL
    MOV WORD [DI + WIN_TITLE], str_about_title
    MOV WORD [DI + WIN_FIRST], about_lbl1
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
; Screen Save/Restore
; ============================================================================

; ----------------------------------------------------------------------------
; _save_screen - Copy VRAM page 0 to page 1, save cursor position
; Page 0 = B800:0000, Page 1 = B800:1000 (offset 1000h = 4096)
; ----------------------------------------------------------------------------
_save_screen: PROC
    PUSH AX
    PUSH BX
    PUSH CX
    PUSH DX
    PUSH SI
    PUSH DI
    PUSH DS
    PUSH ES

    ; Save cursor position (page 0)
    MOV AH, 03h
    XOR BH, BH
    INT 10h
    MOV [saved_cursor], DX      ; DH=row, DL=col

    ; Copy B800:0000 -> B800:1000 (4000 bytes)
    MOV AX, VRAM_SEG
    MOV DS, AX
    MOV ES, AX                  ; DS=ES=B800h
    XOR SI, SI                  ; SI = page 0 offset
    MOV DI, 1000h               ; DI = page 1 offset
    MOV CX, SCR_BYTES / 2       ; 2000 words
    CLD
    REP MOVSW

    POP ES
    POP DS
    POP DI
    POP SI
    POP DX
    POP CX
    POP BX
    POP AX
    RET
ENDP

; ----------------------------------------------------------------------------
; _restore_screen - Restore VRAM page 0 from page 1, restore cursor
; ----------------------------------------------------------------------------
_restore_screen: PROC
    PUSH AX
    PUSH BX
    PUSH CX
    PUSH DX
    PUSH SI
    PUSH DI
    PUSH DS
    PUSH ES

    ; Copy B800:1000 -> B800:0000 (4000 bytes)
    MOV AX, VRAM_SEG
    MOV DS, AX
    MOV ES, AX                  ; DS=ES=B800h
    MOV SI, 1000h               ; SI = page 1 offset
    XOR DI, DI                  ; DI = page 0 offset
    MOV CX, SCR_BYTES / 2       ; 2000 words
    CLD
    REP MOVSW

    ; Restore DS before accessing our data segment variables
    POP ES
    POP DS

    ; Restore cursor shape (TUI hides it; mode 3 default = scan lines 6-7)
    MOV AH, 01h
    MOV CH, 06h
    MOV CL, 07h
    INT 10h

    ; Restore cursor position (page 0)
    MOV AH, 02h
    XOR BH, BH
    MOV DX, [saved_cursor]      ; DH=row, DL=col — now DS is correct
    INT 10h

    POP DI
    POP SI
    POP DX
    POP CX
    POP BX
    POP AX
    RET
ENDP

; ============================================================================
; String Data
; ============================================================================

str_untitled:   DB 'Untitled', 0
str_about_title: DB 'About', 0
str_about_l1:    DB 'DOS Notepad v1.0', 0
str_about_l2:    DB 'Code by Claude Opus', 0
str_about_l3:    DB 'Compiled using Agent86', 0
str_about_l4:    DB 'github:cookertron/agent86', 0
str_exit_prompt: DB 'Unsaved changes. Exit?', 0
str_exit_dlg_ttl: DB 'Exit', 0

str_file:       DB 'File', 0
str_edit:       DB 'Edit', 0
str_search:     DB 'Search', 0
str_help:       DB 'Help', 0

str_new:        DB 'New', 0
str_open:       DB 'Open', 0
str_save:       DB 'Save', 0
str_save_as:    DB 'Save As', 0
str_exit:       DB 'Exit', 0

str_undo:       DB 'Undo', 0
str_redo:       DB 'Redo', 0
str_cut:        DB 'Cut', 0
str_copy:       DB 'Copy', 0
str_paste:      DB 'Paste', 0
str_sel_all:    DB 'Select All', 0

str_find:       DB 'Find', 0
str_find_next:  DB 'Find Next', 0
str_replace:    DB 'Replace', 0

str_about:      DB 'About', 0

; ============================================================================
; Menu Data Structures
; ============================================================================

file_entries:
    DW str_new,      handler_file_new
    DB 0, 'N'                        ; 'N'ew
    DW str_open,     handler_file_open
    DB 0, 'O'                        ; 'O'pen
    DW str_save,     handler_file_save
    DB 0, 'S'                        ; 'S'ave
    DW str_save_as,  handler_file_save_as
    DB 5, 'A'                        ; Save 'A's
    DW str_exit,     handler_file_exit
    DB 1, 'x'                        ; E'x'it

edit_entries:
    DW str_undo,     handler_edit_undo
    DB 0, 'U'                        ; 'U'ndo
    DW str_redo,     handler_edit_redo
    DB 0, 'R'                        ; 'R'edo
    DW str_cut,      handler_edit_cut
    DB 2, 't'                        ; Cu't'
    DW str_copy,     handler_edit_copy
    DB 0, 'C'                        ; 'C'opy
    DW str_paste,    handler_edit_paste
    DB 0, 'P'                        ; 'P'aste
    DW str_sel_all,  handler_edit_sel_all
    DB 7, 'A'                        ; Select 'A'll

search_entries:
    DW str_find,      handler_search_find
    DB 0, 'F'                        ; 'F'ind
    DW str_find_next, handler_search_find_next
    DB 5, 'N'                        ; Find 'N'ext
    DW str_replace,   handler_search_replace
    DB 0, 'R'                        ; 'R'eplace

help_entries:
    DW str_about,     handler_help_about
    DB 0, 'A'                        ; 'A'bout

menu_items:
    DW str_file
    DB 1, 6
    DW file_entries
    DB 5, 9
    DB 0, 21h               ; MI_HOTIDX=0 ('F'), MI_ALTKEY=21h (Alt+F)

    DW str_edit
    DB 7, 6
    DW edit_entries
    DB 6, 12
    DB 0, 12h               ; MI_HOTIDX=0 ('E'), MI_ALTKEY=12h (Alt+E)

    DW str_search
    DB 13, 8
    DW search_entries
    DB 3, 11
    DB 0, 1Fh               ; MI_HOTIDX=0 ('S'), MI_ALTKEY=1Fh (Alt+S)

    DW str_help
    DB 21, 6
    DW help_entries
    DB 1, 8
    DB 0, 23h               ; MI_HOTIDX=0 ('H'), MI_ALTKEY=23h (Alt+H)

menubar_data:
    DB 4
    DB 0FFh
    DB 0
    DB 0FFh
    DB CLR_MENUBAR
    DB CLR_MENU_SEL
    DW menu_items

; ============================================================================
; Editor Control Struct (14 base + 2 extension = 16 bytes)
; ============================================================================

ctrl_editor:
    DB CTYPE_EDITOR                              ; CTRL_TYPE
    DB CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE  ; CTRL_FLAGS
    DB 0                                          ; CTRL_X
    DB 0                                          ; CTRL_Y
    DB 78                                         ; CTRL_W (80 - 2 border)
    DB 22                                         ; CTRL_H (24 - 2 border)
    DW 0                                          ; CTRL_NEXT (end of list)
    DW 0                                          ; CTRL_TEXT (unused)
    DB CLR_ED_TEXT                                ; CTRL_ATTR
    DB CLR_ED_TEXT                                ; CTRL_FATTR
    DW 0                                          ; CTRL_HANDLER (none)
    DW ed_state                                   ; CTRL_ED_STATE

; ============================================================================
; Window Template
; ============================================================================

temp_editor:
    DB WINF_VISIBLE + WINF_BORDER + WINF_TITLE + WINF_MOVABLE + WINF_SHADOW
    DB 0                        ; WIN_X
    DB 1                        ; WIN_Y (below menu bar)
    DB 80                       ; WIN_W
    DB 24                       ; WIN_H (fills rows 1-24)
    DB CLR_WIN_BG               ; WIN_ATTR
    DB CLR_WIN_BDR              ; WIN_BATTR
    DB CLR_WIN_TTL              ; WIN_TATTR
    DW str_untitled             ; WIN_TITLE
    DW ctrl_editor              ; WIN_FIRST → editor control
    DW ctrl_editor              ; WIN_FOCUS → editor control (focused)
    DW 0                        ; WIN_HANDLER
    DB 0                        ; WIN_ZORDER
    DB 0                        ; WIN_ID
    DB 0                        ; WIN_SCROLLX
    DB 0                        ; WIN_SCROLLY

; ============================================================================
; Parse command-line filename from PSP
; Returns: CF=0 if filename found (copied to ed_filename_buf), CF=1 if none
; ============================================================================
_parse_cmdline: PROC
    ; Check command-line length at PSP:0080h
    MOV SI, 0080h
    LODSB                       ; AL = length byte, SI -> 0081h
    OR AL, AL
    JZ .no_file                 ; length = 0 → no args

    MOV CL, AL
    XOR CH, CH                  ; CX = tail length

    ; Skip leading spaces
.skip_spaces:
    JCXZ .no_file               ; ran out of chars
    LODSB
    DEC CX
    CMP AL, 20h
    JE .skip_spaces

    ; AL has first non-space char, SI points past it
    DEC SI                      ; back up to first non-space
    INC CX

    ; Copy filename to ed_filename_buf until space or CR
    MOV DI, ed_filename_buf
.copy_loop:
    JCXZ .done_copy
    LODSB
    DEC CX
    CMP AL, 0Dh                ; CR = end
    JE .done_copy
    CMP AL, 20h                ; space = end
    JE .done_copy
    MOV [DI], AL
    INC DI
    JMP .copy_loop

.done_copy:
    ; Null-terminate
    MOV BYTE [DI], 0
    ; Check if we actually copied anything
    CMP DI, ed_filename_buf
    JE .no_file
    CLC                         ; CF=0: filename found
    RET

.no_file:
    STC                         ; CF=1: no filename
    RET
ENDP

; ============================================================================
; Entry Point
; ============================================================================

main:
    ; Save screen contents and cursor before TUI takes over
    CALL _save_screen

    ; Initialize TUI framework (overwrites page 0 via shadow buffer blit)
    ; No INT 10h mode set — that would wipe all pages including our backup
    CALL tui_init

    ; Initialize mouse support
    CALL tui_mouse_init

    ; Initialize editor gap buffer
    MOV SI, ed_state
    MOV AX, ed_buffer
    MOV CX, ED_BUFSIZE
    CALL gap_init
    CALL undo_init

    ; Check for command-line filename
    CALL _parse_cmdline
    JC _no_cmdline_file
    MOV DX, ed_filename_buf
    CALL _editor_load_file
    JC _no_cmdline_file
    MOV WORD [ed_state + ED_FILENAME], ed_filename_buf
    MOV WORD [temp_editor + WIN_TITLE], ed_filename_buf
_no_cmdline_file:

    ; Set up menu bar
    MOV WORD [fw_state + FW_MENUBAR], menubar_data

    ; Create editor window
    MOV SI, temp_editor
    CALL tui_win_create
    MOV [ed_win_slot], AL

    ; Enter main event loop
    CALL tui_run

    ; Hide hardware mouse cursor before restoring screen
    MOV AX, 0002h
    INT 33h

    ; Restore screen contents and cursor
    CALL _restore_screen

    ; Clean exit to DOS
    INT 20h

; ============================================================================
; Uninitialized Data (at end of binary)
; ============================================================================

about_lbl1:     RESB CTRL_SIZE      ; About dialog label controls
about_lbl2:     RESB CTRL_SIZE
about_lbl3:     RESB CTRL_SIZE
about_lbl4:     RESB CTRL_SIZE

ed_state:       RESB ED_SIZE        ; 23 bytes - editor state struct
ed_buffer:      RESB ED_BUFSIZE     ; 8192 bytes - text buffer
ed_filename_buf: RESB 64            ; current filename string
ed_filedlg_buf:  RESB 64            ; temp buffer for dialog results
ed_clipboard:    RESB ED_BUFSIZE    ; 8192 bytes - clipboard buffer
ed_cliplen:      RESW 1             ; clipboard content length (0 = empty)
ed_find_buf:     RESB 64            ; search term buffer
ed_replace_buf:  RESB 64            ; replacement text buffer
ed_find_len:     RESW 1             ; cached search term length
saved_cursor:    RESW 1             ; saved cursor position (DH=row, DL=col)

; --- Undo/Redo data ---
undo_buf:             RESB UNDO_BUFSIZE
undo_head:            RESW 1
undo_tail:            RESW 1
undo_count:           RESW 1

redo_buf:             RESB REDO_BUFSIZE
redo_head:            RESW 1
redo_count:           RESW 1

undo_scratch_change:  RESW 1
undo_scratch_cursor:  RESW 1
undo_scratch_datalen: RESW 1
undo_scratch_inslen:  RESW 1
undo_from_redo:       RESB 1
