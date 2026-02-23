# TUI Framework - API Reference

A DOS text-mode user interface library in x86 assembly for the agent86 assembler.
80x50 VGA text mode, CP437 character set, .COM memory model (ES=DS).

---

## Table of Contents

1. [Architecture](#architecture)
2. [Module Map](#module-map)
3. [Data Structures](#data-structures)
4. [Constants Reference](#constants-reference)
5. [API Reference](#api-reference)
6. [Register Conventions](#register-conventions)
7. [Color Attribute Format](#color-attribute-format)

---

## Architecture

### Shadow Buffer Compositing

All drawing targets an 8000-byte RAM buffer (`shadow_buf`) rather than writing
directly to video memory. The compositing pipeline:

1. `tui_clear_shadow` fills buffer with desktop background (space + `CLR_DESKTOP`)
2. Windows are drawn bottom-to-top per `z_order[]` array
3. Menu bar and dropdown overlays are drawn on top
4. Combo dropdown popup overlay is drawn on top
5. Mouse cursor overlay (attribute inversion) is applied
6. `tui_blit` copies the entire buffer to VRAM at B800:0000 via `REP MOVSW`

### Row Offset Table

A 50-entry word table (`row_offsets`) pre-computes `row * 160` for each screen
row, eliminating MUL instructions in the hot drawing path. `calc_vram_offset`
uses this table plus the column offset to produce an absolute address within
`shadow_buf`.

### Window Table and Z-Order

- Fixed 16-slot window table (`win_table`), 20 bytes per slot
- Slot 0 of `WIN_FLAGS` == 0 means the slot is free
- `z_order[16]` byte array: index 0 = bottommost window, index `FW_NUMWIN-1` = topmost
- `FW_TOPWIN` caches the slot index of the topmost window
- Active (topmost) window gets double-line border; inactive windows get single-line

### Control Linked List

Each window has a linked list of controls starting at `WIN_FIRST`. Controls are
connected via `CTRL_NEXT` word pointers (0 = end of list). The focused control
is tracked in `WIN_FOCUS`.

### Event Loop

`tui_run` is the main loop:
1. Poll keyboard (non-blocking via INT 21h/06h)
2. Dispatch key to: menu handler -> control handler -> global handler
3. Poll mouse (INT 33h/0003h)
4. If position/button changed, dispatch mouse event
5. If `FW_DIRTY` flag is set, recompose and blit
6. Loop until `FW_RUNNING` == 0

### Handler Callbacks (RET-Trampoline)

Since agent86 does not support `CALL reg`, indirect calls use a RET-trampoline
pattern: push the return address, push the handler address, then `RET` pops the
handler address into IP. The handler returns to the pushed return address.

Handler convention: `SI` = control struct ptr, `DI` = window struct ptr.

---

## Module Map

Include order matters. Use `INCLUDE tui.inc` to get everything.

| # | File | Role | Lines |
|---|------|------|-------|
| 1 | `tui_const.inc` | EQU constants: screen, window, control, flags, colors, keys, structs | 342 |
| 2 | `tui_macros.inc` | PushAll/PopAll macros (IRP-based) | 17 |
| 3 | `tui_video.inc` | Shadow buffer primitives: init, clear, blit, putchar, putstr, hline, fill_rect, darken | 299 |
| 4 | `tui_window.inc` | Window create/draw/compose/close, z-order cycle, keyboard move | 550 |
| 5 | `tui_control.inc` | Control framework: draw, focus, activate, key handlers for all 8 types, scroll bars | 2546 |
| 6 | `tui_menu.inc` | Menu bar + dropdown: draw, keyboard state machine, hotkeys, activate entry | 887 |
| 7 | `tui_mouse.inc` | Mouse: init, poll, cursor, hit test, dispatch, drag, resize, control click, scroll bar drag | 2129 |
| 8 | `tui_event.inc` | Key dispatch, modal dispatch, main event loop (`tui_run`) | 288 |
| 9 | `tui_dialog.inc` | Standard dialogs: msgbox, confirm, input, input2, file selector | 980 |
| 10 | `tui_data.inc` | Data reservations: row offsets, shadow_buf, fw_state, tables, dialog scratch | 80 |
| 11 | `tui.inc` | Master include (includes all above in correct order) | 16 |

---

## Data Structures

### Window Struct (20 bytes)

| Offset | Name | Size | Description |
|--------|------|------|-------------|
| 0 | `WIN_FLAGS` | BYTE | Bitmask of `WINF_*` flags |
| 1 | `WIN_X` | BYTE | Left column (0-79) |
| 2 | `WIN_Y` | BYTE | Top row (0-49) |
| 3 | `WIN_W` | BYTE | Width in columns |
| 4 | `WIN_H` | BYTE | Height in rows |
| 5 | `WIN_ATTR` | BYTE | Interior fill attribute |
| 6 | `WIN_BATTR` | BYTE | Border attribute (inactive window) |
| 7 | `WIN_TATTR` | BYTE | Title text attribute |
| 8 | `WIN_TITLE` | WORD | Pointer to null-terminated title string |
| 10 | `WIN_FIRST` | WORD | Pointer to first control (linked list head, 0=none) |
| 12 | `WIN_FOCUS` | WORD | Pointer to currently focused control |
| 14 | `WIN_HANDLER` | WORD | Window-level event handler (0=none) |
| 16 | `WIN_ZORDER` | BYTE | Z-order position (set by framework) |
| 17 | `WIN_ID` | BYTE | Slot index (set by `tui_win_create`) |
| 18 | `WIN_SCROLLX` | BYTE | Horizontal scroll offset (reserved) |
| 19 | `WIN_SCROLLY` | BYTE | Vertical scroll offset (reserved) |

### Control Struct Base (14 bytes)

All control types share this common header.

| Offset | Name | Size | Description |
|--------|------|------|-------------|
| 0 | `CTRL_TYPE` | BYTE | Control type (`CTYPE_*`) |
| 1 | `CTRL_FLAGS` | BYTE | Bitmask of `CTRLF_*` flags |
| 2 | `CTRL_X` | BYTE | X position relative to window interior |
| 3 | `CTRL_Y` | BYTE | Y position relative to window interior |
| 4 | `CTRL_W` | BYTE | Total rendered width |
| 5 | `CTRL_H` | BYTE | Height (1 for most single-line controls) |
| 6 | `CTRL_NEXT` | WORD | Next control in linked list (0=end) |
| 8 | `CTRL_TEXT` | WORD | Pointer to text string |
| 10 | `CTRL_ATTR` | BYTE | Normal (unfocused) attribute |
| 11 | `CTRL_FATTR` | BYTE | Focused attribute |
| 12 | `CTRL_HANDLER` | WORD | Callback pointer (0=none) |

### Label (14 bytes)

Uses only the base struct. `CTRL_TYPE` = `CTYPE_LABEL` (1). Not focusable.

### Button (14 bytes)

Uses only the base struct. `CTRL_TYPE` = `CTYPE_BUTTON` (2).
Renders as `[ text ]` with centered text. Has normal/focused/pressed visual states.

### TextBox (18 bytes)

`CTRL_TYPE` = `CTYPE_TEXTBOX` (3).

| Offset | Name | Size | Description |
|--------|------|------|-------------|
| 0-13 | (base) | 14 | Common control fields. `CTRL_TEXT` points to editable buffer. |
| 14 | `CTRL_TB_MAXLEN` | BYTE | Maximum characters the buffer can hold |
| 15 | `CTRL_TB_CURPOS` | BYTE | Cursor position (0-based) |
| 16 | `CTRL_TB_SCROLL` | BYTE | Scroll offset (first visible char index) |
| 17 | `CTRL_TB_LEN` | BYTE | Current text length |

### Checkbox (15 bytes)

`CTRL_TYPE` = `CTYPE_CHECKBOX` (4). Renders as `[X] Label` or `[ ] Label`.

| Offset | Name | Size | Description |
|--------|------|------|-------------|
| 0-13 | (base) | 14 | Common control fields |
| 14 | `CTRL_CB_STATE` | BYTE | 0=unchecked, 1=checked |

### Radio Button (16 bytes)

`CTRL_TYPE` = `CTYPE_RADIO` (5). Renders as `(*) Label` or `( ) Label`.
Shares `CTRL_CB_STATE` at offset 14 with Checkbox.

| Offset | Name | Size | Description |
|--------|------|------|-------------|
| 0-13 | (base) | 14 | Common control fields |
| 14 | `CTRL_CB_STATE` | BYTE | 0=unselected, 1=selected |
| 15 | `CTRL_RB_GROUP` | BYTE | Radio group ID (0-255) |

Selecting a radio button clears all other radios in the same group within the
same window.

### Dropdown / Combo Box (21 bytes)

`CTRL_TYPE` = `CTYPE_DROPDOWN` (6).

| Offset | Name | Size | Description |
|--------|------|------|-------------|
| 0-13 | (base) | 14 | Common control fields |
| 14 | `CTRL_DD_ITEMS` | WORD | Pointer to array of WORD string pointers |
| 16 | `CTRL_DD_COUNT` | BYTE | Total number of items |
| 17 | `CTRL_DD_SEL` | BYTE | Committed selection index |
| 18 | `CTRL_DD_TMPSEL` | BYTE | Temporary highlight while popup open |
| 19 | `CTRL_DD_SCROLL` | BYTE | First visible item in popup |
| 20 | `CTRL_DD_MAXVIS` | BYTE | Maximum visible items in popup |

The popup is drawn as a global overlay in `tui_compose` (after all windows).
Only one dropdown popup can be open at a time, tracked by `FW_OPENDD`.

### Listbox (19 bytes)

`CTRL_TYPE` = `CTYPE_LISTBOX` (7).

| Offset | Name | Size | Description |
|--------|------|------|-------------|
| 0-13 | (base) | 14 | Common control fields |
| 14 | `CTRL_LB_ITEMS` | WORD | Pointer to array of WORD string pointers |
| 16 | `CTRL_LB_COUNT` | BYTE | Total number of items |
| 17 | `CTRL_LB_SEL` | BYTE | Currently selected item index |
| 18 | `CTRL_LB_SCROLL` | BYTE | First visible item index |

Auto-hiding vertical scroll bar appears when `COUNT > CTRL_H`.

### Text Viewer (21 bytes)

`CTRL_TYPE` = `CTYPE_TEXTVIEW` (8). Read-only scrollable text display.

| Offset | Name | Size | Description |
|--------|------|------|-------------|
| 0-13 | (base) | 14 | Common control fields |
| 14 | `CTRL_TV_LINES` | WORD | Pointer to array of WORD line pointers |
| 16 | `CTRL_TV_COUNT` | BYTE | Total line count (max 255) |
| 17 | `CTRL_TV_CURLINE` | BYTE | Cursor line index |
| 18 | `CTRL_TV_SCROLL` | BYTE | First visible line index (vertical) |
| 19 | `CTRL_TV_SCROLLX` | BYTE | Horizontal scroll offset (chars) |
| 20 | `CTRL_TV_MAXLEN` | BYTE | Longest line length (set by `tui_tv_parse_text`) |

Auto-hiding vertical and horizontal scroll bars. Two-pass logic determines
which bars are needed based on content dimensions vs. control dimensions.

### Framework State (`fw_state`, 16 bytes reserved)

| Offset | Name | Size | Description |
|--------|------|------|-------------|
| 0 | `FW_NUMWIN` | BYTE | Number of active windows |
| 1 | `FW_TOPWIN` | BYTE | Slot index of topmost window (FFh = none) |
| 2 | `FW_MENUBAR` | WORD | Pointer to menu bar struct (0 = none) |
| 4 | `FW_RUNNING` | BYTE | 1 = event loop active |
| 5 | `FW_DIRTY` | BYTE | 1 = screen needs redraw |
| 6 | `FW_OPENDD` | WORD | Pointer to open dropdown control (0 = none) |
| 8 | `FW_OPENDD_ROW` | BYTE | Popup absolute row |
| 9 | `FW_OPENDD_COL` | BYTE | Popup absolute column |
| 10 | `FW_DLG_RESULT` | BYTE | Dialog return code |
| 11 | `FW_MODAL_WIN` | BYTE | Modal window slot (FFh = none) |

### Menu Bar Struct (8 bytes)

| Offset | Name | Size | Description |
|--------|------|------|-------------|
| 0 | `MBAR_COUNT` | BYTE | Number of top-level menu items |
| 1 | `MBAR_SEL` | BYTE | Selected item index (FFh = inactive) |
| 2 | `MBAR_OPEN` | BYTE | 1 = dropdown open, 0 = bar only |
| 3 | `MBAR_DDSEL` | BYTE | Selected dropdown entry (FFh = none) |
| 4 | `MBAR_ATTR` | BYTE | Normal bar attribute |
| 5 | `MBAR_SELATTR` | BYTE | Selected/highlight attribute |
| 6 | `MBAR_ITEMS` | WORD | Pointer to array of menu items |

Menu bar state machine: **inactive** (SEL=FFh) -> **bar-active** (SEL=0..N) ->
**dropdown-open** (OPEN=1, DDSEL=0..N).

### Menu Item Struct (10 bytes)

| Offset | Name | Size | Description |
|--------|------|------|-------------|
| 0 | `MI_TEXT` | WORD | Pointer to item label string |
| 2 | `MI_X` | BYTE | Column position on menu bar |
| 3 | `MI_W` | BYTE | Width of item on bar (including padding) |
| 4 | `MI_ENTRIES` | WORD | Pointer to dropdown entry array |
| 6 | `MI_ECOUNT` | BYTE | Number of dropdown entries |
| 7 | `MI_DDW` | BYTE | Dropdown interior width |
| 8 | `MI_HOTIDX` | BYTE | Index of hotkey char in `MI_TEXT` (FFh = none) |
| 9 | `MI_ALTKEY` | BYTE | Alt+key scan code that opens this dropdown (0 = none) |

### Menu Dropdown Entry (6 bytes)

| Offset | Name | Size | Description |
|--------|------|------|-------------|
| 0 | `MDE_TEXT` | WORD | Pointer to entry label string |
| 2 | `MDE_HANDLER` | WORD | Callback pointer (0 = no action) |
| 4 | `MDE_HOTIDX` | BYTE | Index of hotkey char in entry text (FFh = none) |
| 5 | `MDE_HOTKEY` | BYTE | ASCII char that activates this entry (0 = none) |

### Mouse State (`mouse_state`, 16 bytes)

| Offset | Name | Size | Description |
|--------|------|------|-------------|
| 0 | `MS_FLAGS` | BYTE | `MSF_*` flags bitmask |
| 1 | `MS_COL` | BYTE | Current cell column (0-79) |
| 2 | `MS_ROW` | BYTE | Current cell row (0-49) |
| 3 | `MS_BUTTONS` | BYTE | Current button state (1=left, 2=right) |
| 4 | `MS_PREV_BTN` | BYTE | Previous frame button state |
| 5 | `MS_DRAG_OX` | BYTE | Drag offset: click col - window X |
| 6 | `MS_DRAG_OY` | BYTE | Drag offset: click row - window Y |
| 7 | `MS_DRAG_WIN` | BYTE | Window slot being dragged (FFh = none) |
| 8 | `MS_PRESSED` | WORD | Pointer to control being mouse-pressed (0 = none) |
| 10 | `MS_PCOL` | BYTE | Previous cell column (for move detection) |
| 11 | `MS_PROW` | BYTE | Previous cell row |
| 12 | `MS_SB_CTRL` | WORD | Control being scroll-bar-dragged |
| 14 | `MS_SB_DIR` | BYTE | 0 = vertical, 1 = horizontal |
| 15 | `MS_SB_OFFSET` | BYTE | Click offset within thumb |

---

## Constants Reference

### Screen Dimensions

| Constant | Value | Description |
|----------|-------|-------------|
| `SCR_W` | 80 | Screen width in columns |
| `SCR_H` | 50 | Screen height in rows |
| `SCR_CELLS` | 4000 | Total cells (80 x 50) |
| `SCR_BYTES` | 8000 | Buffer size (4000 x 2 bytes/cell) |
| `VRAM_SEG` | B800h | Video RAM segment |
| `MAX_WINDOWS` | 16 | Maximum simultaneous windows |

### Window Flags (`WINF_*`)

| Flag | Value | Description |
|------|-------|-------------|
| `WINF_VISIBLE` | 01h | Window is drawn |
| `WINF_BORDER` | 02h | Draw border (single-line inactive, double-line active) |
| `WINF_TITLE` | 04h | Draw centered title on top border |
| `WINF_CLOSEBTN` | 08h | Top-left corner acts as close button |
| `WINF_MOVABLE` | 10h | Window can be moved (Ctrl+Arrows or mouse drag) |
| `WINF_SHADOW` | 20h | Draw drop shadow (right column + bottom row) |
| `WINF_MODAL` | 40h | Reserved (modality handled by `FW_MODAL_WIN`) |
| `WINF_RESIZABLE` | 80h | Window can be resized by dragging bottom-right corner |

### Window Minimums

| Constant | Value | Description |
|----------|-------|-------------|
| `WIN_MINW` | 8 | Minimum window width during resize |
| `WIN_MINH` | 4 | Minimum window height during resize |

### Control Types (`CTYPE_*`)

| Constant | Value | Description |
|----------|-------|-------------|
| `CTYPE_LABEL` | 1 | Static text label |
| `CTYPE_BUTTON` | 2 | Clickable button `[ text ]` |
| `CTYPE_TEXTBOX` | 3 | Editable single-line text input |
| `CTYPE_CHECKBOX` | 4 | Toggle checkbox `[X]` / `[ ]` |
| `CTYPE_RADIO` | 5 | Radio button `(*)` / `( )` |
| `CTYPE_DROPDOWN` | 6 | Combo box with popup list |
| `CTYPE_LISTBOX` | 7 | Scrolling selection list |
| `CTYPE_TEXTVIEW` | 8 | Read-only scrollable text viewer |

### Control Flags (`CTRLF_*`)

| Flag | Value | Description |
|------|-------|-------------|
| `CTRLF_VISIBLE` | 01h | Control is drawn |
| `CTRLF_ENABLED` | 02h | Control accepts input |
| `CTRLF_FOCUSABLE` | 04h | Control can receive focus via Tab |

### Color Palette

DOS text attributes: high nibble = background (0-7), low nibble = foreground (0-F).

#### Window Colors

| Constant | Value | Fg / Bg | Description |
|----------|-------|---------|-------------|
| `CLR_DESKTOP` | 17h | White on Blue | Desktop background fill |
| `CLR_WIN_BG` | 1Fh | Bright White on Blue | Window interior |
| `CLR_WIN_BDR` | 1Bh | Bright Cyan on Blue | Inactive window border |
| `CLR_WIN_BDR_ACT` | 1Eh | Yellow on Blue | Active window border |
| `CLR_WIN_TTL` | 1Eh | Yellow on Blue | Window title text |
| `CLR_SHADOW` | 08h | Dark Grey on Black | Drop shadow |

#### Control Colors

| Constant | Value | Fg / Bg | Description |
|----------|-------|---------|-------------|
| `CLR_LABEL` | 1Fh | Bright White on Blue | Label text |
| `CLR_BTN` | 70h | Black on Light Gray | Button (normal) |
| `CLR_BTN_FOCUS` | 2Fh | Bright White on Green | Button (focused) |
| `CLR_BTN_PRESSED` | 78h | Dark Gray on Light Gray | Button (mouse pressed) |
| `CLR_TEXTBOX` | 70h | Black on Light Gray | TextBox (unfocused) |
| `CLR_TB_FOCUS` | 30h | Black on Cyan | TextBox (focused) |
| `CLR_TB_CURSOR` | 0Fh | Bright White on Black | TextBox block cursor |
| `CLR_CHECKBOX` | 1Fh | Bright White on Blue | Checkbox (normal) |
| `CLR_CB_FOCUS` | 30h | Black on Cyan | Checkbox (focused) |
| `CLR_RADIO` | 1Fh | Bright White on Blue | Radio button (normal) |
| `CLR_RB_FOCUS` | 30h | Black on Cyan | Radio button (focused) |

#### Dropdown Colors

| Constant | Value | Description |
|----------|-------|-------------|
| `CLR_DD_NORMAL` | 70h | Closed dropdown (normal) |
| `CLR_DD_FOCUS` | 30h | Closed dropdown (focused) |
| `CLR_DD_POPUP` | 70h | Popup body + border |
| `CLR_DD_POPSEL` | 0Fh | Popup highlighted item |

#### Listbox Colors

| Constant | Value | Description |
|----------|-------|-------------|
| `CLR_LB_NORMAL` | 70h | Unfocused, unselected |
| `CLR_LB_FOCUS` | 30h | Focused, unselected |
| `CLR_LB_SEL` | 0Fh | Selected, unfocused |
| `CLR_LB_FOCSEL` | 3Eh | Selected + focused |

#### Text Viewer Colors

| Constant | Value | Description |
|----------|-------|-------------|
| `CLR_TV_NORMAL` | 70h | Unfocused, non-cursor line |
| `CLR_TV_FOCUS` | 30h | Focused, non-cursor line |
| `CLR_TV_CURLINE` | 0Fh | Cursor line, unfocused |
| `CLR_TV_CURFOCUS` | 3Eh | Cursor line + focused |

#### Menu Colors

| Constant | Value | Description |
|----------|-------|-------------|
| `CLR_MENUBAR` | 70h | Normal bar background |
| `CLR_MENU_SEL` | 0Fh | Selected bar item |
| `CLR_MENU_DD` | 70h | Dropdown body |
| `CLR_MENU_DDSEL` | 0Fh | Selected dropdown entry |
| `CLR_MENU_DDSHDW` | 08h | Dropdown shadow |
| `CLR_MENU_HOTKEY` | 74h | Bar hotkey letter (red on light gray) |
| `CLR_MENU_DD_HOTKEY` | 74h | Dropdown hotkey letter (red on light gray) |

#### Scroll Bar Colors

| Constant | Value | Description |
|----------|-------|-------------|
| `CLR_SB_ARROW` | 70h | Arrow buttons |
| `CLR_SB_TRACK` | 78h | Track background |
| `CLR_SB_THUMB` | 70h | Thumb indicator |

### Box-Drawing Characters (CP437)

#### Single Line

| Constant | Value | Glyph | Description |
|----------|-------|-------|-------------|
| `BOX_TL` | DAh | `+` | Top-left corner |
| `BOX_TR` | BFh | `+` | Top-right corner |
| `BOX_BL` | C0h | `+` | Bottom-left corner |
| `BOX_BR` | D9h | `+` | Bottom-right corner |
| `BOX_H` | C4h | `-` | Horizontal line |
| `BOX_V` | B3h | `|` | Vertical line |

#### Double Line

| Constant | Value | Description |
|----------|-------|-------------|
| `DBOX_TL` | C9h | Top-left corner |
| `DBOX_TR` | BBh | Top-right corner |
| `DBOX_BL` | C8h | Bottom-left corner |
| `DBOX_BR` | BCh | Bottom-right corner |
| `DBOX_H` | CDh | Horizontal line |
| `DBOX_V` | BAh | Vertical line |

#### Scroll Bar Characters

| Constant | Value | Description |
|----------|-------|-------------|
| `SB_ARROW_UP` | 18h | Up arrow |
| `SB_ARROW_DN` | 19h | Down arrow |
| `SB_ARROW_LT` | 1Bh | Left arrow |
| `SB_ARROW_RT` | 1Ah | Right arrow |
| `SB_THUMB` | DBh | Solid block (thumb) |
| `SB_TRACK` | B0h | Light shade (track) |

#### Miscellaneous

| Constant | Value | Description |
|----------|-------|-------------|
| `DD_ARROW` | 1Fh | Dropdown down-triangle |

### Key Codes

#### Normal Keys (returned in AL with AH=KTYPE_NORMAL)

| Constant | Value | Description |
|----------|-------|-------------|
| `KEY_TAB` | 09h | Tab |
| `KEY_BACKSPACE` | 08h | Backspace |
| `KEY_ENTER` | 0Dh | Enter/Return |
| `KEY_SPACE` | 20h | Space bar |
| `KEY_ESCAPE` | 1Bh | Escape |

#### Extended Keys (returned in AL with AH=KTYPE_EXTENDED)

| Constant | Value | Description |
|----------|-------|-------------|
| `KEY_UP` | 48h | Up arrow |
| `KEY_DOWN` | 50h | Down arrow |
| `KEY_LEFT` | 4Bh | Left arrow |
| `KEY_RIGHT` | 4Dh | Right arrow |
| `KEY_HOME` | 47h | Home |
| `KEY_END` | 4Fh | End |
| `KEY_PGUP` | 49h | Page Up |
| `KEY_PGDN` | 51h | Page Down |
| `KEY_DELETE` | 53h | Delete |
| `KEY_F6` | 40h | F6 (window cycle) |
| `KEY_F10` | 44h | F10 (menu activate) |
| `KEY_CTRL_LEFT` | 73h | Ctrl+Left (window move) |
| `KEY_CTRL_RIGHT` | 74h | Ctrl+Right (window move) |
| `KEY_CTRL_UP` | 8Dh | Ctrl+Up (window move) |
| `KEY_CTRL_DOWN` | 91h | Ctrl+Down (window move) |

### Mouse Flags (`MSF_*`)

| Flag | Value | Description |
|------|-------|-------------|
| `MSF_ENABLED` | 01h | Mouse driver detected |
| `MSF_DRAGGING` | 02h | Window drag in progress |
| `MSF_RESIZING` | 04h | Window resize in progress |
| `MSF_BTN_DOWN` | 08h | Left button currently held |
| `MSF_SB_DRAG` | 10h | Scroll bar thumb drag in progress |

### Hit Test Results (`HIT_*`)

| Constant | Value | Description |
|----------|-------|-------------|
| `HIT_NONE` | 0 | Desktop (no window) |
| `HIT_TITLEBAR` | 1 | Window title bar |
| `HIT_CLOSEBTN` | 2 | Close button (top-left corner) |
| `HIT_INTERIOR` | 3 | Window interior (no control) |
| `HIT_CONTROL` | 4 | A specific control |
| `HIT_BORDER` | 5 | Window border (not title/close) |
| `HIT_MENUBAR` | 6 | Menu bar item |
| `HIT_MENUDROP` | 7 | Menu dropdown entry |
| `HIT_DD_POPUP` | 8 | Combo dropdown popup item |
| `HIT_RESIZE` | 9 | Resize corner (bottom-right) |

### Dialog Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `DLG_OK` | 1 | Dialog result: OK/Yes |
| `DLG_CANCEL` | 0 | Dialog result: Cancel |
| `DLG_YES` | 1 | Dialog result: Yes |
| `DLG_NO` | 0 | Dialog result: No |
| `DLG_MIN_W` | 20 | Minimum dialog interior width |
| `DLG_PAD` | 4 | Horizontal padding (2 each side) |
| `DLG_BTN_OK_W` | 8 | Width of `[ OK ]` button |
| `DLG_BTN_YN_W` | 8 | Width of `[ Yes ]` / `[ No ]` buttons |
| `DLG_BTN_CAN_W` | 10 | Width of `[ Cancel ]` button |
| `DLG_BTN_GAP` | 2 | Gap between buttons |
| `DLG_FILE_MAXENT` | 32 | Max directory entries in file dialog |
| `DLG_FILE_NAMELEN` | 14 | Bytes per name slot (13 chars + backslash) |

---

## API Reference

### Video Module (`tui_video.inc`)

#### `tui_init`
Initialize the TUI framework. Sets 80x50 VGA text mode, hides hardware cursor,
zeroes all framework state, sets `FW_RUNNING=1`, clears shadow buffer.

- **Input:** none
- **Output:** none
- **Clobbers:** none (all registers saved/restored)

#### `tui_clear_shadow`
Fill `shadow_buf` with desktop background (space + `CLR_DESKTOP`).

- **Input:** none
- **Output:** none
- **Clobbers:** none (AX, CX, DI saved/restored)

#### `tui_blit`
Copy `shadow_buf` to VRAM (B800:0000) using `REP MOVSW`. Saves/restores ES.

- **Input:** none
- **Output:** none
- **Clobbers:** none (AX, CX, SI, DI, ES saved/restored)

#### `calc_vram_offset`
Calculate absolute address in `shadow_buf` for a given cell position.

- **Input:** DH = row, DL = col
- **Output:** DI = absolute address in shadow_buf
- **Clobbers:** none (BX saved/restored)

#### `tui_putchar`
Write a single character+attribute to shadow_buf. Clips to screen bounds.

- **Input:** AL = character, AH = attribute, DH = row, DL = col
- **Output:** none
- **Clobbers:** none (DI, BX saved/restored)
- **Note:** Does NOT clobber AL, AH, DH, DL

#### `tui_putstr`
Write a null-terminated string to shadow_buf. Clips at right screen edge.

- **Input:** SI = pointer to string, AH = attribute, DH = row, DL = col
- **Output:** SI advanced past the string
- **Clobbers:** none (DI, BX saved/restored)

#### `tui_hline`
Draw a horizontal line of a single character+attribute. Clips to screen.

- **Input:** AL = character, AH = attribute, DH = row, DL = col, CL = width
- **Output:** none
- **Clobbers:** none (DI, BX, CX saved/restored)
- **Note:** Does NOT clobber AL, AH, DH, DL

#### `tui_fill_rect`
Fill a rectangle with a single character+attribute. Clips to screen bounds.

- **Input:** AL = char, AH = attr, DH = row, DL = col, BH = height, BL = width
- **Output:** none
- **Clobbers:** none (all registers saved/restored via PushAll)
- **Note:** Does NOT clobber AL, AH, DH, DL

#### `tui_darken_cell`
Set a single cell's attribute to `CLR_SHADOW`, preserving the existing character.
Used for window drop shadows.

- **Input:** DH = row, DL = col
- **Output:** none
- **Clobbers:** none (DI, BX saved/restored)

#### `tui_darken_hline`
Set a horizontal line of cells to `CLR_SHADOW`, preserving existing characters.

- **Input:** DH = row, DL = col, CL = width
- **Output:** none
- **Clobbers:** none (DI, BX, CX saved/restored)

---

### Window Module (`tui_window.inc`)

#### `tui_win_create`
Create a new window from a template struct. Finds the first free slot in
`win_table`, copies the template, assigns a slot ID, adds to z-order as topmost.

- **Input:** SI = pointer to 20-byte window template struct
- **Output:** AL = window slot index (0-15), or FFh if table full
- **Side effects:** Increments `FW_NUMWIN`, sets `FW_TOPWIN`, sets `FW_DIRTY`
- **Clobbers:** none (BX, CX, DX, SI, DI saved/restored)

#### `tui_win_draw`
Draw a single window into `shadow_buf`. Draws shadow, border, interior fill,
title, and all controls. Active window gets double-line border; inactive gets
single-line. Resizable windows show resize indicator at bottom-right corner.

- **Input:** SI = pointer to window struct in `win_table`
- **Output:** none
- **Clobbers:** none (all saved via PushAll)

#### `tui_compose`
Full-screen recompose: clear shadow to desktop, draw all visible windows in
z-order (bottom to top), draw menu bar, menu dropdown, combo dropdown popup,
then clear `FW_DIRTY` flag. Called by `tui_run` when dirty.

- **Input:** none
- **Output:** none
- **Side effects:** Clears `FW_DIRTY`
- **Clobbers:** none (all saved via PushAll)

#### `tui_win_close`
Close a window by slot index. Clears the slot's flags, removes from z-order
array, updates `FW_NUMWIN` and `FW_TOPWIN`.

- **Input:** AL = window slot index
- **Output:** none
- **Side effects:** Decrements `FW_NUMWIN`, sets `FW_DIRTY`
- **Clobbers:** none (all saved via PushAll)

#### `tui_zorder_cycle`
Rotate the topmost window to the bottom of the z-order. All other windows
shift up by one. Closes any open dropdown popup.

- **Input:** none (operates on `z_order[]` and `fw_state`)
- **Output:** none
- **Side effects:** Updates `FW_TOPWIN`, sets `FW_DIRTY`, clears `FW_OPENDD`
- **Clobbers:** none (all saved via PushAll)

#### `tui_win_move`
Move the active (topmost) window by one cell in the given direction.
Respects screen bounds and menu bar (minimum Y=1 if menu exists).

- **Input:** AL = scan code (`KEY_UP` / `KEY_DOWN` / `KEY_LEFT` / `KEY_RIGHT`)
- **Output:** none
- **Side effects:** Sets `FW_DIRTY` if moved, clears `FW_OPENDD`
- **Clobbers:** none (all saved via PushAll)

---

### Control Module (`tui_control.inc`)

#### `tui_ctrl_draw_all`
Walk the control linked list for a window and draw each visible control.
Calculates absolute position from window interior origin + control relative position.

- **Input:** SI = window struct pointer
- **Output:** none
- **Clobbers:** none (all saved via PushAll)

#### `tui_ctrl_draw_label`
Draw a label control (static text).

- **Input:** BX = control pointer, DH = abs row, DL = abs col
- **Preserves:** BX, DI, DH, DL

#### `tui_ctrl_draw_button`
Draw a button control with `[ text ]` format. Centered text.
Three visual states: normal, focused, pressed (mouse).

- **Input:** BX = control pointer, DH = abs row, DL = abs col, CH = focused flag
- **Preserves:** BX, DI, DH, DL

#### `tui_ctrl_draw_textbox`
Draw a textbox with scrollable text content and block cursor when focused.

- **Input:** BX = control pointer, DH = abs row, DL = abs col, CH = focused flag
- **Preserves:** BX, DI, DH, DL

#### `tui_ctrl_draw_checkbox`
Draw a checkbox as `[X] Label` (checked) or `[ ] Label` (unchecked).

- **Input:** BX = control pointer, DH = abs row, DL = abs col, CH = focused flag
- **Preserves:** BX, DI, DH, DL

#### `tui_ctrl_draw_radio`
Draw a radio button as `(*) Label` (selected) or `( ) Label` (unselected).

- **Input:** BX = control pointer, DH = abs row, DL = abs col, CH = focused flag
- **Preserves:** BX, DI, DH, DL

#### `tui_ctrl_draw_dropdown`
Draw the closed-state dropdown showing the selected item text and down-arrow.

- **Input:** BX = control pointer, DH = abs row, DL = abs col, CH = focused flag
- **Preserves:** BX, DI, DH, DL

#### `tui_ctrl_draw_listbox`
Draw all visible rows of a listbox. Auto-draws vertical scroll bar when
`COUNT > CTRL_H`.

- **Input:** BX = control pointer, DH = abs row, DL = abs col, CH = focused flag
- **Preserves:** BX, DI, DH, DL

#### `tui_ctrl_draw_textview`
Draw all visible rows of a text viewer with auto-hiding vertical and horizontal
scroll bars. Two-pass logic determines bar visibility.

- **Input:** BX = control pointer, DH = abs row, DL = abs col, CH = focused flag
- **Preserves:** BX, DI, DH, DL

#### `tui_ctrl_focus_next`
Cycle focus to the next focusable+enabled control in the linked list. Wraps
around to the first control after reaching the end.

- **Input:** SI = window struct pointer
- **Output:** CF=0 if focus changed, CF=1 if no other focusable control
- **Side effects:** Updates `WIN_FOCUS`, sets `FW_DIRTY`

#### `tui_ctrl_activate`
Activate the currently focused control: call button handler, toggle checkbox,
select radio button, or call handler for listbox/textview.

- **Input:** SI = window struct pointer
- **Output:** none (may call handler via RET-trampoline)
- **Handler convention:** SI = control ptr, DI = window ptr

#### `tui_ctrl_handle_key`
Route a keypress to the focused control's key handler (TextBox, Dropdown,
Listbox, or TextViewer). Non-handled keys fall through.

- **Input:** AH = key type, AL = char/scan
- **Output:** CF=0 if handled, CF=1 if not handled
- **Clobbers:** none (all saved/restored)

#### `tui_radio_group_select`
Clear all radio buttons in the same group, then select the given one.

- **Input:** BX = radio control to select, SI = window struct pointer
- **Output:** none

#### TextBox Editing Functions

All take BX = TextBox control pointer.

| Function | Description |
|----------|-------------|
| `tui_tb_insert_char` | Insert AL at cursor position (shifts tail right) |
| `tui_tb_backspace` | Delete character before cursor |
| `tui_tb_delete` | Delete character at cursor position |
| `tui_tb_cursor_left` | Move cursor one position left |
| `tui_tb_cursor_right` | Move cursor one position right |
| `tui_tb_home` | Move cursor to beginning |
| `tui_tb_end` | Move cursor to end |
| `tui_tb_ensure_visible` | Adjust scroll so cursor is visible |

#### Dropdown Functions

| Function | Input | Description |
|----------|-------|-------------|
| `tui_dd_handle_key` | AH/AL=key, BX=ctrl, SI=win | Key handler for open/closed dropdown. CF=0/1 |
| `tui_dd_open` | BX=ctrl, SI=win | Open dropdown popup |
| `tui_dd_close_commit` | BX=ctrl | Close popup, copy TMPSEL to SEL |
| `tui_dd_close_cancel` | BX=ctrl | Close popup, discard changes |
| `tui_dd_ensure_visible` | BX=ctrl | Scroll adjustment for popup |
| `tui_dd_draw_popup` | (reads FW_OPENDD) | Draw popup overlay (called from compose) |

#### Listbox Functions

| Function | Input | Description |
|----------|-------|-------------|
| `tui_lb_handle_key` | AH/AL=key, BX=ctrl, SI=win | Arrow/Home/End/PgUp/PgDn navigation. CF=0/1 |
| `tui_lb_ensure_visible` | BX=ctrl | Scroll adjustment for listbox |

#### Text Viewer Functions

| Function | Input | Description |
|----------|-------|-------------|
| `tui_tv_handle_key` | AH/AL=key, BX=ctrl, SI=win | Arrow/Home/End/PgUp/PgDn/Left/Right navigation. CF=0/1 |
| `tui_tv_ensure_visible` | BX=ctrl | Vertical scroll adjustment |
| `tui_tv_parse_text` | SI=raw text, DI=ctrl | Parse CR/LF text into line pointer array. Sets COUNT, MAXLEN. Resets CURLINE, SCROLL, SCROLLX to 0. Clobbers: AX, BX, CX, SI. |

#### Scroll Bar Drawing

| Function | Input | Description |
|----------|-------|-------------|
| `tui_draw_vscrollbar` | DH/DL=pos, BL=height, AL=scroll, AH=visible, CL=total | Draw vertical scroll bar with arrows, track, thumb |
| `tui_draw_hscrollbar` | DH/DL=pos, BL=width, AL=scroll_x, AH=visible_w, CL=total_w | Draw horizontal scroll bar |

---

### Menu Module (`tui_menu.inc`)

#### `tui_menu_draw_bar`
Draw the menu bar on row 0. Fills row with bar attribute, then draws each
item label. Selected item gets highlight attribute. A second pass draws
hotkey letters in `CLR_MENU_HOTKEY` (skipping the selected item, where
selection color takes priority).

- **Input:** none (reads `FW_MENUBAR` from `fw_state`)
- **Output:** none
- **Clobbers:** none (all saved via PushAll)

#### `tui_menu_draw_dropdown`
Draw the open dropdown menu box with entries, border, and shadow. A hotkey
highlight pass draws each entry's hotkey letter in `CLR_MENU_DD_HOTKEY`
(skipping the selected entry, where selection color takes priority).

- **Input:** none (reads `FW_MENUBAR` from `fw_state`)
- **Output:** none
- **Clobbers:** none (all saved via PushAll)

#### `tui_menu_handle_key`
Key handler for the menu bar state machine. When inactive, intercepts F10 and
Alt+key shortcuts (`MI_ALTKEY`). When bar-active, handles Left/Right/Down/
Enter/Escape and Alt+key. When dropdown-open, handles Up/Down/Left/Right/
Enter/Escape, Alt+key to switch menus, and single-letter hotkeys (`MDE_HOTKEY`)
to activate entries directly.

- **Input:** AH = key type, AL = char/scan
- **Output:** CF=0 if handled (key consumed), CF=1 if not handled
- **Side effects:** May open/close dropdown, activate entry, set `FW_DIRTY`

#### `tui_menu_activate_entry`
Execute the selected dropdown entry's handler via RET-trampoline. Closes the
menu (resets SEL/OPEN/DDSEL to inactive state).

- **Input:** DI = menu bar struct pointer
- **Output:** none

#### Internal Helpers

| Function | Description |
|----------|-------------|
| `_mi_index_to_offset` | Convert menu item index (AL) to byte offset (AX = AL * MI_SIZE). Clobbers: AH |
| `_mde_index_to_offset` | Convert dropdown entry index (AL) to byte offset (AX = AL * MDE_SIZE). Clobbers: AH |
| `_mhk_check_alt_key` | Match scan code (AL) against menu items' `MI_ALTKEY` fields. CF=0 + BL=index on match, CF=1 if none |
| `_mhk_check_dd_hotkey` | Match ASCII char (AL, case-insensitive) against current dropdown's `MDE_HOTKEY` fields. CF=0 + BL=index on match, CF=1 if none |

---

### Mouse Module (`tui_mouse.inc`)

#### `tui_mouse_init`
Detect and configure the INT 33h mouse driver. Zeroes `mouse_state`, sets
horizontal range 0-639, vertical range 0-399.

- **Input:** none
- **Output:** Sets `MSF_ENABLED` flag if driver detected
- **Clobbers:** none (all saved via PushAll)

#### `tui_mouse_poll`
Read current mouse state from INT 33h. Converts pixel coordinates to cell
coordinates (divide by 8). Detects position and button changes.

- **Input:** none
- **Output:** AL = change flags (bit 0: position moved, bit 1: button changed)
- **Clobbers:** none (BX, CX, DX saved/restored)

#### `tui_mouse_draw_cursor`
Overlay the mouse cursor on `shadow_buf` by swapping the foreground and
background nibbles of the attribute byte at the cursor position.

- **Input:** none (reads `mouse_state`)
- **Output:** none
- **Clobbers:** none (AX, CX, DI, DX saved/restored)

#### `tui_mouse_dispatch`
Main mouse event dispatcher. Handles drag/resize continuation, scroll bar drag,
button press/release detection, and menu hover.

- **Input:** AL = change flags from `tui_mouse_poll`
- **Output:** none
- **Side effects:** Always sets `FW_DIRTY` (for cursor redraw)
- **Clobbers:** none (all saved via PushAll)

#### `tui_mouse_hit_test`
Determine what UI element is under a given screen position. Checks in order:
combo dropdown popup, menu dropdown, menu bar, then windows (top to bottom).

- **Input:** DH = row, DL = col
- **Output:** AL = hit type (`HIT_*`), SI = window struct ptr (if window hit),
  BX = context (control ptr or item index)
- **Clobbers:** none (CX, DI saved/restored internally)

#### `tui_mouse_on_press`
Handle left-button-down event. Dispatches based on hit test result:
- **Titlebar:** bring to front + start drag
- **Close button:** close window
- **Interior:** bring to front + find control + dispatch by control type
- **Resize corner:** bring to front + start resize
- **Menu bar:** open dropdown
- **Menu dropdown entry:** activate entry
- **Combo popup item:** select + close popup
- **Border:** bring to front

Modal enforcement: blocks all clicks outside the modal dialog window.

#### `tui_mouse_on_release`
Handle left-button-up. If a button was in pressed state and the cursor is still
over it, activates the button's handler.

#### `tui_mouse_on_drag`
Update window position during drag. New position = cursor - offset. Clamps to
screen bounds and respects menu bar minimum Y.

#### `tui_mouse_on_resize`
Update window dimensions during resize drag. New size = cursor - window origin + 1.
Clamps to minimum dimensions (`WIN_MINW` x `WIN_MINH`) and screen bounds.

#### `tui_mouse_bring_to_front`
Reorder `z_order[]` to make a window topmost. Shifts other windows down.

- **Input:** AL = window slot index
- **Side effects:** Updates `FW_TOPWIN`, sets `FW_DIRTY`, clears `FW_OPENDD`

#### `tui_mouse_hit_test_control`
Find the control at a given screen position within a window.

- **Input:** SI = window struct, DH = abs row, DL = abs col
- **Output:** BX = control pointer (0 if none found)
- **Clobbers:** AX

#### `tui_mouse_call_handler`
Call a control's handler via the RET-trampoline pattern.

- **Input:** BX = control ptr, SI = window struct ptr
- **Handler receives:** SI = control struct, DI = window struct

#### `tui_mouse_close_menu`
Close the menu bar if active (reset SEL/OPEN/DDSEL to inactive).

#### `tui_mouse_menu_hover`
Handle mouse hover when menu is open. If cursor moves to a different menu bar
item, switches the dropdown. If cursor moves over a dropdown entry, highlights it.

#### Scroll Bar Mouse Functions

| Function | Description |
|----------|-------------|
| `tui_sb_vbar_click_generic` | Handle click on vertical scroll bar track/thumb. Returns new scroll value. |
| `tui_sb_hbar_click_generic` | Handle click on horizontal scroll bar track/thumb. Returns new scroll value. |
| `tui_mouse_on_sb_drag` | Update scroll position during scroll bar thumb drag. |

---

### Event Module (`tui_event.inc`)

#### `tui_poll_key`
Non-blocking keyboard poll using INT 21h function 06h.

- **Input:** none
- **Output:** ZF=1 if no key available. Otherwise: AH = `KTYPE_NORMAL` (0) or
  `KTYPE_EXTENDED` (1), AL = character or scan code
- **Clobbers:** AX (return value)

#### `tui_dispatch_key`
Handle a keypress. Dispatch order:
1. Modal dialog dispatch (if modal active)
2. Menu handler (consumes all keys when menu active)
3. Control handler (TextBox, Dropdown, Listbox, TextViewer)
4. Tab -> focus cycle (then window cycle if only 1 focusable)
5. Enter/Space -> activate focused control
6. Ctrl+Arrows -> move window
7. F6 -> cycle windows
8. q/Q/Escape -> quit (`FW_RUNNING = 0`)

- **Input:** AH = key type, AL = char/scan
- **Output:** May set `FW_RUNNING` to 0

#### `tui_modal_dispatch`
Key dispatch for modal dialog mode. Only allows: control key handling, Tab
(focus cycle within dialog only), Enter/Space (activate), Escape (cancel + close).
Blocks q/Q quit, menu access, and window cycling.

#### `tui_run`
Main event loop. Performs initial compose+blit, then loops: poll keyboard,
dispatch key, poll mouse, dispatch mouse, recompose if dirty. Exits when
`FW_RUNNING` becomes 0.

- **Input:** none (framework must be initialized, windows created)
- **Output:** none (returns when `FW_RUNNING` == 0)

---

### Dialog Module (`tui_dialog.inc`)

#### `tui_dlg_msgbox`
Display a message box with an OK button. Blocks until OK or Escape.

- **Input:** SI = message string ptr, DI = title string ptr
- **Output:** none
- **Behavior:** Creates a modal dialog window, runs a nested event loop

#### `tui_dlg_confirm`
Display a Yes/No confirmation dialog.

- **Input:** SI = message string ptr, DI = title string ptr
- **Output:** AL = `DLG_YES` (1) or `DLG_NO` (0)

#### `tui_dlg_input`
Display a text input dialog with a prompt, textbox, OK, and Cancel buttons.

- **Input:** SI = prompt string ptr, DI = title string ptr,
  BX = text buffer ptr, CL = max length
- **Output:** AL = `DLG_OK` (1) or `DLG_CANCEL` (0).
  Buffer at BX contains entered text if `DLG_OK`.

#### `tui_dlg_input2`
Display a dual text input dialog with two labeled fields, OK, and Cancel buttons.
Useful for dialogs that need two related inputs (e.g., Find and Replace).

Before calling, the caller must set three memory variables for the second field:
- `[dlg_save_prompt2]` = second prompt string pointer
- `[dlg_save_buf2]` = second text buffer pointer
- `[dlg_save_maxlen2]` = second max length

- **Input:** SI = prompt1 string ptr, DI = title string ptr,
  BX = buffer1 ptr, CL = maxlen1
- **Output:** AL = `DLG_OK` (1) or `DLG_CANCEL` (0).
  Both buffers contain entered text if `DLG_OK`.

**Interior layout** (7 rows, WIN_H=9 with border):
```
Row 0: Label 1 (prompt1)
Row 1: Textbox 1
Row 2: (separator)
Row 3: Label 2 (prompt2)
Row 4: Textbox 2
Row 5: (separator)
Row 6: [ OK ]  [ Cancel ]
```

**Tab order:** textbox1 → textbox2 → OK → Cancel (labels are not focusable).
Initial focus is on textbox1. Pre-populated buffers display existing text with
cursor positioned at the end.

#### `tui_dlg_file`
Display a file selector dialog with directory listing, path display, OK, and
Cancel buttons. Supports directory navigation (double-click or Enter on
directory entries). Always includes `..\ ` for parent navigation.

- **Input:** DI = title string ptr, BX = result buffer ptr, CL = max filename length
- **Output:** AL = `DLG_OK` (1) or `DLG_CANCEL` (0).
  Buffer at BX contains selected filename if `DLG_OK`.
  CWD is left at the directory where the file was selected.

#### `tui_dlg_run_modal`
Internal: create dialog window from `dlg_tmpl`, set modal state, run nested
event loop, restore previous state on return.

- **Input:** `dlg_tmpl` and control structs filled
- **Output:** AL = `FW_DLG_RESULT`

#### Helper Functions

| Function | Description |
|----------|-------------|
| `tui_dlg_strlen` | Measure null-terminated string. Input: SI. Output: CX = length |
| `tui_file_get_path` | Get CWD into `dlg_file_path` (via INT 21h/47h) |
| `tui_file_enumerate` | List directory into `dlg_file_*` buffers (via INT 21h/4Eh-4Fh) |
| `dlg_file_handle_sel` | Process listbox selection: chdir if directory, close if file |
| `dlg_file_lb_handler` | Listbox Enter/click handler (calls `dlg_file_handle_sel`) |
| `dlg_file_ok_handler` | OK button handler (reads listbox selection) |
| `dlg_handler_ok` | Button handler: set result=DLG_OK, close dialog |
| `dlg_handler_cancel` | Button handler: set result=DLG_CANCEL, close dialog |
| `dlg_handler_yes` | Button handler: set result=DLG_YES, close dialog |
| `dlg_handler_no` | Button handler: set result=DLG_NO, close dialog |

---

### Data Module (`tui_data.inc`)

Reserves all framework data areas:

| Symbol | Size | Description |
|--------|------|-------------|
| `row_offsets` | 100 bytes | 50-entry WORD lookup table (row * 160) |
| `shadow_buf` | 8000 bytes | Shadow VRAM buffer |
| `fw_state` | 16 bytes | Framework global state |
| `win_table` | 320 bytes | 16 windows x 20 bytes |
| `z_order` | 16 bytes | Z-order array |
| `bdr_scratch` | 7 bytes | Border scratch (6 box chars + 1 attr) |
| `ctrl_dispatch_addr` | 2 bytes | RET-trampoline temp word |
| `mouse_state` | 16 bytes | Mouse state struct |
| `sb_scratch` | 4 bytes | Scroll bar scratch |
| `dlg_tmpl` | 20 bytes | Dialog window template |
| `dlg_label` | 14 bytes | Dialog label control |
| `dlg_btn1` | 14 bytes | Dialog OK/Yes button |
| `dlg_btn2` | 14 bytes | Dialog Cancel/No button |
| `dlg_textbox` | 18 bytes | Dialog textbox (input dialog) |
| `dlg_label2` | 14 bytes | Dialog second label (input2 dialog) |
| `dlg_textbox2` | 18 bytes | Dialog second textbox (input2 dialog) |
| `dlg_save_prompt2` | 2 bytes | Second prompt string pointer (input2 dialog) |
| `dlg_save_buf2` | 2 bytes | Second buffer pointer (input2 dialog) |
| `dlg_save_maxlen2` | 1 byte | Second max length (input2 dialog) |
| `dlg_listbox` | 19 bytes | Dialog listbox (file dialog) |
| `dlg_file_dta` | 43 bytes | DOS Disk Transfer Area |
| `dlg_file_path` | 65 bytes | CWD display buffer |
| `dlg_file_count` | 1 byte | Directory entry count |
| `dlg_file_items` | 64 bytes | Item pointer array (32 x WORD) |
| `dlg_file_names` | 448 bytes | Name storage (32 x 14 bytes) |

---

## Register Conventions

| Register | Convention |
|----------|-----------|
| SI | "this" pointer to structs (window struct in draw functions, control struct in handlers) |
| DH / DL | Row / column for drawing functions |
| AL / AH | Character / attribute for drawing functions |
| BL / BH | Width / height in `tui_fill_rect`; general scratch otherwise |
| CL | Width in `tui_hline`; loop counter |
| BX | Control pointer in draw/handle functions |
| DI | Window struct pointer in handler callbacks; menu bar pointer in menu functions |
| BX, SI, DI, BP | **Callee-saved** (must be preserved across calls) |
| AX, CX, DX | **Scratch** (may be clobbered by any function) |

**Handler callback convention:** When a control handler is invoked via
RET-trampoline, it receives `SI` = control struct pointer, `DI` = window
struct pointer.

---

## Color Attribute Format

DOS text-mode attributes are a single byte per cell:

```
  Bit 7  6  5  4  3  2  1  0
      B  b  b  b  I  f  f  f

  B     = blink (if enabled) or high-intensity background
  bbb   = background color (0-7)
  I     = intensity (0=normal, 1=bright foreground)
  fff   = foreground color (0-7)
```

### The 16-Color Palette

| Value | Color |
|-------|-------|
| 0 | Black |
| 1 | Blue |
| 2 | Green |
| 3 | Cyan |
| 4 | Red |
| 5 | Magenta |
| 6 | Brown |
| 7 | Light Gray |
| 8 | Dark Gray (bright black) |
| 9 | Light Blue |
| A | Light Green |
| B | Light Cyan |
| C | Light Red |
| D | Light Magenta |
| E | Yellow |
| F | Bright White |

Foreground uses all 16 values (0-F). Background uses only 0-7 (the low 3 bits
of the high nibble). The TUI framework does not use blink mode.

### Example

`CLR_WIN_BDR_ACT = 1Eh` means:
- High nibble `1` = background Blue (1)
- Low nibble `E` = foreground Yellow (E = bright intensity + brown)
- Result: Yellow text on Blue background
