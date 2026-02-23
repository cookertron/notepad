; ============================================================================
; editor_const.inc - Editor-Specific Constants
; ============================================================================

; --- Editor control type ---
CTYPE_EDITOR    EQU 9

; --- Editor colors ---
CLR_ED_TEXT     EQU 17h         ; white on blue (normal text)
CLR_ED_CURSOR   EQU 71h         ; blue on light gray (cursor cell)
CLR_ED_SELECT   EQU 30h         ; black on cyan (selected text)
CLR_ED_CURLINE  EQU 1Fh         ; bright white on blue (cursor line)
CLR_ED_STATUS   EQU 30h         ; black on cyan (status bar)

; --- Ctrl+key ASCII codes (returned as KTYPE_NORMAL via INT 21h/06h) ---
KEY_CTRL_A      EQU 01h         ; Select All
KEY_CTRL_C      EQU 03h         ; Copy
KEY_CTRL_F      EQU 06h         ; Find
KEY_CTRL_N      EQU 0Eh         ; New file
KEY_CTRL_O      EQU 0Fh         ; Open file
KEY_CTRL_R      EQU 12h         ; Replace (not Ctrl+H — conflicts with Backspace)
KEY_CTRL_S      EQU 13h         ; Save file
KEY_CTRL_V      EQU 16h         ; Paste
KEY_CTRL_X      EQU 18h         ; Cut
KEY_CTRL_Y      EQU 19h         ; Redo
KEY_CTRL_Z      EQU 1Ah         ; Undo

; --- Extended key codes for function keys ---
KEY_F3          EQU 3Dh         ; Find Next
KEY_F5          EQU 3Fh         ; Toggle select mode

; --- Editor state struct offsets (23 bytes) ---
ED_BUF          EQU 0           ; WORD - buffer base address
ED_BUFEND       EQU 2           ; WORD - buffer end address (base + bufsize)
ED_GAPSTART     EQU 4           ; WORD - gap start (= cursor position in buffer)
ED_GAPEND       EQU 6           ; WORD - gap end
ED_CURLINE      EQU 8           ; WORD - cursor line (0-based)
ED_CURCOL       EQU 10          ; WORD - cursor column (0-based)
ED_SCROLLY      EQU 12          ; WORD - vertical scroll (first visible line)
ED_SCROLLX      EQU 14          ; WORD - horizontal scroll (first visible col)
ED_FLAGS        EQU 16          ; BYTE - editor flags (EDF_*)
ED_SELANCHOR    EQU 17          ; WORD - selection anchor offset (FFFFh = none)
ED_FILENAME     EQU 19          ; WORD - pointer to filename string (0 = Untitled)
ED_LINECOUNT    EQU 21          ; WORD - total line count (cached, min 1)
ED_SIZE         EQU 23          ; total struct size

; --- Editor flags (ED_FLAGS bitmask) ---
EDF_MODIFIED    EQU 01h         ; buffer has been modified since last save
EDF_SELECTING   EQU 02h         ; selection mode active (F5 toggle)

; --- Editor control struct extension (14 base + 2 extra = 16 bytes) ---
CTRL_ED_STATE   EQU 14          ; WORD - pointer to editor state struct
CTRL_ED_SIZE    EQU 16

; --- Editor buffer size per window ---
ED_BUFSIZE      EQU 8192        ; 8KB per editor buffer

; --- Maximum editor windows ---
ED_MAX_WINS     EQU 2           ; max simultaneous editor windows

; --- Undo record offsets ---
UREC_SIZE       EQU 0           ; WORD - total record size
UREC_CHANGE     EQU 2           ; WORD - buffer offset of change
UREC_CURSOR     EQU 4           ; WORD - cursor to restore after undo
UREC_DATALEN    EQU 6           ; WORD - bytes of deleted text in DATA
UREC_INSLEN     EQU 8           ; WORD - chars that were inserted
UREC_DATA       EQU 10          ; start of variable-length data
UREC_HDR        EQU 12          ; minimum record size (header + trailing size)

; --- Undo buffer sizes ---
UNDO_BUFSIZE    EQU 4096
REDO_BUFSIZE    EQU 2048
