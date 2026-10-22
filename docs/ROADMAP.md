# Pixel Doodle agreed roadmap

## Community milestone status (2026-10-16)

The latest local increment adds schema-5 favorites and resumable console-upload APIs, expands bearer HTTP transport, moves 3DS pairing onto a worker, and fixes the NDS begin-response scope check. Automated PHP (35 tests / 312 assertions) and host checks, plus NDS/3DSX/CIA builds pass. Local 0.3.5 server packages extract and pass content checks; hashes are in build/PixelDoodle-Community-0.3.5-SHA256SUMS.txt. Handheld online gallery/favorites/My uploads/submission UI and chunk-stream integration remain unfinished; this is not an end-to-end completed community milestone. No deployment or hardware transfer was performed.

2026-10-14. This roadmap supersedes conflicting priorities and account defaults in PLAN.md, GALLERY.md and FORMATS-AND-REPLAY.md. Those documents retain useful engineering detail, not implementation or acceptance claims.

## Baseline and workflow

One shared core/UI builds NDS, 3DSX and CIA. Offline editing never requires an account. R2 drawing responsiveness is accepted on DS Lite. The 0.2.0 Aseprite save/load/export/import flows are accepted by the user on physical 3DS after Azahar testing. CIA installation remains untested.

Each slice gets automated checks and native-size layout inspection, then user interaction in Azahar and melonDS where applicable. The user operates emulators. Test hardware before wrapping each slice, more often when input, performance or storage needs it. Send the exact accepted candidate through 3dslink when the netloader is ready; link NDS artifacts for manual copying. Emulator success does not establish hardware acceptance. Preserve the smooth incremental drawing path throughout.

## Community web 0.1.0

Local read-only gallery website/service implemented with synthetic public/hidden fixtures, search, tag filters, paging, artist/detail pages, zoom/tile preview and handheld-sized PNG downloads. HTTP boundary tests and browser flows pass. No deployment or native networking. Durable storage/validated derivatives, verified handheld transport, account pairing and uploads/moderation remain. See community/README.md and GALLERY.md.

## 0.12.0 Community foundation

User deferred editor testing and requested continued work. First handheld gallery stage is a real offline PNG cache browser with thumbnail pages, selected-art preview, refresh and confirmed import. The cache is manually populated; no online service/account/moderation authority is implied. Website/service and verified handheld transport are next, followed by pairing/favorites/uploads. Direct DS/DSi/3DS uploads remain required before Community is complete. Existing user acceptance is not inferred from the new build.

## 0.11.1 static-studio polish

User deferred interaction testing and requested continuing. Canvas history shortcuts now use the remapped Zoom modifier plus L/R, with pen/menu isolation and a Keys help tab. The current Aseprite subset is documented in ASEPRITE-COMPATIBILITY.md. Optional DS pressure is deferred pending calibration and device testing; it must not compromise the accepted fixed-size drawing path. Community remains the next major planned area after accumulated editor acceptance; animation stays later.

## Current follow-up: 0.11.0 pencil dither and presets

User asked to continue and will test the accumulated changes later. 0.10.0 selection scaling/duplication and PNG artwork import are built, with interaction acceptance pending. 0.11.0 adds pencil-only ordered dither at 25/50/75% plus Solid, and four persistent Load/Store brush presets in the existing pencil options. Host checks, native UI inspection and all platform builds pass. See AGENTS.md for coverage/alpha semantics and PBP1 storage. Hardware/perceived responsiveness remains unverified for these candidates.

Next static-editor follow-ups remain optional calibrated DS pressure, shortcut polish and clear Aseprite subset documentation before community work; animation/timeline remains deferred. Larger selections and more New 3DS layer/frame capacity remain explicit limits, not silently implemented by these brush changes.

## Capacity milestone: up to 512 on DSi and 3DS

Requested 2026-10-15 after physical 3DS acceptance of 0.8.0 pixel-perfect pencil. This supersedes the earlier 128-only decision and the provisional capacity experiments below. The first implementation is the 0.9.0 candidate; host/build/desktop interop checks pass, emulator and hardware acceptance remain pending. See AGENTS.md for exact enabled budgets and current clipboard/file-buffer limitations.

- DS Lite and all NTR execution: 1–128 per axis. DSi TWL and 3DS: target 1–512 per axis, including rectangular canvases. Detect runtime capabilities in the platform adapter; do not infer extra RAM from a UI label.
- Add 256/512 presets on eligible hardware and a confirmation for either dimension above 128: “Large canvases may be slower. Available layers, undo and some tools depend on memory.” Show concrete limits/reasons wherever an operation is unavailable. Never silently remove layers, flatten art or drop an edit's undo.
- Replace embedded maximum-sized pixel planes with owned, dimension-based storage. Allocate layer planes as needed, and reserve transaction/composite buffers before replacing the current project. Keep ordinary small-canvas drawing allocation-free. Allocation failure must preserve the existing artwork and explain the failed operation.
- Budget resident artwork, composite, transaction snapshot, history, masks, clipboard, rendering buffers and peak file/tool scratch together. One 512-square RGBA plane is 1 MiB. Determine layer limits from a conservative platform budget and allocation checks; do not promise eight large layers on DSi. Report estimated capacity separately from measured free heap.
- Generalize stride/indexing everywhere, including damage, mirrored/wrapped strokes, selection and clipboard. Replace 16-bit flood-fill indices for canvases above 65536 pixels. History's fixed 16-by-16 tile map and 512 KiB arena cannot represent a dense 512-square XOR transaction: give large edits an explicit preflight/history policy before mutation.
- Introduce a dimension-aware native project format with old PDD1–8 read compatibility. Stream or otherwise bound save/load/verification memory rather than keeping several full copies. Update ASE/PNG dimensions, parser limits, browser previews and recovery consistently. Reject files exceeding the device profile without replacing the current project.
- Validate 129, 256 and 512 boundaries, rectangular extremes, fill indices above 65535, undo/redo, selection/transforms, pixel-perfect guides, legacy saves, large native/ASE/PNG round trips, corrupt input and allocation failures. Retest small DS drawing. Launch Azahar and melonDS for user testing, then measure DSi and 3DS hardware before declaring the capacity milestone accepted.

User follow-up on 2026-10-15: keep dimensions capped at 512 even on New 3DS. Use available application memory to expand layer/frame/history budgets in later milestones. 0.9.0 detects New 3DS and adds the larger history profile; eight layer slots and single-frame files remain the current format/UI ceiling. Do not derive available app memory from physical RAM totals alone.

## Current milestone status (2026-10-15)

0.9.1 accepted on New 3DS XL, with manageable heavy 512-square layer/guide slowdown noted. 0.10.0 adds selection duplication, integer nearest-neighbor scaling and PNG artwork import; automated/build checks pass and emulator acceptance is pending. This completes the initial selection/transform implementation within the documented clipboard/scale-input bounds. Next after acceptance: static studio tools, starting with dither and brush presets. Animation and increased New 3DS layer/frame capacity remain later work.

## Ordered slices

1. **Workspace and controls.** Collab Doodle visual language, original icons, collapsible toolbox, selected-tool feedback and contextual options. Bottom bar: undo/redo/color/layers/menu. Top: overview with viewport outline, project/layer/zoom/dirty status. Hold Y/Right and tap large plus/minus buttons; integer 1–32x, center anchor, no ink until lift after modifier gestures. Balanced bindings, Circle Pad pan, handedness/overlay side, help and remapping. Persistent settings with recovery/reset. Review native-size layouts before replacing the entire workspace.
2. **Color and palettes.** HSV/RGB/hex/alpha, foreground/background swap, recent colors, ordered editable swatches. Import/export GPL, HEX and palette-strip PNG. Global default and project palettes; up to 256 entries. RGBA documents first, indexed mode later.
3. **Files and project workflow.** Shared custom touch keyboards on 3DS and DS, cursor/case/symbols/repeat-delete. Browser thumbnails, paths, paging, recent files, rename/duplicate, confirmed deletion and recovery. New-project presets, optional resume, idle recovery autosave separate from the named project.
4. **Capacity and layers.** Variable dimensions and more than four layers. Stable layer IDs, add/delete/rename/duplicate/reorder/merge, visibility/lock/alpha-lock/opacity. Bounded tile-based history/storage. Measure full dense documents, scratch and undo before enabling limits.
5. **Essential tools.** Pencil/eraser size and square/round tips, pixel-perfect pencil, line/rectangle/ellipse outline and fill, bounded flood/global fill, tolerance, active/composite sampling and color replacement.
6. **Selections and transforms.** Rectangle/freehand, add/subtract, cut/copy/paste/move/duplicate, flips, quarter-turns, integer nearest-neighbor scaling, PNG import, symmetry, wrapping and tile preview.
7. **Static studio completion.** Dither, brush presets, Normal/Multiply/Screen/Overlay, optional calibrated DS/DSi pressure, shortcut polish and explicit Aseprite compatibility subset.
8. **Community.** Gallery browsing first; pairing, favorites and uploads next. Direct uploads from DS, DSi and 3DS are required before this milestone is complete. Website precedes animation. No comments or DMs initially.
9. **Animation.** Frames/durations, add/duplicate/delete/reorder, playback, onion skinning and frame/layer UI. Multi-frame Aseprite round trip and sprite-sheet/GIF export. Frames also support alternate artwork variants.
10. **Process recording.** Versioned editing events, checkpoints, seeking and playback, followed by website viewer. Process replay is separate from sprite animation; publishing it is explicit opt-in.

Capacity targets are now DS/NTR up to 128 per side and DSi TWL/3DS up to 512 per side. Layer/feature budgets for large canvases are not yet enabled or measured; the next capacity milestone above governs implementation.

## Customization

Settings groups: workspace, input, canvas/grid, tools, palettes, files/recovery and online. Separate global defaults from project properties. Include handedness, toolbox/labels, overview, checkerboard/background, pixel grid (optional automatic display at 4x), tile-grid size/offset/color/opacity, tool defaults and palette defaults. Pressure defaults off. Proposed recovery interval is two minutes at idle/pen-up; never silently overwrite the user's named save. No networking during drawing.

## Community and accounts

Browse approved gallery artwork anonymously on website or handheld: recent art, tags/search, artist profiles, paged thumbnails, top-screen selected art, touch navigation, zoom/pan and actual size. Use bounded caches and device-appropriate derivatives. Favorites require sign-in. Artwork submissions are console-only; web upload UI and API stay disabled. My uploads shows published/pending/declined and any moderation reason. Downloads expose flattened images; layered originals only with creator opt-in and local compatibility checks.

Community actions requiring an account initiate pairing: console shows an expiring code and website URL; browser signs in or creates an account through email magic link/code or Discord OAuth with minimal identity permissions. User explicitly approves that device. Device receives scoped revocable credentials, never Discord tokens or email login secrets. Pairing user code and private polling credential are distinct. Rate-limit and expire both flows.

One account can link verified email and Discord through authenticated website settings. Verify each new method, do not auto-merge accounts by matching email text, handle existing-account conflicts explicitly, and do not unlink the last usable login method. Use maintained authentication/session components, CSRF protection and secure cookies. Email links are single-use and short-lived; no long-lived credentials in URLs/logs/art files. Account-free offline work remains available. DS verified TLS and trustworthy independent entropy provisioning are required; no plaintext fallback or copied Doodle credentials.

## Sharing and moderation

Upload an explicit saved immutable revision after previewing image/title/tags. Public still by choice; editable source is separately opt-in. No automatic private backups to Discord and no full cloud sync initially. Keep service/database/storage/credentials separate from Collab Doodle. Validate uploads in a bounded isolated worker.

Publication starts pending. Send a private Discord notification with image, artist, title, dimensions, submission ID, deadline and authenticated moderation link. Confirm initial delivery and retain message ID. Eight-hour autoapproval begins at successful initial notification delivery. Failed or uncertain initial delivery requires manual review; notification retries do not automatically re-arm publishing. Approve immediately, deny with optional artist-visible reason, or Hold to disable the timer. Durable state transitions prevent timers from publishing denied/held submissions. Changed pixels require a new moderated revision; published art can be removed.

Moderation links open an authenticated website; viewing a link never performs approval. Webhook credentials stay server-side; suppress mentions and audit decisions. Validate notification failures, deadlines, holds, denial races, account/device revocation, file parsing and cross-account access in staging before any live deployment.

## Slice 1 progress

Slice 1 accepted on physical 3DS by user after remapping candidate test: drawing, menus, palettes, layers and controls feel good. Final small follow-up prefills PNG/ASE export names from the current saved/opened project basename, removing only the final extension; untitled projects remain blank. This follow-up is host/build checked, not separately hardware accepted. The first slice is closed; remaining color/palette features proceed in slice 2. Earlier pending statements below are historical.

User accepted remapping locally. On 2026-10-14, sent the exact build/test-slice1-remap/PixelDoodle.3dsx candidate (SHA-256 224C01AA14DB7E9665AED2083991AC6B9EB7939A6036C40B0F7D38BDAAEE90E0) to the ready 3DS netloader at 192.168.1.225 using 3dslink; exit 0. Hardware interaction acceptance is pending before closing slice 1.

User accepted palette performance/help locally. Paired-control remapping candidate adds Help > Remap: cycle Tools/Pan/Sample/Zoom across B/Down, A/Left, X/Up and Y/Right. Conflicts swap, preserving all four actions. Settings Save/Cancel/defaults include bindings; active help and status follow saved assignments. L/R, START and SELECT remain fixed; per-button/chord remapping is not implemented. PDS2 extends the 16-byte settings record with a validated four-action permutation at byte 10; PDS1 loads original bindings. Host migration, persistence, cancel and invalid-map recovery tests pass. Local remapping and slice hardware acceptance remain pending.

Palette scroll/edit UI accepted locally; user reported residual melonDS sluggishness. Follow-up routes scroll/reorder redraws through the color-only path, skips artwork/top rendering, limits swatch iteration to visible rows, computes a hue ramp once per field and two checker shades per alpha-slider column. Both native-size scroll outputs match full redraws in host checks. Perceived performance awaits user retest. Settings now offers Controls / Help with Draw, Color and Layers tabs; returning preserves settings drafts. Remapping and hardware acceptance remain outstanding.

Square picker + editable palette follow-up: one Picker/Palette toggle, vertical four-swatch strip on Picker, square-swatch scrolling Palette without opacity slider, 1–256 RGBA entries, Add/Edit/Remove with picker Save/Back, 150 ms hold-to-reorder and edge scrolling. Global palette dual-slot persistence is independent of art. See PALETTE-SLICE1.md. This brings basic palette editing into slice 1; file import/export and project-specific palettes remain later work.

Layer row-controls follow-up: per-row eye/menu, opacity slider, supported blend modes, duplicate, restricted Normal merge-down, compact delete confirmation, 150 ms hold-to-reorder, scrollbar placeholder and honest used/free slot + allocated document memory indicators. Persistence advances to PDD4; see LAYERS-SLICE1.md. Four slots remain the cap, not a computed free-RAM promise. This moves these layer controls into slice 1; larger capacity remains deferred.

User requested basic layer management in slice 1: one-layer defaults, New, Rename, confirmed Delete and top-screen selected-layer preview. Implemented at the existing four-layer capacity, with undo/redo and PDD3/Aseprite persistence. See LAYERS-SLICE1.md for compatibility and validation. This moves basic layer management forward; larger capacity, reorder/merge and remaining advanced layer controls stay in the later capacity slice.

Latest color UX decision: color and alpha changes apply immediately in both Picker and Palette. Remove Use Color/Cancel and draft semantics; tapping the Color shortcut again or leaving via another menu keeps the selected color. The field/palette gains 24 pixels of height; toolbar color updates during drag. This supersedes earlier draft/apply/cancel notes. Settings still use explicit Save/Cancel. Host gesture and incremental/full picker comparisons pass.

User accepted the darker menus and 3DS picker, but reported picker drag latency in melonDS. Follow-up skips artwork recomposition/top-screen presentation during picker-only changes, reuses the existing bottom-screen HSV field when hue is unchanged (restoring only the previous cursor), and skips stationary updates. No extra framebuffer allocation. Host incremental/full picker output comparisons pass on DS/3DS layouts. Dedicated Picker/Palette tabs share a draft; Palette offers eight larger preset swatches and alpha adjustment. Native DS palette render inspected. All builds pass; improved perceived latency awaits local/hardware feedback. Custom palette editing/import/export remains later work.

User accepted settings/panning locally and requested stronger menu contrast plus color-picker work before closing slice 1. Candidate now uses the toolbox dark theme across menus, outlined controls, white labels, bright teal accents and matching top status. Color picker moves into slice 1: live HSV field/hue strip, alpha slider, old/new preview, RGB/hex readouts, eight preset swatches, explicit Use Color/Cancel. Closing the picker also discards the draft. Exact numeric entry, palette-file support and editable palettes remain slice 2. Native DS/3DS menu fixtures reviewed; host color gesture/apply/cancel and existing drawing checks pass. Hardware acceptance is still pending.

User accepted the icon/toolbox candidate locally. Settings candidate adds Menu > Settings with toolbox left/right placement, zoom side follow-toolbox/left/right, draft defaults, explicit Save/Cancel, and CRC-validated dual settings slots (settings0.bin/settings1.bin in the app directory). Corrupt-newest fallback and persistence are host-tested; no power-loss guarantee. 3DS Circle Pad pans only with stylus lifted and canvas unobstructed. All builds and rendering regression checks pass. Remapping/help and hardware acceptance remain outstanding; this is not a completed slice.

Hold-to-zoom candidate accepted by user locally. Next candidate adds an original icon bar (undo, redo, color, layers, toolbox, menu/back), collapsible pencil/eraser/sampler toolbox and overview viewport outline. B/Down toggles toolbox; selecting a tool collapses it and consumes the gesture through lift. L/R remains temporary erase. Host gesture and incremental-render checks plus all platform builds pass; DS native toolbox render inspected. User layout acceptance, settings, Circle Pad, handedness/remapping/help and hardware acceptance remain before this slice is complete.

## Slice 2 and slice 3 progress

Slice 2 color/palette implementation is ready for local testing; see COLORS-SLICE2.md for formats, UI, limits and checks. Project palettes use PDD5 and Aseprite palette chunks; explicit default writes replace the previous automatic global-palette behavior. Hardware acceptance is pending. Slice 3 has started with native 3DS text entry and a shared DS QWERTY keyboard with case/symbols, cursor movement and timed delete. Browser thumbnails, recent files, rename/duplicate/delete, project presets and idle recovery remain outstanding. Existing fixed canvas limits continue until slice 4.

User accepted initial slice 2 behavior locally and requested square vertical swatches, a wider FG/BG column with recent colors and NUM, and clearer default restoration. The follow-up distinguishes saved custom default from original built-in colors and returns directly to the palette after restoring. Tests cover both restore paths and preservation of the saved default. Hardware acceptance remains pending.

## Slice 3 keyboard and file actions follow-up

User accepted the slice 2 layout/default restore and explicitly waived its physical 3DS test before continuing. Slice 2 is closed under that waiver; it is not separately hardware-verified. User explicitly prefers our own keyboard on both consoles, superseding the native-applet plan. Recent colors retain their order when reused; only new distinct colors enter the front.

Slice 3 candidate uses shared staggered QWERTY rows, case/symbol modes, wide space bar, cursor arrows, tap-to-position and timed delete; key height scales for 3DS. Native keyboard invocation removed. Browser selection plus the top-right ... button opens Rename, Duplicate and confirmed Delete. Rename/duplicate preserve extension and refuse collisions; duplication verifies bytes. Directory operations are excluded. Renaming the active project updates its save path; deleting its stored file keeps RAM artwork and makes it untitled/unsaved. Browser thumbnails, recent files, new-project presets and idle recovery remain outstanding. Local acceptance pending.

Recent-color bug follow-up: the previous stable-order change suppressed promotion for all existing colors. Palette selections now promote existing colors as well as adding new ones, while recent-swatch taps remain stable. Host regression checks cover full-capacity promotion/eviction, both palette views and recent selection.

Slice 3 previews/recents candidate: selecting a supported PDD/Aseprite project shows a 64x64 cached composite on the top screen without loading it into the live document. Decode occurs only on selection, using the shared blend math and existing bounded parsers; PNG artwork and palette-file previews are not included. Open/Save browser has RECENT for the last eight successfully opened/native-saved projects. Relative paths persist in app-owned recent0.bin/recent1.bin; rename updates entries, delete removes them, missing files report errors without replacing artwork. Host preview/composite, recent persistence and open-cancel checks pass. Project presets and idle recovery remain outstanding; local acceptance pending.

Slice 3 browser/setup/recovery follow-up: prior previews/recents accepted locally. Browser now uses drag-to-scroll/release-to-select, type filters and selected palette previews. New-project setup stays 128x128 with background, one-to-four layers and palette choices. Idle recovery writes a separate verified native snapshot after two minutes and two seconds idle, preserves dirty/history/named save, and offers startup restore or keep-for-later plus confirmed discard. Corrupt primary falls back to temporary/backup; pending old recovery blocks automatic overwrites. Autosave toggling is session-only. Host checks cover scroll versus selection, palette previews, new-project cancel, idle timing, named-save preservation, startup restore/cancel and corrupt-primary fallback. Native layouts inspected; local/hardware acceptance pending. Variable sizes remain slice 4; automatic named-project resume and PNG artwork previews are not implemented.

Slice 4 history foundation (2026-10-14): browser/palette previews/project setup/recovery accepted locally by user; slice 3 hardware acceptance remains pending. Shared undo now stores changed 8x8 XOR tiles in a fixed 512 KiB arena, up to 32 transactions. Dense edits fall back to 64 KiB raw XOR, preserving eight full-layer transactions. Property-only edits consume metadata only. Oldest transactions are evicted by byte/entry limits; branching discards redo. No allocation during drawing or commit. Layers preview reports actual undo cursor/count and used pixel-history KiB. PdDocument is 923288 bytes on the host (2560 bytes above prior metadata size); arena remains fully reserved. Canvas remains 128x128, four layers. Variable dimensions/more layers and measured hardware performance remain outstanding. Host sparse/dense eviction/branch/cancel/metadata and drawing-oracle checks pass; emulator acceptance pending.

Variable dimensions (2026-10-14): user caps canvases at 128 per side; larger profiles are no longer planned. New Project > Size offers 16/32/64/128 square presets and custom independent width/height 1–128. Tap W/H, type digits, tap Set, then Done. New small projects fit at an integer zoom; drawing, panning and overview respect dimensions. PDD6 adds validated dimensions in the existing width/height header fields while retaining the PDD5 fixed-stride payload and CRC; legacy PDD1–5 load as 128x128. Aseprite and PNG serialize only actual dimensions; recovery uses PDD6 too. Smaller canvases currently retain the same preallocated RAM capacity. Native/ASE roundtrip, bounds, rectangular drawing and desktop PNG dimensions/pixels are checked. Local/hardware acceptance pending.

Eight-layer candidate (2026-10-14): shared capacity is eight layers on NDS, DSi and 3DS, still defaulting to one. New Project supports one through eight starting layers. Quick vertical drag scrolls the layer list; hold 150 ms then drag reorders, with edge scrolling. Visibility/options remain per-row. Core uses eight stable slots and an eight-bit enabled mask; undo pixel budget stays 512 KiB. PdDocument is 1185976 host bytes; extra slots add about 256 KiB. Calculated NDS app/document + two presentation buffers + native import staging/file buffer is about 2.6 MiB before code/SDK/stack overhead; this is not measured free RAM. Hardware full-document stress/performance acceptance remains pending.

PDD7 writes eight property pairs, eight 64-byte names, eight fixed 128-stride pixel planes, eight blend bytes, eight order bytes, then the existing count/256-color palette payload. Header layer capacity is 8; width/height remain 1–128. CRC and atomic-write workflow unchanged. PDD1–6 reads preserve their four-slot wire layouts and expand into eight-slot memory. Recovery also uses PDD7; Aseprite import/export supports up to eight flat single-frame RGBA layers. Higher counts reject transactionally. Preserve existing projects and recovery files; old builds cannot read PDD7. User accepted variable sizes locally; this candidate awaits local/hardware tests.

Layer protections (2026-10-14): layer options now have undoable Lock and Alpha Lock toggles, with X to close. Lock blocks pixel writes, erase, deletion and destructive merges; visibility, opacity, rename and reorder remain available. Alpha Lock preserves existing per-pixel alpha, blends brush RGB by brush alpha, skips empty pixels and ignores erase. Merge into alpha-locked layers is refused. PDD8 appends eight validated protection bytes (bits 0/1) after the PDD7 palette payload; PDD1–7 remain readable. Native recovery preserves both flags. Aseprite editable bit preserves layer lock; alpha lock is native-only and export reports this. Document memory size unchanged. Host protection/UI/persistence checks pass. Eight-layer candidate and protection hardware acceptance remain pending before closing slice 4 and starting brush size/grid.

Brush/grid candidate (2026-10-14): toolbox > Brush/Grid offers independent pencil/eraser sizes 1–16, square/round tips and pixel-grid toggle. Grid shows at 4x+ only on the editing canvas, never in artwork, exports or sampled colors. Both render paths preserve the grid. Brush footprints clip to canvas bounds and contribute to incremental damage. Even sizes extend one extra pixel left/up. Alpha-locked paint samples the pre-stroke pixel so overlapping stamps do not repeatedly accumulate color within a stroke. Protection applies to all stamps. Tool preferences persist on pen release in tools0.bin/tools1.bin (PDB1, 20 bytes, sequence, two size/tip fields, grid flag, CRC at byte 16; dual-slot readback). Preserve these files during deployment. Host tests cover tips/sizes/edges/undo/preferences and 1455 incremental/full render frames. User accepted protections locally and requested continuing tools; slice 4 hardware acceptance remains pending. Fill is the next tool after brush/grid acceptance.

Brush/grid candidate sent successfully via 3dslink to the user-confirmed ready 192.168.1.225 netloader. Exact build/test-slice5-brush-grid/PixelDoodle.3dsx SHA-256 FF2D7CD6B5B7784D4BB531085A2510F225F4145AD578C57858B5207B841F6540. Transfer exit 0; physical interaction acceptance pending. Existing emulator sessions were left running to preserve their artwork.


Slice 5 follow-up: brush size/shape moved to the top pencil/eraser popup. Separate # Guides provides grid, exact X/Y symmetry, and X/Y stroke wrapping with interactive repeating preview. Candidate built and host checked; emulator acceptance pending before fill.

0.3.0 follow-up: restore full toolbox entry, drawn Guides icon, tiled original-canvas border, and incremental guided drawing. Hardware performance retest pending before continuing fill. Use semantic versions for user-facing builds; slice names are planning milestones only.

0.4.0: bounded exact-color active-layer Fill plus expanded tiled panning built and host checked. Local/hardware acceptance pending. Next: rectangle selection, move, copy/cut/paste.

0.5.0: rectangle/lasso selection masks, replace/add/remove, live move preview and session copy/cut/paste built and host checked. 0.4.0 accepted locally. Local selection acceptance pending before the next tool work.

0.5.1: selection quick-action strip and alpha-correct source-over move/paste supersede the 0.5.0 replacement behavior. Candidate built and tested; user acceptance pending.

0.6.0: line/rectangle/ellipse with preview, outline/filled variants, shared brush/selection/guides; selection flips and 90-degree rotation. Built and host checked, local acceptance pending. 0.5.1 accepted locally.

0.7.0 extends the existing Fill tool with contiguous/global replacement, tolerance and active/visible reference sampling. Built and host checked; local acceptance pending. 0.6.0 accepted locally.

0.8.0: optional pixel-perfect size-1 pencil corner cleanup, with snapshot restoration and incremental guide damage. Built and tested; local acceptance pending. 0.7.0 accepted locally.

### Artwork pages and drawing playback (2026-10-15 refinement)

Keep direct per-artwork pages with image, artist, description, dimensions and downloads, inspired by Colors Live. Add genuine timelapse playback after native timed-action recording and upload support: versioned replay data, bounded recording/storage on DS, palette/layer/tool changes and undo semantics, validated server ingestion, and a web player. PNG alone cannot reproduce drawing history. Show playback controls only for artworks with validated replay data; existing artwork remains fully viewable without it. Gallery preserves canvas aspect ratios and supports persistent light/dark themes.


## Community 0.3.0 durable storage foundation (2026-10-15)

Initial source committed/pushed as ca5fe19 to origin/main at user's request.
Subsequent local work adds opt-in SQLite repository (Node 24+), schema v1,
transactional explicit trusted demo seeding, stored PNGs and metadata, and
restart/hidden-media regression tests. No HTTP mutation/auth/ingestion or real
moderation implemented. Default remains in-memory demo; GALLERY_DATABASE opts
into persistence and never seeds implicitly. Both owned preview processes now
use ignored runtime/community-preview.sqlite, retaining loopback/Tailscale URLs.
Nine test groups pass. Next: bounded validated artwork ingestion/derivatives,
then verified transport and account/device pairing. These subsequent changes
remain local, separate from the requested initial push.


## Community 0.4.0 local ingestion and derivatives (2026-10-15)

Added bounded static PNG decoding/canonical RGBA re-encoding, nearest-neighbor
128px derivatives, existing-artist local ingest CLI and transactional pending/
private imports. Schema v2 atomically backfills derivatives; failures preserve
v1. Thumbnail and full-image routes both gate published/public. Gallery uses
thumbnails; detail offers separate full and handheld PNG downloads. See
community/README.md for strict PNG subset, 2 MiB input / 1 MiB canonical limit,
metadata stripping and operator-only scope. Sixteen Node groups plus independent
Pillow pixel/decode checks pass. No HTTP uploads, real authentication, approval,
notifications or network-native integration yet. Next: account/device pairing
and authenticated upload/moderation; verified handheld transport remains needed.
No further commit/push requested; changes remain local after initial ca5fe19.

## PHP/Apache and interactive Discord moderation (2026-10-15)

User switched production backend to PHP/Apache; see community-php/README.md.
Laravel 13 read-only gallery port is tested. Next: port auth/session+Discord OAuth,
then device pairing, bounded ingestion and moderation ledger/outbox to PHP; retire
Node runtime after parity checks. Keep frontend and native API contracts stable.
Dedicated bot review posts have images, descriptions, uploader and action buttons;
see DISCORD-REVIEW-BOT.md. Bot and web decisions share one backend authority.


## PHP community deployment package (2026-10-15)

Community site PHP port now includes authenticated artist profiles and bounded
PNG uploads, private pending submissions/owner history, moderator queue with
Approve/Reject/optional reason/Hold, Ed25519-verified Discord HTTP interactions,
moderator role + explicit Discord account link commands, and a scheduled queue
worker with receipt-gated eight-hour approval. Gallery schema 4 safely upgrades
existing schema 3. Local-only public/private ZIPs are under build; they include
production dependencies but no secrets, databases or fixture artwork. Live server,
SMTP, Discord portal, worker timer and final first-user acceptance remain in
HANDS-ON-CHECKLIST.md. Direct handheld uploads, Aseprite website uploads, account
link UI and timelapse playback remain later work. No production deployment or
GitHub publication was performed.

## Device pairing and 3DS transport candidate (2026-10-15)

PHP pairing is wired to the auth-database migrations and opt-in native routes.
Short-lived one-time codes, explicit website owner approval, hashed private
polling/access credentials, backoff, bounded scopes, 30-day device grants,
owner-scoped revocation and disabled-account checks are covered by the local
suite. `ENABLE_DEVICE_PAIRING` remains false by default and was not changed in
production configuration. The 3DS Community screen uses libctru httpc against
the fixed HTTPS origin, keeps certificate verification enabled, rejects
redirects, polls only on that screen, validates returned scope/code fields, and
stores a CRC-checked scoped bearer in alternating app-owned SD slots. The
offline cache and stylus path remain independent. Local PHP tests and 3DSX/CIA
builds pass; physical certificate-chain, failure-path and pairing checks remain
pending.

NDS/DSi account pairing is unavailable in this candidate. The installed
BlocksDS/libnds toolchain and NDS portlibs expose no verified HTTPS client or
reviewed TLS dependency, and the client has no validated cryptographic entropy
source. Do not use plaintext HTTP, `rand()`, or another application's seed. A
TLS library/license review, entropy plan and physical resource/handshake tests
are required before implementing that transport. The SD bearer is not encrypted
at rest; see `docs/GALLERY.md` and `docs/HANDS-ON-CHECKLIST.md`.



