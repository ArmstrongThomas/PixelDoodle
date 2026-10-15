# Slice 2: color and palette workflow

Implementation candidate, 2026-10-14. User accepted the slice 2 flows and layout/reset follow-up locally, then explicitly waived the physical 3DS check before slice 3.

## Controls

Tap the picker HEX, R, G or B readout to enter exact values. Tap the A label for numeric alpha. HEX accepts RRGGBB or RRGGBBAA; decimal channels accept 0-255. First digit replaces the displayed value, Delete edits it, Set applies, Back discards. When editing a palette swatch, Set returns to its draft picker; Save still commits the swatch. The FG/BG preview swaps foreground and background when tapped. Both colors are session state; swapping never fills artwork.

The wider FG/BG column has recent swatches and a NUM shortcut below its readouts. DS shows the four newest colors; 3DS shows up to twelve. More retains the full recent-color page, palette import/export and defaults. The vertical palette strip uses square swatches. Up to 12 deduplicated recent colors are session-only. Tapping a recent swatch does not move it. Selecting from either palette view or committing a picker/numeric change promotes that color to the front; new distinct colors evict the oldest when all 12 slots are full. The current palette belongs to the project. Add/Edit/Remove/reorder/import mark the project dirty without recoloring pixels. Palette changes are not part of the eight-entry drawing undo history. Set Project Palette As Default explicitly writes the existing dual PPL1 slots; Use My Default copies them into the current project. Restore Built-in Colors restores the original eight colors to this project without overwriting the saved custom default. Both restore actions immediately return to the palette so the result is visible. New projects and older files without palettes start from that default. Existing PPL1 palettes migrate as defaults without being overwritten by ordinary project edits.

## Palette files

Imports are staged, 1-256 entries, maximum 32 KiB; rejection does not alter the existing palette. The file browser confirms replacement. Exports reuse verified temporary-file/backup writes and explicit overwrite confirmation.

- GPL: classic GIMP Palette RGB lines, names/comments accepted; opaque RGB export. Transparent palettes must use HEX or PNG. Extended GPL v2 channels are not supported.
- HEX: one RRGGBB or RRGGBBAA per line, optional leading #; export always RRGGBBAA to preserve alpha. Blank lines allowed.
- PNG: one-pixel-high or one-pixel-wide strip, at most 256 colors, 8-bit RGB/RGBA, noninterlaced, all five row filters. Indexed/grayscale strips, interlace, and RGB tRNS are rejected; convert these to RGBA. Compressed data is bounded to 2048 bytes. Export is Nx1 RGBA.

## Project compatibility

PDD5 extends PDD4 by 1028 bytes: little-endian palette count then 256 ARGB words, covered by the existing checksum. Count zero represents legacy/no palette; maximum 256. PDD1-PDD4 reads remain supported. Palette dirty state is tracked separately from drawing history so Undo cannot incorrectly mark unsaved palette changes clean. The old dual PDD1 writer refuses documents with palette metadata.

Aseprite exports include the modern 0x2019 palette chunk. Imports read modern and legacy RGB/6-bit palette chunks, preserving order and alpha where encoded; modern chunks take precedence. Existing single-frame 128x128 RGBA/four-layer restrictions remain. Color-entry names are not retained. Sources: [Aseprite format](https://github.com/aseprite/aseprite/blob/main/docs/ase-file-specs.md), [GIMP palette specification](https://developer.gimp.org/core/standards/gpl/), [PNG specification](https://www.w3.org/TR/png/).

## Validation

Host checks cover exact color parsing, numeric entry/back, swaps, defaults, 256-color HEX/GPL/PNG round trips, alpha preservation, unsupported-alpha GPL rejection, malformed imports retaining state, dirty tracking, native/Aseprite palette persistence and existing drawing regressions. Desktop Aseprite round-trip checks compare palette data as well as artwork. Independent PNG fixtures cover all five filters, and Pillow verifies exported strip pixels. Native-size layouts inspected; all target builds are checked separately. No hardware acceptance claim yet.
