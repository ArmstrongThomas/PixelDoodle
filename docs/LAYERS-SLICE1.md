# Layer management candidate

## 0.14.4 touch workspace

The layer list now shows six compact rows on NDS and all eight on 3DS. Each
row has a dedicated grip, visibility eye, selectable name/mode/opacity area,
direct Lock toggle and Options control. The footer provides New Layer and a
dedicated Delete Layer action. Delete uses the destructive confirmation before
changing the document. Each row's Options sheet retains opacity, a four-choice
blend dropdown, Rename, Duplicate, Merge Down, Lock and Alpha Lock. The Options
sheet and dropdown use the same centered width: 192 pixels on NDS and 240 on
3DS. Blend selection commits on touch-down, eliminating release delay.
The compact duplicate action is labeled `DUPE`.

Tap a row to select it. A quick drag scrolls the list. Hold a row or its grip
for 150 ms, then drag to reorder; release commits the move. The upper screen
shows composite and active-layer previews side by side, with a visible hidden
state and the active layer's blend, opacity, Lock and Alpha Lock state. The
workspace introduces no project, settings or server format changes.

Host tests cover row hit zones, direct visibility and Lock controls, the
options sheet, footer Delete confirmation, cancellation, scrolling, 150 ms
reorder, hidden active-layer preview and both native render sizes.

## Row-controls follow-up

Layers now displays topmost first. Each row has a far-left menu button, visibility eye, name, blend mode and opacity. Only New Layer remains at the bottom; a permanent scroll track reserves space for later larger documents. The header reports used/free slots under the measured four-slot capacity. Top-screen status reports the allocated document structure in KiB and retained undo position; it does not claim runtime free RAM or infer additional capacity from it.

The row popup offers a continuous opacity slider (one undo transaction per gesture), Normal/Multiply/Screen/Overlay, Rename, Duplicate, Merge Down, Delete and Close. Delete uses a compact nested confirmation. Hold the name for at least 150 ms and drag to another row to reorder on release; the floating row provides drag feedback. Release before the threshold only selects. Reordering, blending, duplication, opacity and merges are undoable. Duplicate is inserted above its source; new layers enter at the top. Merge Down currently requires two visible Normal layers, and otherwise shows a reason without modifying the document. This avoids silently baking backdrop-dependent blends incorrectly.

Native saves now use PDD4: PDD3 header semantics plus eight appended payload bytes, four blend-mode bytes (0–3) and a four-slot bottom-to-top permutation. CRC covers the extension. Readers validate the permutation and modes and retain PDD1–PDD3 loading. Inactive pixel slots are zeroed on disk, even when RAM retains them for merge undo. Aseprite import/export preserves supported blend modes and display order with compact cel indices. These details supersede PDD3-only and no-reordering statements below.

Host tests cover blend examples, reorder threshold/release/undo, eye controls, opacity transaction coalescing, duplicate, merge/undo and popup confirmation. Desktop Aseprite round trips preserve reordered blend-mode projects. Existing render, picker, file and format checks pass; NDS/3DSX/CIA build/package checks pass. Native DS row, popup, confirmation and drag renders inspected. Local user interaction and hardware acceptance remain pending for this follow-up.

New documents start with one layer. Existing projects keep their layers. Capacity remains four for the current memory profile; expanding it belongs to the later capacity work.

Layers lists existing layers bottom to top. New creates an empty layer in the first available slot. Rename opens the shared keyboard, including space for names (63-byte maximum; imported names preserved). Delete names the selected layer and requires confirmation. Keep at least one layer. Add, rename, deletion, visibility and opacity changes participate in undo/redo; deleted pixels return on Undo. Reusing a deleted slot branches history normally. Layer ordering/reordering is not added here.

Selecting a layer previews its raw RGBA contents on top, including hidden layers and before layer opacity is applied. The header names the layer and the preview is explicitly labeled. Exiting Layers restores the full composite overview. The preview never changes visibility, exported pixels or canvas contents.

## Persistence

Named saves now use PDD3, retaining PDD2's fixed payload size and four storage slots. Header magic is PDD3, version is 3, and little-endian offset 20 contains a nonzero four-bit existence mask (1–15). Other header fields, name slots, pixel encoding and CRC coverage are unchanged. In-memory slots are stable so deletion need not copy the whole document or enlarge undo pixel buffers. Native saves preserve slot existence separately from visibility. Old PDD1/PDD2 import enables all four historical slots; those formats never recorded layer existence. Old builds reject PDD3 rather than silently misreading it. Existing files are not migrated until explicitly saved, with the existing .bak write policy.

Aseprite export writes only existing layers, preserving relative order and remapping cel indices to the compact export order. Import enables exactly the number of layer records received. One-layer files no longer gain three extra visible UI layers. PNG remains the visible composite. The unused legacy PDD1 writer refuses documents with missing slots; normal app saves use the named PDD3 path.

## Validation

Host checks: one-layer defaults, add/capacity/last-layer protection, rename, delete and undo/redo/branching, native sparse-slot save/open, compact Aseprite indices, one-layer import, delete confirmation/cancel, and hidden-layer solo preview. Existing drawing/picker/render/file tests pass. Desktop Aseprite accepts one-layer and deleted-middle-layer exports and preserves their content on return. Synthetic PDD2 compatibility, compressed cel interoperability and invalid-file preservation pass. NDS/3DSX/CIA build/package checks pass. Native layer layouts inspected. Emulator interaction and hardware acceptance are pending for this candidate.
