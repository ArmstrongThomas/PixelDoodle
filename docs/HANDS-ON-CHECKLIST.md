## Timeline moves and Menu cleanup 0.24.0 (2026-10-26)

Sent save-only to DSi Link at `192.168.1.165`: `Homebrew/Dev/Received/PixelDoodle-Private.nds` (V0.24.0P, CRC-32 B2125FE5) and `PixelDoodle.nds` (V0.24.0, CRC-32 EC4E0412). Launch from Received.

- [ ] Menu has no Frames button. Row 1 is New Project alone; Layers from the
      rail opens the layer list, and the TIMELINE tab opens the grid.
- [ ] Pen lifted, hold L and tap frame numbers 2, 3, 4: each gets a yellow bar
      and yellow number; the memory line reads `3 SEL`. Tap 3 again with L to
      unmark it; tap 2 without L to clear all marks and select frame 2.
- [ ] Mark frames 2-4, hold one marked number for about 0.3 s, drag to column 7
      and release. Frames 2-4 move as one block starting at column 7; other
      frames shift. Their art, durations and links travel with them.
- [ ] Single-frame move: no marks, hold a frame number and drag to another
      column. The frame lands at that column; the current frame follows its art.
- [ ] After a block move, paint on a moved frame, undo it, and confirm undo
      still targets the moved art. Marks remain on the moved frames.
- [ ] Marks clear after NEW, DUPE, DELETE, NEW LINKED FRAME, opening a project
      and creating a new one.
- [ ] Hold a cel (not a number) for about 0.4 s until "Drag cel to another
      layer or frame" appears, drag to another layer in the same frame and
      release. The destination gets the image, the source cell becomes empty,
      links on both cels are cleared and undo history is cleared.
- [ ] Repeat dragging a cel to a different frame. The current frame follows the
      destination and the destination cell is outlined while dragging.
- [ ] Drop a cel back on its own cell: the move cancels. A normal sideways drag
      started before 0.4 s still scrolls the timeline; confirm this does not
      accidentally move cels.
- [ ] Single-frame project: hold-drag a cel between two layers; the same
      transfer and undo clearing apply.
- [ ] Note: tag ranges stay at their columns after frame moves; they do not
      follow the moved art. Verify this is acceptable or request tag-aware
      moves later.
- [ ] Same checks on 3DS when available. No 0.24.0 3DSX transfer was made.

## Tags and linked cels 0.23.0 (2026-10-26)

Sent to DSi Link (save-only): `Homebrew/Dev/Received/PixelDoodle-Private.nds`
(V0.23.0P, CRC-32 566F5C2E) and `PixelDoodle.nds` (V0.23.0, CRC-32 35674BA6).
Launch from Received in Pico Launcher.

- [ ] Timeline tag band: create a tag from a frame number > FRAME... > TAG row;
      set FROM/TO, direction, colour and name. The band paints those columns with
      the tag colour, a white cap on the current frame, and is readable on both
      screen sizes.
- [ ] Tap the band over a tagged column: the tag sheet opens for that tag. Tap an
      untagged column: nothing opens. Tap the band again after Done: still works.
- [ ] Playback with the playhead inside the tag loops only that range, honours
      reverse/ping-pong and returns to the editing frame on stop. A one-frame tag
      falls back to the whole animation.
- [ ] Cel sheet: tap the selected cel again, then Link with previous, edit either
      frame and confirm both change. Unlink leaves the current image independent.
      New linked frame adds a frame joined to the current group. Undo across a
      linked edit behaves as one step.
- [ ] Rename a tag through the keyboard; names longer than 31 bytes are limited,
      and Cancel keeps the previous name. REMOVE deletes the tag and closes.
- [ ] Export > Aseprite from a tagged/linked project, open it on a desktop with
      Aseprite if available: frames, durations, tags (names/ranges/directions/
      colours) and linked cels are present and editable.
- [ ] Import an animated `.aseprite` with several frames, tags and linked cels.
      The browser preview shows its frame count without a long pause; Open builds
      all frames and the timeline shows the tag band and joined cels. Editing a
      linked cel updates its group.
- [ ] Import rejection: a file with layer groups, tilemaps, indexed colour or a
      cel outside the canvas reports an error and leaves the current drawing
      intact.
- [ ] Memory: open a large animation on DSi; note any failure. A malformed or
      over-limit file keeps the open project, but an allocation failure while
      frames are added reports an error with a partly imported project.
- [ ] Same checks on 3DS when available. No 3DS transfer was performed for this
      candidate.

## File screens 0.21.0 (2026-10-26)

Sent to DSi Link (save-only): `Homebrew/Dev/Received/PixelDoodle-Private.nds`
(V0.21.0P) and `PixelDoodle.nds` (V0.21.0).

- [ ] Menu order: New Project | Frames; Save | Save As | Export; Open;
  Settings | Recovery; Exit | Send Sprite (private). Each opens the right page.
- [ ] Layers footer: New Layer, Adjust, Delete. Adjust Apply/Cancel return to Layers.
- [ ] Browser: 7 rows on DSi, icons match types, folders show `>` and open on a
  second tap, Up returns, drag scrolls without selecting.
- [ ] Type dropdown: opens, every type filters correctly, tapping outside closes it.
- [ ] More opens Rename/Duplicate/Delete for the selected file; Recent still works.
- [ ] Upper screen: name in real case, type tag, size/layers/frames/file size,
  unsaved-art warning when the current art is unsaved; palettes show colors.
- [ ] Keyboard: lowercase default, Shift once, caps lock on double tap, 123/abc,
  Backspace hold-repeat, cursor keys, extension shown; hex color entry unchanged.
- [ ] File and layer names keep the typed case everywhere (browser, Layers, Frames,
  upper-screen title).
- [ ] Same checks on 3DS when available (9 browser rows).


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

Animated ASE IMPORT remained unsupported in 0.20.0; 0.23.0 supersedes that and
now imports frames, tags and linked cels. Preserve PDD as the editable device
master. Test exported files in actual desktop Aseprite as well as the
independent parser. Desktop batch tests can run while Windows is locked; no
Azahar visual acceptance is claimed.

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

## 0.16.1 retest (2026-10-22)

- [x] Standard and private NDS sent save-only (both OK SAVED).
- [x] No 3DS transfer: user said it was not ready.
- [ ] Confirm V0.16.1 (standard) / V0.16.1P (private) at upper-left in-app.
- [ ] DSi: quick handwriting and tiny lift/touch strokes; compare with 0.16.0.
- [ ] Diagnostics: Q ON, peak depth, LOST overflow incidents, PEN/INK deltas.
- [ ] Rapid touches across menu changes never activate an unseen new menu.
- [ ] Private Send Sprite, text entry, slider/long-press drags and Exit still work.
- [ ] DS Lite and New 3DS: queued capture and FREE/LARGEST, when available.

0.16.0 FRAMES menu presence is confirmed by the user; missed handwriting strokes
remain. Frames and Layers must remain separate until the user tests thoroughly.
DSi pre-0.16.1 FREE/LARGEST are both 14135 KB; card write/read 1241/1433 KB/s.

## 0.16.0 candidate (2026-10-22)

- [x] Standard 3DSX sent through 3dslink (exit 0).
- [x] Standard and private NDS sent through DSi Link (both OK SAVED).
- [ ] DSi TWL, DS Lite NTR, New 3DS: rerun Memory + Card Test; record FREE,
  LARGEST and card rates. Previous photos confirmed masks/card rates only.
- [ ] Rapid handwriting/taps: no completely missing strokes; compare 0.15.0.
- [ ] Overview catches up after lifting; guide/eraser/undo colors remain correct.
- [ ] Menu > Frames: NEW, DUPE, layer/cel selection and durations.
- [ ] DS six-row timeline scroll track reaches all eight layers.
- [ ] Pan+L/R and New 3DS ZL/ZR step frames without drawing/undo conflicts.
- [ ] Native animation Save As/reopen/recovery retains every frame and duration.
  Use a new filename: older builds cannot open PDDA/version 10.
- [ ] Delete Frame cancellation preserves art/history; confirmation clears undo.
- [ ] Private Send Sprite still uploads the current frame.
- [ ] Regression: selection/copy/cut/paste, layers, file browser, palette, Settings.

0.15.0 private Send Sprite is accepted by the user. This does not accept the
new 0.16.0 build. Playback/onion skin/animated ASE are not present to test yet.

## DSi Link (2026-10-21)

- [x] DSi Link joins Wi-Fi on the DSi LL and shows its address.
- [x] A sent build is received, checked and saved to `Homebrew/Dev/`.
- [ ] Booting the sent build through Pico Loader (first try returned to DSi
  Link with SD CARD NOT AVAILABLE).
- [ ] Exiting a booted app returns to DSi Link.
- [x] The PixelDoodle.nds saved by DSi Link boots from Pico Launcher.
- [ ] Run 0.15.0 Settings > Diagnostics > Run Memory + Card Test on the DSi.

## Memory optimization 0.15.0 (2026-10-21)

- [ ] Standard and private NDS builds boot on the DSi; colors, text and the
  canvas look the same as 0.14.9 on both screens.
- [ ] Settings > Diagnostics > Run Memory + Card Test on the DSi. Record FREE,
  LARGEST, MASKS and card WRITE/READ. Masks should read 128 KB in DSi mode.
- [ ] If possible, repeat in DS mode (or on DS Lite). Masks should read 8 KB.
- [ ] Repeat on the 3DS (already sent by 3dslink).
- [ ] Copy/cut/paste, selection, layers, file open/save and drawing still work.
- [ ] Private build: Send Sprite still uploads; standard build has no Send.

## Fill and shape dither 0.14.9 (2026-10-21)

- [ ] Fill panel: tap DITHER to cycle Solid, 25%, 50%, 75%. Fill an area over
  existing art at each density; gaps keep the art underneath.
- [ ] Global fill and Visible match still select the same region with dither on.
- [ ] Line, Rect and Ellipse: cycle DITHER in the shape panel. Draw outline and
  filled shapes; the preview while dragging matches the committed result.
- [ ] Dithered shapes and fills line up with Pencil dither at the same density.
- [ ] Fill Done (now lower in the panel) and the X both close the panel.
- [ ] Each dithered fill or shape is one Undo step.
- [ ] Relaunch: Fill and shape dither settings persist separately.

Host suite, builds, icon/header/CIA checks pass; Azahar booted the exact 3DSX.
Interactive emulator use and DSi hardware acceptance are pending.

## Tool drawer sliders and labels 0.14.8 (2026-10-21)

These checks apply to 0.14.9 as well; its launcher subtitle shows 0.14.9.

- [ ] Pencil, Eraser, Fill tolerance, Line, Rect and Ellipse: drag each slider
  repeatedly both ways. The knob stays under the stylus; no stale touches,
  skipped steps or jumps.
- [ ] Tap once on each slider track and confirm the value moves to the tap.
- [ ] Open Eraser, then Line, Rect and Ellipse. Confirm the shape size slider
  moves and the eraser size is unchanged.
- [ ] Fill: the - and + buttons beside `TOL n` change tolerance by one.
- [ ] Select panel: Delete, SEL ALL, Deselect and Transform labels stay inside
  their buttons, and each tap triggers the button under the stylus.
- [ ] With Tools open and no settings panel, tap the canvas beside the rail
  and confirm the drawer closes without drawing.
- [ ] Brush Details previews and Load/Store; Pixel Grid at 4x.
- [ ] Private Send Sprite with `fat:/PixelDoodle/private/discord-webhook.txt`:
  `share.png` is created, uploaded, removed, and the UI never freezes.

Host suite, builds, icon/header/CIA checks pass. Azahar booted the exact 3DSX;
emulator drawer interaction and DSi hardware acceptance are pending.

## Palette workspace 0.14.2 (2026-10-21)

Use the Color icon in the persistent shortcut rail on DSi and 3DS.

- [ ] Confirm the upper screen stays touch-inactive, shows FOREGROUND TARGET,
  BACKGROUND TARGET or SWATCH instead of "DISPLAY ONLY", and uses the full
  display for active color, FG/BG, selected swatch, RGB, HSV, alpha and recent
  colors. Confirm recent swatches stop before the screen edge.
- [ ] Confirm the persistent shortcut rail uses the thin mockup Undo, Redo,
  Color, Layers, Toolbox and Menu glyphs.
- [ ] In Swatches, switch density through 8, 6 and 5 columns. Confirm every tile
  remains square and the list scrolls without selecting while dragging.
- [ ] Select FG and BG separately, apply swatches, and verify the swap control.
- [ ] Hold a swatch for 150 ms and drag to reorder it.
- [ ] Open Picker. Confirm the saturation/value field remains square and the
  vertical hue rail, alpha strip and six-column quick palette respond quickly.
- [ ] Scroll the quick palette without changing the current color.
- [ ] Scroll until a row is partly hidden. Confirm it clips at the palette panel
  boundary and never draws over FG/BG, swap or the hexadecimal field.
- [ ] Tap the HEX field, enter six digits through the hex OSK, press Done, and
  confirm the exact RGB value reaches only the selected FG or BG target.
- [ ] Cancel once and confirm FG, BG, hue, saturation and value are restored.
  Repeat with Use Color and confirm the values persist.
- [ ] In Edit, exercise Add, Replace, Remove, Move, Load and Save. Relaunch and
  confirm the saved palette persists.
- [ ] Recheck Pencil size/dither/pressure and Select rectangle/lasso options,
  plus layer blend, opacity, clipping, alpha lock and hold-drag reorder.

Host tests pass on both native layouts. Package/build checks pass; emulator and
physical acceptance remain pending.

## Layer workspace 0.14.4 (2026-10-21)

- [ ] On NDS, confirm six layer rows are visible at once; on 3DS confirm all
  eight fit below the persistent rail.
- [ ] Use the row grip, name area, visibility eye, Lock control and Options
  control without selecting the wrong target.
- [ ] Open Options and exercise opacity, blend, Rename, Duplicate, Merge Down,
  Lock and Alpha Lock.
- [ ] Open Blend and choose each of Normal, Multiply, Screen and Overlay directly
  from the dropdown. Confirm the first touch selects immediately and tapping
  the closed Blend button never changes mode.
- [ ] Use the dedicated footer Delete Layer action and confirm both Cancel and
  Delete. Confirm locked and last-layer cases explain why deletion is blocked.
- [ ] Quick-drag to scroll, then hold 150 ms and drag to reorder. Confirm the
  order changes only after release.
- [ ] Confirm the top screen shows Composite and Active Layer side by side and
  marks a hidden active layer.

## Drawing tools drawer 0.14.5 (2026-10-21)

- [ ] Open the toolbox from the persistent rail and confirm the single tool
  column fits without scrolling on DSi and 3DS.
- [ ] Select Pencil, Eraser, Fill, Select, and each shape and confirm the
  matching settings panel opens to the right while the column stays visible.
- [ ] Select Sample and confirm the drawer closes without opening an options
  screen.
- [ ] Open Guides and Close independently, then reopen the drawer from the
  persistent rail.

## Transparent Tools drawer 0.14.6 (2026-10-21)

- [ ] Open Tools without selecting a tool and confirm the canvas remains
  visible to the right of the single-column rail.
- [ ] Select a tool and confirm only its compact settings panel covers the
  canvas; close the settings panel or drawer and confirm the canvas returns.

## Pixel grid 0.14.7 (2026-10-21)

- [ ] At 4x zoom, enable Pixel Grid in Guides and confirm grid lines appear.
- [ ] Disable Pixel Grid and confirm the artwork pixels and saved exports are
  unchanged.
- [ ] Open Pencil options and confirm the footprint preview changes with size,
  tip and dither settings. Confirm the slider knob is centered in its track.
- [ ] Open Brush Details and confirm all four presets show distinct brush
  previews, then Load and Store them.
- [ ] Open Brush Details from Pencil and Transform Selection from the Select
  panel. Confirm both centered sheets can be closed without changing canvas.

## Private server test overlay 0.3.7 (2026-10-21)

`build/PixelDoodle-Community-0.3.7-app-private.zip` is a private local test
overlay for the installed 0.3.6 server. It changes device API authentication
responses: malformed, expired or revoked grants return HTTP 401; a valid grant
missing the required scope returns HTTP 403. This has no schema or configuration
change. The PHP suite passes (36 tests / 318 assertions).

Install by extracting the ZIP over `F:/pixeldoodle-app` and running
`F:/pixeldoodle-app/Update-Community.bat`. Back up the private databases and
preserve `.env`, `APP_KEY`, assets and device grants. The updater runs Laravel
and gallery migrations; the current schema-5 migration is a no-op on an already
updated 0.3.6 installation. The archive contains no public webroot ZIP and was
not deployed. Re-test a revoked/expired grant (401) and a valid grant missing a
scope (403); confirm both clients clear credentials only for 401.

## Server update 0.3.6 (2026-10-16)

PHP fixture suite rerun outside sandbox: 35 tests / 317 assertions pass.
PHP is_readable returned false inside sandbox despite readable autoload.php;
this was not a missing Composer dependency. Server ZIP refresh includes schema 5
and current device APIs. Update-Community.bat clears config/routes/views,
discovers packages and runs Laravel + gallery migrations. No APP_KEY generation
or .env overwrite. Compiled bootstrap caches excluded from archives.
Extract private ZIP into F:/pixeldoodle-app and public ZIP into F:/pixeldoodle.
Back up private data and .env first; run F:/pixeldoodle-app/Update-Community.bat.
Keep existing pairing enabled and browser uploads disabled. No new credentials.
Native submission UI and hardware acceptance are still pending separately.

## Community milestone continuation (2026-10-16, local only)

This increment connects the native UI to the local device API. Both handhelds
now expose Cached, Recent, Favorites and My Uploads modes; remote pages use
bounded parsing and page-sized thumbnail caches. Public items show dimensions
and artist, favorites can be toggled with the scoped bearer API, and My Uploads
shows moderation state and a bounded review reason. Import uses the server's
device-sized PNG derivative and the existing unsaved-art confirmation. Pairing
requests the three community scopes only when the user explicitly pairs or
re-pairs; existing grants are not upgraded silently.

The shared Community dropdown now captures the opening touch, clips rows,
supports swipe and button scrolling, closes on outside tap and dismisses on Back.
Grant responses distinguish 401 invalid/expired/revoked credentials from 403
valid grants missing a scope. Both clients clear credentials only on 401 and
preserve them on 403 while showing explicit re-pair guidance. NDS hostname
resolution now uses bounded nonblocking DNS UDP across frame updates.

Still incomplete: native submission metadata/preview and resumable chunk upload
are not connected to the handheld UI. The server API supports those operations,
but native submission is not end-to-end. Host tests pass, including dropdown,
community mode/relaunch/auth fixtures, DNS packet cases, HTTPS framing/bounds,
131,072 render-oracle comparisons and 16,316 stroke-frame comparisons. PHP
feature tests pass (36 tests / 318 assertions). A fresh 3DSX/CIA build and CIA
metadata check pass. The NDS build was attempted with the pinned BlocksDS image
but Docker could not connect to its Linux engine, so current NDS binary/header
verification remains unconfirmed.

No production deployment/configuration, external notification, GitHub write,
hardware transfer or card write occurred. Hardware TLS/pairing and online flows
remain unaccepted. Keep the existing live pairing setting unchanged.

## DSi pairing candidate (2026-10-21)

The pinned BlocksDS v1.24.0 build now passes. NDS header, ARM bounds and DLDI
verification pass; artifact is `PixelDoodle.nds` at the repository root with
SHA-256 `FD14F6B430EAE0F5A3D721D3EF89F04AFCF6F726594ED041D698CEF7E8BB76FA`.
Docker Desktop's stale AF_UNIX runtime socket directories were preserved by
renaming them to quarantine paths, then Docker Desktop was updated in place to
4.96.0. The Linux engine responds as 29.9.1. No factory reset, Docker data
deletion, WSL unregister or physical device transfer occurred.

Next physical DSi test: start pairing from Menu > Community > Pair. Confirm the
console remains responsive during DNS/TLS setup, displays an approval code,
and reaches Connected after approving it on the website. Then switch among
Recent, Favorites and My Uploads, exercise the mode dropdown (including swipe
scroll), refresh and a favorite toggle, exit and relaunch, and confirm the grant
still works. A revoked grant should prompt re-pair; a valid grant missing a
scope should retain the grant and explain that re-pairing is needed. Basic
drawing/save responsiveness around the network flow should remain intact.
Native artwork submission is not implemented and is outside this test.
## Latest 3DS transport candidate

- [x] Collab Doodle software TLS replaces failed HTTPc transport.
- [x] Host HTTP parser tests and live verified begin/pending poll.
- [x] Live wrong-hostname rejection.
- [ ] New Azahar pairing pass (UI automation failed targeting load dialog).
- [ ] Hardware code/approval/poll/persistence/revocation.
- [x] Move the existing 3DS pairing TLS request off the UI thread into an owned worker.

No hardware transmission tonight, per user request. No server update needed.

# Pixel Doodle Community deployment checklist

## Before server setup

- [ ] PHP/Apache runtime is supported PHP 8.3+ with `pdo_sqlite`, `sqlite3`,
  `openssl`, `curl`, `mbstring`, `fileinfo`, `sodium` and `zlib` enabled in both
  CLI and Apache.
- [ ] Extract `*-public.zip` into `F:\pixeldoodle` and `*-private.zip` into
  `F:\pixeldoodle-app`. Keep `F:\pixeldoodle-data` private.
- [ ] Confirm `F:\pixeldoodle-app\community\public\index.html` exists after
  extraction before visiting the site.
- [ ] Add `deploy/apache.conf` from the public archive to Apache, verify `httpd
  -t`, restart the virtual host on port 8086, and confirm the Cloudflare Tunnel
  reaches `https://pixels.7db.pw/`.
- [ ] Back up the pre-existing website gallery DB and identify its exact path.
  Stop its old writer before schema migration; do not migrate a Collab Doodle DB.
- [ ] Copy `.env.example` to the private `.env`, create/secure data folders,
  preserve a fresh `APP_KEY`, run app/gallery migrations and configure HTTPS.

## Account and publishing checks

- [ ] Register the exact Discord OAuth callback; keep Public Client off; set
  client secret only in private `.env`, then test consent/cancel/logout.
- [ ] Set real SMTP password in private `.env`, enable email sign-in and test
  message delivery/spam, invalid/expired/replayed codes, resend and logout.
- [ ] Create and edit an artist profile. Confirm the studio has no browser upload
  form and clearly says artwork submission is console-only.
- [ ] POST directly to `/api/account/studio/upload`; confirm it returns 404 and
  creates no artwork while `WEB_UPLOADS_ENABLED` is false or unset.
- [ ] Verify existing submission status/reason views and moderation still work.
  Native My Uploads listing is implemented; native console submission is not.

## Discord moderation checks

- [ ] Create a private review channel, bot user and moderator role. Set Discord
  Public Key and application ID, enable `DISCORD_BOT_ENABLED=true`, load Sodium
  in Apache PHP, and clear Laravel config before saving the endpoint URL:
  `https://pixels.7db.pw/api/discord/interactions`.
- [ ] After Discord accepts the endpoint, configure the bot token, guild/channel
  and role IDs in private `.env`; give the bot only Send Messages, Embed Links
  and Attach Files.
- [ ] Grant an existing verified account moderator access; explicitly link its Discord
  user ID with community:link-discord if it previously signed in using email. Test a
  non-moderator separately.
- [ ] Run `php artisan community:review-tick` manually on one private test upload.
  Confirm image, title, description, artist and dimensions in the locked channel.
- [ ] Test Approve, Reject with/without reason, Hold, duplicate/stale buttons,
  channel/guild/role rejection, concurrent website decision, edited message and
  worker restart.
- [ ] Schedule `php artisan schedule:run` in Windows Task Scheduler every minute.
  Test a confirmed post's eight-hour deadline using a staging clock or test DB;
  confirm Hold/Reject cancels its timer and public approval occurs once.
- [ ] Check handling of rate limit, explicit failure and uncertain network timeout;
  uncertain sends must not be resent or start the approval clock automatically.

## Operations

- [ ] Keep `ENABLE_DEVICE_PAIRING=false` in production. In an isolated staging
  database and HTTPS host, first test the current 3DS candidate on hardware:
  reject an untrusted/expired certificate and redirects, begin a pairing code,
  approve and deny from the signed-in website, confirm the one-time grant is
  stored under `sdmc:/3ds/PixelDoodle`, then revoke it from another signed-in
  browser session. Confirm rate limits, expired/replayed codes, disabled
  accounts, and cross-account device revocation. Do not copy the bearer file
  into screenshots, logs or support bundles.
- [ ] On each NDS/DSi card, run `scripts/provision-nds-seed.ps1 -AppDataRoot
  <card>\PixelDoodle` once to provision a unique TLS seed. Never clone that
  private folder to another card or reuse Collab Doodle's seed. Verify that the
  NDS client rejects invalid certificates/hostnames before approving its TLS.
- [ ] After NDS/DSi TLS checks pass, set `ENABLE_DEVICE_PAIRING=true` in the PHP
  server `.env`, clear config cache, then test approval/denial, expiry, grant
  persistence, account disable and revocation separately on DSi and DS Lite.
  Keep pairing default-off until those hardware checks pass.
- [ ] Configure backups for both private databases and `.env`; test a restore,
  including SQLite WAL contents. Confirm service account permissions and restart.
- [ ] Ensure Collab Doodle remains healthy; its code, DB and vhost are separate.

Local automated checks for the 2026-10-16 continuation: host regression suite
passes, including 131,072 composite-oracle comparisons and 16,316 drawing-frame
comparisons; 3DSX/CIA build and CIA identity checks pass (title 000400000CE47800,
version 208). The pinned BlocksDS v1.24.0 NDS ROM built and NDS/DSi header, ARM
bounds and DLDI checks pass. PHP tests previously passed (35 / 317 assertions);
current checkout lacks `community-php/vendor/autoload.php`, preventing a rerun.
Docker required elevated access to its named pipe. All-platform build command:
`powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Target all`.
Azahar booted the 3DSX and reached Community/Pair, but its HTTPS pairing request
failed; emulator logs show default-certificate handling is stubbed. No persistent
device grant was issued. Next hardware checks are TLS rejection and pairing
approval/denial/expiry/revocation on 3DS, DSi and DS Lite. The user enabled
pairing on the live server for testing. Node reference 33 groups. Browser checks
used synthetic local-only accounts/artwork. Live email/Discord, uploaded user art
and other hardware flows remain untested; no deployment or GitHub write occurred.






