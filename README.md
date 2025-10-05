# 2048-in-terminal

Terminal-based clone of [the 2048 game](https://play2048.co/)

![screen](screen.gif)

## Controls

### Game Movement

- **Arrow keys / hjkl**: Move tiles

### Undo/Redo

- **u**: Undo last move
- **U / y**: Redo last undone move

### Save/Load

- **s**: Save game (manual save with description)
- **g**: Load game (choose from saved games)
- **F5**: Quick save (saves to slot 0)
- **F9**: Quick load (loads from slot 0)

### Other

- **r**: Restart game
- **a**: Toggle animations
- **q**: Quit game

## Features

- **Undo/Redo**: Up to 50 levels of undo/redo history with slow, visible animations
- **Animated Transitions**: Dramatic visual effects for undo (blue) and redo (green) operations with proper timing
- **Multiple Save Slots**: 10 save slots (0-9) with custom descriptions
- **Auto-save**: Game automatically saves on exit and resumes on startup
- **Save History**: Undo/redo history is preserved in save files
- **Save Metadata**: Each save includes timestamp and description
- **Quick Save/Load**: Instant save/load using F5/F9 keys
- **Toggle Animations**: Press 'a' to enable/disable all animations including undo/redo
- **Visible Animation Speed**: Undo/redo animations are intentionally slower (0.2s per step) for clear visibility

---

## Requirements

- GNU Make
- C compiler (GCC or Clang)
- pkg-config
- ncurses library
- ncurses development files

---

## Build

Build using default parameters:

`make`

Or build with parameters, e.g.:

`make CC=clang NCURSES_LIB=ncursesw EXE=2048`

---

## Install

Install to default location (`/usr/local/bin/`):

`sudo make install`

Alter the `PREFIX` parameter to install to `<PREFIX>/bin`, e.g.:

`make install PREFIX=~/.local`

## Uninstall

Run `make uninstall`. Specify `PREFIX` if you did so during installation.
