# Square palette and picker workspace 0.14.2

This document supersedes `PALETTE-SLICE1.md`.

The lower screen has three explicit modes: **Swatches**, **Picker**, and **Edit**.
The touch-inactive upper screen uses the full display for a large active-color
preview, foreground and background cards, the selected swatch, RGB, HSV, alpha,
recent colors and the current palette position. Its header names the active
target instead of using a generic "display only" label. Preview swatches are
clipped on both axes and keep a 12-pixel NDS or 16-pixel 3DS outer margin.

## Swatches

Tapping a square swatch applies it to the active `FG` or `BG` target. The target
buttons and swap control are on the touch screen. Swatches use square geometry
and three density presets:

- Compact: eight columns.
- Medium: six columns.
- Large: five columns.

Drag vertically more than five pixels within 150 ms to scroll without selecting
or reordering. Hold still for 150 ms and drag to reorder; the target shows
`MOVE`, and dragging near an edge scrolls the list. Releasing commits the move.
The default palette contains 40 colors and supports up to 256.

## Picker

The saturation/value selector is kept square. A vertical rainbow hue rail sits
beside it, with the alpha strip below. The right side contains:

- Foreground and background targets with a swap control.
- A hexadecimal field that opens the shared on-screen keyboard in a six-digit,
  hexadecimal-safe layout.
- A scrollable six-column quick palette.

Quick-palette rows are clipped to the scroll panel while moving, so a partial
row never draws over the foreground/background controls or hexadecimal field.

The persistent rail uses six packed 16x16 line icons from the accepted mockup.
They live in otherwise unreachable lowercase slots of the existing UI font
table, so the build does not embed a second icon font.

`CANCEL` restores the foreground, background, hue, saturation, and value from
when Picker opened. `USE COLOR` keeps the current values and returns to
Swatches. Picker edits apply to the selected target immediately for a responsive
canvas preview.

## Edit

Management actions live only in Edit: Add, Replace, Remove, Move, Load, and Save.
Add and Replace use the active foreground or background target. Save writes the
global palette. Load restores the last saved global palette. Existing artwork
pixels are never recolored by palette management.

## Persistence

This is a global device palette, separate from project documents. Palette
mutations save on stylus release to alternating app-owned `palette0.bin` and
`palette1.bin` slots. PPL1 records are 1040 bytes: magic, little-endian sequence
and count, 256 RGBA words, then CRC32 of the preceding 1036 bytes. Counts and
CRC are validated; the newest valid slot wins. Writes flush, sync, close, and
read back. Failure leaves changes usable in RAM and reports that they were not
saved. FAT writes do not provide a power-loss guarantee.

Host checks cover all three modes, 8/6/5-column density, foreground and
background targets, swap, exact hex entry through the OSK, palette-management
actions, both scroll regions, 150 ms reorder behavior, persistence, corrupt
newest fallback, non-overlapping picker controls and native DS/3DS renders.
