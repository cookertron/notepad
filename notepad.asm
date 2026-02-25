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

    ; Label 1: "DOS Notepad v1.0.0" (18 chars) → x = (29-18)/2 = 5
    MOV DI, about_lbl1
    MOV BYTE [DI + CTRL_TYPE], CTYPE_LABEL
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE
    MOV BYTE [DI + CTRL_X], 5
    MOV BYTE [DI + CTRL_Y], 1
    MOV BYTE [DI + CTRL_W], 18
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

str_mda_error:  DB 'This program requires a CGA, EGA, or VGA display adapter.', 0Dh, 0Ah, '$'
str_untitled:   DB 'Untitled', 0
str_about_title: DB 'About', 0
str_about_l1:    DB 'DOS Notepad v1.0.1', 0
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

; --- Accelerator display strings ---
str_acc_ctrl_n: DB 'Ctrl+N', 0
str_acc_ctrl_o: DB 'Ctrl+O', 0
str_acc_ctrl_s: DB 'Ctrl+S', 0
str_acc_ctrl_z: DB 'Ctrl+Z', 0
str_acc_ctrl_y: DB 'Ctrl+Y', 0
str_acc_ctrl_x: DB 'Ctrl+X', 0
str_acc_ctrl_c: DB 'Ctrl+C', 0
str_acc_ctrl_v: DB 'Ctrl+V', 0
str_acc_ctrl_a: DB 'Ctrl+A', 0
str_acc_ctrl_f: DB 'Ctrl+F', 0
str_acc_f3:     DB 'F3', 0
str_acc_ctrl_r: DB 'Ctrl+R', 0
str_acc_alt_x:  DB 'Alt+X', 0

; ============================================================================
; Menu Data Structures
; ============================================================================

file_entries:
    DW str_new,      handler_file_new
    DB 0, 'N'                        ; 'N'ew
    DW str_acc_ctrl_n                ; accel text
    DB 0, 0                          ; accel_key=0 (display only), pad
    DW str_open,     handler_file_open
    DB 0, 'O'                        ; 'O'pen
    DW str_acc_ctrl_o
    DB 0, 0
    DW str_save,     handler_file_save
    DB 0, 'S'                        ; 'S'ave
    DW str_acc_ctrl_s
    DB 0, 0
    DW str_save_as,  handler_file_save_as
    DB 5, 'A'                        ; Save 'A's
    DW 0                             ; no accel
    DB 0, 0
    DW str_exit,     handler_file_exit
    DB 1, 'x'                        ; E'x'it
    DW str_acc_alt_x
    DB 2Dh, 0                        ; accel_key=2Dh (Alt+X scan), pad

edit_entries:
    DW str_undo,     handler_edit_undo
    DB 0, 'U'                        ; 'U'ndo
    DW str_acc_ctrl_z
    DB 0, 0
    DW str_redo,     handler_edit_redo
    DB 0, 'R'                        ; 'R'edo
    DW str_acc_ctrl_y
    DB 0, 0
    DW str_cut,      handler_edit_cut
    DB 2, 't'                        ; Cu't'
    DW str_acc_ctrl_x
    DB 0, 0
    DW str_copy,     handler_edit_copy
    DB 0, 'C'                        ; 'C'opy
    DW str_acc_ctrl_c
    DB 0, 0
    DW str_paste,    handler_edit_paste
    DB 0, 'P'                        ; 'P'aste
    DW str_acc_ctrl_v
    DB 0, 0
    DW str_sel_all,  handler_edit_sel_all
    DB 7, 'A'                        ; Select 'A'll
    DW str_acc_ctrl_a
    DB 0, 0

search_entries:
    DW str_find,      handler_search_find
    DB 0, 'F'                        ; 'F'ind
    DW str_acc_ctrl_f
    DB 0, 0
    DW str_find_next, handler_search_find_next
    DB 5, 'N'                        ; Find 'N'ext
    DW str_acc_f3
    DB 0, 0
    DW str_replace,   handler_search_replace
    DB 0, 'R'                        ; 'R'eplace
    DW str_acc_ctrl_r
    DB 0, 0

help_entries:
    DW str_about,     handler_help_about
    DB 0, 'A'                        ; 'A'bout
    DW 0                             ; no accel
    DB 0, 0

menu_items:
    DW str_file
    DB 1, 6
    DW file_entries
    DB 5, 18                ; MI_ECOUNT=5, MI_DDW=18 (widened for accel text)
    DB 0, 21h               ; MI_HOTIDX=0 ('F'), MI_ALTKEY=21h (Alt+F)

    DW str_edit
    DB 7, 6
    DW edit_entries
    DB 6, 18                ; MI_ECOUNT=6, MI_DDW=18 (widened for accel text)
    DB 0, 12h               ; MI_HOTIDX=0 ('E'), MI_ALTKEY=12h (Alt+E)

    DW str_search
    DB 13, 8
    DW search_entries
    DB 3, 18                ; MI_ECOUNT=3, MI_DDW=18 (widened for accel text)
    DB 0, 1Fh               ; MI_HOTIDX=0 ('S'), MI_ALTKEY=1Fh (Alt+S)

    DW str_help
    DB 21, 6
    DW help_entries
    DB 1, 8                 ; MI_DDW=8 (unchanged, no accel)
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
    ; Check for MDA adapter (mode 7) — only 4KB VRAM, no page 1
    MOV AH, 0Fh                ; get current video mode
    INT 10h                    ; AL = mode
    CMP AL, 07h                ; mode 7 = MDA
    JNZ _not_mda
    MOV AH, 09h                ; print string
    MOV DX, str_mda_error
    INT 21h
    INT 20h                    ; exit
_not_mda:

    ; Save current drive and directory (restored on exit)
    MOV AH, 19h                ; get current drive
    INT 21h
    MOV [saved_drive], AL      ; 0=A, 1=B, 2=C, ...
    MOV AH, 47h                ; get current directory
    MOV DL, 0                  ; 0 = default (current) drive
    MOV SI, saved_cwd + 1      ; buffer (skip leading backslash)
    INT 21h
    MOV BYTE [saved_cwd], '\'  ; prepend backslash

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

    ; Zero BSS variables not covered by init routines
    MOV WORD [ed_cliplen], 0
    MOV WORD [ed_find_len], 0

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

    ; Restore original drive and directory
    MOV AH, 0Eh                ; select disk
    MOV DL, [saved_drive]
    INT 21h
    MOV AH, 3Bh                ; chdir
    MOV DX, saved_cwd
    INT 21h

    ; Clean exit to DOS
    INT 20h

; ============================================================================
; BSS Layout (uninitialized data beyond end of binary)
; ============================================================================
; DOS allocates a full 64KB segment for .COM programs.  Memory beyond the
; binary is available but NOT zero-filled.  All variables below are either
; initialized by their respective init routines (tui_init, gap_init, etc.)
; or written before first read.  Two exceptions (ed_cliplen, ed_find_len)
; are explicitly zeroed in main startup.
;
; Using EQU instead of RESB avoids emitting ~42KB of zero padding that
; would push the .COM binary past the 64KB segment limit.
; ============================================================================

_bss:

; --- TUI framework (tui_data.inc) ---
shadow_buf          EQU _bss                        ; SCR_BYTES (4000)
fw_state            EQU shadow_buf + SCR_BYTES      ; 16
win_table           EQU fw_state + 16               ; 320
z_order             EQU win_table + 320             ; 16
bdr_scratch         EQU z_order + 16                ; 7
ctrl_dispatch_addr  EQU bdr_scratch + 7             ; 2
mouse_state         EQU ctrl_dispatch_addr + 2      ; 16
sb_scratch          EQU mouse_state + 16            ; 4

; --- Dialog scratch ---
dlg_tmpl            EQU sb_scratch + 4              ; 20
dlg_label           EQU dlg_tmpl + 20               ; 14
dlg_btn1            EQU dlg_label + 14              ; 14
dlg_btn2            EQU dlg_btn1 + 14               ; 14
dlg_textbox         EQU dlg_btn2 + 14               ; 18
dlg_label2          EQU dlg_textbox + 18            ; 14
dlg_textbox2        EQU dlg_label2 + 14             ; 18
dlg_btn3            EQU dlg_textbox2 + 18           ; 14
dlg_checkbox        EQU dlg_btn3 + 14               ; 15
dlg_checkbox2       EQU dlg_checkbox + 15           ; 15

; --- Dialog temp saves ---
dlg_save_title      EQU dlg_checkbox2 + 15          ; 2
dlg_save_buf        EQU dlg_save_title + 2          ; 2
dlg_save_maxlen     EQU dlg_save_buf + 2            ; 1
dlg_save_prompt2    EQU dlg_save_maxlen + 1         ; 2
dlg_save_buf2       EQU dlg_save_prompt2 + 2        ; 2
dlg_save_maxlen2    EQU dlg_save_buf2 + 2           ; 1

; --- File dialog ---
dlg_listbox         EQU dlg_save_maxlen2 + 1        ; 19
dlg_file_dta        EQU dlg_listbox + 19            ; 43
dlg_file_path       EQU dlg_file_dta + 43           ; 65
dlg_file_count      EQU dlg_file_path + 65          ; 1
dlg_file_items      EQU dlg_file_count + 1          ; 256 (128 ptrs)
dlg_file_names      EQU dlg_file_items + 256        ; 1792 (128 x 14)

; --- Drive dropdown ---
dlg_drive_dd        EQU dlg_file_names + 1792       ; 21
dlg_drive_count     EQU dlg_drive_dd + 21           ; 1
dlg_drive_items     EQU dlg_drive_count + 1         ; 52 (26 ptrs)
dlg_drive_names     EQU dlg_drive_items + 52        ; 78 (26 x 3)

; --- About dialog labels ---
about_lbl1          EQU dlg_drive_names + 78        ; CTRL_SIZE (14)
about_lbl2          EQU about_lbl1 + CTRL_SIZE
about_lbl3          EQU about_lbl2 + CTRL_SIZE
about_lbl4          EQU about_lbl3 + CTRL_SIZE

; --- Editor data ---
ed_state            EQU about_lbl4 + CTRL_SIZE      ; ED_SIZE (23)
ed_buffer           EQU ed_state + ED_SIZE           ; ED_BUFSIZE (8192)
ed_filename_buf     EQU ed_buffer + ED_BUFSIZE       ; 64
ed_filedlg_buf      EQU ed_filename_buf + 64         ; 64
ed_clipboard        EQU ed_filedlg_buf + 64          ; ED_BUFSIZE (8192)
ed_cliplen          EQU ed_clipboard + ED_BUFSIZE    ; 2
ed_find_buf         EQU ed_cliplen + 2               ; 64
ed_replace_buf      EQU ed_find_buf + 64             ; 64
ed_find_len         EQU ed_replace_buf + 64          ; 2
saved_cursor        EQU ed_find_len + 2              ; 2
saved_drive         EQU saved_cursor + 2             ; 1
saved_cwd           EQU saved_drive + 1              ; 65

; --- Undo/Redo ---
undo_buf            EQU saved_cwd + 65               ; UNDO_BUFSIZE (4096)
undo_head           EQU undo_buf + UNDO_BUFSIZE      ; 2
undo_tail           EQU undo_head + 2                ; 2
undo_count          EQU undo_tail + 2                ; 2
redo_buf            EQU undo_count + 2               ; REDO_BUFSIZE (2048)
redo_head           EQU redo_buf + REDO_BUFSIZE      ; 2
redo_count          EQU redo_head + 2                ; 2
undo_scratch_change EQU redo_count + 2               ; 2
undo_scratch_cursor EQU undo_scratch_change + 2      ; 2
undo_scratch_datalen EQU undo_scratch_cursor + 2     ; 2
undo_scratch_inslen EQU undo_scratch_datalen + 2     ; 2
undo_from_redo      EQU undo_scratch_inslen + 2      ; 1

; --- Find/Replace scratch (editor_find.inc) ---
str_repl_count_msg  EQU undo_from_redo + 1           ; 28

; --- Undo selection temp (editor_undo.inc) ---
undo_sel_tmp        EQU str_repl_count_msg + 28      ; ED_BUFSIZE (8192)

_bss_end            EQU undo_sel_tmp + ED_BUFSIZE
