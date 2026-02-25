; ============================================================================
; tui_data.inc - TUI Framework Initialized Data
; ============================================================================
; NOTE: All uninitialized buffers (shadow_buf, fw_state, win_table, etc.)
; are defined as EQU constants in the BSS block at the end of notepad.asm.
; This keeps the .COM binary small (under the 64KB segment limit).

; --- Row offset lookup table (50 entries, each = row * 160) ---
row_offsets:
    DW 0, 160, 320, 480, 640, 800, 960, 1120, 1280, 1440
    DW 1600, 1760, 1920, 2080, 2240, 2400, 2560, 2720, 2880, 3040
    DW 3200, 3360, 3520, 3680, 3840, 4000, 4160, 4320, 4480, 4640
    DW 4800, 4960, 5120, 5280, 5440, 5600, 5760, 5920, 6080, 6240
    DW 6400, 6560, 6720, 6880, 7040, 7200, 7360, 7520, 7680, 7840

; --- Static button label strings ---
dlg_str_ok:     DB 'OK', 0
dlg_str_cancel: DB 'Cancel', 0
dlg_str_yes:    DB 'Yes', 0
dlg_str_no:     DB 'No', 0

; --- File dialog static strings ---
dlg_file_pattern: DB '*.*', 0
dlg_file_rootdir: DB '\', 0
dlg_str_drive:   DB 'Drive:', 0
