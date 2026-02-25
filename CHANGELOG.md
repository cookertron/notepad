# Changelog

All notable changes to DOS Notepad will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/),
and this project adheres to [Semantic Versioning](https://semver.org/).

## [1.0.1] - 2026-02-25

### Fixed
- Binary exceeded 64 KB .COM segment limit, causing hang on real DOS/DOSBox-X.
  Replaced RESB zero-fill with EQU-based BSS layout, reducing binary from 65 KB to 24 KB.

## [1.0.0] - 2026-02-25

### Added
- CGA text-mode editor with gap buffer engine (8 KB buffer)
- Full keyboard navigation: arrows, Home/End, PgUp/PgDn
- Text selection via Shift+arrow keys, Ctrl+A, and mouse click/drag
- Clipboard with Cut (Ctrl+X), Copy (Ctrl+C), and Paste (Ctrl+V)
- Undo (Ctrl+Z) and Redo (Ctrl+Y) with dedicated stacks
- Find (Ctrl+F), Find Next (F3), and Replace (Ctrl+R) with Search Up option
- File I/O: New, Open, Save, Save As via DOS INT 21h
- Drive selection dropdown in file dialogs
- Command-line filename argument for opening files on startup
- Menu bar with Alt+key shortcuts and dropdown hotkeys
- Menu accelerator key labels in dropdown entries
- TAB character display with 8-column tab stops
- Vertical scroll bar with click and drag support
- Status bar showing line/column, modified flag, clip/undo/free memory
- Mouse support for cursor placement, text selection, and UI interaction
- About dialog with program information
- Unsaved-changes prompt on exit
