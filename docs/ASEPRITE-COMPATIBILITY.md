# Aseprite compatibility in Pixel Doodle 0.11.1

This describes the implemented reader/writer in `core/files.c`, not the full Aseprite format. Animation remains planned.

## Recommended round trip

1. Keep a native `.pdd` working copy on the handheld. Use Export > Aseprite for the desktop copy.
2. On the desktop, keep the portable copy single-frame, RGB/RGBA, with flat ordinary layers. Use Normal, Multiply, Screen or Overlay blending.
3. Open the `.aseprite` copy through Menu > Open on the handheld. Imports become unsaved native projects; Save As creates a `.pdd` copy rather than overwriting the imported file.
4. Export again after editing. Keep your richer desktop original separately if it uses features outside this subset.

## What is supported

| Feature | Current behavior |
| --- | --- |
| Canvas | Independent width/height: 1–128 in DS/NTR; 1–512 on actual DSi/TWL and 3DS. Square pixels only. |
| Frames | Exactly one frame; exported duration is 100 ms. Original timing is not retained. |
| Pixels | 32-bit RGBA, including partially transparent pixels. Indexed and grayscale Aseprite documents must be converted to RGB first. |
| Layers | Up to eight flat ordinary layers, subject to device canvas capacity. DSi allows eight at 256 square and two at 512 square. |
| Layer properties | Names up to 63 bytes, order, visibility, opacity, and editable/locked state. The handheld font cannot display every Unicode character even when name bytes survive. |
| Blend modes | Normal, Multiply, Screen and Overlay. Other modes reject on import. |
| Cels | Raw or zlib image cels, at most one per layer, fully within canvas bounds, cel opacity 255 and zero Z offset. Empty layers are supported. |
| Palette | Up to 256 RGBA entries; modern palette and legacy palette data are read. Palette entry names are not retained. |
| Color profile | Unspecified/basic sRGB accepted without color conversion. Custom/embedded profiles and custom gamma reject. Export omits profile metadata. |

Exports compact enabled layers into display order and write full-canvas raw RGBA cels. Native blank/unused slots are not additional exported layers. Compression is not required to reopen the file in desktop Aseprite.

## Limits and rejected features

Multi-frame animation, linked cels, groups, tilemaps, nonstandard layer flags, out-of-canvas cels, per-cel opacity and Z offsets are rejected. Tags, slices, user data and other unsupported chunks also reject rather than silently disappear. Remove those features from a separate portable copy if an import reports unsupported metadata. A malformed, unsupported, oversized or memory-constrained import retains the current project.

Native-only alpha lock does not survive Aseprite export; the UI reports this when exporting protected art. Undo history, selections, clipboard, viewport, guides, device settings and brush presets are not Aseprite project data. They are not a promise of portable session state in native saves either.

Native working files retain Pixel Doodle layer alpha lock and project palettes. PNG export produces the visible composite with alpha, without editable layers. PNG artwork import is a separate decoder and can accept indexed/grayscale PNG even though the Aseprite reader requires RGBA.

Files are limited to 9 MiB. Opening and exporting use bounded temporary buffers and can fail under memory pressure, even when canvas dimensions fit. Save verifies a temporary file before replacing the destination and retaining a backup; this is not a guarantee against power loss on FAT storage.

## Verification

`scripts/test-interop.py` exercises synthetic exports through installed desktop Aseprite, then reimports and compares layer pixels/properties, including blend/order, palette, protection and large dimensions. Host tests cover unsupported-input preservation. Physical 3DS file round trips were accepted earlier; newer size/capacity combinations still require device-specific testing. CIA installation and animation interoperability are not established by these checks.
