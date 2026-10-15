## Hands-on testing and setup

Running queue: [docs/HANDS-ON-CHECKLIST.md](docs/HANDS-ON-CHECKLIST.md).
Provider/hosting walkthrough: [docs/SETUP-GUIDE.md](docs/SETUP-GUIDE.md).

# Pixel Doodle 0.12.0

Offline solo pixel art for Nintendo DS/DSi and Nintendo 3DS. **One repository, one shared C engine and UI, three outputs: NDS, 3DSX and CIA.**

Current priorities: [agreed roadmap](docs/ROADMAP.md). Slice 1 is accepted on physical 3DS. User accepted the 0.2.0 file/Aseprite flows on physical 3DS and R2 drawing responsiveness on DS Lite. Recent palette scrolling/editing was accepted locally, with residual melonDS sluggishness motivating the latest optimization. Paired-control remapping is accepted on 3DS; slice 2 covers remaining color/palette features.

## Local community website prototype

The [community website/service](community/README.md) now runs locally at http://127.0.0.1:4178 when started with `node community/server.mjs`. Explore synthetic artwork, search/filter, visit artist pages, inspect integer zoom/tile previews and download PNGs for the handheld cache. Public read boundaries are tested. Accounts, uploads, moderation and direct handheld networking remain future work; nothing is published online. Native build version remains 0.12.0.

## 0.12.0 Community: offline cache viewer

**Menu > Community** opens a six-thumbnail gallery with top-screen preview, pages, Refresh and **Open Copy**. Opening confirms before replacing current art, then creates an unsaved imported project. Browsing never changes your artwork.

For this first stage, manually place PNGs up to 128x128 in `PixelDoodle/community` on DS/DSi, or `3ds/PixelDoodle/community` on the 3DS SD card. No folder or storage shows an empty-state explanation. Invalid images remain unreadable instead of replacing your art. This is a local cache viewer; website downloads, accounts, favorites and uploads are not connected yet.

Host checks, native layout inspection and all three builds pass. User testing remains deferred. [NDS candidate](build/test-0.12.0/PixelDoodle.nds) / [3DSX candidate](build/test-0.12.0/PixelDoodle.3dsx).

## 0.11.1 shortcut polish

On the canvas with the pen lifted, hold the configured **Zoom** button and tap **L to Undo** or **R to Redo**. Default Zoom is Y / Right; remapping also changes this modifier. Each shoulder press performs one step; holding does not repeat. Both shoulders together do nothing to history. Ordinary L/R temporary erasing remains available without Zoom.

Settings > Controls / Help > **Keys** lists the shortcuts. They do not act inside menus, confirmations or the toolbox, or during a stylus gesture. See the current [Aseprite compatibility guide](docs/ASEPRITE-COMPATIBILITY.md) for supported round trips and limitations.

Host regressions, native help layout and all three builds pass. Interaction testing remains queued for the user. [NDS candidate](build/test-0.11.1/PixelDoodle.nds) / [3DSX candidate](build/test-0.11.1/PixelDoodle.3dsx).

## 0.11.0 dither and brush presets candidate

Open **Pencil > Dither / Presets** for Solid or 25/50/75% coverage. Dither gaps retain the underlying pixels, and the pattern stays aligned to the canvas through overlapping strokes, symmetry and tiling. Eraser, fill and shapes remain solid; pixel-perfect applies to solid size-1 pencil only.

Choose one of four slots, then **Load** to use it or **Store** to replace it with your current pencil settings. Presets retain size, tip, pixel-perfect and density; your current color stays selected. Presets and active density persist separately from project files.

Interaction testing of 0.10.0 and 0.11.0 is deferred at the user's request. Candidate links and automated results follow after building.

## 0.11.0 dither and brush presets candidate

Open **Pencil > Dither / Presets** for Solid or 25/50/75% coverage. Dither gaps retain the underlying pixels, and the pattern stays aligned to the canvas through overlapping strokes, symmetry and tiling. Eraser, fill and shapes remain solid; pixel-perfect applies to solid size-1 pencil only.

Choose one of four slots, then **Load** to use it or **Store** to replace it with your current pencil settings. Presets retain size, tip, pixel-perfect and density; your current color stays selected. Presets and active density persist separately from project files.

Host checks (including 16,316 incremental/full drawing comparisons), native UI inspection and NDS/3DSX/CIA builds pass. Interaction testing of 0.10.0 and 0.11.0 is deferred at the user's request. [NDS candidate](build/test-0.11.0/PixelDoodle.nds) / [3DSX candidate](build/test-0.11.0/PixelDoodle.3dsx). Both emulators were reloaded with this candidate after the user confirmed saved sessions; no hardware transfer.

## 0.10.0 selection and PNG candidate

**Select > More** adds Duplicate to Layer and nearest-neighbor Scale 2x/3x/4x / Half Size. Duplicate keeps the original and selects the new layer for movement. Scaling preserves alpha and selection holes, uses one undo step and requires an unlocked visible layer. Scale input is currently limited to a 128x128 selection; output can fill the supported canvas. Half Size samples every other pixel and rounds odd dimensions upward. Operations that cannot fit refuse without changing art.

**Menu > Open** now previews and imports PNG artwork as one RGBA layer, with transparency and indexed palettes preserved. Use Save As to keep a layered native project. Non-interlaced RGB/RGBA, gray/gray-alpha and indexed PNGs are supported; APNG, 16-bit channels, interlacing and embedded ICC profiles are explicitly rejected. Palette import remains separate. Existing device limits and 0.9.1 smooth panning remain.

Automated, independent PNG, desktop Aseprite, and all platform build checks pass. Open in Azahar and melonDS for interaction testing. [NDS candidate](build/test-0.10.0/PixelDoodle.nds) · [3DSX candidate](build/test-0.10.0/PixelDoodle.3dsx). User accepted 0.9.1 on New 3DS XL, noting manageable lag at extreme 512-square layer/guide settings; 0.10.0 interaction acceptance is pending.

## 0.9.1 panning follow-up

Clean pans reuse the already composited artwork. Stylus and Circle Pad panning now move in screen pixels instead of jumping by entire zoomed artwork pixels. Stationary/clamped pans do not redraw repeatedly. Automated cached/full rendering and drawing-alignment checks pass; perceived hardware improvement awaits testing. The 0.9.0 large-canvas limits below remain unchanged.

Latest test builds: [NDS](build/test-0.9.1/PixelDoodle.nds), [3DSX](build/test-0.9.1/PixelDoodle.3dsx). Both emulators were reloaded and the same 3DSX sent to the ready hardware netloader.

## 0.9.0 large-canvas candidate

New Project > Size supports 256/512 presets and custom rectangular canvases up to 512 per side on DSi TWL and 3DS. DS Lite / NTR mode stays at 128. A confirmation explains possible slowdown and memory limits. Drawing, fill, shapes, selections/move, guides, save/recovery, Aseprite import/export and PNG export support the new dimensions. Copy/cut and flip/rotate currently require a selection no larger than 128x128; larger selections can still move or delete.

| Profile | Maximum canvas | Layers at 512 square | Pixel undo budget at 512 square |
|---|---|---|---|
| DS / NTR | 128x128 | Not supported | 512 KiB at 128 |
| DSi TWL | 512x512 | 2 (8 at 256 square) | 2 MiB |
| 3DS | 512x512 | 8 | 2 MiB |
| New 3DS, expanded application memory | 512x512 | 8 for now | 8 MiB requested, 2 MiB fallback |

All profiles retain a 32-transaction ceiling; dense edits consume more history than small strokes. New 3DS is detected at runtime, and its expanded budget requires at least 96 MiB in the application memory region. More layers and animation frames remain future work. Capacity figures are configured limits, not measurements of free RAM. Large open/export/preview operations can fail cleanly when temporary memory is unavailable; current artwork is retained. Large files use PDD9; small files retain PDD8 and old files remain readable. Older builds cannot open PDD9.

Host checks, desktop Aseprite/PNG interoperability and NDS/3DSX/CIA builds pass. 0.9.0 is launched in both emulators and sent to physical 3DS; interaction and DSi performance acceptance remain pending. Stable artifacts: [NDS](build/test-0.9.0/PixelDoodle.nds), [3DSX](build/test-0.9.0/PixelDoodle.3dsx).

The sections below retain the development history; this profile table supersedes earlier universal 128/eight-layer limits.

## Current editor

Slice 2 candidate adds numeric HEX/R/G/B/A entry, FG/BG swap, recent colors, GPL/HEX/PNG-strip palettes, explicit defaults and project palettes preserved in PDD5/Aseprite. Read [color/palette workflow and limits](docs/COLORS-SLICE2.md). Slice 2 is locally accepted, with its hardware test waived by the user. Slice 3 uses a custom keyboard on both consoles: staggered QWERTY, case/symbols, wide space, cursor movement/tap positioning and hold-to-delete. Select a browser file then tap ... for rename, verified duplicate or confirmed deletion; these new actions await local acceptance.

1–128 pixels per side RGBA canvas, up to eight layers and up to 32 undo/redo transactions within a fixed 512 KiB pixel-history budget. New projects start with one layer. Layers has per-row visibility and options, rename, confirmed deletion, duplicate, opacity, Normal/Multiply/Screen/Overlay, restricted Normal merge-down and 150 ms hold-to-reorder. Selecting a layer previews its contents alone on the top screen, even when hidden. Closing Layers restores the artwork overview. Memory indicators report allocated document memory and slot capacity, not free heap. See [layer behavior and compatibility](docs/LAYERS-SLICE1.md).

The square Picker has a labeled vertical palette strip and one Palette/Picker toggle. Palette supports 1-256 colors in a square-swatch grid with Add, Edit and Remove. Edit opens the selected color with Save/Back. Drag vertically to scroll either palette; hold 150 ms to reorder, with edge scrolling. Regular picker changes apply immediately. Palette changes save separately from artwork when storage is available. See [palette workflow](docs/PALETTE-SLICE1.md).

Settings > Controls / Help > Remap cycles Tools/Pan/Sample/Zoom across B/Down, A/Left, X/Up and Y/Right. Conflicting assignments swap. Return to Settings and Save to apply, or Cancel to discard; Defaults resets the draft. L/R, START and SELECT remain fixed. Old PDS1 settings load with original controls; PDS2 preserves the new mapping.

Settings > Controls / Help explains drawing bindings, palette gestures and layer controls. Help returns to the unchanged settings draft. Settings offers toolbox side and zoom-overlay side, explicit Save/Cancel, draft defaults and drawing timings.

## Build and checks

```powershell
./scripts/build.ps1              # host tests, 3DSX, CIA, then NDS
./scripts/build.ps1 -Target 3ds
./scripts/build.ps1 -Target nds
./scripts/test-host.ps1
python scripts/test-interop.py
```

Windows builds use devkitPro at C:/devkitPro and the pinned BlocksDS v1.24.0 Docker image in scripts/build.ps1. Do not replace devkitPro to build NDS. Host checks use the installed Visual Studio C++ tools. Desktop interoperability checks use installed Aseprite and Python/Pillow. See scripts for exact tool paths.

Built artifacts: [NDS](PixelDoodle.nds), [3DSX](build/3ds/PixelDoodle.3dsx), [CIA](build/3ds/PixelDoodle.cia). Copy NDS to Homebrew/Dev/PixelDoodle.nds. CIA packaging does not imply installed-CIA acceptance.

## Controls

- A / Left + drag: pan.
- B / Down: toggle toolbox; selecting Pencil, Eraser or Sample closes it.
- X / Up + tap: sample visible artwork.
- Hold Y / Right and tap plus/minus: integer zoom 1-32x, centered on the viewport. Lift before drawing again.
- L / R: temporary eraser.
- START: save; SELECT: menu.
- 3DS Circle Pad: pan with stylus lifted and no menu/tool overlay.
- Touch toolbar: Undo, Redo, Color, Layers, Toolbox, Menu/Back.

Toolbox and zoom placement are persistent. Modifier gestures consume the stylus contact until lift to avoid unintended ink. Drawing uses the DS Lite-accepted incremental damage path; overview catches up on lift.

## Files and storage

Selecting a supported PDD/Aseprite project previews it on the top screen without replacing the current drawing. RECENT in the Open/Save browser lists eight last opened/saved projects, persisted as relative paths in recent0.bin/recent1.bin. Rename/delete maintain this list; opening still requires confirmation. Preview errors retain current art. GPL, HEX and supported PNG-strip palettes preview as square swatches without modifying the current palette. PNG artwork previews are not yet implemented.

Menu offers Save, Save As, Open, Export, New, Exit and Settings. START saves a named native project, or opens Save As for untitled/imported artwork. Drag browser rows vertically to scroll; release a tap to enter folders or preview files. Up returns to the parent. Type cycles Projects/PDD/Aseprite/Palettes/GPL/HEX/PNG/All; folders stay visible. New Dir creates folders. DS and 3DS use our shared touch keyboard; no native keyboard applet. Filenames allow up to 55 bytes, with extensions added automatically; layer names allow 63 bytes. Path separators and invalid filename characters are rejected. Open and replacement prompt before discarding or overwriting artwork.

Named saves use PDD5, preserving enabled layers, names, visibility, opacity, blend and order. PDD1-PDD4 remain readable. Aseprite import/export supports the current 128x128 single-frame RGBA subset, up to eight layers and supported blend modes; unsupported features are rejected transactionally. PNG export contains the visible composite with alpha. Animation, larger canvases and PNG artwork import remain planned. See [file milestone](docs/FILES-0.2.0.md), with subsequent format changes in [layer notes](docs/LAYERS-SLICE1.md).

Writes use temporary files, verification and backups; this is not a FAT power-loss guarantee. Preserve artwork, recovery files, settings0.bin/settings1.bin and palette0.bin/palette1.bin during deployment. Global palettes and settings are separate from projects. Missing storage allows drawing and session palette editing but cannot persist changes. The browser stays under the app's storage directory. Platform adapters select their app-owned storage root; never mix it with Collab Doodle identity/settings paths.

## Testing and acceptance

Use Azahar and melonDS for local iteration; the user operates the emulators. Do not automate their drawing or terminate unsaved sessions. Before wrapping a slice, test the exact accepted candidate on hardware. Physical 3DS netloader address supplied by user: 192.168.1.225, default port 17491. H: and I: were used for DSi LL and DS Lite cards; verify mounts before any authorized copy. Always link the NDS candidate for manual copying.

Host checks cover core/history/storage, file rejection, settings recovery, palette editing/scrolling, layer operations and UI gestures. Rendering checks include 131072 compositing comparisons and 360 incremental drawing frames. Latest optimized palette scrolling matches full redraws on DS and 3DS layouts. Emulator/build success is not hardware performance or durability acceptance.

## Design and provenance

Read [AGENTS.md](AGENTS.md) before changes. [Roadmap](docs/ROADMAP.md) supersedes older planning defaults. [Prototype notes](docs/PROTOTYPE-0.1.0.md), [provenance](docs/PROVENANCE.md), [research](docs/RESEARCH.md) and [format/replay planning](docs/FORMATS-AND-REPLAY.md) retain supporting detail. Offline work never requires an account. Community/gallery, sharing and animation are future milestones; no website or server is deployed by this prototype.

## Slice 3 project setup and recovery

New Project offers 128x128 transparent, white or current-background-color setup, one to four starting layers, and the default or current project palette. Creation requires replacement confirmation; variable canvas sizes remain slice 4.

Recovery defaults on each session. After two minutes and at least two seconds of pen-up idle on the canvas, changed unsaved art is written to `.recovery/session.pdd`, with the existing verified temporary/backup write flow. This does not overwrite named saves, clear the unsaved indicator, or change undo. Storage writes are synchronous at idle and may briefly pause input; hardware timing remains to be measured.

Startup offers recovery when a snapshot, backup or interrupted temporary exists. Restore validates the primary, then temporary, then backup and opens a dirty untitled project: Save As to retain it. Restore/cancel and confirmed discard preserve current art unless replacement is confirmed. Keep for Later protects the previous recovery and pauses automatic snapshots until Restore or Discard. Menu > Recovery can reopen this screen and toggle autosave for the current session. Preserve the hidden `.recovery` directory during deployment. Recovery is best-effort local storage, not a power-loss guarantee; named saves remain the durable user workflow.

Slice 4 history foundation (2026-10-14): browser/palette previews/project setup/recovery accepted locally by user; slice 3 hardware acceptance remains pending. Shared undo now stores changed 8x8 XOR tiles in a fixed 512 KiB arena, up to 32 transactions. Dense edits fall back to 64 KiB raw XOR, preserving eight full-layer transactions. Property-only edits consume metadata only. Oldest transactions are evicted by byte/entry limits; branching discards redo. No allocation during drawing or commit. Layers preview reports actual undo cursor/count and used pixel-history KiB. PdDocument is 923288 bytes on the host (2560 bytes above prior metadata size); arena remains fully reserved. Canvas remains 128x128, four layers. Variable dimensions/more layers and measured hardware performance remain outstanding. Host sparse/dense eviction/branch/cancel/metadata and drawing-oracle checks pass; emulator acceptance pending.

Variable dimensions (2026-10-14): user caps canvases at 128 per side; larger profiles are no longer planned. New Project > Size offers 16/32/64/128 square presets and custom independent width/height 1–128. Tap W/H, type digits, tap Set, then Done. New small projects fit at an integer zoom; drawing, panning and overview respect dimensions. PDD6 adds validated dimensions in the existing width/height header fields while retaining the PDD5 fixed-stride payload and CRC; legacy PDD1–5 load as 128x128. Aseprite and PNG serialize only actual dimensions; recovery uses PDD6 too. Smaller canvases currently retain the same preallocated RAM capacity. Native/ASE roundtrip, bounds, rectangular drawing and desktop PNG dimensions/pixels are checked. Local/hardware acceptance pending.

Eight-layer candidate (2026-10-14): shared capacity is eight layers on NDS, DSi and 3DS, still defaulting to one. New Project supports one through eight starting layers. Quick vertical drag scrolls the layer list; hold 150 ms then drag reorders, with edge scrolling. Visibility/options remain per-row. Core uses eight stable slots and an eight-bit enabled mask; undo pixel budget stays 512 KiB. PdDocument is 1185976 host bytes; extra slots add about 256 KiB. Calculated NDS app/document + two presentation buffers + native import staging/file buffer is about 2.6 MiB before code/SDK/stack overhead; this is not measured free RAM. Hardware full-document stress/performance acceptance remains pending.

PDD7 writes eight property pairs, eight 64-byte names, eight fixed 128-stride pixel planes, eight blend bytes, eight order bytes, then the existing count/256-color palette payload. Header layer capacity is 8; width/height remain 1–128. CRC and atomic-write workflow unchanged. PDD1–6 reads preserve their four-slot wire layouts and expand into eight-slot memory. Recovery also uses PDD7; Aseprite import/export supports up to eight flat single-frame RGBA layers. Higher counts reject transactionally. Preserve existing projects and recovery files; old builds cannot read PDD7. User accepted variable sizes locally; this candidate awaits local/hardware tests.

Layer protections (2026-10-14): layer options now have undoable Lock and Alpha Lock toggles, with X to close. Lock blocks pixel writes, erase, deletion and destructive merges; visibility, opacity, rename and reorder remain available. Alpha Lock preserves existing per-pixel alpha, blends brush RGB by brush alpha, skips empty pixels and ignores erase. Merge into alpha-locked layers is refused. PDD8 appends eight validated protection bytes (bits 0/1) after the PDD7 palette payload; PDD1–7 remain readable. Native recovery preserves both flags. Aseprite editable bit preserves layer lock; alpha lock is native-only and export reports this. Document memory size unchanged. Host protection/UI/persistence checks pass. Eight-layer candidate and protection hardware acceptance remain pending before closing slice 4 and starting brush size/grid.

Brush/grid candidate (2026-10-14): toolbox > Brush/Grid offers independent pencil/eraser sizes 1–16, square/round tips and pixel-grid toggle. Grid shows at 8x+ only on the editing canvas, never in artwork, exports or sampled colors. Both render paths preserve the grid. Brush footprints clip to canvas bounds and contribute to incremental damage. Even sizes extend one extra pixel left/up. Alpha-locked paint samples the pre-stroke pixel so overlapping stamps do not repeatedly accumulate color within a stroke. Protection applies to all stamps. Tool preferences persist on pen release in tools0.bin/tools1.bin (PDB1, 20 bytes, sequence, two size/tip fields, grid flag, CRC at byte 16; dual-slot readback). Preserve these files during deployment. Host tests cover tips/sizes/edges/undo/preferences and 1455 incremental/full render frames. User accepted protections locally and requested continuing tools; slice 4 hardware acceptance remains pending. Fill is the next tool after brush/grid acceptance.


Guides and brush popup (2026-10-14): user accepted brush drawing on physical 3DS. Top pencil/eraser icon now opens a compact size 1–16 / square-round popup over the canvas. Tap outside to dismiss without drawing; Tools or B opens the toolbox. Selecting Pencil/Eraser also opens its popup. Toolbox # Guides separates pixel grid, Symmetry X/Y and Tile X/Y. Symmetry reflects actual stamped pixels about canvas center (including even tips). Tile axes both wrap brush footprints and allow drawing on repeated previews; unwrapped input coordinates prevent seam-crossing chords. Preview/grid do not alter exports. Tools slots now write PDB2, retaining PDB1 reads; byte 13 stores four guide bits, existing CRC/dual-slot workflow retained. Preferences are device-local, not project fields. Ordinary drawing retains its incremental render path; guided strokes currently redraw the full canvas, so guided-mode DS performance needs user testing. Core seam/mirror/undo, UI repeated-copy input, repeated-preview oracle, preference reload and prior 1455 incremental-frame checks pass; NDS/3DSX/CIA build and header checks pass. Local and hardware acceptance of this candidate pending.


## 0.3.0 guided drawing follow-up (2026-10-14)

The header and package descriptions now share core/version.h (0.3.0); CIA metadata packs major/minor/patch into the platform version field (48 for 0.3.0). Toolbox toolbar button again opens all tools. Pencil/Eraser rows open their compact options popup; keep this tool/options separation for future selection variants. Guides uses a drawn lattice glyph, not a text #. Tiled canvas has a one-screen-pixel gold border around the original canvas, clipped below the toolbar; exports stay unchanged.

User reported severe DS/DSi guided drawing latency in the preceding candidate. Guided strokes now record actual changed pixels in a fixed 2 KiB app-owned bitset, composite only those pixels, repaint their visible wrapped copies, and defer top-screen work until pen lift. Ordinary drawing keeps its existing damage path. Host incremental/full equivalence covers 2618 stroke frames including symmetry and tiling at both screen sizes/all zooms, alpha/erase/undo. Native performance acceptance remains pending; do not claim measured hardware speed from host tests. No document or artwork format change. Preserve PDB2 tool slots.



## 0.4.0 fill and tiled panning (2026-10-14)

User accepted the 0.3.0 responsiveness improvement locally. Enabled tile axes now pan from one viewport before the canvas through one viewport beyond it; non-tiled axes keep their original limits. Disabling an axis clamps its viewport immediately. The top overview viewport marker is clipped to the artwork so off-canvas panning cannot draw over the header/status.

Toolbox adds Fill: exact RGBA matching, four-connected, active layer only, one tap/one undo transaction. Uses a bounded iterative queue and visited bitmap (34 KiB temporary heap, freed after fill), no recursion. Allocation failure leaves artwork untouched. Lock/hidden layers refuse; Alpha Lock retains alpha and blends RGB using foreground alpha, skipping empty pixels/transparent erasure. Enabled tile axes connect opposing region edges. Symmetry fills independent regions from mirrored seed points, each matched against the pre-fill layer. No tolerance or all-layer sampling yet. L/R still temporarily erases. No project format change. Host tests cover region barriers, diagonals, mirrored seeds, tiled connectivity, 1x128 and full 128x128 canvases, alpha/protection, undo/redo/no-op, one-fill-per-gesture and pan limits. 3494 incremental/full render comparisons cover off-canvas panning. NDS/3DSX/CIA build checks pass; 0.4.0 local/hardware acceptance pending. Next tool milestone is rectangle selection/move/copy/cut/paste.



## 0.5.0 selections and clipboard (2026-10-15)

User accepted 0.4.0 in melonDS/Azahar and requested lasso plus add/remove selection in this slice. Toolbox > Select opens Rectangle/Lasso and Replace/Add/Remove, Copy/Cut/Paste, Select All/Deselect, Delete and Done. Replace-mode drag inside an existing selection moves active-layer pixels with live preview and one undo transaction on lift. Pan, a tool shortcut, toolbar contact or temporary erase/sample cancels an in-flight move. Drawing tools and fill respect the mask. Selection geometry itself is session UI state (not in pixel undo); undo/layer changes/project replacement clear stale masks. Clipboard survives project/layer changes during the session, not application restart.

Shared fixed 128-stride masks preserve irregular shapes and holes. Lasso is a bounded 512-point even-odd polygon at pixel centers; an overflowing gesture preserves the old selection with a notice. Rectangle and lasso combine through the same replace/union/subtract operations. Move stays entirely within canvas bounds, never silently clips selected pixels; selection gestures on repeated tiles map to the original canvas and clamp at its edges. Symmetry does not duplicate selection geometry/moves. Paste uses selected RGBA replacement, including transparent selected pixels, with alpha-lock semantics; unselected holes remain untouched. Copy reads the active layer, not merged visible layers. Clipboard too large for the destination rejects unchanged. Move/cut/delete require a visible layer without Lock or Alpha Lock. Copy remains allowed on protected layers. Paste respects Lock/visibility/Alpha Lock.

App-owned clipboard/mask/lasso state adds approximately 75 KiB fixed RAM; document/history/file formats are unchanged. Selection drags defer top-screen work. Move preview recomposites the union of the old/new/source bounds; ordinary painting retains incremental damage. Host checks cover concave lasso against an independent point-in-polygon reference, overlap moves/cancel/undo, clipboard/protection, add/remove UI and clipping paint/fill. 4079 incremental/full render comparisons include selections and tiled drawing. NDS/3DSX/CIA builds pass. Native-size popup/canvas fixtures inspected. User-visible/hardware acceptance of 0.5.0 pending.



## 0.5.1 selection quick actions and alpha (2026-10-15)

Select mode now reserves a 26-pixel quick strip below the main toolbar: Rectangle/Lasso toggle, Replace, Add, Remove, Copy, Cut, Paste and Deselect. Modes highlight and show explanatory status text. Canvas centering/pan/zoom use the reduced viewport so the strip never hides its top edge. The full popup remains available for select-all/delete and labeled commands.

Supersedes 0.5.0 clipboard replacement semantics: move and paste use straight-alpha source-over. Fully transparent selected source pixels do not erase destination art; partial alpha blends against destination alpha/RGB. Move clears the original selected region, then composites from the unchanged transaction snapshot, so overlapping previews never accumulate blending. Paste honors alpha lock, preserving destination alpha. After paste the active mask includes only nontransparent clipboard pixels, avoiding subsequent movement of background beneath transparent clipboard areas. These are committed raster edits, not a floating paste layer. Copy/cut preserve original RGBA. Undo restores the exact prior pixels. Tests cover transparent and semitransparent move/paste over opaque and translucent destinations, alpha lock, quick-strip actions, and existing 4079 incremental frame comparisons. NDS/3DSX/CIA builds pass; native-size quick-strip layout inspected. 0.5.1 emulator/hardware acceptance pending.



## 0.6.0 shape tools and selection transforms (2026-10-15)

User accepted 0.5.1 locally. Toolbox now has a second column for Line, Rectangle and Ellipse. Shape options share pencil size/tip/color; rectangle and ellipse support outline or filled interiors (filled interiors ignore outline size). Outline width uses the pencil footprint. Integer ellipse rasterization supports odd/even bounds and degenerate single-pixel rows/columns. Drag previews replace the prior preview from the transaction snapshot; lift commits one undo entry. Pan, zoom, sample, temporary erase, toolbar or other button actions cancel pending shapes. Shape strokes honor selection masks, layer protection, symmetry and tiling. Extents are bounded to 128 pixels per axis; normal canvas endpoints clamp to dimensions, tiled coordinates remain unwrapped for seam crossings.

Preview rendering retains only old/new changed-pixel masks, updates their visible copies and defers top-screen work. No allocation during shape previews. Select > More adds Flip Horizontal, Flip Vertical and Rotate 90 CW for the masked active-layer content. Transforms retain alpha/source-over behavior and selection holes; one undo entry. They require visible layers without Lock/Alpha Lock. Rotation that cannot fit rejects; otherwise its origin clamps so no selected pixels are clipped. Transform scratch is one temporary PdClipboard (about 66 KiB), freed immediately; allocation failure leaves art unchanged. Ordinary clipboard contents survive transforms. Selection geometry itself remains session state, not pixel undo.

Host checks cover line/rectangle/ellipse rasterization, ellipse symmetry, fill/outline, mask/guide clipping, old-preview restoration and cancellation on both screen sizes, plus flips/rotation/alpha/fit refusal/undo. Prior 4079 ordinary incremental-frame comparisons pass. Native-size toolbox, shape popup and transform popup inspected. NDS/3DSX/CIA builds pass (CIA version 96). 0.6.0 emulator/hardware acceptance pending.



## 0.7.0 existing Fill options (2026-10-15)

User accepted 0.6.0 locally and explicitly confirmed these belong in the existing Fill tool. Toolbox > Fill now opens one popup: Contiguous Region / Global (Replace Color), Match Active Layer / Visible Image, tolerance 0–255 with minus/plus and captured slider, Done. Default remains contiguous/exact/active layer. Global changes all matching pixels within the selection and canvas; contiguous traverses four-neighbor connectivity with enabled tile wrapping. Visible mode matches the unchanged composited image but writes only the active layer. Layer lock/visibility/alpha lock, mirrored seed points and one transaction per fill remain. No extra toolbox tool.

Tolerance compares each canonical RGBA channel to the original seed (maximum channel difference, not a drifting neighbor-to-neighbor threshold). Fully transparent RGB is ignored by canonicalizing zero-alpha colors. For visible matching, composite is snapshotted before any fill writes. Each mirrored region traverses independently to avoid tolerance-dependent bridge omissions; repeated writes use pre-fill values so alpha lock does not accumulate. Fixed 34 KiB temporary fill scratch remains. No project format changes.

Tool preferences now write PDB3 in the same 20-byte dual slots; PDB1/PDB2 read compatibility retained. Byte 14 stores global/visible bits; byte 15 tolerance. CRC/readback unchanged; preserve tools0.bin/tools1.bin and do not downgrade. Host checks cover global/disconnected versus contiguous matching, tolerance boundaries, visible boundaries on another layer, selection limits, transparent RGB canonicalization, UI and preference reload; prior shape/selection/history/render checks pass. Native popup inspected. NDS/3DSX/CIA builds pass (CIA version 112). 0.7.0 local/hardware acceptance pending.



## 0.8.0 pixel-perfect pencil (2026-10-15)

User accepted 0.7.0 locally. Pencil popup adds Pixel Perfect, off by default, active only for size-1 pencil strokes. Larger brushes, eraser and shape tools retain their raster behavior. The shared bounded stroke state removes the middle pixel of consecutive orthogonal unit steps making a diagonal corner. It restores the exact pixel values captured immediately before that stamp (including up to four unique reflected positions), preserving pre-existing art and earlier ink at overlaps. Tile coordinates remain unwrapped for path decisions; wrapped/reflected restoration participates in presentation damage. Alpha lock and selection clipping remain. State resets for new strokes, gaps and temporary erase/larger brush paths. No allocations or full redraws are introduced while drawing.

Tool slots now write PDB4, still 20 bytes with CRC/readback and PDB1–3 compatibility. Byte-14 bit 2 persists pixel-perfect; global/visible fill bits and tolerance remain. Preserve tools0.bin/tools1.bin, no downgrade writes. Host checks cover corner restoration of existing semi-transparent pixels, alpha lock, symmetry/tile damage, undo/redo, persisted toggle and DS/3DS partial/full equivalence. Prior 4079 incremental frames and all other tests pass. NDS/3DSX/CIA builds pass (CIA version 128); native popup inspected. 0.8.0 emulator/hardware acceptance pending.


### App identity assets

The community five-square logo is used for the NDS banner icon, 3DSX SMDH, CIA icon and HOME Menu banner. Sources and generated rasters live in assets/. To regenerate icons run `python scripts/generate-icons.py` (Pillow required). `python scripts/verify-icons.py` verifies packed NDS/3DSX icon pixels after building. The CIA banner generator also uses Pillow. Icon-update candidates are in build/test-0.12.0-icons; native version remains 0.12.0.
