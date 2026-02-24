# TUI Framework - Getting Started Guide

A step-by-step tutorial for building DOS text-mode user interfaces with the
TUI framework and the agent86 assembler.

---

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Hello World](#hello-world)
3. [Adding Controls](#adding-controls)
4. [Control Linked Lists](#control-linked-lists)
5. [Handling Input](#handling-input)
6. [Adding a Menu Bar](#adding-a-menu-bar)
7. [Mouse Support](#mouse-support)
8. [Listbox and Text Viewer](#listbox-and-text-viewer)
9. [Standard Dialogs](#standard-dialogs)
10. [Window Flags Reference](#window-flags-reference)
11. [Common Patterns](#common-patterns)
12. [Troubleshooting](#troubleshooting)

---

## Prerequisites

- **agent86 assembler** — the custom x86 assembler used for this project
- **DOS environment** — DOSBox-X or similar (VGA-capable, 80x50 text mode)
- **.COM memory model** — programs start at offset 100h, ES=DS=CS=SS

Programs are built with:
```
agent86.exe --run-source program.asm
```

For screenshots during development:
```
agent86.exe --screenshot output.bmp --font 8x8 --run-source program.asm --input "\x1b"
```

---

## Hello World

The minimal TUI program: initialize the framework, create a window, run the
event loop, and exit cleanly.

```asm
ORG 100h

    JMP main

; Include the entire TUI framework
INCLUDE tui.inc

; Application data
str_title:  DB 'Hello TUI', 0

; Window template (20 bytes matching WIN_* struct layout)
temp_win:
    DB WINF_VISIBLE + WINF_BORDER + WINF_TITLE + WINF_SHADOW
    DB 10                   ; WIN_X: column 10
    DB 5                    ; WIN_Y: row 5
    DB 40                   ; WIN_W: 40 columns wide
    DB 15                   ; WIN_H: 15 rows tall
    DB CLR_WIN_BG           ; WIN_ATTR: interior
    DB CLR_WIN_BDR          ; WIN_BATTR: border
    DB CLR_WIN_TTL          ; WIN_TATTR: title
    DW str_title            ; WIN_TITLE: pointer to title string
    DW 0                    ; WIN_FIRST: no controls
    DW 0                    ; WIN_FOCUS: no focused control
    DW 0                    ; WIN_HANDLER: no handler
    DB 0                    ; WIN_ZORDER: assigned by create
    DB 0                    ; WIN_ID: assigned by create
    DB 0                    ; WIN_SCROLLX
    DB 0                    ; WIN_SCROLLY

main:
    CALL tui_init           ; Initialize framework (sets 80x50 mode)
    MOV SI, temp_win
    CALL tui_win_create     ; Create the window (AL = slot index)
    CALL tui_run            ; Enter event loop (blocks until quit)
    INT 20h                 ; Clean exit to DOS
```

The framework does not provide a built-in quit key. Applications must handle
exit themselves (e.g., via a menu entry or control handler that sets
`FW_RUNNING = 0`). To add a simple quit mechanism, handle the key in a
window handler or control handler.

### What Happens

1. `tui_init` sets VGA to 80x50 text mode, clears the shadow buffer with a
   blue desktop, and sets `FW_RUNNING = 1`.
2. `tui_win_create` copies the 20-byte template into the first free slot of
   `win_table`, assigns an ID, and adds it to the z-order.
3. `tui_run` composes the screen (drawing the desktop + window), blits to VRAM,
   then enters a loop: poll keyboard, poll mouse, redraw if dirty.

---

## Adding Controls

Controls are defined as static data structures and linked together in a chain.
Each control has a common 14-byte header, with type-specific fields appended.

### Label + Button Example

```asm
; Strings
str_prompt: DB 'Click the button:', 0
str_ok:     DB 'OK', 0

; Label control (14 bytes)
ctrl_label:
    DB CTYPE_LABEL              ; CTRL_TYPE
    DB CTRLF_VISIBLE            ; CTRL_FLAGS (not focusable)
    DB 1                        ; CTRL_X (relative to window interior)
    DB 1                        ; CTRL_Y
    DB 17                       ; CTRL_W (text width)
    DB 1                        ; CTRL_H
    DW ctrl_button              ; CTRL_NEXT -> next control
    DW str_prompt               ; CTRL_TEXT
    DB CLR_LABEL                ; CTRL_ATTR
    DB CLR_LABEL                ; CTRL_FATTR (unused for labels)
    DW 0                        ; CTRL_HANDLER (unused for labels)

; Button control (14 bytes)
ctrl_button:
    DB CTYPE_BUTTON             ; CTRL_TYPE
    DB CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    DB 5                        ; CTRL_X
    DB 3                        ; CTRL_Y
    DB 10                       ; CTRL_W (total button width including brackets)
    DB 1                        ; CTRL_H
    DW 0                        ; CTRL_NEXT (end of list)
    DW str_ok                   ; CTRL_TEXT
    DB CLR_BTN                  ; CTRL_ATTR (normal: black on light gray)
    DB CLR_BTN_FOCUS            ; CTRL_FATTR (focused: white on green)
    DW my_handler               ; CTRL_HANDLER (callback address)
```

Wire the controls into the window template:
```asm
temp_win:
    DB WINF_VISIBLE + WINF_BORDER + WINF_TITLE + WINF_SHADOW
    DB 15, 10, 30, 8            ; X, Y, W, H
    DB CLR_WIN_BG, CLR_WIN_BDR, CLR_WIN_TTL
    DW str_title                ; WIN_TITLE
    DW ctrl_label               ; WIN_FIRST -> head of control list
    DW ctrl_button              ; WIN_FOCUS -> initially focused control
    DW 0, 0, 0                  ; handler, zorder/id, scroll
```

### TextBox Example

TextBox needs a buffer and 4 extra bytes (18 bytes total):

```asm
str_input:  DB 'Type here', 0
tb_buffer:  RESB 32                 ; editable text buffer
            DB 0                    ; null terminator

ctrl_textbox:
    DB CTYPE_TEXTBOX
    DB CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    DB 1, 2, 20, 1                  ; X, Y, W, H
    DW 0                            ; CTRL_NEXT
    DW tb_buffer                    ; CTRL_TEXT -> editable buffer
    DB CLR_TEXTBOX                  ; CTRL_ATTR
    DB CLR_TB_FOCUS                 ; CTRL_FATTR
    DW 0                            ; CTRL_HANDLER
    DB 31                           ; CTRL_TB_MAXLEN (buffer capacity)
    DB 0                            ; CTRL_TB_CURPOS
    DB 0                            ; CTRL_TB_SCROLL
    DB 0                            ; CTRL_TB_LEN (current text length)
```

TextBox supports: typing printable characters, Backspace, Delete, Home, End,
Left/Right arrow keys, and horizontal scrolling when text exceeds the visible width.

---

## Control Linked Lists

Controls form a singly-linked list via the `CTRL_NEXT` field (offset 6, WORD).
The window's `WIN_FIRST` points to the head.

```
Window
  WIN_FIRST -> [Label] -> [TextBox] -> [Button] -> NULL (0)
  WIN_FOCUS -> [TextBox]  (the currently focused control)
```

- Set `CTRL_NEXT` of each control to point to the next one
- Set the last control's `CTRL_NEXT` to 0
- Set `WIN_FIRST` to the first control in the chain
- Set `WIN_FOCUS` to whichever control should start with focus

Tab key cycles focus through all controls with `CTRLF_FOCUSABLE + CTRLF_ENABLED`
flags set. Labels are typically not focusable.

---

## Handling Input

### Keyboard Dispatch Order

The event loop (`tui_run`) polls for keys and dispatches them in this order:

1. **Menu handler** — if a menu is active, it consumes all keys
2. **Control handler** — TextBox handles printable chars + editing keys;
   Dropdown, Listbox, TextViewer handle arrows/Home/End/PgUp/PgDn;
   Editor handles typing, selection, clipboard, find/replace, undo/redo
3. **Global keys:**
   - Tab → focus cycle (within window, then cycle windows)
   - F6 → cycle window z-order
   - Enter/Space → activate focused control
   - Ctrl+Arrows → move active window

### Handler Callbacks

When a button is pressed (Enter/Space while focused, or mouse click), the
framework calls its `CTRL_HANDLER` if non-zero. Since agent86 does not support
`CALL reg`, an indirect call is done via a **RET-trampoline**:

```asm
; Framework does internally:
MOV WORD [ctrl_dispatch_addr], handler_address
MOV AX, return_label
PUSH AX                     ; push return address
MOV AX, [ctrl_dispatch_addr]
PUSH AX                     ; push handler address
RET                         ; pops handler addr into IP
```

Your handler receives:
- `SI` = pointer to the control struct
- `DI` = pointer to the window struct

Your handler simply ends with `RET` to return to the framework.

```asm
my_handler:
    ; SI = control struct, DI = window struct
    ; Do something (e.g., close window, set a flag, etc.)
    MOV BYTE [fw_state + FW_RUNNING], 0   ; quit the app
    RET
```

---

## Adding a Menu Bar

The menu bar is a separate layer on row 0, not a window. You define the structs
in your data section and set `FW_MENUBAR` to point to the bar struct.

### Menu Data Structures

```asm
; Dropdown entry handlers
file_new_handler:
    ; ... your code ...
    RET

file_quit_handler:
    MOV BYTE [fw_state + FW_RUNNING], 0
    RET

help_about_handler:
    ; ... show about dialog ...
    RET

; Dropdown entry strings
str_new:    DB 'New', 0
str_open:   DB 'Open', 0
str_quit:   DB 'Quit', 0
str_about:  DB 'About', 0

; Menu item strings
str_file:   DB 'File', 0
str_help:   DB 'Help', 0

; Accelerator display strings (optional)
str_acc_ctrl_n: DB 'Ctrl+N', 0
str_acc_alt_q:  DB 'Alt+Q', 0

; Dropdown entries (10 bytes each: text + handler + hotkey + accel)
file_entries:
    DW str_new,  file_new_handler       ; entry 0: text + handler
    DB 0, 'N'                           ; MDE_HOTIDX=0 ('N'), MDE_HOTKEY='N'
    DW str_acc_ctrl_n                   ; MDE_ACCEL: right-aligned display text
    DB 0, 0                             ; MDE_ACCELKEY=0 (display only), MDE_PAD
    DW str_open, 0                      ; entry 1 (no handler)
    DB 0, 'O'                           ; MDE_HOTIDX=0 ('O'), MDE_HOTKEY='O'
    DW 0                                ; MDE_ACCEL: no accelerator
    DB 0, 0                             ; MDE_ACCELKEY, MDE_PAD
    DW str_quit, file_quit_handler      ; entry 2
    DB 0, 'Q'                           ; MDE_HOTIDX=0 ('Q'), MDE_HOTKEY='Q'
    DW str_acc_alt_q                    ; MDE_ACCEL: "Alt+Q"
    DB 10h, 0                           ; MDE_ACCELKEY=10h (Alt+Q scan), MDE_PAD

help_entries:
    DW str_about, help_about_handler    ; entry 0
    DB 0, 'A'                           ; MDE_HOTIDX=0 ('A'), MDE_HOTKEY='A'
    DW 0                                ; MDE_ACCEL: no accelerator
    DB 0, 0                             ; MDE_ACCELKEY, MDE_PAD

; Menu items (10 bytes each)
menu_items:
    ; File menu
    DW str_file         ; MI_TEXT
    DB 0                ; MI_X (column on bar)
    DB 6                ; MI_W (width on bar)
    DW file_entries     ; MI_ENTRIES
    DB 3                ; MI_ECOUNT
    DB 8                ; MI_DDW (dropdown interior width)
    DB 0, 21h           ; MI_HOTIDX=0 ('F'), MI_ALTKEY=21h (Alt+F)
    ; Help menu
    DW str_help         ; MI_TEXT
    DB 6                ; MI_X
    DB 6                ; MI_W
    DW help_entries     ; MI_ENTRIES
    DB 1                ; MI_ECOUNT
    DB 8                ; MI_DDW
    DB 0, 23h           ; MI_HOTIDX=0 ('H'), MI_ALTKEY=23h (Alt+H)

; Menu bar struct (8 bytes)
my_menubar:
    DB 2                ; MBAR_COUNT (2 top-level items)
    DB 0FFh             ; MBAR_SEL (FFh = inactive)
    DB 0                ; MBAR_OPEN
    DB 0FFh             ; MBAR_DDSEL
    DB CLR_MENUBAR      ; MBAR_ATTR
    DB CLR_MENU_SEL     ; MBAR_SELATTR
    DW menu_items       ; MBAR_ITEMS
```

### Activating the Menu Bar

After `tui_init`, set the menu bar pointer:
```asm
    CALL tui_init
    MOV WORD [fw_state + FW_MENUBAR], my_menubar
```

Keyboard: **F10** activates the menu bar, arrow keys navigate, **Enter** opens
a dropdown or activates an entry, **Escape** closes. **Alt+key** shortcuts
(defined in `MI_ALTKEY`) open dropdowns directly from any state. When a dropdown
is open, pressing a hotkey letter (defined in `MDE_HOTKEY`) activates the
matching entry immediately (case-insensitive).

**Accelerator keys** (defined in `MDE_ACCELKEY`) are global shortcuts that
trigger an entry's handler without opening the menu. These are dispatched when
the menu is inactive — for example, `Alt+X` can trigger Exit directly. Set
`MDE_ACCELKEY` to the Alt+key scan code for global dispatch, or 0 for
display-only accelerators (where the shortcut is handled elsewhere, e.g.,
`Ctrl+S` in an editor control). The `MDE_ACCEL` field points to a display
string (e.g., `"Ctrl+S"`, `"Alt+X"`) that is rendered right-aligned in the
dropdown entry. Set to 0 for no display.

Mouse: Click on a menu bar item to open its dropdown, click an entry to activate.

---

## Mouse Support

### Enabling Mouse

Call `tui_mouse_init` after `tui_init`:
```asm
    CALL tui_init
    CALL tui_mouse_init     ; detect INT 33h driver, set ranges
```

The mouse cursor is drawn as an attribute-inverted cell (foreground and
background colors are swapped). It's overlaid on the shadow buffer after
compositing, before blitting.

### What Mouse Does Automatically

Once initialized, the framework handles:

- **Click on window titlebar** — bring to front + start drag
- **Click on close button** — close window (top-left corner if `WINF_CLOSEBTN`)
- **Click on window interior** — bring to front + set focus to clicked control
- **Click on resize corner** — start resize (bottom-right if `WINF_RESIZABLE`)
- **Click on button** — press visual feedback on mouse-down, activate on mouse-up
- **Click on checkbox** — toggle state
- **Click on radio button** — select (deselect others in group)
- **Click on textbox** — position cursor at clicked column
- **Click on dropdown** — open popup
- **Click on dropdown popup item** — select and close
- **Click on listbox item** — select item
- **Click on listbox scroll bar** — scroll (arrows, page, thumb drag)
- **Click on text viewer** — set cursor line
- **Click on text viewer scroll bars** — scroll (V and H bars)
- **Click on menu bar** — open dropdown
- **Click on menu entry** — activate handler
- **Hover over menu** — switch dropdowns, highlight entries
- **Modal enforcement** — clicks outside modal dialog are blocked

---

## Listbox and Text Viewer

### Listbox

A scrolling list with selection highlighting and auto-hiding vertical scroll bar.

```asm
; Item strings
str_item0: DB 'Apple', 0
str_item1: DB 'Banana', 0
str_item2: DB 'Cherry', 0

; Item pointer array (array of WORD pointers to strings)
lb_items:
    DW str_item0, str_item1, str_item2

; Listbox control (19 bytes)
ctrl_listbox:
    DB CTYPE_LISTBOX
    DB CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    DB 1, 1, 20, 5                  ; X, Y, W (including scroll bar), H (visible rows)
    DW 0                            ; CTRL_NEXT
    DW 0                            ; CTRL_TEXT (unused)
    DB CLR_LB_NORMAL                ; CTRL_ATTR
    DB CLR_LB_FOCUS                 ; CTRL_FATTR
    DW my_lb_handler                ; CTRL_HANDLER (called on Enter/Space/click)
    DW lb_items                     ; CTRL_LB_ITEMS
    DB 3                            ; CTRL_LB_COUNT
    DB 0                            ; CTRL_LB_SEL (initially first item)
    DB 0                            ; CTRL_LB_SCROLL
```

Keyboard: Up/Down, Home/End, PgUp/PgDn to navigate. Enter/Space to activate.
The scroll bar appears automatically when item count exceeds visible height.

### Text Viewer

A read-only scrollable text display with cursor line highlighting and auto-hiding
vertical + horizontal scroll bars.

```asm
; Line strings (can be parsed from raw text, or set up manually)
str_line0: DB 'Line one of the document', 0
str_line1: DB 'Line two with more text here', 0
str_line2: DB 'Line three', 0

; Line pointer array
tv_lines:
    DW str_line0, str_line1, str_line2

; Text viewer control (21 bytes)
ctrl_textview:
    DB CTYPE_TEXTVIEW
    DB CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    DB 1, 1, 30, 10                 ; X, Y, W (including scroll bars), H
    DW 0                            ; CTRL_NEXT
    DW 0                            ; CTRL_TEXT (unused)
    DB CLR_TV_NORMAL                ; CTRL_ATTR
    DB CLR_TV_FOCUS                 ; CTRL_FATTR
    DW 0                            ; CTRL_HANDLER
    DW tv_lines                     ; CTRL_TV_LINES
    DB 3                            ; CTRL_TV_COUNT
    DB 0                            ; CTRL_TV_CURLINE
    DB 0                            ; CTRL_TV_SCROLL
    DB 0                            ; CTRL_TV_SCROLLX
    DB 28                           ; CTRL_TV_MAXLEN (longest line length)
```

#### Parsing Raw Text

Instead of manually setting up line pointers, you can parse a raw text buffer
with CR/LF line endings:

```asm
; Raw text buffer
raw_text: DB 'First line', 0Dh, 0Ah
          DB 'Second line', 0Dh, 0Ah
          DB 'Third line', 0

; Line pointer array (sized for max expected lines)
tv_lines: RESW 64                    ; room for 64 line pointers

; After creating the textview control:
    MOV SI, raw_text                 ; SI = raw text buffer
    MOV DI, ctrl_textview            ; DI = textview control struct
    CALL tui_tv_parse_text           ; fills LINES, COUNT, MAXLEN
```

`tui_tv_parse_text` replaces CR/LF with null bytes in-place, so the raw buffer
is modified. It sets `CTRL_TV_COUNT`, `CTRL_TV_MAXLEN`, and resets cursor/scroll
to 0.

Keyboard: Up/Down, Home/End, PgUp/PgDn for vertical navigation. Left/Right for
horizontal scrolling.

---

## Standard Dialogs

The framework provides four ready-made modal dialogs. Each creates a window,
runs a nested event loop, and returns a result.

### Message Box

```asm
    MOV SI, str_message         ; "Operation complete!"
    MOV DI, str_title           ; "Info"
    CALL tui_dlg_msgbox         ; blocks until OK or Esc
```

### Confirmation Dialog

```asm
    MOV SI, str_question        ; "Delete this file?"
    MOV DI, str_title           ; "Confirm"
    CALL tui_dlg_confirm        ; blocks until Yes/No/Esc
    CMP AL, DLG_YES
    JZ .do_delete
    ; user chose No or Esc
```

### Input Dialog

```asm
input_buf: RESB 32
           DB 0

    MOV SI, str_prompt          ; "Enter filename:"
    MOV DI, str_title           ; "Input"
    MOV BX, input_buf           ; buffer for entered text
    MOV CL, 31                  ; max length
    CALL tui_dlg_input
    CMP AL, DLG_OK
    JZ .use_input
    ; user cancelled
```

### Dual Input Dialog

For dialogs needing two text fields (e.g., Find/Replace), use `tui_dlg_input2`.
Set up the second field's parameters in memory before calling:

```asm
find_buf:    RESB 42
replace_buf: RESB 42

str_find:    DB 'Find what:', 0
str_replace: DB 'Replace with:', 0
str_title:   DB 'Replace', 0

    ; Set up second field parameters
    MOV WORD [dlg_save_prompt2], str_replace
    MOV WORD [dlg_save_buf2], replace_buf
    MOV BYTE [dlg_save_maxlen2], 40

    ; Call with first field parameters in registers
    MOV SI, str_find            ; prompt1
    MOV DI, str_title           ; dialog title
    MOV BX, find_buf            ; buffer1
    MOV CL, 40                  ; maxlen1
    CALL tui_dlg_input2
    CMP AL, DLG_OK
    JZ .do_replace
    ; user cancelled
```

The dialog shows two labeled textboxes stacked vertically with OK/Cancel buttons.
Tab cycles: textbox1 → textbox2 → OK → Cancel. Both buffers can be
pre-populated with existing text (cursor starts at end).

### File Selector Dialog

```asm
file_buf: RESB 14
          DB 0

    MOV DI, str_title           ; "Open File"
    MOV BX, file_buf            ; buffer for selected filename
    MOV CL, 13                  ; max filename length
    CALL tui_dlg_file
    CMP AL, DLG_OK
    JZ .open_file
    ; user cancelled
    ; Note: CWD may have changed to the selected directory
```

The file dialog shows a drive selector dropdown, the current directory path,
a scrollable list of files and subdirectories (with `..\ ` for parent
navigation), and OK/Cancel buttons. The drive dropdown detects available drives
at open time and allows switching between them. Selecting a directory entry
navigates into it; selecting a file copies its name to the buffer and closes
with `DLG_OK`. Up to 128 directory entries are supported.

---

## Window Flags Reference

Combine flags with `+` (agent86 doesn't support bitwise OR in expressions):

```asm
DB WINF_VISIBLE + WINF_BORDER + WINF_TITLE + WINF_SHADOW + WINF_MOVABLE
```

| Flag | Effect |
|------|--------|
| `WINF_VISIBLE` | Window is drawn during compose. Required for the window to appear. |
| `WINF_BORDER` | Draws a border. Active window = double-line, inactive = single-line. Interior shrinks by 1 cell on each side. |
| `WINF_TITLE` | Centers the title string on the top border row. Requires `WINF_BORDER`. |
| `WINF_CLOSEBTN` | Top-left corner of the border acts as a mouse close button. |
| `WINF_MOVABLE` | Ctrl+Arrow keys move the window. Also enables mouse title-bar drag. |
| `WINF_SHADOW` | Draws a drop shadow (1 cell right, 1 cell below). |
| `WINF_MODAL` | Reserved flag. Actual modality is handled by `FW_MODAL_WIN` in `fw_state`. |
| `WINF_RESIZABLE` | Replaces bottom-right corner with a resize indicator. Mouse drag on that corner resizes the window. Minimum size: 8x4. |

---

## Common Patterns

### Program Skeleton

```asm
ORG 100h
    JMP main

INCLUDE tui.inc

; ... data definitions ...

main:
    CALL tui_init
    CALL tui_mouse_init         ; optional: enable mouse
    MOV WORD [fw_state + FW_MENUBAR], my_menubar  ; optional: set menu bar

    MOV SI, temp_win1
    CALL tui_win_create
    ; ... create more windows ...

    CALL tui_run                ; blocks until quit
    INT 20h                     ; return to DOS
```

### Focus Cycling

Tab cycles focus through controls with `CTRLF_FOCUSABLE + CTRLF_ENABLED`.
If only one (or zero) focusable controls exist in the active window, Tab
cycles windows instead (same as F6).

### Modal Dialogs

When a modal dialog is active (`FW_MODAL_WIN != FFh`):
- Only the dialog window receives keyboard input
- Escape cancels/closes the dialog
- Tab only cycles focus within the dialog
- Mouse clicks outside the dialog are blocked
- The dialog runs its own nested `tui_run` loop

### Control Handlers

Handlers are called for:
- **Buttons:** Enter/Space (keyboard) or mouse click-release
- **Checkboxes:** Enter/Space (keyboard) or mouse click (after toggle)
- **Radio buttons:** Enter/Space (keyboard) or mouse click (after selection)
- **Dropdowns:** Enter/Space (keyboard) or mouse click (after commit)
- **Listboxes:** Enter/Space (keyboard) or mouse click on item
- **Text viewers:** Enter/Space (keyboard activate)

---

## Troubleshooting

### Label / EQU Name Collisions

Labels and EQU constants share the same namespace (case-insensitive). A label
`win_title:` collides with `WIN_TITLE EQU 8`. Use distinct names:
```asm
; BAD:
win_title: DB 'Hello', 0     ; collides with WIN_TITLE EQU 8

; GOOD:
str_title: DB 'Hello', 0     ; unique name
```

### Register Constraints in Memory Operands

Only BX, BP, SI, DI are valid inside `[]`. The register must come first:
```asm
; GOOD:
MOV AL, [BX + my_array]

; BAD:
MOV AL, [my_array + BX]      ; treated as constant expression, fails
MOV AL, [AX + my_array]      ; AX not a valid base register
```

### Conditional Jump Range

Conditional jumps (JZ, JNZ, JC, etc.) have a -128 to +127 byte range.
For long jumps, invert the condition and use an unconditional JMP:
```asm
; Instead of:
;   JZ far_label          ; may be out of range

; Use:
    JNZ .skip
    JMP far_label
.skip:
```

### Hex Literal Format

Hex values starting with A-F must have a leading `0`:
```asm
MOV AL, 0FFh          ; GOOD
MOV AL, FFh           ; BAD - treated as identifier
```

### Expressions

Only `+`, `-`, `*`, `/` are supported. No bitwise operators. Combine flags
with `+` (safe because flag bits don't overlap):
```asm
DB WINF_VISIBLE + WINF_BORDER + WINF_TITLE    ; GOOD
DB WINF_VISIBLE | WINF_BORDER | WINF_TITLE    ; BAD - | not supported
```

### Escape with Open Menus/Dropdowns

Escape closes an open menu dropdown or combo popup. In modal dialogs,
Escape cancels/closes the dialog. The framework does not assign any default
quit behavior to Escape — applications handle exit themselves.

### RESB/RESW in .COM Programs

`RESB` and `RESW` emit actual zero bytes in the .COM binary. There is no true
BSS segment. Large reservations increase the binary size.

### Mouse Coordinates are Pixels

When scripting mouse events with `--mouse` or `--events`, coordinates are
**pixel** values (0-639 x, 0-399 y), not cell coordinates. To click cell
(col, row), use pixel `(col * 8, row * 8)`.

### PushAll/PopAll Macros

The `PushAll` and `PopAll` macros in `tui_macros.inc` use IRP, which requires
agent86's MACRO support. They push/pop AX, BX, CX, DX, SI, DI (in forward
order for Push, reverse for Pop). If you need to expand them manually:
```asm
; PushAll equivalent:
PUSH AX
PUSH BX
PUSH CX
PUSH DX
PUSH SI
PUSH DI

; PopAll equivalent:
POP DI
POP SI
POP DX
POP CX
POP BX
POP AX
```
