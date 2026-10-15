# File browser and Aseprite interoperability — 0.2.0

2026-10-14. User requested named save/open/export and desktop Aseprite interoperability, explicitly accepting the initial 128x128/four-layer/single-frame RGBA scope. Simple animation/timeline is an explicit deferred requirement, not implemented in this release. No changes to the R2 stroke rasterization or incremental render path.

## Implementation

`core/files.c` owns bounded listing, PDD1/PDD2 parsing, Aseprite parsing/export, PNG export and verified replace-with-backup writes. `ui/app.c` owns the shared browser, touch keyboard, folder creation and confirmations. Both hardware adapters use it unchanged. Paths are app-root-relative, bounded to 512 bytes; user-created names cannot contain separators or traversal. Listing is capped at 128 entries, names 63 bytes. Imported files cap at 1 MiB. Imports stage layer data before mutating the live document; malformed or unsupported data leaves pixels/history/revisions intact.

PDD2 header: 32 little-endian bytes: magic PDD2, version 2, width 128, height 128, layer count 4, reserved word, payload length, CRC32. CRC covers header bytes 0..27 followed by payload. Payload: eight visibility/opacity bytes, four zero-terminated 64-byte layer-name slots, then four 128x128 little-endian straight-alpha ARGB arrays. Total 262440 bytes. No native struct padding is serialized. Old PDD1 files remain readable. Native format and ASE store layers; PNG stores only the visible composite.

Aseprite implementation is newly authored against the [official format specification](https://github.com/aseprite/aseprite/blob/main/docs/ase-file-specs.md). Single frame, 32-bit RGBA, exactly 128x128, at most four normal layers and Normal blend. Raw and zlib image cels supported, with positive dimensions, no out-of-canvas content, no links/Z offsets and cel opacity 255. Layer names, visibility and opacity preserved; unsupported layer flags, group/tilemap layouts, animation, custom profiles and non-palette ancillary chunks are rejected. Basic sRGB/unspecified profiles and palettes are accepted. Unknown feature chunks are rejected rather than silently discarded. Four layers are exported; missing imported layers become blank. No animation data is flattened implicitly.

PNG is 128x128 RGBA with filter 0 and stored DEFLATE blocks, standard Adler/CRC checksums. zlib's inflate/CRC/Adler subset is vendored unmodified; PNG/ASE writers do not require a compression workspace. PdDocument now 918016 bytes in the host build (256 additional bytes for names). File parsing allocates outside strokes; memory peak on DS has not been measured.

Writes: refuse an existing .tmp recovery file; write/flush/fsync/close/readback; move existing destination to .bak; rename temp to destination. Rename failure attempts restoring the backup and reports failure. Existing backup rotation is confined to the selected destination. This does not promise power-loss atomicity on FAT. Legacy startup still loads the old two slots; named project resume is via Open. History/selection/viewport are not serialized.

## Verification and acceptance

- Host core/storage/history/gesture checks pass.
- Browser touch tests cover Save As, filename entry, Save, overwrite cancellation, Open cancellation and successful Open; native DS/3DS file-screen renders inspected.
- 131072 compositing comparisons and 360 incremental/full-render comparisons still pass.
- Native save/open preserves layers and names; .bak creation and overwrite refusal checked.
- Installed desktop Aseprite reads our export and saves a file that reopens with identical layer data. This installation writes raw cels; an independent Python zlib variant is additionally checked by both desktop Aseprite and our importer. See scripts/test-interop.py.
- Rejected animation, oversized canvas, chunk overflow, blend/group properties, bad CRC and bad zlib all leave the existing document byte-for-byte unchanged.
- PNG independently decoded using Pillow; size/alpha/hidden-layer behavior checked.
- NDS build/header/DLDI, 3DSX build and CIA title/version checks pass. CIA ID unchanged, TMD version 32 (0.2.0). CIA not installed.
- User accepted Save As/Open/Export in Azahar, then confirmed netloader ready. Exact accepted stable 3DSX sent by 3dslink to 192.168.1.225; exit 0. Physical acceptance pending.

Artifacts:
- NDS SHA-256: 3a4c62fb5ef8bad545fa92757c135fa73ab778d0f4565bb5e18bdd0f634bb886
- 3DSX SHA-256: b2b6fccb2860a28185973136bc2dcc0f5c204744e6be29b0932392c7ec79d128
- CIA SHA-256: 3c83144d2dc13738c1b06bb0c3bb3629dd0fe91c3c2e80f6d60fa9a4cadc0553
- Stable Azahar/hardware copy: build/test-0.2.0/PixelDoodle-files-b2b6fccb.3dsx

No source publication, updater, server changes, CIA installation or NDS deployment performed for 0.2.0. User prefers direct links for manual NDS copying.
