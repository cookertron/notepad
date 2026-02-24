; ============================================================================
; tui_const.inc - TUI Framework Constants
; ============================================================================

; --- Screen dimensions (80x50 CGA text mode) ---
SCR_W       EQU 80
SCR_H       EQU 25
SCR_CELLS   EQU 2000       ; 80 * 25
SCR_BYTES   EQU 4000       ; 2000 * 2 (char + attr per cell)
VRAM_SEG    EQU 0B800h

; --- Window struct offsets (20 bytes per window) ---
WIN_FLAGS   EQU 0           ; BYTE  - window flags bitmask
WIN_X       EQU 1           ; BYTE  - left column
WIN_Y       EQU 2           ; BYTE  - top row
WIN_W       EQU 3           ; BYTE  - width in columns
WIN_H       EQU 4           ; BYTE  - height in rows
WIN_ATTR    EQU 5           ; BYTE  - interior attribute (char+bg color)
WIN_BATTR   EQU 6           ; BYTE  - border attribute
WIN_TATTR   EQU 7           ; BYTE  - title attribute
WIN_TITLE   EQU 8           ; WORD  - pointer to null-terminated title string
WIN_FIRST   EQU 10          ; WORD  - pointer to first control (linked list)
WIN_FOCUS   EQU 12          ; WORD  - pointer to focused control
WIN_HANDLER EQU 14          ; WORD  - pointer to event handler callback
WIN_ZORDER  EQU 16          ; BYTE  - z-order position
WIN_ID      EQU 17          ; BYTE  - window ID / slot index
WIN_SCROLLX EQU 18          ; BYTE  - horizontal scroll offset
WIN_SCROLLY EQU 19          ; BYTE  - vertical scroll offset
WIN_SIZE    EQU 20          ; total bytes per window struct

; --- Window flags ---
WINF_VISIBLE  EQU 01h
WINF_BORDER   EQU 02h
WINF_TITLE    EQU 04h
WINF_CLOSEBTN EQU 08h
WINF_MOVABLE  EQU 10h
WINF_SHADOW   EQU 20h
WINF_MODAL    EQU 40h
WINF_RESIZABLE EQU 80h     ; window can be resized by dragging BR corner

; --- Window minimum dimensions for resize ---
WIN_MINW    EQU 8           ; minimum window width
WIN_MINH    EQU 4           ; minimum window height

; --- Framework state offsets ---
FW_NUMWIN   EQU 0           ; BYTE  - number of active windows
FW_TOPWIN   EQU 1           ; BYTE  - index of topmost window
FW_MENUBAR  EQU 2           ; WORD  - pointer to menu bar (0 = none)
FW_RUNNING  EQU 4           ; BYTE  - 1 = event loop active
FW_DIRTY    EQU 5           ; BYTE  - 1 = screen needs redraw

; --- Color palette ---
CLR_DESKTOP     EQU 17h     ; white on blue (desktop background)
CLR_WIN_BG      EQU 1Fh     ; bright white on blue (window interior)
CLR_WIN_BDR     EQU 1Bh     ; bright cyan on blue (window border)
CLR_WIN_BDR_ACT EQU 1Eh     ; yellow on blue (active window border)
CLR_WIN_TTL     EQU 1Eh     ; yellow on blue (window title)
CLR_SHADOW      EQU 08h     ; dark grey on black (shadow)

; --- Box-drawing characters (CP437) ---
BOX_TL      EQU 0DAh        ; top-left corner
BOX_TR      EQU 0BFh        ; top-right corner
BOX_BL      EQU 0C0h        ; bottom-left corner
BOX_BR      EQU 0D9h        ; bottom-right corner
BOX_H       EQU 0C4h        ; horizontal line
BOX_V       EQU 0B3h        ; vertical line

; --- Key codes ---
KEY_TAB     EQU 09h             ; Tab (normal key)
KEY_UP      EQU 48h             ; scan codes (extended keys)
KEY_DOWN    EQU 50h
KEY_LEFT    EQU 4Bh
KEY_RIGHT   EQU 4Dh
KTYPE_NORMAL   EQU 0           ; returned in AH from tui_poll_key
KTYPE_EXTENDED EQU 1

; --- Double-line box-drawing characters (CP437) ---
DBOX_TL     EQU 0C9h
DBOX_TR     EQU 0BBh
DBOX_BL     EQU 0C8h
DBOX_BR     EQU 0BCh
DBOX_H      EQU 0CDh
DBOX_V      EQU 0BAh

; --- Border scratch offsets ---
BDR_TL      EQU 0
BDR_TR      EQU 1
BDR_BL      EQU 2
BDR_BR      EQU 3
BDR_H       EQU 4
BDR_V       EQU 5
BDR_ATTR    EQU 6

; --- Control struct offsets (14 bytes per control) ---
CTRL_TYPE    EQU 0           ; BYTE  - 0=free, 1=label, 2=button
CTRL_FLAGS   EQU 1           ; BYTE  - visible, enabled, focusable
CTRL_X       EQU 2           ; BYTE  - x relative to window interior
CTRL_Y       EQU 3           ; BYTE  - y relative to window interior
CTRL_W       EQU 4           ; BYTE  - total rendered width
CTRL_H       EQU 5           ; BYTE  - height (1 for label/button)
CTRL_NEXT    EQU 6           ; WORD  - next control in linked list (0=end)
CTRL_TEXT    EQU 8           ; WORD  - pointer to text string
CTRL_ATTR    EQU 10          ; BYTE  - normal attribute
CTRL_FATTR   EQU 11          ; BYTE  - focused attribute (buttons only)
CTRL_HANDLER EQU 12          ; WORD  - callback pointer (buttons only, 0=none)
CTRL_SIZE    EQU 14

; --- Control types ---
CTYPE_LABEL   EQU 1
CTYPE_BUTTON  EQU 2
CTYPE_TEXTBOX EQU 3
CTYPE_CHECKBOX EQU 4
CTYPE_RADIO    EQU 5
CTYPE_LISTBOX  EQU 7

; --- Control flags ---
CTRLF_VISIBLE   EQU 01h
CTRLF_ENABLED   EQU 02h
CTRLF_FOCUSABLE EQU 04h

; --- Control colors ---
CLR_BTN         EQU 70h     ; black on light gray (normal button)
CLR_BTN_FOCUS   EQU 2Fh     ; bright white on green (focused button)
CLR_LABEL       EQU 1Fh     ; bright white on blue (label, same as window bg)
CLR_TEXTBOX     EQU 70h     ; black on light gray (unfocused textbox)
CLR_TB_FOCUS    EQU 30h     ; black on cyan (focused textbox)
CLR_TB_CURSOR   EQU 0Fh     ; bright white on black (block cursor)
CLR_CHECKBOX    EQU 1Fh     ; bright white on blue (checkbox)
CLR_CB_FOCUS    EQU 30h     ; black on cyan (focused checkbox)
CLR_RADIO       EQU 1Fh     ; bright white on blue (radio)
CLR_RB_FOCUS    EQU 30h     ; black on cyan (focused radio)

; --- Additional key codes ---
KEY_F6          EQU 40h     ; F6 scan code (extended)
KEY_ENTER       EQU 0Dh     ; Enter (normal key)
KEY_SPACE       EQU 20h     ; Space (normal key)
KEY_BACKSPACE   EQU 08h     ; Backspace (normal key)
KEY_HOME        EQU 47h     ; Home scan code (extended)
KEY_END         EQU 4Fh     ; End scan code (extended)
KEY_DELETE      EQU 53h     ; Delete scan code (extended)
KEY_PGUP        EQU 49h     ; PgUp scan code (extended)
KEY_PGDN        EQU 51h     ; PgDn scan code (extended)
KEY_CTRL_LEFT   EQU 73h     ; Ctrl+Left scan code (extended)
KEY_CTRL_RIGHT  EQU 74h     ; Ctrl+Right scan code (extended)
KEY_CTRL_UP     EQU 8Dh     ; Ctrl+Up scan code (extended)
KEY_CTRL_DOWN   EQU 91h     ; Ctrl+Down scan code (extended)

; --- TextBox struct offsets (appended after base 14 bytes) ---
CTRL_TB_MAXLEN  EQU 14      ; BYTE  - max chars buffer can hold
CTRL_TB_CURPOS  EQU 15      ; BYTE  - cursor position (0-based)
CTRL_TB_SCROLL  EQU 16      ; BYTE  - scroll offset (first visible char index)
CTRL_TB_LEN     EQU 17      ; BYTE  - current text length
CTRL_TB_SIZE    EQU 18

; --- Checkbox struct offsets (appended after base 14 bytes) ---
CTRL_CB_STATE   EQU 14      ; BYTE  - 0=off, 1=on
CTRL_CB_SIZE    EQU 15

; --- Radio button struct offsets ---
CTRL_RB_GROUP   EQU 15      ; BYTE  - radio group ID (0-255)
CTRL_RB_SIZE    EQU 16

; --- Menu bar struct offsets (8 bytes) ---
MBAR_COUNT   EQU 0           ; BYTE  - number of top-level items
MBAR_SEL     EQU 1           ; BYTE  - selected item index (FFh = inactive)
MBAR_OPEN    EQU 2           ; BYTE  - 1 = dropdown open, 0 = bar-only
MBAR_DDSEL   EQU 3           ; BYTE  - selected dropdown entry (FFh = none)
MBAR_ATTR    EQU 4           ; BYTE  - normal bar attribute
MBAR_SELATTR EQU 5           ; BYTE  - selected/highlight attribute
MBAR_ITEMS   EQU 6           ; WORD  - pointer to array of menu items
MBAR_SIZE    EQU 8

; --- Menu item struct offsets (8 bytes) ---
MI_TEXT      EQU 0            ; WORD  - pointer to item label string
MI_X         EQU 2            ; BYTE  - column position on bar
MI_W         EQU 3            ; BYTE  - width of item on bar
MI_ENTRIES   EQU 4            ; WORD  - pointer to dropdown entry array
MI_ECOUNT    EQU 6            ; BYTE  - number of dropdown entries
MI_DDW       EQU 7            ; BYTE  - dropdown width
MI_HOTIDX    EQU 8            ; BYTE  - index of hotkey char in MI_TEXT (FFh = none)
MI_ALTKEY    EQU 9            ; BYTE  - Alt+key scan code (0 = none)
MI_SIZE      EQU 10

; --- Dropdown entry struct offsets (10 bytes) ---
MDE_TEXT      EQU 0             ; WORD  - pointer to entry label string
MDE_HANDLER   EQU 2             ; WORD  - callback pointer (0 = no action)
MDE_HOTIDX    EQU 4             ; BYTE  - index of hotkey char in entry text (FFh = none)
MDE_HOTKEY    EQU 5             ; BYTE  - ASCII char that activates this entry (0 = none)
MDE_ACCEL     EQU 6             ; WORD  - pointer to accelerator display text (0 = none)
MDE_ACCELKEY  EQU 8             ; BYTE  - Alt+key scan code for global dispatch (0 = none)
MDE_PAD       EQU 9             ; BYTE  - reserved (0)
MDE_SIZE      EQU 10

; --- Menu colors ---
CLR_MENUBAR     EQU 70h       ; black on light gray (normal bar)
CLR_MENU_SEL    EQU 0Fh       ; bright white on black (selected item)
CLR_MENU_DD     EQU 70h       ; black on light gray (dropdown body)
CLR_MENU_DDSEL  EQU 0Fh       ; bright white on black (selected entry)
CLR_MENU_DDSHDW EQU 08h       ; dark gray on black (dropdown shadow)
CLR_MENU_HOTKEY EQU 74h       ; red on light gray (hotkey letter)
CLR_MENU_DD_HOTKEY EQU 74h    ; red on light gray (dropdown hotkey letter)

; --- Additional key codes (menu) ---
KEY_F10         EQU 44h        ; F10 scan code (extended)
KEY_ESCAPE      EQU 1Bh        ; Escape (normal key)

; --- Dropdown control type ---
CTYPE_DROPDOWN  EQU 6

; --- Listbox struct offsets (after 14-byte base) ---
CTRL_LB_ITEMS   EQU 14         ; WORD  - ptr to array of WORD string pointers
CTRL_LB_COUNT   EQU 16         ; BYTE  - total number of items
CTRL_LB_SEL     EQU 17         ; BYTE  - currently selected item index
CTRL_LB_SCROLL  EQU 18         ; BYTE  - first visible item index
CTRL_LB_SIZE    EQU 19

; --- Listbox colors ---
CLR_LB_NORMAL   EQU 70h        ; black on light gray (unfocused, unselected)
CLR_LB_FOCUS    EQU 30h        ; black on cyan (focused, unselected)
CLR_LB_SEL      EQU 0Fh        ; bright white on black (selected, unfocused)
CLR_LB_FOCSEL   EQU 3Eh        ; yellow on cyan (selected + focused)

; --- Text viewer control type ---
CTYPE_TEXTVIEW  EQU 8

; --- Text viewer struct offsets (after 14-byte base) ---
CTRL_TV_LINES   EQU 14          ; WORD  - ptr to array of WORD line pointers
CTRL_TV_COUNT   EQU 16          ; BYTE  - total line count (max 255)
CTRL_TV_CURLINE EQU 17          ; BYTE  - cursor line index
CTRL_TV_SCROLL  EQU 18          ; BYTE  - first visible line index
CTRL_TV_SCROLLX EQU 19          ; BYTE  - horizontal scroll offset (chars)
CTRL_TV_SIZE    EQU 20

; --- Text viewer colors ---
CLR_TV_NORMAL   EQU 70h         ; black on light gray (unfocused, non-cursor)
CLR_TV_FOCUS    EQU 30h         ; black on cyan (focused, non-cursor)
CLR_TV_CURLINE  EQU 0Fh         ; bright white on black (cursor line, unfocused)
CLR_TV_CURFOCUS EQU 3Eh         ; yellow on cyan (cursor line + focused)

; --- Dropdown struct offsets (after 14-byte base) ---
CTRL_DD_ITEMS   EQU 14          ; WORD  - ptr to items array (array of WORD string ptrs)
CTRL_DD_COUNT   EQU 16          ; BYTE  - total items
CTRL_DD_SEL     EQU 17          ; BYTE  - committed selection index
CTRL_DD_TMPSEL  EQU 18          ; BYTE  - temp highlight while popup open
CTRL_DD_SCROLL  EQU 19          ; BYTE  - first visible item in popup
CTRL_DD_MAXVIS  EQU 20          ; BYTE  - max visible items
CTRL_DD_SIZE    EQU 21

; --- Framework state: open dropdown (fw_state offsets 6-9) ---
FW_OPENDD       EQU 6           ; WORD  - ptr to open dropdown control (0=none)
FW_OPENDD_ROW   EQU 8           ; BYTE  - popup absolute row
FW_OPENDD_COL   EQU 9           ; BYTE  - popup absolute col

; --- Dropdown popup colors ---
CLR_DD_NORMAL   EQU 70h         ; black on light gray (closed dropdown)
CLR_DD_FOCUS    EQU 30h         ; black on cyan (focused dropdown)
CLR_DD_POPUP    EQU 70h         ; black on light gray (popup body + border)
CLR_DD_POPSEL   EQU 0Fh         ; bright white on black (popup highlighted item)
DD_ARROW        EQU 1Fh         ; filled down-triangle char (CP437)

; --- Mouse state struct offsets ---
MS_FLAGS    EQU 0           ; BYTE  - MSF_* flags
MS_COL      EQU 1           ; BYTE  - current cell column (0-79)
MS_ROW      EQU 2           ; BYTE  - current cell row (0-49)
MS_BUTTONS  EQU 3           ; BYTE  - current button state (1=left, 2=right)
MS_PREV_BTN EQU 4           ; BYTE  - previous frame button state
MS_DRAG_OX  EQU 5           ; BYTE  - drag: click col minus window X
MS_DRAG_OY  EQU 6           ; BYTE  - drag: click row minus window Y
MS_DRAG_WIN EQU 7           ; BYTE  - window slot being dragged (FFh=none)
MS_PRESSED  EQU 8           ; WORD  - ptr to control being mouse-pressed (0=none)
MS_PCOL     EQU 10          ; BYTE  - previous cell column (for move detection)
MS_PROW     EQU 11          ; BYTE  - previous cell row
MS_SIZE     EQU 16

; --- Mouse flags ---
MSF_ENABLED   EQU 01h       ; mouse driver detected
MSF_DRAGGING  EQU 02h       ; window drag in progress
MSF_RESIZING  EQU 04h       ; window resize in progress
MSF_BTN_DOWN  EQU 08h       ; left button currently held

; --- Hit test result types (returned in AL) ---
HIT_NONE        EQU 0       ; nothing (desktop)
HIT_TITLEBAR    EQU 1       ; window title bar
HIT_CLOSEBTN    EQU 2       ; close button
HIT_INTERIOR    EQU 3       ; window interior (no control)
HIT_CONTROL     EQU 4       ; a specific control
HIT_BORDER      EQU 5       ; window border (not title/close)
HIT_MENUBAR     EQU 6       ; menu bar item
HIT_MENUDROP    EQU 7       ; menu dropdown entry
HIT_DD_POPUP    EQU 8       ; combo dropdown popup item
HIT_RESIZE      EQU 9       ; resize corner (bottom-right)

; --- Mouse-specific colors ---
CLR_BTN_PRESSED EQU 78h     ; dark gray on light gray (button pressed state)

; --- Dialog result constants ---
DLG_OK      EQU 1
DLG_CANCEL  EQU 0
DLG_YES     EQU 1
DLG_NO      EQU 0
DLG_REPL_ALL EQU 2

; --- Framework state: dialog (fw_state offsets 10-11) ---
FW_DLG_RESULT EQU 10           ; BYTE - dialog return code
FW_MODAL_WIN  EQU 11           ; BYTE - modal window slot (FFh = none)

; --- Dialog sizing ---
DLG_MIN_W     EQU 20           ; minimum dialog interior width
DLG_PAD       EQU 4            ; horizontal padding (2 each side)
DLG_BTN_OK_W  EQU 8            ; button width for [ OK ]
DLG_BTN_YN_W  EQU 8            ; button width for [ Yes ]/[ No ]
DLG_BTN_CAN_W EQU 10           ; button width for [ Cancel ]
DLG_BTN_GAP   EQU 2            ; gap between buttons

; --- File dialog ---
DLG_FILE_MAXENT  EQU 128         ; max directory entries
DLG_FILE_NAMELEN EQU 14          ; bytes per name slot (13 chars + backslash)

; --- Scroll bar characters (CP437) ---
SB_ARROW_UP   EQU 18h
SB_ARROW_DN   EQU 19h
SB_ARROW_LT   EQU 1Bh
SB_ARROW_RT   EQU 1Ah
SB_THUMB      EQU 0DBh
SB_TRACK      EQU 0B0h

; --- Scroll bar colors ---
CLR_SB_ARROW  EQU 70h         ; black on light gray
CLR_SB_TRACK  EQU 78h         ; dark gray on light gray
CLR_SB_THUMB  EQU 70h         ; black on light gray

; --- Mouse scroll bar drag flag ---
MSF_SB_DRAG   EQU 10h         ; scroll bar thumb drag in progress
MSF_CTRL_DRAG EQU 20h         ; control-level drag in progress (e.g., text selection)

; --- Mouse state scroll bar fields ---
MS_SB_CTRL    EQU 12           ; WORD - control being scroll-dragged
MS_SB_DIR     EQU 14           ; BYTE - 0=vertical, 1=horizontal
MS_SB_OFFSET  EQU 15           ; BYTE - click offset within thumb

; --- Text viewer MAXLEN field ---
CTRL_TV_MAXLEN EQU 20          ; BYTE - longest line length (set by parse)
CTRL_TV_SIZE2  EQU 21          ; total size with MAXLEN

; --- Limits ---
MAX_WINDOWS EQU 16
