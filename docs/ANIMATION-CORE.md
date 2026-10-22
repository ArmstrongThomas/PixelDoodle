# Animation, onion skin and ASE export: 0.20.0

## 0.20.0 animated Aseprite export candidate (2026-10-22)

Export > Aseprite now includes all frames, enabled layers (including hidden
layers), order, names, visibility, opacity, supported blend modes, generic
layer locks, palette and per-frame durations. RGBA cels are lossless, including
RGB under zero alpha. Alpha Lock remains native-only, with existing UI notice.
The flat RGBA file uses raw cels and is streamed row by row; no whole-animation
buffer. A current-frame snapshot must fit before export. All later seeking and
restoration allocate nothing. Original frame, pixels, history and dirty state
are restored even on write failure. File size is capped at 64 MiB before
writing. Existing verified .tmp/.bak replacement now checks streamed ASE
length and CRC by readback before replacing an existing destination.

Animated ASE IMPORT remains unsupported and explicitly rejected; preserve PDD
as the editable device master. This increment enables desktop interchange in
the export direction, not linked cels/groups/tags/indexed import. Test exported
files in actual desktop Aseprite as well as the independent parser. Desktop
batch tests can run while Windows is locked; no Azahar visual acceptance is
claimed. No device transfer or live webhook post while user is away.

### Frame ordering and playback polish in 0.20.0

Tap the selected frame number in Frames to open frame actions: duration,
Move Left/Right and confirmed deletion. The ordinary Delete footer remains.
Moving is allocation-free and keeps the same working frame pixels selected;
paint-history frame indices are remapped in both directions, including redo.
Frame order marks the project structurally dirty and persists in PDD/ASE/GIF.
Move itself is not undoable, matching current insert/duration behavior; paint
undo is retained. Out-of-bounds moves and moves during a transaction refuse
without changing art. Playback stop now brings the original editing frame
back into the visible column range. Single-frame playback reports its actual
need for a second frame, rather than a generic memory error.

See docs/ANIMATION-HARDWARE.md for physical test requirements. Verification and
artifact hashes are recorded below after native builds.


## 0.19.0 onion skin candidate (2026-10-22)

Checkpoint 2ce8a56 was committed and pushed before this work. 0.18.0P animated
GIF delivery and 0.17.0 playback are user-accepted on DSi. New hardware tests
are tracked in docs/ANIMATION-HARDWARE.md; do not send builds while user sleeps.

Frames has ONION OFF / ON beside Play. Previous frame is a faint red silhouette;
next is blue, with no wrap at the first/last frame. The current frame paints on
top. Onion affects the bottom editing canvas only; overview, sampler, saved
pixels, GIF/PNG exports and playback stay clean. Toggle is session-only.
The alpha-only preview honors visibility, opacity and layer order; blend RGB
does not change composite alpha. Compressed neighbor tiles are read directly;
no frame switching, snapshots, packed-pool allocations or live-pixel mutation.

Cache uses two alpha planes (32 KiB at 128x128; scales with padded plane),
allocated only when enabled and freed when disabled. Packed-frame visual
generation, frame selection, structure and relevant layer metadata invalidate
it. Painting the current frame reuses the cache. Allocation refusal disables
onion with a notice and retains art. Playback suppresses ghosts. Hardware
latency and large-canvas memory acceptance remain pending. Existing direct
input is unchanged. Frames and Layers remain separate; handwriting deferred.

Implementation verification and exact build results follow below.


## 0.18.0 animated GIF webhook candidate (2026-10-22)

User accepted 0.17.0 playback on hardware and requested GIF sending through the
private Discord webhook. Rapid separate-stroke work stays deferred. Frames and
Layers remain separate; direct input remains unchanged.

Private Send Sprite keeps active-layer PNG and visible-composite PNG, and adds
SEND ANIMATION GIF. GIF includes visible layers in every frame, starts at frame
1 regardless of selection, loops forever and uses per-frame delays rounded to
10 ms. It restores the editing frame/pixels and preserves undo, revision and
structural dirty state. GIF has binary transparency (alpha below 128 becomes
transparent), not partial RGBA. Each frame keeps exact RGB when it fits 255
opaque colors plus transparency; otherwise it uses a nearest 216-color RGB
cube. The send screen explains binary alpha and possible color reduction.
PNG and native project pixels remain full RGBA and are not quantized.

Encoder streams to a private share.gif.tmp then renames on success. Existing
output/temp files are refused by the encoder; the sender owns cleanup of its
private share files. Output is capped at the existing 2 MiB application limit;
oversize/write failure removes the partial file and sends nothing. One indexed
frame buffer plus bounded dictionary/palette scratch is used, not all frames
in memory. A snapshot must fit before multi-frame export; later frame seeking
and restoration allocate nothing. GIF uses local palettes, disposal 2 and a
Netscape infinite-loop extension. No delta frames or dithering in this slice.

Both native transports use one shared multipart prefix builder with .gif /
image/gif or .png / image/png. allowed_mentions disables mention expansion.
NDS keeps streaming its staged file; 3DS retains its existing buffered upload.
Private credentials/config, TLS validation, endpoint and retry behavior stay
unchanged. Standard builds do not expose the private sender. No server change.

Host tests cover state restoration, existing-file refusal, bounded output and
cleanup, RGB palette overflow, alpha threshold, and PNG/GIF multipart metadata.
An independent Pillow decoder checks all pixels of three frames, including an
empty frame after opaque art, disposal/transparency, exact colors, loop and
110/250/500 ms delays, plus fallback colors. DS and 3DS send layouts inspected.
Native builds and hardware webhook delivery results are recorded separately.


## 0.17.0 animation playback candidate (2026-10-22)

User explicitly deferred the quick separate-stroke issue and requested animation
work to continue. Keep direct input; do not reintroduce the rejected queued
capture. Photo 143534.jpg confirms V0.16.2P / INPUT DIRECT: input 16856 us,
render 1459 us, copy 8814 us, sample gap 16717 us / N150. FREE/LARGEST
14139 KB, card write 1226/read 1434 KB/s, PEN16 / INK12. This establishes
improved measured timings, not a claim that rapid strokes are fixed.

Menu > FRAMES now has PLAY / STOP. Playback loops all frames using their
individual durations and follows the active frame column. Any new touch or
button stops playback and consumes that action, returning to the original
editing frame. Editing, navigation, file actions and recovery are suspended
while playing. Start requires two frames and a successful snapshot of current
pixels. A failed snapshot leaves the editor unchanged. After start, ticking
and stopping restore immutable stored frames without allocations or repacking.
Clock rollover and delayed-loop catch-up are bounded; complete elapsed cycles
are skipped before choosing the visible frame. Playback does not change pixels,
undo history, dirty generation or durations. Frame buffers still render at the
existing display cadence; actual hardware playback performance needs testing.

Host playback tests pass on both layouts: unequal durations, wraparound, long
stalls, action consumption, exact pixel return and unchanged undo/generation.
Tick/stop also pass with allocation failure injected. Native 256x192 and
320x240 playback fixtures were visually inspected for readable controls.

Frames and Layers remain separate. Onion skin, linked cels, tags, animated ASE
and sprite sheets remain future work. Historical sections describing playback
as pending are superseded by this candidate. No hardware transfer is implied.


## 0.16.2 latency rollback (2026-10-22)

0.16.1 is rejected on DSi hardware. Photo 143533.jpg shows V0.16.1P,
input 17315 us, render 219991 us, copy 19938 us, sample gap 267440 us
(N 10), queue peak 16 with zero overflows, PEN 9 / INK 5. These independent
maxima must not be summed. User reports massively increased drawing latency.

0.16.2 removes the queued IRQ/worker capture experiment from both platform
adapters and restores one direct SDK scan and one app_input call per display
loop. The queue consumer called app_input again before rendering, invalidating
pending partial damage and forcing expensive full renders. Queue-only host
tests did not exercise this platform/render interaction and were insufficient.
The unused queue implementation and its tests are removed. Diagnostics now
show INPUT DIRECT. The visible V0.16.2 / V0.16.2P label, heap probe correction,
animation core and separate FRAMES page remain. Frames and Layers must stay
separate until the user accepts thorough testing. Playback/onion skin remain
pending. This is a latency rollback candidate; the original missing quick
strokes are still unresolved. Hardware responsiveness must be retested.

Historical 0.16.1 delivery notes below describe a rejected build, not the
current input path. No 3DS transfer is authorized while its receiver is absent.


Animation includes frame/cel storage, playback, onion skin, frame ordering,
GIF sending and animated Aseprite export. Tags, linked-cel editing, animated
Aseprite import and sprite-sheet export remain pending. New features need
hardware acceptance; passing host tests does not establish device performance.

## Controls and model

Menu > Frames, beside New Project, opens a layer-by-frame grid. Layers are
rows in the same order as the Layers screen; frames are columns. A filled mark
means the cel contains pixel data. Tap a cel to select its layer and frame.
The arrows scroll frame columns. On DS/DSi, tap the upper/lower half of the thin
layer scroll track to reach all eight layers. The regular Layers screen keeps
its existing six/eight visible rows and controls.

- NEW inserts an empty frame after the current frame; DUPE copies its pixels.
- Tap the duration to choose a preset or adjust by 10 ms (10Ã¢â‚¬â€œ60,000 ms).
- Pan + L/R steps frames with the pen lifted. ZL/ZR also step on New 3DS.
  These are single presses; holding to repeat is not implemented yet.
- Tap the selected frame number for duration and allocation-free Move Left/Right.
  Paint undo follows reordered artwork; moving itself is not undoable.
- Delete asks for confirmation and clears undo history. At least one frame
  remains. Frame insertion and duration changes are not yet undoable.
- The upper-screen status names the current frame when there is more than one.
- POOL reports packed storage against its budget. Working layers, clipboard,
  undo and screen buffers are separate. The active cel may have edits that have
  not been packed yet; the meter is not a free-heap measurement.

Names, order, visibility, opacity, blend, locks and the palette are shared
across frames, following the user's Aseprite reference. Cel pixels and duration
belong to frames. Duplicating a layer copies its cels in every frame. Deleting a
layer hides its cels everywhere, while retaining inactive-frame data for undo.
A removed slot with retained cels is not recycled into a new layer. Animated
layer merging is refused until it can merge every frame safely.

Reference: [Aseprite Timeline](https://www.aseprite.org/docs/timeline/),
[Sprites](https://www.aseprite.org/docs/sprite/), and
[Linked Cels](https://www.aseprite.org/docs/linked-cels/). The tile pool shares
storage, but DUPE has independent editing semantics; it is not a linked cel.
Animated export follows the [official Aseprite file specification](https://github.com/aseprite/aseprite/blob/main/docs/ase-file-specs.md).
Implementation and assets are original or reused from this repository.

## Storage and failure behavior

The active frame uses the existing full-color working layers. Stored frames
use sparse references to shared 8Ãƒâ€”8 tiles. All-zero tiles are omitted; uniform
tiles use one RGBA value. Tiles with 2Ã¢â‚¬â€œ47 distinct RGBA values use a local
palette and 64 one-byte indices; more varied tiles retain 64 RGBA values.
Hash matches are checked against exact encoded bytes, preserving transparency
and RGB values even when alpha is zero. Editing a duplicate never modifies its
source frame.

Packing builds a replacement snapshot before releasing the old one. Allocation
failure leaves working art, frame selection and history intact. The frame cap
is 256. Initial packed-pool budgets are conservative: 1 MiB for NTR, 6 MiB for
TWL, 12 MiB for standard 3DS, 32 MiB for expanded New 3DS. Allocator costs are
included conservatively. These limits are not measured frame-count promises.
Packing needs temporary headroom; edits can exhaust it even before the meter
reaches the limit. Switching then refuses without changing art. Native saving
streams the active frame directly without allocating a packed snapshot, even
when the pool is full. Import staging has a bounded temporary allowance for
the active frame; its packed copy is released after restoring working pixels.
Actual heap exhaustion can still refuse an import without replacing live art.
Hardware heap measurements and further pressure tuning remain important.

Undo entries record their frame. Undo/redo packs the current frame and returns
to the target frame before applying its XOR edit. Inserting a frame remaps
history indices; deleting a frame clears history explicitly. Structural
changes have a separate dirty generation, including in recovery scheduling,
so undoing paint cannot incorrectly mark a changed animation as saved.

## Native files

Animated/native frame projects use PDDA magic, version 10, with a 64-byte
little-endian header, separate header/payload CRCs, global layer properties,
palette, shared tile dictionary, then frame durations and sparse cel references.
Writes stream stored tiles and the active working pixels directly to `.tmp`,
sync, verify the streamed CRC, then use the
existing `.bak` replacement workflow. Imports stage and validate the bounded
pool before replacing the live document. Recovery uses the same format and
does not mark the named project saved. Older PDD1Ã¢â‚¬â€œ9 imports remain supported;
ordinary single-frame projects without animation state keep PDD8/PDD9 output.
Older app versions cannot read PDDA: use new filenames while testing.

Multi-frame Aseprite export streams all frames/layers and verifies its output.
Animated Aseprite import remains unsupported; keep PDD masters. PNG exports
only the current frame. Private Send Sprite can choose current-frame PNG or
an all-visible-frame animated GIF. Single-frame Aseprite I/O remains unchanged.

## Handwriting and diagnostics

Pen release previously committed history and forced a full canvas/overview
redraw before the next input sample. The candidate keeps already-rendered ink
and waits for 80 ms of idle time before refreshing the overview. An unrendered
last batch still renders its damage. A newly started stroke always stamps its
first point, including when entering the canvas from outside. Platform timing
counters now include release work. The user subsequently confirmed missing strokes still occur in 0.16.0.
Historical rejected 0.16.1 experiment (removed in 0.16.2): independent capture used NDS VBlank IRQ and a
4 ms higher-priority HID worker on 3DS, with a 64-sample locked queue. The
consumer drains captured edges/positions before rendering. Idle repeats are
coalesced; overflow resets safely and requires a new lift, and route changes
flush stale touches before another menu can act. Capture never calls app code.
SDK contacts entirely between hardware samples still cannot be recovered.
This capture experiment caused unacceptable hardware latency and is removed.
Current diagnostics show INPUT DIRECT plus PEN/INK and version/variant labels.
The user explicitly wants Frames and Layers kept separate while testing.

The ARM optimizer removed malloc/free pairs from the old free-heap search,
making it assume a near-limit allocation would fit. A volatile write makes
each successful probe observable. The total probe is capped by its configured
limit, and a completed zero result is distinguished from an unrun test. The
optimized ARM object was inspected to confirm the malloc/write/free remains.

## Design and verification

Antislop during mode was selected by the user. Design direction is the existing
native editor and approved animation mockup, with Aseprite's row/column model.
ENERGY 2 / RHYTHM 1 / MOTION 1. Pixel typography and cyan selection reuse the
existing interface for readability and state; no new decorative assets or
motion. The grid gives related cels uniform geometry so time and layer position
can be read directly. Delete remains neutral until its confirmation screen.
Native 256Ãƒâ€”192 and 320Ãƒâ€”240 synthetic fixtures were inspected for clipping,
ordered row/cel hitboxes, scroll access and duration/action placement.

Host coverage includes rapid one-sample strokes, first-point entry, delayed
release refresh, tile sharing, exact RGBA, duplicate isolation, cross-frame undo,
global names, all-frame layer duplication, budget/cap limits, allocation-failure
rollback, native/recovery roundtrips, saving eight dense layers with no pool
headroom, and corrupt-file preservation. Existing
brush, selection, file, HTTPS and render-equivalence suites also run. Consult
HANDOFF.md for the final build results and exact hashes.
