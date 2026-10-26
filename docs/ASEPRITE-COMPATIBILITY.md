# Aseprite compatibility in Pixel Doodle 0.23.0

This describes the implemented reader/writer in `core/files.c` and
`core/ase_export.c`, not the full Aseprite format. Animation, tags and linked
cels are supported; the editable master remains the native `.pdd`/PDDA file.

## Recommended round trip

1. Keep a native `.pdd` working copy on the handheld. Use Export > Aseprite for
   the desktop copy.
2. Use RGBA sprites with flat ordinary layers and Normal, Multiply, Screen or
   Overlay blending. Groups and tilemaps are not imported.
3. Open the `.aseprite` copy through Menu > Open on the handheld. Imports become
   unsaved native projects; Save As creates a `.pdd` copy rather than
   overwriting the imported file.
4. Export again after editing. Keep your richer desktop original separately if
   it uses features outside this subset.

## What is supported

| Feature | Current behavior |
| --- | --- |
| Canvas | Independent width/height: 1–128 in DS/NTR; 1–512 on actual DSi/TWL and 3DS. Square pixels only. |
| Frames | 1–256 frames on import and export. Frame durations are read and written per frame; 0 uses the header speed, then 100 ms, and is clamped to 10–60000 ms. |
| Pixels | 32-bit RGBA, including partially transparent pixels. Indexed and grayscale Aseprite documents must be converted to RGB first. |
| Layers | Up to eight flat ordinary image layers, subject to device canvas capacity. DSi allows eight at 256 square and two at 512 square. |
| Layer properties | Names up to 63 bytes, order, visibility, opacity, and editable/locked state. The handheld font cannot display every Unicode character even when name bytes survive. Layer UUIDs are accepted and ignored. |
| Blend modes | Normal, Multiply, Screen and Overlay. Other modes reject on import. |
| Cels | Raw or zlib image cels, at most one per layer and frame, fully within canvas bounds, cel opacity 255 and zero Z offset. Empty layers are supported. |
| Linked cels | Cel type 1 is imported and exported. A group may span non-contiguous frames; exports point every member after the first at that first frame. Editing any member updates the group in the handheld editor. |
| Tags | Chunk 0x2018: up to 32 tags with inclusive ranges, forward/reverse/ping-pong/pong-ping direction, repeat count and colour. Names are stored up to 31 bytes; longer names are cut at a UTF-8 boundary on import. Colours from following user-data chunks are preferred. |
| Palette | Up to 256 RGBA entries; modern palette and legacy palette data are read. Palette entry names are not retained. |
| Color profile | Unspecified/basic sRGB accepted without color conversion. Custom/embedded profiles and custom gamma reject. Export omits profile metadata. |

Export streams every frame, enabled layers in display order, per-frame
durations, tags plus one 0x2020 colour chunk per tag, and linked-cel
references. Raw RGBA cels are used for the first frame of each linked group and
for unlinked cels; later group members use cel type 1. A one-frame project with
tags uses the same streaming exporter. Compression is not required to reopen
the file in desktop Aseprite.

## Limits and rejected features

Layer groups, tilemaps, indexed/grayscale sprites, nonstandard layer flags,
out-of-canvas or negative cel origins, per-cel opacity and nonzero Z offsets
are rejected. Unknown critical metadata rejects rather than silently
disappearing; cel-extra (0x2006), slice (0x2022) and user-data (0x2020)
chunks are validated where needed and otherwise skipped. Remove unsupported
features from a separate portable copy if an import reports them.

Import validates and decodes the whole file before replacing the current
drawing, so malformed, unsupported and oversized files retain the open
project. An allocation failure while frames are being added can still abort
with a partly imported project; the error is reported. Preview decodes only
the first frame and reports the header frame count, so selecting a large
animation stays cheap.

Native-only alpha lock does not survive Aseprite export; the UI reports this
when exporting protected art. Undo history, selections, clipboard, viewport,
guides, device settings and brush presets are not Aseprite project data. They
are not a promise of portable session state in native saves either.

Native working files retain Pixel Doodle layer alpha lock, tags, linked cels
and project palettes. PNG export produces the visible composite with alpha,
without editable layers. PNG artwork import is a separate decoder and can
accept indexed/grayscale PNG even though the Aseprite reader requires RGBA.

Files are limited to 9 MiB. Opening and exporting use bounded temporary
buffers and can fail under memory pressure, even when canvas dimensions fit.
Save verifies a temporary file before replacing the destination and retaining
a backup; this is not a guarantee against power loss on FAT storage.

## Verification

`scripts/test-animation-ase.py` independently parses exports, including the
tags chunk, per-tag user-data colours, non-contiguous linked cels, frame sizes,
layer properties and palettes. Host checks also reimport exported files,
compare every frame and layer, exercise shared edits after import, and open a
hand-built file whose tag colour lives in a user-data chunk beside ignored
cel-extra and slice chunks. `scripts/test-interop.py` covers the older
single-frame desktop-Aseprite round trips. Physical hardware acceptance of
animated import and linked-cel editing is still pending; CIA installation and
animation interoperability are not established by these checks.
