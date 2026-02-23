; ============================================================================
; tui.inc - Master Include File
; Include this single file to get the entire TUI framework.
; Order matters: constants first, then macros, then code, then data last.
; ============================================================================

INCLUDE tui_const.inc
INCLUDE tui_macros.inc
INCLUDE tui_video.inc
INCLUDE tui_window.inc
INCLUDE tui_control.inc
INCLUDE tui_menu.inc
INCLUDE tui_mouse.inc
INCLUDE tui_event.inc
INCLUDE tui_dialog.inc
INCLUDE tui_data.inc
