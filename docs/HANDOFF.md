# Pixel Doodle development handoff

## Hardware acceptance and overnight continuation (2026-10-22)

User confirms 0.18.0P successfully sent an animated GIF to Discord; attachment
143535.gif is the received animation. 0.17.0 playback was also accepted.
This confirms the tested DSi private flow, not DS Lite/3DS parity, maximum
canvas/frame capacity, palette fallback appearance or all failure paths.
Rapid separate-stroke work is explicitly deferred. Keep direct input and keep
Frames and Layers separate. User authorized a checkpoint commit and push before
further animation improvements, then autonomous progress with handoff and
hardware checklist updates after each improvement. PC may be locked; native
host fixtures/builds remain available. Do not infer device receiver readiness
or perform more live webhook sends while the user is away.

Next priorities: safe onion skin preview, animation file interchange, local
exports and practical timeline controls. Record exact verification and remaining
hardware checks for every increment; do not describe untested hardware as accepted.


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

### Verified 0.18.0 candidates

Private and standard host/native builds pass, including independent GIF decode,
16-bit UI checks, 131072 composite comparisons and 16436 incremental render
comparisons. Five candidates staged with SHA-256 readback. Standard builds
retain sender exclusion. No 3DS transfer. Live Discord GIF acceptance pending.

- `G:\dev\3ds\PixelDoodle\build\test-0.18.0\PixelDoodle.3dsx`: 848472 bytes; SHA-256 `B7CF5DEB4D46B8B165394AE4DBFD9801897F441B28A36382B73EE66BAA61A18E`
- `G:\dev\3ds\PixelDoodle\build\test-0.18.0\PixelDoodle.cia`: 686016 bytes; SHA-256 `209503D20128BE933813CE7D6602CDA36327CCC4ED32D2E28262DB1F3E12E304`
- `G:\dev\3ds\PixelDoodle\build\test-0.18.0\PixelDoodle.nds`: 349696 bytes; SHA-256 `86D84193A2D132411843EE02971D49D9DF71B9792066754BE7889A60E58C8A13`
- `G:\dev\3ds\PixelDoodle\build\private\test-0.18.0\PixelDoodle-Private.3dsx`: 856156 bytes; SHA-256 `EFB27C8CBB37CF5BF821730C1267D0D8FB6850242027DC56DBA7036C1417DD6F`
- `G:\dev\3ds\PixelDoodle\build\private\test-0.18.0\PixelDoodle-Private.nds`: 738816 bytes; SHA-256 `A8D33ECE8921B2F3154D139F1C4C8218E1E352DF8831D0A694FD13C34CF3B6B0`

### 0.18.0 private delivery

Private NDS and 3DSX builds pass host/native checks. DSi Link at 192.168.1.162
reports OK SAVED for the private NDS: 738816 bytes, CRC-32 F14150C6 (save-only).
ARM9 V0.18.0P, GIF option/encoder and direct-input strings verified; staged
SHA-256 readback matches. No 3DS transfer or live webhook post was performed
by the agent. User must test device-generated GIF delivery/playback in Discord.

- `G:\dev\3ds\PixelDoodle\build\private\test-0.18.0\PixelDoodle-Private.3dsx`: 856156 bytes; SHA-256 `EFB27C8CBB37CF5BF821730C1267D0D8FB6850242027DC56DBA7036C1417DD6F`
- `G:\dev\3ds\PixelDoodle\build\private\test-0.18.0\PixelDoodle-Private.nds`: 738816 bytes; SHA-256 `A8D33ECE8921B2F3154D139F1C4C8218E1E352DF8831D0A694FD13C34CF3B6B0`



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


### Verified 0.17.0 candidates

Private and standard host/native builds pass. Five artifacts staged with exact
SHA-256 readback; NDS ARM9 build/PLAY/STOP/direct-input labels verified.
Private NDS sent by DSi Link to 192.168.1.162: OK SAVED, 734720 bytes,
CRC-32 857075AF (save-only). No 3DS transfer. Hardware playback acceptance
is pending.

- `G:\dev\3ds\PixelDoodle\build\test-0.17.0\PixelDoodle.nds`: 349696 bytes; SHA-256 `38D6DBDDD9143AA68D62A7BF1CBECFBE1B6B778D76780899F89894BB9F16EDFB`
- `G:\dev\3ds\PixelDoodle\build\test-0.17.0\PixelDoodle.3dsx`: 848320 bytes; SHA-256 `941E95DA6C7695C5FE27C50630AB368C17C0693AE858DE507A7A8BA817417A43`
- `G:\dev\3ds\PixelDoodle\build\test-0.17.0\PixelDoodle.cia`: 686016 bytes; SHA-256 `1E058AEE955B1AD91C30EA20300E502AA8881DDBAAABDF40A009E0996C29753C`
- `G:\dev\3ds\PixelDoodle\build\private\test-0.17.0\PixelDoodle-Private.nds`: 734720 bytes; SHA-256 `F4EA86A239BE84804572242FDEFD0C033A8138F532CE4E01949D917F7D1D6268`
- `G:\dev\3ds\PixelDoodle\build\private\test-0.17.0\PixelDoodle-Private.3dsx`: 851144 bytes; SHA-256 `602D49028F3A19ABD8EEF8BCD0B4CBFC0276FB1F640442A97DEB9CB3AA11A359`

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

### 0.16.2 private delivery

Private NDS saved successfully to DSi Link 192.168.1.162 (734720 bytes,
CRC-32 34C9A12E). Save-only transfer; no remote launch. Private build passes
host, native build, NDS header/DLDI and pinned TLS checks. ARM9 V0.16.2P,
INPUT DIRECT and FRAMES strings and staged SHA-256 readback verified.
Both platform loops contain exactly one app_input call. Hardware retest pending.

- `PixelDoodle-Private.3dsx`: 850268 bytes, SHA-256 `F905DBCABE3253B1586E24E6BDB7DBA9A4DD467EC6BD9584AE3FBD0E2619BAA5`
- `PixelDoodle-Private.nds`: 734720 bytes, SHA-256 `AA1AB8610D914CA193089232BB702FE6DCD3DF225195A2D3A859366F149B4329`

No 3DS transfer performed. Standard artifacts are staged below.


### Verified 0.16.2 artifacts

Private and standard native builds and host suites pass. Standard NDS, 3DSX
and CIA staged with SHA-256 readback verification; no standard or 3DS transfer
performed. Private NDS delivery is recorded above. Hardware acceptance pending.

- `G:\dev\3ds\PixelDoodle\build\test-0.16.2\PixelDoodle.3dsx`: 847796 bytes; SHA-256 `615EC45CDCB5BAB2ABCFBF1CE1BFF5544EB0CF335AB5E8C3B91F5E46660A416B`
- `G:\dev\3ds\PixelDoodle\build\test-0.16.2\PixelDoodle.cia`: 686016 bytes; SHA-256 `1E25BFE1401FBB2A019407C8D9ECC959907F9D8889123E815119A72332FB1E22`
- `G:\dev\3ds\PixelDoodle\build\test-0.16.2\PixelDoodle.nds`: 348672 bytes; SHA-256 `0547CFE9834F8AC6D0CABAADA7095D24604992050730EC6944E7F808C771566A`
- `G:\dev\3ds\PixelDoodle\build\private\test-0.16.2\PixelDoodle-Private.3dsx`: 850268 bytes; SHA-256 `F905DBCABE3253B1586E24E6BDB7DBA9A4DD467EC6BD9584AE3FBD0E2619BAA5`
- `G:\dev\3ds\PixelDoodle\build\private\test-0.16.2\PixelDoodle-Private.nds`: 734720 bytes; SHA-256 `AA1AB8610D914CA193089232BB702FE6DCD3DF225195A2D3A859366F149B4329`

Historical 0.16.1 delivery notes below describe a rejected build, not the
current input path. No 3DS transfer is authorized while its receiver is absent.


## 0.16.1 input capture and build identification (2026-10-22)

User confirms Menu > FRAMES exists in 0.16.0, but fast handwriting still misses
whole strokes. The 0.16.0 redraw mitigation is therefore not an accepted fix.
The exact transferred NDS hashes and frame strings inside both ARM9 payloads
were verified. Missing in-app version identification was real.

The user wants Frames and Layers to remain separate until thorough testing;
do not merge them as part of follow-up animation work without that request.
0.16.1 adds V0.16.1 at the upper-left of the main canvas/timeline preview and
in Diagnostics; private builds show V0.16.1P. Long project names are clipped
beside the label. Menu entry remains FRAMES.

Touch capture is now independent of main-thread painting/rendering. NDS captures
latched keys/touch in its VBlank IRQ, preserving cooperative main-loop yielding.
3DS uses a higher-priority, 4 ms polling worker; only that worker calls HID scan
when active. A 64-sample queue retains press/release edges and changed positions;
stationary samples update time without filling the queue. NDS dequeues under an
IRQ critical section; 3DS uses a mutex. Overflow resets the queue and requires
a fresh lift, preventing a chord between unrelated strokes. Route changes
flush stale queued touches before a new menu can act on them. No drawing,
allocation, networking or rendering occurs in the capture callback/worker.

Diagnostics: Q ON means independent capture is running; Q POLL is 3DS fallback
if worker allocation fails. PEAK is queue depth; LOST counts overflow/reset
incidents, not exact missed strokes. PEN counts sampled contact starts including
menu taps; INK counts started pencil/eraser strokes. Compare changes over a
known drawing sequence; do not expect raw PEN and INK totals to match after
menu use. Hardware contacts absent from every SDK sample cannot be recovered.

Host tests preserve 24 complete short contacts during a stalled consumer, cover
stationary coalescing and overflow reset, and verify that long names cannot
cover the version. Native build-label/diagnostics fixtures were inspected. The
normal full suite remains required; hardware acceptance of queued capture is
pending. Keep playback/onion skin/animated ASE work pending until this retest.

Pre-0.16.1 DSi photo 143405.jpg: DSi TWL, masks 128 KB; FREE 14135 KB and
LARGEST 14135 KB; write 1241 KB/s, read 1433 KB/s. Drawing maxima: input
14101 us, render 491 us, copy 8810 us; sample gap 16717 us / N 5. Maxima
are independent and must not be summed as a measured frame. Variant is not
identified by this photo. Corrected free-heap measurement is confirmed on DSi;
DS Lite and New 3DS FREE/LARGEST results still need collection.

User has DSi Link ready at 192.168.1.162 and explicitly has no 3DS ready now.
Do not send 0.16.1 to 3DS without renewed receiver readiness. Final build hashes
and successful DSi transfer results are recorded below.


## Verified 0.16.1 delivery

Both build profiles pass (private then standard). Host suite, 16-bit framebuffer
check, HTTPS suite, icons, CA, NDS headers/DLDI and CIA version 257 checks pass.
The suite includes the queued-input/route-flush and visible-version regression
checks, 16,436 render-equivalence frames and 131,072 composite comparisons.
The actual ARM9 payloads contain the matching V0.16.1/V0.16.1P labels, FRAMES
entry and queue diagnostics; all staged size/SHA-256 readbacks match source.

Standard and private NDS were sent save-only to user-confirmed DSi Link
192.168.1.162; both returned OK SAVED (349696/735744 bytes; CRC-32
4E6A5DF7/0FE7C972). Launch from Received in Pico Launcher. No 0.16.1 3DS
transfer or CIA installation occurred; the user explicitly had no 3DS ready.
Frames and Layers remain separate. Hardware acceptance of queued capture is
pending. The DSi photo preceding this candidate confirms the heap probe, not
the queued-input change. No commit, push or publication was performed.

Standard immutable copies: `build/test-0.16.1`; credential-bearing private
copies remain separate under `build/private/test-0.16.1`. Previous 0.16.0
staged binaries and manifests are preserved. Exact current SHA-256:

- `PixelDoodle.nds` (349696 bytes):
  `EE13BB94BE1B06DC043DA5943F08299FCF69A935413CE9DF2110A76F63B2018E`
- `build/3ds/PixelDoodle.3dsx` (848876 bytes):
  `F05ABD654499EA0752353101F786DBB096F27AAC516718CF264813D6DE836C3E`
- `build/3ds/PixelDoodle.cia` (686528 bytes):
  `67A7B4ED50340722C8B80D482B3908EC88DFE0F3731CAA2AF113F0C9CCA601F8`
- `build/private/nds/PixelDoodle-Private.nds` (735744 bytes):
  `B1056C48CBD2C685B190FDAE3356CF6BC659C2BF37C312871EC929061E98CA60`
- `build/private/3ds/PixelDoodle-Private.3dsx` (851608 bytes):
  `47063D796B6A4623F8C8D37FFE2E5535F298862D32C08C221E8E752556B6BE1C`

## Hardware report (2026-10-21)

User supplied Diagnostics photos from DSi LL (TWL), DS Lite (NTR), and New
3DS. Card throughput and mask allocation shown:

| Device | Masks | Write | Read |
|---|---:|---:|---:|
| DSi LL / DSpico | 128 KB | 1216 KB/s | 1434 KB/s |
| DS Lite | 8 KB | 369 KB/s | 1345 KB/s |
| New 3DS | 128 KB | 1219 KB/s | 4380 KB/s |

All three display NOT MEASURED YET for free/largest memory despite completed
card tests. Free memory and largest block remain unverified; investigate the
probe before using these results to set animation capacity.

User confirms private-build Send Sprite works wonderfully. User reports that
rapid separate strokes (such as writing letters) are occasionally missed
completely on all three devices: DSi, DS Lite, and New 3DS. This is missing
input, not reported lag or connecting strokes. Do not count this as complete
drawing regression acceptance.

Initial source inspection: both native main loops scan touch once after each
VBlank, then run input and rendering synchronously. The shared pen-release
path commits history and requests a full redraw. Short touches between scans
and work during stroke boundaries are investigation targets; the cause is not
yet established. Existing drawing timing counters only measure frames where
a->stroke remains active, so they omit the pen-release commit/full redraw.
This was the initial investigation; see the subsequent 0.16.0 candidate notes.
New 3DS photo shows drawing maxima: input 827 us, render 273 us, copy 334 us;
sample gap 16731 us / N 23. These are one photo's counters, not a benchmark.


## Current state (2026-10-22)

- Repository: `G:/dev/3ds/PixelDoodle`, branch `main`; last commit remains
  `6c835c4` (0.14.9). Version is now **0.16.1**, including the uncommitted
  0.15.0 memory work. Do not commit or push without the user's request.
- 0.16.0 includes the handwriting candidate fix, corrected memory diagnostics,
  animation step 2, and an initial Aseprite-inspired layer/frame timeline.
  Read `docs/ANIMATION-CORE.md` for controls, storage and current limitations.
- New content files: `core/animation.c`, `core/animation.h`,
  `tests/animation-checks.h`, `docs/ANIMATION-CORE.md`, plus the two mockups
  already untracked at session start. Existing content changes cross core,
  UI, both adapters, build/test scripts and documentation. The unrelated 33
  line-ending-only files remain untouched; inspect with
  `git diff --ignore-cr-at-eol --stat` before any future commit.
- Host tests pass, including 16,436 incremental/full stroke frames, 131,072
  composite oracle comparisons, frame/cel and failure tests. Native-size
  timeline renders were inspected on both screen sizes.
- Standard and private NDS/3DS builds, icons, CA and CIA/version checks pass.
  The exact hashes and device transfer results are below. Hardware acceptance of
  0.16.0, the corrected free-heap readout and handwriting improvement is pending.

## 0.16.0 implementation notes

The optimizer had removed the malloc/free pairs in the free-heap binary search.
A volatile probe write preserves the allocations; the optimized ARM object was
checked. Totals are capped and a completed zero result is distinguished from
an unrun test. The old photos' card speeds and mask sizes remain valid; their
free-memory figures remain unknown.

Ink already displayed is retained on pen lift. Full overview refresh waits for
80 ms idle, with outstanding stroke damage still rendered. A new stroke always
stamps its first point even when entering from outside the canvas. Timers now
include pen-release work. Native scanning remains once per VBlank, so contacts
wholly between samples can still be missed; do not claim a hardware fix yet.

Menu > Frames is beside New Project. The first timeline follows the user's
Aseprite reference: global layer properties, frame columns, and cel markers.
NEW, DUPE, durations, sparse shared tiles, frame switching, frame-aware drawing
undo, streaming PDDA/version-10 native saves and recovery are implemented.
Pan+L/R and New 3DS ZL/ZR step frames. POOL measures packed storage, not heap.
A dense active frame can be saved with no pool headroom. Frame deletion asks
for confirmation and clears undo; frame creation/durations are not undoable.
Playback, onion skin, tags, linked-cel editing, animation ASE I/O, sprite sheets
and animated layer merge are still pending. Old builds cannot open PDDA files;
use new test filenames. No networking/protocol/settings format change.

## Devices and what is on them

- **DSi LL / DSpico** (new SD card; the old one failed from repeated removal).
  Builds now go over Wi-Fi with DSi Link, never by hand. On the card:
  - DSi Link 1.3 at `Homebrew/Dev/Received/DSiLink.nds` (the one the user runs;
    last address `192.168.1.162:17494`). Older copies: 1.2 at
    `Homebrew/Dev/DSiLink.nds`, 1.0 at `Homebrew/DevTools/DSiLink.nds`.
  - Standard and private 0.16.1 were sent save-only to
    `Homebrew/Dev/Received/PixelDoodle.nds` and `PixelDoodle-Private.nds`
    on 2026-10-22; both returned OK SAVED. Launch from Pico Launcher.
    `Homebrew/Dev/PixelDoodle.nds` retains the older working copy.
  - User accepted private-build Send Sprite before 0.16.0; new retest pending.
- **3DS** (`192.168.1.225:17491`, Homebrew Launcher netloader): the standard
  0.16.0 3DSX was sent by 3dslink (exit 0) on 2026-10-22. It is the standard
  staged candidate. Private 3DSX was not sent; CIA was not installed. Hardware
  acceptance is pending; the missed-stroke report covers the preceding build.
- Commands for both are at the top of `AGENTS.md` ("Device test workflow").
  The user set this as the standard workflow: 3dslink to the 3DS, DSi Link to
  the DSi. Say what you sent and its hash.

## Next hardware checks

1. Run 0.16.0 Diagnostics on DSi TWL, DS Lite NTR and New 3DS. Record FREE,
   LARGEST, MASKS and WRITE/READ. Expected masks remain 128/8/128 KB.
2. Write letters quickly, repeatedly lifting between strokes; compare with the
   reported completely missed strokes in 0.15.0. Check ordinary and guided
   drawing, tiny taps, undo/redo and the deferred top preview.
3. Menu > Frames: NEW, DUPE, cel/frame selection, durations, the DS layer scroll
   track, Pan+L/R, ZL/ZR, native save/reopen/recovery and cancelled Delete.
4. Regression: copy/cut/paste, selections, layers, palette, file open/save and
   Settings. Retest private Send Sprite for the current frame.
5. Record the results here and in the 0.16.0 AGENTS.md section. Do not infer
   tested frame capacity or performance parity from host/build success.

## What comes next

The user approved continuing animation and specifically requested Aseprite's
layer/timeline approach as the implementation reference. Keep layers global,
frames horizontal, cels at intersections, and independent DUPE editing even
though identical tiles share storage. Step 2 and the initial grid are in
0.16.0. Next are playback, onion-skin/cached composites, timeline gestures,
frame-structure undo, tags and animated Aseprite I/O. Card streaming waits for
actual corrected memory/card measurements. Do not infer that every feature in
`docs/mockups/pixel-doodle-animation.html` is implemented.

The approved file-screen redesign in
`docs/mockups/pixel-doodle-files-menu.html` is still pending. Preserve the
six-icon rail, DUPE spelling, six/eight-row Layers screen, dedicated Delete
Layer, blend dropdown, single-column Tools drawer, 4x Pixel Grid and centered
Brush Details/Transform sheets.

## Gotchas from this session

- When editing C or Markdown through shell heredocs, backslashes can be
  turned into escapes (a backslash-3 became byte 3, backslash-b a backspace,
  backslash-x1b an ESC byte, and Windows paths lost characters). Use `chr(92)`
  in Python, a script file, or the Edit tool, and grep for control bytes after.
- Most source files are CRLF in the index; preserve each file's line endings.
- Render-equivalence tests copy apps with `clone_app()` in `tests/test.c`
  because masks are heap buffers now; never copy a `PdApp` with `*a = *b`.
- In `PD_FB16` (NDS) builds every screen write must go through `pd_fb()`.
  `scripts/test-host.ps1` runs an MSVC syntax check that fails otherwise.
- Docker Desktop worked without elevation this session; earlier sessions
  needed elevated access. NDS builds use the pinned BlocksDS image in
  `scripts/build.ps1`.
- Never drive emulator windows with synthetic clicks; they may land in the
  user's other windows. The user operates emulators.

## 0.15.0 memory optimization (animation step 1)

- Standard NDS builds drop Wi-Fi/TLS/pairing: image 652 KiB to 254 KiB.
- Masks sized by `app_set_profile()` (8 KiB total in DS mode); clipboard and
  gallery thumbnails allocated on use; NDS screens are 16-bit (`PD_FB16`).
- Settings > Diagnostics > Run Memory + Card Test reports free memory, largest
  block, mask memory and card speed. Run it on the DSi in DSi mode and, if
  possible, in DS mode, and on the 3DS; record the figures here.
- Frames in the core are now in 0.16.0; see `docs/ANIMATION-CORE.md`.

## 0.14.9 Fill and shape dither

- Fill panel and shared Line/Rect/Ellipse panel each have a `DITHER` button
  cycling Solid/25%/50%/75%, using the pencil's canvas-anchored pattern.
- Fill traverses the full matched region; only gap pixels are left unchanged.
- Fill Done moved down to make room (y 164-182 on both screens).
- Tool preferences now write PDB5 (fill dither bits 3-4, shape dither bits 5-6
  of byte 14); PDB1-PDB4 still load.

## 0.14.8 fixes (from fixture review; no new hardware report)

- Line/Rect/Ellipse size slider no longer edits the eraser size after Eraser
  was opened (it previously looked frozen).
- All drawer sliders set the value on first touch and round to the nearest
  step, so the knob tracks the stylus.
- Fill tolerance shows its existing - and + touch zones as buttons; Fill and
  shape tracks show their filled portion.
- NDS Select panel `SELECT ALL` overflowed its button; it reads `SEL ALL` where
  the full label does not fit. A host pixel check guards the two-column rows.

## Completed in this UI pass

- Swatches, Picker and Edit palette modes.
- 8/6/5-column square palette density with scrolling and hold-to-reorder.
- Full color dashboard on the upper screen.
- Picker quick-palette clipping and screen-edge protection.
- Antialiased packed mockup glyphs for the persistent shortcut rail.
- Six-row NDS and eight-row 3DS Layers screen.
- Direct per-layer visibility, Lock and Options controls.
- Compact per-layer Options sheet with a four-choice Blend dropdown.
- Dedicated footer Delete Layer with confirmation.
- `DUPE` label for the duplicate-layer action.
- Single-column Tools drawer with right-side tool settings.
- Transparent canvas area closes the complete Tools drawer.
- Brush footprint preview and centered size slider.
- Visual previews for all four brush presets.
- Fill tolerance, shape width and other tool sliders use widened drag regions.
- Selection actions remain inside their button bounds.
- Pixel Grid works from 4x zoom.
- Advanced Brush Details and Transform Selection use centered overlay sheets.
- Editable mockups are stored under `docs/mockups/`.

## Remaining work

### Hardware acceptance

1. Send the private NDS build to the DSi with DSi Link (see AGENTS.md).
2. Confirm the app boots in DSi TWL mode.
3. Open Color and test Swatches, Picker, Edit, palette scrolling, reorder,
   alpha, hex entry and Cancel/Use Color.
4. Open Layers and test six-row scrolling, visibility, Lock, Options, DUPE,
   Merge, Delete confirmation and hold-drag reorder.
5. Open Tools and test every left-column entry.
6. Test the transparent canvas area beside Tools closes the drawer.
7. Drag each slider repeatedly in both directions. Confirm no stale touches,
   skipped movement or value jumps. Also tap a track once and confirm the
   value moves to the tap. Open Eraser first, then Line/Rect/Ellipse, and
   confirm the shape size slider still moves.
8. Verify Brush Details previews and Load/Store behavior.
9. Verify Selection Delete, SEL ALL and Transform remain inside their buttons.
   Cycle DITHER in Fill and in Line/Rect/Ellipse; confirm gaps keep the art
   beneath and the pattern lines up with Pencil dither.
10. Enable Pixel Grid at 4x and confirm it is useful and does not obscure art.
11. Retest private Sprite sending with the rotated Discord webhook override at
    `fat:/PixelDoodle/private/discord-webhook.txt`. Confirm `share.png` is
    created and removed, the upload completes, and the UI does not freeze.
12. Run the equivalent UI checks on 3DS/Azahar and physical 3DS when available.

### Release checks

- Re-run `powershell -ExecutionPolicy Bypass -File scripts/test-host.ps1`.
- Rebuild mode 0 before treating root 3DSX/CIA artifacts as release packages.
- Re-run `python scripts/verify-icons.py`.
- Re-run `python scripts/verify-nds.py PixelDoodle.nds`.
- Re-run `python scripts/verify-nds.py build/private/nds/PixelDoodle-Private.nds`.
- Re-run `python scripts/verify-cia.py build/3ds/PixelDoodle.cia`.
- Update hashes in `README.md` and `AGENTS.md` after any new build.
- Send builds to devices per the device workflow at the top of AGENTS.md.
  Do not push or publish to GitHub unless the user explicitly asks.

### Deferred product work

- Community functionality remains postponed and hidden from standard builds.
- NDS and 3DS hardware acceptance of the new touch interactions is pending.
- Existing legacy option pages still exist for compatibility and internal tests;
  the new tool drawer routes normal interaction through the compact side panel.
- Further UI passes should preserve the original `DUPE` spelling and the
  4x Pixel Grid threshold.

## Current artifact hashes (0.16.1, CIA version 257)

- `PixelDoodle.nds` (349696 bytes):
  `EE13BB94BE1B06DC043DA5943F08299FCF69A935413CE9DF2110A76F63B2018E`
- `build/3ds/PixelDoodle.3dsx` (848876 bytes):
  `F05ABD654499EA0752353101F786DBB096F27AAC516718CF264813D6DE836C3E`
- `build/3ds/PixelDoodle.cia` (686528 bytes):
  `67A7B4ED50340722C8B80D482B3908EC88DFE0F3731CAA2AF113F0C9CCA601F8`
- `build/private/nds/PixelDoodle-Private.nds` (735744 bytes):
  `B1056C48CBD2C685B190FDAE3356CF6BC659C2BF37C312871EC929061E98CA60`
- `build/private/3ds/PixelDoodle-Private.3dsx` (851608 bytes):
  `47063D796B6A4623F8C8D37FFE2E5535F298862D32C08C221E8E752556B6BE1C`

Standard copies: `build/test-0.16.1`. Private copies: `build/private/test-0.16.1`.
Only the two NDS variants were sent for 0.16.1; 3DS still has the earlier 0.16.0.

## Prompt for the next LLM

Continue Pixel Doodle in `G:/dev/3ds/PixelDoodle`. Read this handoff and
AGENTS.md, then `docs/ANIMATION-CORE.md`. 0.16.1 is uncommitted, including the
prior 0.15.0 changes. No commit, push or publication was performed. For 0.16.1, both NDS variants were sent with user-confirmed DSi Link readiness
and both returned OK SAVED. No 0.16.1 3DS transfer occurred; the user had no 3DS
ready. The 3DS retains standard 0.16.0. No CIA was installed. Preserve
unrelated line-ending-only changes and all credentials, preferences, identity
and runtime data.

The user reported completely missed fast handwriting strokes on DSi, DS Lite
and New 3DS, confirmed private Send Sprite works, asked to fix input and proceed
with animation, and selected antislop during mode for this session. Aseprite's
layer/timeline is the requested reference. 0.16.0 did not resolve missed strokes. The 0.16.1 candidate captures touch
independently into a bounded queue and restores an in-app version/variant label.
The DSi heap readout is now confirmed at 14135 KB free/largest before 0.16.1.
Keep Frames and Layers separate until the user tests thoroughly. It also has
delayed pen-lift redraw, first-point stamping and corrected heap measurement, plus shared-tile
frames, global layers, frame-aware undo, streaming native save/recovery and an
initial timeline. Hardware acceptance and corrected FREE/LARGEST figures are
still needed. Full animation (playback/onion skin/tags/animated ASE) is pending.

DSi delivery uses DSi Link save-only; get its current on-screen address. 3DS
uses 3dslink at the established address once its netloader is waiting. Do not
send to hardware without the user's request/readiness. Use the exact staged
candidate and report its hash. Build private then standard with scripts/build.ps1;
Docker may need elevated access to its engine. Do not replace the pinned image.
