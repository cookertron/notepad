; ============================================================================
; tui_macros.inc - TUI Framework Macros
; ============================================================================

; --- PushAll: save scratch + index registers ---
PushAll MACRO
    IRP r, <AX, BX, CX, DX, SI, DI>
        PUSH r
    ENDM
ENDM

; --- PopAll: restore in reverse order ---
PopAll MACRO
    IRP r, <DI, SI, DX, CX, BX, AX>
        POP r
    ENDM
ENDM
