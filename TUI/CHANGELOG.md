# TUI Framework - Changelog

Development history organized by implementation phase.

---

## Phase 1 — Core Framework + Single Window

Established the foundation: shadow buffer compositing, row offset table,
VGA 80x50 text mode setup, and single-window rendering.

**Added:**
- `tui_init` — framework initialization (80x50 mode, hidden cursor, state zeroing)
- `tui_clear_shadow` — fill shadow buffer with desktop background
- `tui_blit` — REP MOVSW copy from shadow buffer to VRAM B800h
- `calc_vram_offset` — row offset table lookup for cell address calculation
- `tui_putchar`, `tui_putstr`, `tui_hline`, `tui_fill_rect` — drawing primitives
- `tui_darken_cell`, `tui_darken_hline` — shadow effect helpers
- `tui_win_create` — create window from 20-byte template struct
- `tui_win_draw` — draw single window (border, interior, title, shadow)
- `tui_compose` — clear + draw all windows + blit
- `tui_poll_key` — non-blocking keyboard poll (INT 21h/06h)
- `tui_run` — main event loop (poll, dispatch, redraw)
- Window struct (20 bytes), framework state, row_offsets table, shadow_buf

**Files:** `tui_const.inc`, `tui_video.inc`, `tui_window.inc`, `tui_event.inc`, `tui_data.inc`
**Test:** `test1.asm`

---

## Phase 2 — Multiple Windows + Z-Order

Added support for 16 simultaneous windows with z-order management and keyboard
window movement.

**Added:**
- 16-slot window table with z_order byte array
- `tui_win_close` — close window by slot, remove from z-order
- `tui_zorder_cycle` — rotate topmost window to bottom (F6 key)
- `tui_win_move` — move active window by arrow keys (Ctrl+Arrows)
- Active window border: double-line (DBOX_*) vs single-line (BOX_*)
- `bdr_scratch` area for per-window border style selection

**Test:** `test2.asm`

---

## Phase 3 — Control Framework + Buttons + Labels

Introduced the control linked list architecture with label and button control types.

**Added:**
- Control struct base (14 bytes: type, flags, position, size, text, attrs, handler)
- `tui_ctrl_draw_all` — walk linked list, dispatch by type
- `tui_ctrl_draw_label` — static text rendering
- `tui_ctrl_draw_button` — `[ text ]` with centered text, normal/focused attrs
- `tui_ctrl_focus_next` — Tab focus cycling through focusable+enabled controls
- `tui_ctrl_activate` — call handler via RET-trampoline (CALL reg not supported)
- `ctrl_dispatch_addr` — indirect call temp word for RET-trampoline pattern
- Tab dispatches to control focus cycling before window cycling

**Files:** `tui_control.inc`
**Test:** `test3.asm`

---

## Phase 4 — TextBox (Editable Text Input)

Added single-line editable text input with cursor, scrolling, and full editing.

**Added:**
- TextBox struct extension (+4 bytes: maxlen, curpos, scroll, len)
- `tui_ctrl_draw_textbox` — text rendering with scroll offset + block cursor
- `tui_ctrl_handle_key` — routes keys to focused TextBox
- `tui_tb_insert_char` — insert character at cursor, shift tail right
- `tui_tb_backspace`, `tui_tb_delete` — delete before/at cursor
- `tui_tb_cursor_left`, `tui_tb_cursor_right` — cursor movement
- `tui_tb_home`, `tui_tb_end` — jump to start/end
- `tui_tb_ensure_visible` — auto-scroll to keep cursor visible
- Printable range 20h-7Eh routed to insert; Backspace, Delete, Home, End, arrows handled

**Test:** `test4.asm`

---

## Phase 5 — Checkbox + Radio Buttons

Added toggle and exclusive-selection controls.

**Added:**
- Checkbox struct extension (+1 byte: state)
- Radio button struct extension (+2 bytes: state, group)
- `tui_ctrl_draw_checkbox` — `[X]/[ ] Label` rendering
- `tui_ctrl_draw_radio` — `(*)/( ) Label` rendering
- `tui_radio_group_select` — clear all radios in group, select one
- Enter/Space activates: toggles checkbox, selects radio
- Checkbox/radio colors: `CLR_CHECKBOX`, `CLR_CB_FOCUS`, `CLR_RADIO`, `CLR_RB_FOCUS`

**Test:** `test5.asm`

---

## Phase 6 — Menu Bar + Dropdown Menus

Added a full menu bar system with dropdown menus as a separate overlay layer.

**Added:**
- Menu bar struct (8 bytes), menu item struct (10 bytes), dropdown entry (10 bytes)
- `tui_menu_draw_bar` — draw menu bar on row 0
- `tui_menu_draw_dropdown` — draw open dropdown with border + shadow
- `tui_menu_handle_key` — state machine: inactive / bar-active / dropdown-open
- `tui_menu_activate_entry` — execute entry handler, close menu
- Menu bar is a separate layer, not a window (drawn on top during compose)
- F10 activates menu, arrows navigate, Enter opens/activates, Escape closes
- Menu colors: `CLR_MENUBAR`, `CLR_MENU_SEL`, `CLR_MENU_DD`, `CLR_MENU_DDSEL`

**Files:** `tui_menu.inc`
**Test:** `test6.asm`

---

## Phase 7 — Dropdown List (Combo Box)

Added a combo box control with a popup overlay for item selection.

**Added:**
- Dropdown struct extension (+7 bytes: items, count, sel, tmpsel, scroll, maxvis)
- `tui_ctrl_draw_dropdown` — closed state: selected item text + down-arrow
- `tui_dd_draw_popup` — popup overlay drawn after windows in compose
- `tui_dd_handle_key` — open/closed state handling, arrow/Home/End navigation
- `tui_dd_open`, `tui_dd_close_commit`, `tui_dd_close_cancel`
- `tui_dd_ensure_visible` — popup scroll adjustment
- Global `FW_OPENDD` tracks the single open popup (only one at a time)
- Popup position computed from window + control absolute coordinates

**Test:** `test7.asm`

---

## Phase M1 — Mouse Infrastructure + Cursor

Introduced INT 33h mouse driver detection and visual cursor overlay.

**Added:**
- `tui_mouse_init` — detect driver, set pixel ranges (0-639, 0-399)
- `tui_mouse_poll` — read position + buttons, convert pixels to cells (divide by 8)
- `tui_mouse_draw_cursor` — invert attribute at cursor position (swap fg/bg nibbles)
- Mouse state struct (16 bytes)
- Compose pipeline: compose -> draw cursor -> blit
- Mouse poll integrated into `tui_run` event loop

**Files:** `tui_mouse.inc`
**Test:** `test_m1.asm`

---

## Phase M2 — Window Click-to-Activate + Drag + Close

Added mouse-driven window management: click to focus, drag to move, click to close.

**Added:**
- `tui_mouse_hit_test` — determine what's under cursor (window, border, title, etc.)
- `tui_mouse_on_press` — handle left-button-down: dispatch by hit type
- `tui_mouse_on_release` — handle left-button-up
- `tui_mouse_on_drag` — update window position during drag
- `tui_mouse_bring_to_front` — reorder z_order to make window topmost
- Titlebar click starts drag, close button click closes window
- Hit test results: `HIT_NONE`, `HIT_TITLEBAR`, `HIT_CLOSEBTN`, `HIT_INTERIOR`, `HIT_BORDER`

**Test:** `test_m2.asm`

---

## Phase M3 — Control Click Interaction

Added mouse click support for all interactive control types.

**Added:**
- `tui_mouse_hit_test_control` — find control at cursor position within window
- Control click handlers: button press/release, checkbox toggle, radio select
- `tui_mouse_call_handler` — call control handler via RET-trampoline
- Button pressed visual state (`MS_PRESSED` + `CLR_BTN_PRESSED`)
- Click sets focus to the clicked control

**Test:** `test_m3.asm`

---

## Phase M4 — Menu Bar Mouse

Added mouse interaction for the menu bar and dropdown menus.

**Added:**
- Click on menu bar item opens dropdown
- Click on dropdown entry activates handler
- `tui_mouse_menu_hover` — hover switches dropdowns, highlights entries
- `tui_mouse_close_menu` — close menu when clicking outside
- Hit test: `HIT_MENUBAR`, `HIT_MENUDROP` result types
- Menu dropdown hit test with entry index calculation

**Test:** `test_m4.asm`

---

## Phase M5 — TextBox + Dropdown Combo Mouse

Added mouse support for TextBox cursor positioning and dropdown popup interaction.

**Added:**
- TextBox click: position cursor at clicked column (accounting for scroll)
- Dropdown click: open popup on control click
- Combo popup click: select item + close popup
- Combo popup dismiss: clicking outside closes popup
- Hit test: `HIT_DD_POPUP` result type
- Combo popup hit test with item index calculation from row position

**Test:** `test_m5.asm`

---

## Phase M6 — Window Resize via Mouse Drag

Added mouse-driven window resizing by dragging the bottom-right corner.

**Added:**
- `WINF_RESIZABLE` flag (80h) — enables resize for a window
- Resize indicator character (CP437 0x12) at bottom-right corner
- `tui_mouse_on_resize` — update width/height during drag
- `HIT_RESIZE` hit test result for bottom-right corner
- Minimum dimensions: `WIN_MINW` (8) x `WIN_MINH` (4)
- Screen bounds clamping during resize

**Test:** `test_m6.asm`

---

## Phase 8 — Standard Dialogs (Message Box, Confirm, Input)

Added a modal dialog framework with three standard dialog types.

**Added:**
- `tui_dlg_run_modal` — create dialog window, run nested event loop, return result
- `tui_dlg_msgbox` — message box with OK button
- `tui_dlg_confirm` — Yes/No confirmation dialog
- `tui_dlg_input` — text input dialog with prompt, textbox, OK/Cancel
- `tui_modal_dispatch` — restricted key dispatch for modal dialogs
- Dialog button handlers (`dlg_handler_ok/cancel/yes/no`)
- Dialog sizing: auto-width based on message length, centered on screen
- Modal enforcement: blocks q/Q quit, menu access, window cycling, outside clicks
- `FW_DLG_RESULT`, `FW_MODAL_WIN` in framework state
- Dialog scratch area (`dlg_tmpl`, `dlg_label`, `dlg_btn1`, `dlg_btn2`, `dlg_textbox`)

**Files:** `tui_dialog.inc`
**Test:** `test8.asm`

---

## Phase 9 — Listbox (Scrolling List)

Added a scrolling selection list control with auto-hiding vertical scroll bar.

**Added:**
- Listbox struct extension (+5 bytes: items, count, sel, scroll)
- `tui_ctrl_draw_listbox` — draw visible rows with selection highlighting
- `tui_lb_handle_key` — Up/Down/Home/End/PgUp/PgDn navigation
- `tui_lb_ensure_visible` — scroll adjustment to keep selection visible
- `tui_draw_vscrollbar` — generic vertical scroll bar drawing
- Auto-hiding: scroll bar only appears when item count > visible height
- Four color states: normal, focused, selected, focused+selected
- Mouse: click to select item, scroll bar arrows/page/thumb
- `tui_sb_vbar_click_generic` — generic scroll bar click handler
- `sb_scratch` area for scroll bar math
- Scroll bar characters: `SB_ARROW_UP/DN`, `SB_THUMB`, `SB_TRACK`

**Test:** `test9.asm`

---

## Phase 10 — File Selector Dialog

Added a file browser dialog for selecting files from the filesystem.

**Added:**
- `tui_dlg_file` — file selector with directory listing, OK/Cancel buttons
- `tui_file_enumerate` — list directory via INT 21h FindFirst/FindNext (4Eh/4Fh)
- `tui_file_get_path` — get CWD via INT 21h/47h
- `dlg_file_handle_sel` — shared handler: chdir for directories, select for files
- Always adds `..\ ` entry for parent directory navigation
- Directory entries shown with trailing backslash
- On file selection: copies name to caller's buffer, closes with DLG_OK
- On directory selection: chdir, re-enumerate, update listbox
- File dialog data buffers: DTA (43B), path (65B), items (64B), names (448B)
- Maximum 32 directory entries

**Test:** `test10.asm`

---

## Phase 11 — Scrollable Text Viewer Control

Added a read-only text viewer with cursor line, vertical + horizontal scrolling,
and auto-hiding scroll bars.

**Added:**
- Text viewer struct extension (+7 bytes: lines, count, curline, scroll, scrollx, maxlen)
- `tui_ctrl_draw_textview` — draw visible rows with cursor line highlighting
- `tui_tv_handle_key` — Up/Down/Home/End/PgUp/PgDn + Left/Right for H-scroll
- `tui_tv_ensure_visible` — vertical scroll adjustment
- `tui_tv_parse_text` — parse CR/LF delimited text into line pointer array
- `tui_draw_hscrollbar` — generic horizontal scroll bar drawing
- Two-pass auto-hide logic: determines which scroll bars are needed based on
  content dimensions vs control dimensions (each bar affects the other's calculation)
- Corner cell rendering when both bars are visible
- Mouse: click to set cursor line, click on V/H scroll bar arrows/page/thumb
- `tui_sb_hbar_click_generic` — horizontal scroll bar click handler
- `tui_mouse_on_sb_drag` — scroll bar thumb drag (vertical + horizontal)
- `MSF_SB_DRAG` flag and `MS_SB_CTRL/DIR/OFFSET` fields in mouse state

**Test:** `test11.asm`

---

## Phase 11b — Dual-Input Dialog (`tui_dlg_input2`)

Added a dual-textbox dialog for use cases needing two related text inputs
(e.g., Find/Replace). No changes to existing framework logic — only new data
reservations and one new function.

**Added:**
- `tui_dlg_input2` — dual text input dialog with two labeled fields + OK/Cancel
- Dialog layout: label1 (row 0), textbox1 (row 1), label2 (row 3), textbox2 (row 4), buttons (row 6)
- WIN_H=9 (7 interior rows + 2 border rows)
- Control chain: dlg_label → dlg_textbox → dlg_label2 → dlg_textbox2 → dlg_btn1 → dlg_btn2
- Tab order: textbox1 → textbox2 → OK → Cancel (labels not focusable)
- Caller pre-sets `dlg_save_prompt2`, `dlg_save_buf2`, `dlg_save_maxlen2` for second field
- Width auto-calculated from max(prompt1_len, prompt2_len) + padding
- Pre-populated buffers supported (cursor positioned at end of existing text)
- Data reservations: `dlg_label2` (14B), `dlg_textbox2` (18B), `dlg_save_prompt2` (2B), `dlg_save_buf2` (2B), `dlg_save_maxlen2` (1B)

**Files modified:** `tui_dialog.inc`, `tui_data.inc`
**Test:** `test_input2.asm`

---

## Phase 11c — Menu Bar Hotkey Highlights + Dropdown Entry Hotkeys

Extended the menu system with visual hotkey indicators and keyboard activation
for both the menu bar and dropdown entries.

### Phase 9 (Bar Hotkeys — previously undocumented in changelog)

Extended menu item struct from 8 to 10 bytes to support Alt+key bar hotkeys.

**Added:**
- `MI_HOTIDX` (BYTE, offset 8) — index of hotkey character in menu item text (FFh = none)
- `MI_ALTKEY` (BYTE, offset 9) — Alt+key scan code that opens this dropdown (0 = none)
- `_mi_index_to_offset` — helper to convert menu item index to byte offset (replaces hardcoded SHL×3)
- Hotkey highlight pass in `tui_menu_draw_bar` — draws hotkey letter in `CLR_MENU_HOTKEY` (74h, red on light gray)
- `_mhk_check_alt_key` — scan code matcher for Alt+key shortcuts
- Alt+key handling wired into INACTIVE, BAR-ONLY, and DROPDOWN-OPEN menu states
- `CLR_MENU_HOTKEY` color constant (74h)

### Phase 9b (Dropdown Entry Hotkeys)

Extended dropdown entry struct from 4 to 6 bytes (later to 10, see Phase 13)
to support single-letter hotkeys within open dropdown menus.

**Added:**
- `MDE_HOTIDX` (BYTE, offset 4) — index of hotkey character in entry text (FFh = none)
- `MDE_HOTKEY` (BYTE, offset 5) — ASCII character that activates this entry (0 = none)
- `_mde_index_to_offset` — helper to convert entry index to byte offset (replaces hardcoded SHL×2)
- Hotkey highlight pass in `tui_menu_draw_dropdown` — draws hotkey letter in `CLR_MENU_DD_HOTKEY` (74h); skips selected entry (selection color takes priority)
- `_mhk_check_dd_hotkey` — case-insensitive ASCII matcher for dropdown hotkeys
- Dropdown hotkey handling wired into DROPDOWN-OPEN normal key handler: matching letter selects and activates the entry
- `CLR_MENU_DD_HOTKEY` color constant (74h)

**Files modified:** `tui_const.inc`, `tui_menu.inc`

---

## Phase 12 — Drive Selection Dropdown in File Dialog

Added a drive dropdown to the file selector dialog, allowing users to switch
between available drives (A:, C:, D:, etc.). Also increased the maximum
directory entry limit from 32 to 128.

**Added:**
- `tui_file_detect_drives` — probe drives A-Z via INT 21h AX=4409h (IOCTL check block device), populate `dlg_drive_items[]` and `dlg_drive_names[]`
- `_dlg_file_find_cur_drive` — get current drive (AH=19h), linear search items array, return index
- `dlg_file_drive_handler` — dropdown commit handler: select disk (AH=0Eh), chdir root (AH=3Bh "\\"), re-enumerate files, update listbox and path display
- Drive data buffers: `dlg_drive_dd` (21B dropdown control), `dlg_drive_count` (1B), `dlg_drive_items` (52B), `dlg_drive_names` (78B)
- `dlg_str_drive` ("Drive:") and `dlg_file_rootdir` ("\\") static strings

**Modified:**
- `tui_dlg_file` — row 0 now has "Drive:" label (col 0), drive dropdown (col 7, w=5), and path label (col 13, w=19). Control chain: `dlg_label2 → dlg_drive_dd → dlg_label → dlg_listbox → btn1 → btn2`. WIN_FIRST = `dlg_label2`
- `DLG_FILE_MAXENT` — increased from 32 to 128 (with corresponding buffer expansions: items 64→256 bytes, names 448→1792 bytes)

**Files modified:** `tui_const.inc`, `tui_data.inc`, `tui_dialog.inc`

---

## Phase 13 — Menu Accelerator Keys (Display + Global Dispatch)

Extended dropdown entries with optional accelerator key display and global
shortcut dispatch. Accelerator text (e.g., "Ctrl+S", "Alt+X") is rendered
right-aligned in dropdown entries. Non-zero `MDE_ACCELKEY` values enable
global dispatch: the entry's handler is called directly without opening the
menu, triggered by the corresponding Alt+key scan code from the INACTIVE state.

**Added:**
- `MDE_ACCEL` (WORD, offset 6) — pointer to accelerator display string (0 = none)
- `MDE_ACCELKEY` (BYTE, offset 8) — Alt+key scan code for global dispatch (0 = display only)
- `MDE_PAD` (BYTE, offset 9) — reserved padding byte
- Accelerator text rendering pass in `tui_menu_draw_dropdown` — draws right-aligned text using the entry's normal or selected attribute
- `_mhk_check_accel` — scans all `MDE_ACCELKEY` fields across all menus, calls matching handler via RET-trampoline

**Modified:**
- `MDE_SIZE` — increased from 6 to 10 bytes
- `tui_menu_handle_key` INACTIVE state — after `_mhk_check_alt_key` fails, tries `_mhk_check_accel` before falling through to not-handled

**Files modified:** `tui_const.inc`, `tui_menu.inc`

---

## Phase 6b — Control-Level Drag Infrastructure (MSF_CTRL_DRAG)

Added a generic control-level drag mechanism, mirroring the existing
`MSF_SB_DRAG` pattern. This allows individual control types to receive
continuous drag events while the mouse button is held.

**Added:**
- `MSF_CTRL_DRAG` (20h) flag in mouse state flags
- `tui_mouse_on_ctrl_drag` — dispatcher that reads `MS_PRESSED` control pointer and calls the appropriate control-type drag handler each frame while `MSF_CTRL_DRAG` is active and button held
- Integration into `tui_mouse_dispatch`: on button-held, checks `MSF_CTRL_DRAG` before other drag types; on button-release, clears the flag

**Files modified:** `tui_const.inc`, `tui_mouse.inc`

---

## Phase 9 — Quit Behavior Change

Removed the default `q`/`Q`/`Escape` quit behavior from `tui_dispatch_key`.
The framework no longer provides a built-in quit mechanism; applications must
handle exit themselves (e.g., via a menu entry or control handler that sets
`FW_RUNNING = 0`). This prevents accidental quits when typing in text controls.

**Modified:**
- `tui_dispatch_key` normal key handler — removed `q`, `Q`, `KEY_ESCAPE` checks that set `FW_RUNNING = 0`

**Files modified:** `tui_event.inc`

---

## Phase 14 — CTYPE_EDITOR Dispatch in TUI Framework

Extended the TUI control dispatch to support `CTYPE_EDITOR` (9), an
application-defined control type for the notepad editor. The control type
constant and editor state struct are defined in `editor_const.inc` (outside
the TUI framework), but the TUI dispatch tables were extended to call the
editor's draw, key, mouse press, scroll bar drag, and control drag handlers.

**Added:**
- `CTYPE_EDITOR` (9) dispatch in `tui_ctrl_draw_all` → calls `tui_ctrl_draw_editor`
- `CTYPE_EDITOR` dispatch in `tui_ctrl_handle_key` → calls `tui_ed_handle_key`
- `CTYPE_EDITOR` dispatch in `tui_mouse_on_press` interior handler → calls `tui_ed_mouse_press`
- `CTYPE_EDITOR` dispatch in `tui_mouse_on_sb_drag` → WORD-sized scroll math for editor (vs BYTE for listbox/textview)
- `CTYPE_EDITOR` dispatch in `tui_mouse_on_ctrl_drag` → calls `tui_ed_mouse_drag`

**Files modified:** `tui_control.inc`, `tui_mouse.inc`
