; ============================================================================
; tui_data.inc - TUI Framework Data Reservations
; ============================================================================

; --- Row offset lookup table (50 entries, each = row * 160) ---
row_offsets:
    DW 0, 160, 320, 480, 640, 800, 960, 1120, 1280, 1440
    DW 1600, 1760, 1920, 2080, 2240, 2400, 2560, 2720, 2880, 3040
    DW 3200, 3360, 3520, 3680, 3840, 4000, 4160, 4320, 4480, 4640
    DW 4800, 4960, 5120, 5280, 5440, 5600, 5760, 5920, 6080, 6240
    DW 6400, 6560, 6720, 6880, 7040, 7200, 7360, 7520, 7680, 7840

; --- Shadow VRAM buffer (8000 bytes) ---
shadow_buf:
    RESB 8000

; --- Framework global state ---
fw_state:
    RESB 16

; --- Window table (16 windows x 20 bytes = 320 bytes) ---
win_table:
    RESB 320

; --- Z-order array (index 0 = bottom, higher = closer to top) ---
z_order:
    RESB 16

; --- Border scratch area (6 box chars + 1 attr, filled per window at draw time) ---
bdr_scratch:
    RESB 7

; --- Control indirect call temp (1 word for RET-trampoline) ---
ctrl_dispatch_addr:
    RESW 1

; --- Mouse state (16 bytes) ---
mouse_state:
    RESB 16

; --- Scroll bar scratch (4 bytes: thumb_size, thumb_pos, track_len, temp) ---
sb_scratch:
    RESB 4

; --- Dialog scratch area for building controls/templates dynamically ---
dlg_tmpl:       RESB 20        ; window template
dlg_label:      RESB 14        ; message/prompt label control
dlg_btn1:       RESB 14        ; OK/Yes button
dlg_btn2:       RESB 14        ; Cancel/No button
dlg_textbox:    RESB 18        ; textbox (input dialog only)
dlg_label2:     RESB 14        ; second prompt label (input2 dialog)
dlg_textbox2:   RESB 18        ; second textbox (input2 dialog)

; --- Dialog temp saves (used during struct building) ---
dlg_save_title:  RESW 1
dlg_save_buf:    RESW 1
dlg_save_maxlen: RESB 1
dlg_save_prompt2:  RESW 1      ; second prompt string pointer
dlg_save_buf2:     RESW 1      ; second buffer pointer
dlg_save_maxlen2:  RESB 1      ; second max length

; --- File dialog scratch ---
dlg_listbox:     RESB 19        ; listbox control struct (file dialog)

; --- File dialog data ---
dlg_file_dta:    RESB 43        ; DOS Disk Transfer Area
dlg_file_path:   RESB 65        ; CWD display buffer ("\DIRNAME\0")
dlg_file_count:  RESB 1         ; number of entries in list
dlg_file_items:  RESW 32        ; item pointer array (32 WORD ptrs = 64 bytes)
dlg_file_names:  RESB 448       ; name storage (32 entries x 14 bytes)

; --- Static button label strings ---
dlg_str_ok:     DB 'OK', 0
dlg_str_cancel: DB 'Cancel', 0
dlg_str_yes:    DB 'Yes', 0
dlg_str_no:     DB 'No', 0

; --- File dialog static strings ---
dlg_file_pattern: DB '*.*', 0
