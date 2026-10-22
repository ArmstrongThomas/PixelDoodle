# Product and implementation plan

The [agreed roadmap](ROADMAP.md) supersedes conflicting historical priorities and acceptance statements below.

Planning baseline: 2026-10-14. Current narrower implementation: [prototype status](PROTOTYPE-0.1.0.md). All capacities and timings below are engineering targets pending prototypes. No software or hardware acceptance has been performed for this application.

## Scope and principles

Build a focused solo pixel editor, using Collab Doodle's proven handheld interaction patterns while separating its collaborative transport and authority. A project remains useful with Wi-Fi disabled and without a website account. Native documents are portable between devices when within the receiving device's budget. A large 3DS project must never silently lose layers on DS.

First usable version: pencil, transparent eraser, palette/eyedropper, pan and high zoom, four editable layers, undo/redo, save/open recovery, and PNG export on DS/DSi and 3DS. Add the requested symmetry, tiling, pixel-perfect toggle, blend modes, PNG import and process recording before the feature-complete beta. Website publication is a later independent milestone.

1.0 includes polished local editing, layer add/delete/rename/reorder/duplicate/merge, visibility/lock/opacity, Normal/Multiply/Screen/Overlay, line/rectangle tools, bounded flood fill, rectangular selection/move, all requested pixel tools, replay and optional gallery. Defer animation cels, layer groups/masks, text rendering, arbitrary rotation/resampling, plugins, collaboration, Photoshop compatibility and a full browser editor. Process playback is not sprite animation.

## Native interface

Preserve the familiar dark header, light panels, teal selection, explicit focus/current indicators and predictable Back behavior. On DS use two 256x192 layouts; on 3DS use 400x240 top and 320x240 bottom. Adapt to each screen instead of shrinking the 3DS menu. Aim for 24-pixel-high DS rows where possible; inspect native-size renders and physical stylus hit areas.

Top screen: large complete-art overview with viewport outline; optional repeated 3x3 tile preview; compact project name, active layer, zoom and saved/unsaved/recording state. Bottom: nearly full drawing area with a narrow collapsible strip for Undo, Redo and Layers. Top screen is not touch-sensitive: navigation uses a temporary bottom-screen overview.

Tools: Draw / Color / Layers tabs, with Pressure in Draw's settings to avoid four crowded DS tabs. Layers shows four rows per page on DS, thumbnail, visibility and lock, selected layer and explicit actions; opacity and blend mode in a detail panel. Settings drafts use Apply/Cancel; actual layer edits are immediately undoable document transactions. Destructive file deletion and flatten/export conversions require clear confirmation. Save failure retains the dirty project and offers Retry/Save As.

Proposed Balanced bindings retain Down/B Tools, Left/A + drag Pan, Up/X + touch Sample, Right/Y Zoom overlay, L/R temporary eraser, SELECT project menu. START becomes Quick Save because there is no shared canvas to refresh. Undo/redo remain visible touch buttons with optional remapping; do not introduce conflicting hidden chords. Preserve right/left-handed layouts and show live bindings in Help. 3DS Circle Pad pans. Pan, menus, overview, sample and zoom consume the gesture through pen-up, preventing accidental ink on modifier release.

Zoom levels: 1, 2, 4, 8, 16, 32x plus fit overview. Integer nearest-neighbor display; optional pixel grid at >=4x. Zoom anchors at the touched canvas point, or viewport center for buttons. Use integer/fixed-point coordinate transforms; panning stores document coordinates and clips correctly for canvases smaller than the screen. Fit view navigates, rather than mapping several source pixels to a single drawing pixel.

Pixel-perfect toggle: enabled by default for the one-pixel hard pencil/line only. Specify corner cleanup with a short tentative stroke tail so redundant staircase corner pixels can be removed before commit. Do not remove preexisting pixels: retain their original values in the stroke transaction. Larger/pressure brushes explicitly show the toggle as unavailable. Tests cover diagonal turns, reversals, self-crossings and pen lifts.

Symmetry: independent horizontal/vertical mirror axes, default centered, adjustable in pixel or half-pixel increments. Draw guides separately from art. Tiling X and Y are independent: wrap strokes on enabled axes, clip on disabled axes, and use repeated preview. Resolve source coordinates and corner cleanup before symmetry, then wrap; deduplicate coincident destinations per stamp so axes/seams do not double opacity. Rasterize in unwrapped coordinates before modular reduction to avoid seam-crossing lines traversing the whole canvas. Cap brush/segment work. Fill uses bounded traversal and visited bits on the selected topology.

Pressure: reuse the DSi resistance-proxy calibration/filter approach, never describe it as measured physical force. Off by default for crisp pixel placement; optional integer brush-size steps or opacity. Record resolved brush values rather than raw ADC calibration. DS Lite support depends on valid input and testing; 3DS uses fixed settings and does not promise pressure. DSi in NTR mode receives the DS resource budget.

## Architecture and proposed tree

Use a portable C99 core with an explicit C ABI, callable from the C NDS shell, C++ 3DS shell, desktop tests and WebAssembly replay worker. Avoid exceptions, platform headers and hidden heap allocations in core operations. Shared action IDs and rendering semantics are authoritative; platform adapters supply input, allocation budgets, storage and presentation.

```text
core/                 document, tiled RGBA storage, rasterizer, compositor
core/history/         transactions, bounded undo and redo
core/io/              project codec, validation, recovery and replay
platform/nds/         BlocksDS shell, NTR/TWL, RGB15 display, pressure, FAT
platform/3ds/         libctru shell, input, framebuffer, SD and lifecycle
ui/                   common view models, geometry/theme, native layouts
web/                  responsive gallery and WebAssembly playback wrapper
service/              NEW gallery API, metadata store and isolated render worker
tools/                host replay renderer, validators and packaging tools
tests/                synthetic golden images, format fuzzing, input traces
third_party/          only audited dependencies and preserved license notices
docs/                 decisions, format spec, benchmarks, release acceptance
```

Prototype core, shared UI, platform adapters, tests and build scripts now exist. Keep service code private by default if the repository later gains a public remote; split it into its own private repository before public client publication. Never include the existing private Doodle service.

Data path: input -> resolved document gesture -> transaction -> dirty tiles -> composite -> viewport presentation. Saving and recording consume committed transactions. Networking only transfers saved immutable revisions. UI operates on a semantic action layer, preventing one press from both closing a menu and drawing/deleting.

Represent layers as sparse 16x16 RGBA8888 tiles; allocate full tiles only on mutation, with a zero tile for empty areas. Budget for fully dense canvases, not hoped-for compression. Flat bounded tile directories avoid per-pixel allocation. A dirty composite tile cache and viewport scaler let zoomed editing touch a small area. Full layer reorder/opacity changes invalidate the composite; time-slice large recompositions, display progress and prevent edits against an incomplete state. Keep storage/network work outside the ordinary stylus path. No TLS connection during active drawing in the first network version.

Use CPU integer compositing for identical behavior across targets. DS RGB15 conversion happens only when presenting; preserve full RGBA in documents/exports. GPU acceleration on 3DS is optional after the CPU reference is stable, and must match golden output.

## Hardware budgets

DS has 4 MiB main RAM; DSi TWL has 16 MiB, with runtime-reserved regions [BlocksDS memory guidance](https://blocksds.skylyrac.net/docs/internal/memory_map/). These physical totals are not free heap. 3DS budgets must use actual launch-mode heap availability: libctru allocates application and linear heaps from the runtime memory allowance, not simply all installed RAM ([allocator source](https://github.com/devkitPro/libctru/blob/master/libctru/source/system/allocateHeaps.c)). Treat Old 3DS as the baseline and New 3DS as an optional measured upgrade.

| Profile | Initial dense canvas/layers | Layer bytes | Composite | Undo pool | Proposed working envelope* |
|---|---|---:|---:|---:|---:|
| DS / DS Lite / NTR | 128x128, 4 | 256 KiB | 64 KiB | 512 KiB | 1.5 MiB |
| DSi TWL | 256x256, 8 | 2 MiB | 256 KiB | 2 MiB | 6 MiB |
| Old/New 3DS baseline | 512x512, 16 | 16 MiB | 1 MiB | 8 MiB | 30 MiB |

*Envelope includes these pools plus provisional scratch, active transaction, tile metadata and presentation buffers; executable, SDK, stacks and network reserve are additional. Validate the complete link map and peak runtime allocation before enabling a profile. Explicitly reserve at least 512 KiB DS / 2 MiB DSi / 8 MiB 3DS remaining headroom after total application allocations; reduce profiles if this fails. Do not allocate networking libraries into the DS drawing envelope without remeasurement.

Alternative capacity experiments: DS 256x256/two layers, DSi 512x512/four layers, 3DS 1024x1024/four layers. These exchange layers for size; they are not simultaneous promises of maximum dimensions and layer count. Do not enable them until full-canvas fill, undo, merge and save tests fit. New 3DS larger profiles are post-baseline work. PNG and replay parsers must enforce profile limits before allocating.

One full 128x128 RGBA layer is 64 KiB; before+after full-layer undo costs 128 KiB, so a 512 KiB pool holds fewer than four such edits once overhead is included. Small strokes retain many more. Display available history by actual retained transactions; never promise a fixed 100-step undo count. Reserve transient transaction capacity before edits; details are in the format document.

An incompatible large project offers a read-only flattened preview or an explicitly created smaller/flattened copy on a capable device/website. Preserve the original. Never fall back to Doodle's fitted lossy editable canvas.

## Validation and release criteria

M0 measurements determine feature caps. Record build hash, runtime mode, loader, launch package, canvas/layers, free/peak heap, largest allocation, SD/flashcart, tool settings and clock configuration. Synthetic art only. Report median/p95 input-to-display latency using high-frame-rate camera plus timing instrumentation; brush time alone is not latency.

Targets: ordinary pencil path sustains 60 Hz presentation and p95 touch-to-visible <=50 ms on each baseline profile; no unexplained >100 ms stalls during ordinary strokes. Full fill, merge, save and seeking can be slower but must have bounded work and responsive progress/cancel where transactional safety allows. Overview can update at 10-15 Hz. These are acceptance targets, not measured results. Reduce capacity or optimize if missed, then document the actual supported profile.

Test on Japanese DSi LL/DSpico WRFUxxed/Pico Launcher in verified TWL and, where launchable, NTR; DS Lite with its actual flashcart; Azahar for native-size 3DS interaction and later requested Old/New 3DS hardware. DSi acceptance does not establish DS Lite behavior. Test SD absence/full, failed writes, sleep, relaunch, deliberately truncated saves and controlled interrupted writes with sacrificial synthetic projects. No touching existing user art or Doodle identities. Original DS claims remain separate from DS Lite evidence.

Core tests: blend/alpha rounding including transparent destinations; zoom edges; mirror/seam collisions; pixel-perfect tails; flood fill limits; transactions for every layer edit; save/open equality; malformed/decompression-bomb rejection; interrupted journal recovery; replay after undo branching; native/host/WASM final-state hashes. Performance workload includes dense noise, all blend modes, 1000 short strokes, a long continuous stroke, maximum undo, save during low free space, and 30-minute draw/save/reopen cycles. Native screenshot checks cover every menu, error and confirmation.

## Milestones and exit gates

| Stage | Deliverable | Exit gate |
|---|---|---|
| M0 feasibility | Allocation/composite prototype, source provenance manifest, native input/display spike, format fixtures | Budget fits each proposed baseline; measured latency; no inherited service calls or paths |
| M1 usable offline alpha | Both shells, pencil/eraser, 1-32x, four layers, undo/redo, save/open recovery, PNG export | Draw-save-exit-reopen exactness on supported hardware; native-size UI accepted |
| M2 pixel tools | Pixel-perfect, symmetry, X/Y wrapping, fill, shapes, selection, opacity/blends, PNG import, DSi pressure | Golden tests plus seam/alpha/pressure UX acceptance; platform capacity profiles frozen |
| M3 recording beta | Append recording, timeline undo events, checkpoints, on-device playback, shared host/WASM renderer | Identical states at checkpoints and end on all targets; safe truncation and long-session bounded RAM |
| M4 companion site | Private library, explicit publishing, phone browsing, device pairing, revision upload, replay viewer | Permission matrix, malicious-file tests, sync conflict tests, performance isolation and restore drill in local/staging only |
| M5 1.0 candidate | Polish, onboarding/help, export/recovery docs, artifact provenance, compatibility table | Hardware matrix complete or unsupported targets explicitly excluded; no known save-loss/replay divergence; user acceptance |

Milestones are ordered by risk and acceptance, not calendar promises. M4 front-end mockups can follow M2 while native recording matures; service deployment requires later authorization. Optional encoded video export follows M4 with worker resource limits. No auto-updater for the first offline alpha; new title IDs, install paths and future package-update gates require deliberate design, never reuse Doodle payloads.

## Consequential open questions and defaults

1. **Name and distribution license:** use Pixel Doodle locally; keep private until branding and ownership/provenance are resolved. Propose MIT for newly authored client/core code only after owner approval, preserving third-party obligations. Service remains private by default.
2. **DS scope:** accept a fully featured 128x128/four-layer baseline rather than requiring DSi-sized projects on DS. Increase caps only after M0 evidence.
3. **Sharing recordings:** local recording on with a visible status; upload defaults private and still-only. Publishing playback is an explicit separate choice because hidden/deleted/undone work may appear. Default shared playback preserves the actual process when enabled.
4. **Website access:** invite-only account creation and authenticated private libraries initially, public view-only gallery. No comments, direct messages or anonymous uploads in 1.0. Separate account namespace from Doodle.

## Updated priority after R2 / file milestone (2026-10-14)

DS Lite R2 responsiveness accepted. 0.2.0 implements named files and single-frame Aseprite round trip; see FILES-0.2.0.md. User explicitly requests simple animation/timeline similar to Aseprite as a follow-on. Plan shared frame/cel storage, frame duration, add/duplicate/delete/select, playback and multi-frame ASE compatibility under measured DS memory limits. Until that work lands, multi-frame import fails explicitly rather than flattening. Keep frame-aware PDD versioning separate from PDD2's static payload.
