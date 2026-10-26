# Pixel Doodle development handoff

Current continuation summary: `docs/ANIMATION-NEXT-SESSION.md` (animation);
file-screen redesign: the 0.21.0 section below.
Pushed baseline: `2ce8a56`; local commits `0f51d8c`, `e8967a3`, `5207761` and
`738b07d` (0.21.0); not pushed.
Current device candidate: 0.21.0; hardware-accepted DSi baseline: 0.18.0P.
UI mockups (design references, with what is implemented): `docs/mockups/README.md`.

## 0.21.0 file screens redesign (2026-10-26)

Implements the user-approved mockup `docs/mockups/pixel-doodle-files-menu.html`
on top of 0.20.0. No file-format, settings or animation change.

- **Menu:** New Project | Frames; Save | Save As | Export; Open / File Browser;
  Settings | Recovery; Exit | Send Sprite (Send only in private builds; Exit
  takes the full row in standard builds). One `menu_buttons()` table drives
  drawing and taps. Adjust is no longer in the Menu.
- **Layers footer:** + New Layer | Adjust | Delete Layer (`DELETE` where the
  full label does not fit). Adjust opens the existing page and its Apply and
  Cancel now return to Layers.
- **File browser:** title and path in the header, Recent at the right; Up and a
  type dropdown (reusing `PdDropdown`) with icons and extensions; one-line rows
  (7 visible on DSi, 9 on 3DS) with a 10x8 type icon, the real-case name and a
  type tag or `>` for folders; footer New Dir | More | Cancel | Open/Name/Import.
  More replaces the old `...` file-actions button. `browser_layout()` is the
  shared geometry. Row tags use the extension; no per-file size lookups.
- **Upper-screen preview:** header with icon, name and type tag; framed preview;
  details column (size, layers, frames, file size, format, or palette colors);
  footer warning when opening would replace unsaved art. `pd_preview_file_info()`
  in `core/files.c` reports size, layers and frames from the same decode; the
  file size comes from one `fopen`/`ftell` when a file is selected.
- **Keyboard:** lowercase by default; Shift gives one capital, a second tap caps
  lock (underlined); Backspace beside M with hold-to-repeat; bottom row
  123/abc, dash, space, underscore and cursor keys; footer Cancel | Done; the
  field shows the extension that will be added. Hex color entry keeps its own
  layout. `keyboard_keys()` drives drawing, taps and hold-to-delete.
- **Real-case names:** `text()` still capitalises labels and messages;
  `text_raw()` draws names the user typed (files, folders, paths, layers in the
  Layers list, options sheet and timeline, the upper-screen title). The font
  already had lowercase glyphs.

Tests: menu taps use `app_test_menu_point()` (PD_TEST only) via `tap_menu()`;
browser taps use `tap_file_row()`, `tap_file_footer()` and `tap_file_filter()`,
which mirror `browser_layout()`. New checks cover the type dropdown (open,
choose, outside tap), preview size/layers/frames/bytes, lowercase default,
one-shot Shift, caps lock, symbols toggle and Backspace hold-repeat. Host suite,
16-bit screen check, icon, NDS/DSi header and DLDI, and CIA (version 336) checks
pass. Native fixtures inspected for DSi and 3DS. The 3DS compiler rejected two
`snprintf` truncation patterns; both now copy with explicit lengths.

Sent save-only by DSi Link to `192.168.1.165` (the DSi's address changed from
.162; ask for the on-screen address each time): private OK SAVED (755200 bytes,
CRC-32 A52C369E) and standard OK SAVED (367104 bytes, CRC-32 AA53956B), both in
`Homebrew/Dev/Received/`. No 3DS transfer. Hardware acceptance pending.
Docker Desktop was not running; it was started for the NDS builds.

- `G:\dev\3ds\PixelDoodle\build\test-0.21.0\PixelDoodle.nds`: 367104 bytes; SHA-256 `121CFAA46C57B772409043630F7DC81628DED6C5839500D910F1BC254EFD5D92`
- `G:\dev\3ds\PixelDoodle\build\test-0.21.0\PixelDoodle.3dsx`: 868316 bytes; SHA-256 `BABFC9CE11A4EADA816512C2DBF423CC0486A7BAED7D90092E6C3AC82B7F4844`
- `G:\dev\3ds\PixelDoodle\build\test-0.21.0\PixelDoodle.cia`: 699328 bytes; SHA-256 `098606CFF49894B4649D3E83C764CD968267C60C1BE1C6817D268561E991B8AC`
- `G:\dev\3ds\PixelDoodle\build\private\test-0.21.0\PixelDoodle-Private.nds`: 755200 bytes; SHA-256 `B40F577EC53F81D5D56E5776445D55502EB193EBAB9C5FC1DB3364B29D784C33`
- `G:\dev\3ds\PixelDoodle\build\private\test-0.21.0\PixelDoodle-Private.3dsx`: 875692 bytes; SHA-256 `331D466C8316C3A6EDA74539A5719F65ECF51405F1E16A022673A0FEA7091DE1`

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

### Verified 0.20.0 candidates

Private and standard host/native builds pass. Added checks cover streamed ASE
frames/layers/durations/palette, exact RGBA, readback/overwrite/state preservation,
allocation refusal, allocation-free frame movement, cross-frame undo remapping,
boundaries and native actions. Onion and 16436 render-equivalence checks plus
independent GIF/ASE parsers pass. Desktop Aseprite batch opened/saved all test
frames and matched rendered RGBA within rounding differences; empty clearing
and durations pass. Broader desktop interop suite also passes: native/ASE roundtrips, compressed
cels, malformed-file preservation, palettes and variable/large PNG pixels.
DS/3DS action fixtures visually inspected. No Azahar visual
acceptance or device testing claimed. Five artifacts staged with SHA-256 readback;
NDS ARM9 version, direct input, onion and move labels verified. Hardware queue:
docs/ANIMATION-HARDWARE.md. No overnight transfers or webhook sends.

- `G:\dev\3ds\PixelDoodle\build\test-0.20.0\PixelDoodle.nds`: 355840 bytes; SHA-256 `95E9CD2B01D05F09D6512CF8A46550F3EAEC03721CF29174708B75E79B704749`
- `G:\dev\3ds\PixelDoodle\build\test-0.20.0\PixelDoodle.3dsx`: 855892 bytes; SHA-256 `85D632855D6476DFC77B1A2B8B98706D6EE0031C6151602756C291A1AA097272`
- `G:\dev\3ds\PixelDoodle\build\test-0.20.0\PixelDoodle.cia`: 690624 bytes; SHA-256 `7C362019B98B17654E02A8CD1AEBA0EBF1A484FE45CA2068DCD3FB3AE77DEB15`
- `G:\dev\3ds\PixelDoodle\build\private\test-0.20.0\PixelDoodle-Private.nds`: 743936 bytes; SHA-256 `0F82784A73E74168B64BA51EBDA44AFDB38102EDE1F81BC3D5268C629495781A`
- `G:\dev\3ds\PixelDoodle\build\private\test-0.20.0\PixelDoodle-Private.3dsx`: 863224 bytes; SHA-256 `879E81C852BF164AA4522E26591D6F76402D7C03830EEC394A6CAC6892F4C8E2`

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

### 0.19.0 verification and saved candidates

Private and standard host/native builds pass, including new onion tests and
repeated same-size project reload invalidation. Cached current-frame painting,
read-only neighbor alpha, opacity/visibility changes and ordered composite
alpha match the oracle. Existing GIF and 16436 render comparisons pass.
DS/3DS synthetic onion fixtures inspected. Five artifacts staged with SHA-256
readback. No hardware transfer or acceptance; use ANIMATION-HARDWARE.md.

- `G:\dev\3ds\PixelDoodle\build\test-0.19.0\PixelDoodle.nds`: 353792 bytes; SHA-256 `2D06308FAA8624313628A7371B8EA2401747956D1BF797CA54265CCF2591B64E`
- `G:\dev\3ds\PixelDoodle\build\test-0.19.0\PixelDoodle.3dsx`: 851456 bytes; SHA-256 `35AC50912217770092D5D6B30D8F1CF4A7404BAB33837E3646C6CC16D1AF1276`
- `G:\dev\3ds\PixelDoodle\build\test-0.19.0\PixelDoodle.cia`: 688064 bytes; SHA-256 `A1527E164318AC026960D1ABFB38FF06BED4E3697DB13ADD5207B8FB7204B43B`
- `G:\dev\3ds\PixelDoodle\build\private\test-0.19.0\PixelDoodle-Private.nds`: 741888 bytes; SHA-256 `2F0E7C0B36D957C744945447B88FF7A03C0B03972F9BEABF65592279C3EFBE63`
- `G:\dev\3ds\PixelDoodle\build\private\test-0.19.0\PixelDoodle-Private.3dsx`: 859112 bytes; SHA-256 `ABE559F354EC5CA03C3CD74830BA237CF5034849043F5AE805099D4209390A96`




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

- Current local source: **0.20.0**. Pushed checkpoint: `2ce8a56` (accepted
  animation/GIF baseline); onion increment: local `0f51d8c`. Later work is
  committed locally when verified; do not infer an extra push was requested.
- User accepted 0.17.0 playback and 0.18.0P animated GIF delivery to Discord
  on DSi. 0.19.0 onion skin and 0.20.0 ASE export/frame ordering require hardware
  testing. See `docs/ANIMATION-HARDWARE.md` for the current acceptance matrix.
- Quick separate-stroke issue is deferred. 0.16.1 queued capture was rejected
  for severe latency and removed. Keep direct input and version labels.
- Frames and Layers remain separate. Local candidate adds cached onion skin,
  streamed animated ASE export and allocation-free frame ordering that retains
  paint undo. Animated ASE import, linked cels, tags, structure undo, animated
  layer merge and sprite-sheet export remain future work.
- Native PDD/PDDA remains the device editing master. Older builds cannot read
  PDDA. Preserve user data/credentials and unrelated line-ending-only changes.
- Current candidate hashes/build results are near the top of this handoff;
  older artifact sections are historical. No overnight hardware sends or live
  webhook posts, and no CIA installation/Azahar visual acceptance claimed.

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

- DSi LL / DSpico: last confirmed running build is **0.18.0P** from
  `Homebrew/Dev/Received/PixelDoodle-Private.nds`. GIF delivery succeeded.
  Last DSi Link address: `192.168.1.162:17494`; use save-only Wi-Fi transfer
  after renewed readiness. Do not remove the SD card for manual copies.
- Standard NDS Received copy was last sent as 0.16.1 (rejected); use an exact
  newer staged standard candidate for any future test, not that old copy.
- 3DS last received standard 0.16.0 via 3dslink at `192.168.1.225:17491`;
  no later private transfer or CIA installation. Receiver is not assumed ready.
- 0.19.0/0.20.0 candidates are local only. User is asleep; do not send builds
  or webhook messages until requested with receiver readiness.

## Next hardware checks

Use `docs/ANIMATION-HARDWARE.md`. Start with onion On/Off responsiveness,
frame move plus cross-frame undo/redo, original-frame restoration after playback,
then layered/duration ASE export and card/memory refusal paths. Repeat on
DS Lite, DSi, standard 3DS and expanded New 3DS as available. Preserve native
masters; animated ASE import is intentionally rejected. Do not spend this
pass on the deferred rapid-handwriting issue.

## What comes next

Continue the Aseprite-style model: global layers, horizontal frames and cels.
Remaining priorities are bounded transactional animated ASE import, frame-range
selection/copy, local GIF/sprite-sheet export, tags/loop ranges, frame-structure
undo and all-frame layer merge. Keep DUPE edits independent despite shared tile
storage. Do not merge Frames and Layers without the user's instruction.

The approved file-screen redesign remains pending. Preserve the existing
six-icon rail, Layers layout, dedicated Delete Layer, blend dropdown and Tools
drawer. Use `docs/ANIMATION-HARDWARE.md` for acceptance rather than historical
pending lists below.

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

## Historical artifact hashes (rejected 0.16.1, CIA version 257)

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

Continue PixelDoodle in `G:/dev/3ds/PixelDoodle`. Read current sections of
this handoff, AGENTS.md, ANIMATION-CORE.md and ANIMATION-HARDWARE.md. User is
away and authorized local animation progress with documentation after each
increment. Checkpoint `2ce8a56` was pushed before new work. Later feature
commits are local unless explicitly documented otherwise. Preserve unrelated
whitespace-only changes, all private credentials and runtime data.

Latest user acceptance: DSi 0.18.0P animated GIF sent successfully to Discord.
0.17.0 playback also accepted. New onion/ASE/frame-order work needs hardware
checks. Do not reintroduce queued capture; rapid separate-stroke work is
explicitly deferred. Keep Frames and Layers separate. Do not claim animated
ASE import is supported. Native PDD is the editable master.

Run scripts/build.ps1 private then standard using the pinned Docker image;
record exact staged artifacts and hashes. Host suites include independent GIF
and ASE parsers. Desktop Aseprite supports --batch while Windows is locked.
Hardware delivery uses DSi Link save-only and 3dslink after renewed readiness.
Do not send unattended builds/messages or publish releases/updater payloads.
