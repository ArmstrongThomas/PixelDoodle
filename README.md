
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

## Hands-on testing and setup

Running queue: [docs/HANDS-ON-CHECKLIST.md](docs/HANDS-ON-CHECKLIST.md).
Provider/hosting walkthrough: [docs/SETUP-GUIDE.md](docs/SETUP-GUIDE.md).
Editable UI mockups: [docs/mockups/pixel-doodle-workflow.html](docs/mockups/pixel-doodle-workflow.html).
Current developer handoff: [docs/HANDOFF.md](docs/HANDOFF.md).

# Community implementation status (2026-10-16)

Local PHP work includes schema-5 scoped favorites and resumable PNG upload APIs. Previously prepared 0.3.5 server archives remain local under `build/`; this increment produced no new server package. Fresh native build and verification results are recorded in `docs/HANDS-ON-CHECKLIST.md`.

The native Community page now has Cached, Recent, Favorites and My Uploads modes on both handhelds, with a reusable touch-scroll dropdown, bounded remote thumbnails, favorite toggles, moderation status/reason and confirmed import of the device-sized PNG derivative. Grant errors distinguish invalid/revoked grants (401, clear and re-pair) from missing scopes (403, preserve and explicitly re-pair). NDS DNS resolution is nonblocking and bounded. Console submission metadata, immutable preview and resumable chunk upload are still unimplemented. Host and PHP checks pass; 3DSX/CIA and NDS builds pass. On 2026-10-21, Docker Desktop 4.82's stale runtime sockets were quarantined without deleting them, then Docker Desktop was updated in place to 4.96.0; Linux Engine 29.9.1 is running. DSi pairing and online-gallery hardware acceptance are the next tests. No device transfer or card write occurred.
# Pixel Doodle 0.18.0 candidate

Offline solo pixel art for Nintendo DS/DSi and Nintendo 3DS. **One repository, one shared C engine and UI, three outputs: NDS, 3DSX and CIA.**

Historical 0.16.1 notes (capture rejected and removed in 0.16.2): The
main upper-screen header and Diagnostics now show **V0.16.1**, or **V0.16.1P**
for private builds. Drawing input is captured independently of rendering using
a bounded queue (NDS VBlank capture; 3DS input worker). Diagnostics reports
queue status/depth/overflow and contact/stroke counts. The 0.16.0 redraw change
did not eliminate missed strokes on hardware; 0.16.1 needs a fresh test.

0.16.0 adds the first animation core and timeline: **Menu > Frames** beside New
Project. Create empty or duplicate frames, choose cels in the layer/frame grid,
set durations, and save/reopen native animations. Layer properties are shared
across frames. Pan+L/R and New 3DS ZL/ZR step frames. The frame cap is 256,
subject to memory. See [animation core](docs/ANIMATION-CORE.md) for controls,
file compatibility and limits. Playback, onion skin and animated Aseprite I/O
are not implemented yet. Frame deletion confirms and clears undo history.

The handwriting candidate retains displayed ink on lift and defers full
preview redraw until a brief pause. New strokes always stamp their first point.
Diagnostics fixes an optimized-build bug that previously left FREE/LARGEST
unmeasured. Hardware acceptance on all three devices is pending.

## Current build profiles

Community pages are disabled in standard builds while that feature is postponed. The separate private builds add **Send Sprite** to Menu, with **Send Active Layer** and **Send Visible Composite** choices. The first sends only the selected layer, preserving its transparency and opacity; the second flattens all visible layers. Build both private console packages with `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Target all -Private`. Outputs are `build/private/nds/PixelDoodle-Private.nds` and `build/private/3ds/PixelDoodle-Private.3dsx` (homebrew 3DSX; no CIA is produced).

**Menu > Adjust** opens the hue, saturation and brightness workspace. Choose
Selection, Current Layer or Visible Layers; use signed sliders and fine-step
buttons; then review the side-by-side Before/After top-screen preview and choose
Apply or Cancel. Visible-layer changes preserve layer structure, opacity, blend
and alpha and share one undo transaction. See
[color adjustments](docs/COLOR-ADJUSTMENTS.md).

The lower-screen palette workspace now has Swatches, Picker, and Edit modes.
Swatches are square at 8, 6, or 5 columns per row, scroll vertically, and apply
to explicit foreground/background targets. The Picker keeps a square
saturation/value field with a vertical hue rail, alpha, foreground/background
swap, direct hexadecimal entry through the shared keyboard, and a scrollable
six-column quick palette. Add, Replace, Remove, Move, Load, and Save are isolated
in Edit mode. The persistent shortcut rail uses the accepted mockup line icons
as six packed 16x16 glyphs in otherwise unused font slots. The upper screen
names the active target or selected swatch and uses the full display for the
active color, FG/BG cards, selected swatch, RGB/HSV/alpha and recent colors,
with clipped swatches and a visible outer margin. See
[palette workspace](docs/PALETTE-WORKSPACE.md).

The persistent toolbox icon opens a single-column drawing-tool drawer with
Pencil, Eraser, Sample, Fill, Select, Line, Rectangle, Ellipse and Guides.
Selecting a tool opens its settings in a compact panel to the right where
applicable; Sample has no settings panel. With no settings panel open, the
canvas remains visible through the unused right side of the drawer. See the
controls below.

Advanced Brush Details and Transform Selection open as centered sheets over the
canvas with consistent headers, close controls and grouped actions. Brush
Details includes visual previews of all four saved presets, and brush size uses
a centered filled slider.

Tool settings sliders set their value on the first touch and follow the stylus
to the nearest step. The Line, Rect and Ellipse size slider always edits the
shared pencil size, even after using Eraser. Fill tolerance shows its - and +
buttons beside the value.

Fill, Line, Rectangle and Ellipse can use the same 25/50/75% ordered dither as
Pencil. Tap **DITHER** in the Fill panel or the shape panel to cycle Solid,
25%, 50% and 75%. Gaps keep the pixels underneath, and the pattern lines up
with Pencil dither on the canvas. Fill and the shapes keep separate settings,
which persist with the other tool preferences.

0.15.0 frees DS memory ahead of animation support. Standard NDS builds leave
out the network code (Send Sprite stays in private builds), DS mode sizes its
selection masks to 128 x 128, the clipboard is created on first copy, and the
DS screens use 16-bit buffers. **Settings > Diagnostics > Run Memory + Card
Test** shows free memory, the largest free block and card read/write speed;
it writes a 256 KB test file in the app folder and removes it.

Standard/private 0.16.1 artifact SHA-256 (CIA version 257):

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

Staged standard copies: `build/test-0.16.1`; private: `build/private/test-0.16.1`.
Both NDS variants were sent to the DSi on 2026-10-22 (OK SAVED); 0.16.1 was
not sent to the 3DS. Hardware drawing acceptance is pending.

Private builds embed the webhook URL from the ignored, one-line `discord-webhook.txt` in the repository root at build time. The URL supplied for this candidate is already in that local build input; no webhook file needs to be copied to either console. A one-line `sdmc:/3ds/PixelDoodle/private/discord-webhook.txt` on 3DS or `fat:/PixelDoodle/private/discord-webhook.txt` on DSi overrides the embedded URL when present. An invalid override stops the send. Treat the private binaries and any override file as credentials. The app temporarily writes `share.png` in its private app folder and removes it after the upload finishes. A connection error is shown in the app and does not trigger a second upload automatically.

The 2026-10-21 private NDS diagnostic image fixes the Wi-Fi ARM7 core selection
that caused a lockup immediately after `share.png` was written. The upload now
reports Wi-Fi initialization, association/DHCP, DNS, certificate verification,
upload, and response stages in the app's notice line. Load
`build/private/nds/PixelDoodle-Private.nds` with melonDS DLDI enabled; folder
sync makes the virtual SD card writable and exposes `PixelDoodle/private`.

Current priorities: [agreed roadmap](docs/ROADMAP.md). On physical New 3DS XL, the user has accepted symmetry/tile, selection tools, brush options, layers and file/Aseprite flows. A combined 512x canvas + both symmetry/tile axes + large brush + six layers is slow. The replacement 3DS software TLS passes live host begin/pending-poll checks; new emulator and hardware acceptance remain pending. Native Online/Cached gallery browsing, favorites and My Uploads are implemented but awaiting live-device acceptance.

## Local community website prototype

The [community website/service](community/README.md) is deployed separately as PHP/Apache at `https://pixels.7db.pw/`. Website uploads remain hidden/disabled. Verified account pairing is opt-in through `ENABLE_DEVICE_PAIRING`; explicit pair/re-pair requests the gallery, favorites and upload scopes, while existing grants are not silently upgraded. NDS TLS requires a fresh unique seed in each card's app-private folder; use `scripts/provision-nds-seed.ps1` and do not clone or reuse Collab Doodle's seed. The 3DS client now reuses Collab Doodle software TLS. Run `scripts/test-https-live.ps1` explicitly to test the crypto configuration and POST parser using host adapters against the live site (creates an unapproved, expiring request). NDS hardware pairing remains untested.

## 0.12.0 Community: offline cache viewer

**Menu > Community** opens a six-thumbnail gallery with top-screen preview, pages, Refresh and **Open Copy**. Opening confirms before replacing current art, then creates an unsaved imported project. Browsing never changes your artwork.

For this first stage, manually place PNGs up to 128x128 in `PixelDoodle/community` on DS/DSi, or `3ds/PixelDoodle/community` on the 3DS SD card. No folder or storage shows an empty-state explanation. Invalid images remain unreadable instead of replacing your art. This is also the local cache mode. Online modes use the paired account and cache one page of device-sized thumbnails. Import requires confirmation and creates an unsaved project; full-resolution media download and native submission are not implemented yet.

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

Automated, independent PNG, desktop Aseprite, and all platform build checks pass. Open in Azahar and melonDS for interaction testing. [NDS candidate](build/test-0.10.0/PixelDoodle.nds) Ã‚Â· [3DSX candidate](build/test-0.10.0/PixelDoodle.3dsx). User accepted 0.9.1 on New 3DS XL, noting manageable lag at extreme 512-square layer/guide settings; 0.10.0 interaction acceptance is pending.

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

1Ã¢â‚¬â€œ128 pixels per side RGBA canvas, up to eight layers and up to 32 undo/redo transactions within a fixed 512 KiB pixel-history budget. New projects start with one layer. Layers shows six rows on NDS and all eight on 3DS, with direct grip, visibility, lock and options controls. The touch screen retains rename, confirmed deletion, duplicate, opacity, a Normal/Multiply/Screen/Overlay dropdown, Alpha Lock, restricted Normal merge-down and 150 ms hold-to-reorder. Dedicated footer Delete uses a confirmation. The top screen shows composite and active-layer previews side by side, even when the active layer is hidden. Closing Layers restores the artwork overview. Memory indicators report allocated document memory and slot capacity, not free heap. See [layer behavior and compatibility](docs/LAYERS-SLICE1.md).

The palette workspace uses Swatches, Picker, and Edit tabs. The square Picker
has a vertical hue rail, alpha, foreground/background targets and swap, direct
hex entry through the shared OSK, and a scrollable six-column quick palette.
Quick-palette rows are clipped below the target and hex controls while scrolling.
Swatches support 1-256 colors in an 8/6/5-column square grid. Drag vertically to
scroll; hold 150 ms to reorder, with edge scrolling. Palette management actions
live only in Edit. The touch-inactive upper screen now uses a complete color
dashboard. See
[palette workflow](docs/PALETTE-WORKSPACE.md).

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
- Touch toolbar: packed mockup glyphs for Undo, Redo, Color, Layers, Toolbox,
  Menu/Back.

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

Variable dimensions (2026-10-14): user caps canvases at 128 per side; larger profiles are no longer planned. New Project > Size offers 16/32/64/128 square presets and custom independent width/height 1Ã¢â‚¬â€œ128. Tap W/H, type digits, tap Set, then Done. New small projects fit at an integer zoom; drawing, panning and overview respect dimensions. PDD6 adds validated dimensions in the existing width/height header fields while retaining the PDD5 fixed-stride payload and CRC; legacy PDD1Ã¢â‚¬â€œ5 load as 128x128. Aseprite and PNG serialize only actual dimensions; recovery uses PDD6 too. Smaller canvases currently retain the same preallocated RAM capacity. Native/ASE roundtrip, bounds, rectangular drawing and desktop PNG dimensions/pixels are checked. Local/hardware acceptance pending.

Eight-layer candidate (2026-10-14): shared capacity is eight layers on NDS, DSi and 3DS, still defaulting to one. New Project supports one through eight starting layers. Quick vertical drag scrolls the layer list; hold 150 ms then drag reorders, with edge scrolling. Visibility/options remain per-row. Core uses eight stable slots and an eight-bit enabled mask; undo pixel budget stays 512 KiB. PdDocument is 1185976 host bytes; extra slots add about 256 KiB. Calculated NDS app/document + two presentation buffers + native import staging/file buffer is about 2.6 MiB before code/SDK/stack overhead; this is not measured free RAM. Hardware full-document stress/performance acceptance remains pending.

PDD7 writes eight property pairs, eight 64-byte names, eight fixed 128-stride pixel planes, eight blend bytes, eight order bytes, then the existing count/256-color palette payload. Header layer capacity is 8; width/height remain 1Ã¢â‚¬â€œ128. CRC and atomic-write workflow unchanged. PDD1Ã¢â‚¬â€œ6 reads preserve their four-slot wire layouts and expand into eight-slot memory. Recovery also uses PDD7; Aseprite import/export supports up to eight flat single-frame RGBA layers. Higher counts reject transactionally. Preserve existing projects and recovery files; old builds cannot read PDD7. User accepted variable sizes locally; this candidate awaits local/hardware tests.

Layer protections (2026-10-14): layer options now have undoable Lock and Alpha Lock toggles, with X to close. Lock blocks pixel writes, erase, deletion and destructive merges; visibility, opacity, rename and reorder remain available. Alpha Lock preserves existing per-pixel alpha, blends brush RGB by brush alpha, skips empty pixels and ignores erase. Merge into alpha-locked layers is refused. PDD8 appends eight validated protection bytes (bits 0/1) after the PDD7 palette payload; PDD1Ã¢â‚¬â€œ7 remain readable. Native recovery preserves both flags. Aseprite editable bit preserves layer lock; alpha lock is native-only and export reports this. Document memory size unchanged. Host protection/UI/persistence checks pass. Eight-layer candidate and protection hardware acceptance remain pending before closing slice 4 and starting brush size/grid.

Brush/grid candidate (2026-10-14): toolbox > Brush/Grid offers independent pencil/eraser sizes 1Ã¢â‚¬â€œ16, square/round tips and pixel-grid toggle. Grid shows at 4x+ only on the editing canvas, never in artwork, exports or sampled colors. Both render paths preserve the grid. Brush footprints clip to canvas bounds and contribute to incremental damage. Even sizes extend one extra pixel left/up. Alpha-locked paint samples the pre-stroke pixel so overlapping stamps do not repeatedly accumulate color within a stroke. Protection applies to all stamps. Tool preferences persist on pen release in tools0.bin/tools1.bin (PDB1, 20 bytes, sequence, two size/tip fields, grid flag, CRC at byte 16; dual-slot readback). Preserve these files during deployment. Host tests cover tips/sizes/edges/undo/preferences and 1455 incremental/full render frames. User accepted protections locally and requested continuing tools; slice 4 hardware acceptance remains pending. Fill is the next tool after brush/grid acceptance.


Guides and brush popup (2026-10-14): user accepted brush drawing on physical 3DS. Top pencil/eraser icon now opens a compact size 1Ã¢â‚¬â€œ16 / square-round popup over the canvas. Tap outside to dismiss without drawing; Tools or B opens the toolbox. Selecting Pencil/Eraser also opens its popup. Toolbox # Guides separates pixel grid, Symmetry X/Y and Tile X/Y. Symmetry reflects actual stamped pixels about canvas center (including even tips). Tile axes both wrap brush footprints and allow drawing on repeated previews; unwrapped input coordinates prevent seam-crossing chords. Preview/grid do not alter exports. Tools slots now write PDB2, retaining PDB1 reads; byte 13 stores four guide bits, existing CRC/dual-slot workflow retained. Preferences are device-local, not project fields. Ordinary drawing retains its incremental render path; guided strokes currently redraw the full canvas, so guided-mode DS performance needs user testing. Core seam/mirror/undo, UI repeated-copy input, repeated-preview oracle, preference reload and prior 1455 incremental-frame checks pass; NDS/3DSX/CIA build and header checks pass. Local and hardware acceptance of this candidate pending.


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

User accepted 0.6.0 locally and explicitly confirmed these belong in the existing Fill tool. Toolbox > Fill now opens one popup: Contiguous Region / Global (Replace Color), Match Active Layer / Visible Image, tolerance 0Ã¢â‚¬â€œ255 with minus/plus and captured slider, Done. Default remains contiguous/exact/active layer. Global changes all matching pixels within the selection and canvas; contiguous traverses four-neighbor connectivity with enabled tile wrapping. Visible mode matches the unchanged composited image but writes only the active layer. Layer lock/visibility/alpha lock, mirrored seed points and one transaction per fill remain. No extra toolbox tool.

Tolerance compares each canonical RGBA channel to the original seed (maximum channel difference, not a drifting neighbor-to-neighbor threshold). Fully transparent RGB is ignored by canonicalizing zero-alpha colors. For visible matching, composite is snapshotted before any fill writes. Each mirrored region traverses independently to avoid tolerance-dependent bridge omissions; repeated writes use pre-fill values so alpha lock does not accumulate. Fixed 34 KiB temporary fill scratch remains. No project format changes.

Tool preferences now write PDB3 in the same 20-byte dual slots; PDB1/PDB2 read compatibility retained. Byte 14 stores global/visible bits; byte 15 tolerance. CRC/readback unchanged; preserve tools0.bin/tools1.bin and do not downgrade. Host checks cover global/disconnected versus contiguous matching, tolerance boundaries, visible boundaries on another layer, selection limits, transparent RGB canonicalization, UI and preference reload; prior shape/selection/history/render checks pass. Native popup inspected. NDS/3DSX/CIA builds pass (CIA version 112). 0.7.0 local/hardware acceptance pending.



## 0.8.0 pixel-perfect pencil (2026-10-15)

User accepted 0.7.0 locally. Pencil popup adds Pixel Perfect, off by default, active only for size-1 pencil strokes. Larger brushes, eraser and shape tools retain their raster behavior. The shared bounded stroke state removes the middle pixel of consecutive orthogonal unit steps making a diagonal corner. It restores the exact pixel values captured immediately before that stamp (including up to four unique reflected positions), preserving pre-existing art and earlier ink at overlaps. Tile coordinates remain unwrapped for path decisions; wrapped/reflected restoration participates in presentation damage. Alpha lock and selection clipping remain. State resets for new strokes, gaps and temporary erase/larger brush paths. No allocations or full redraws are introduced while drawing.

Tool slots now write PDB4, still 20 bytes with CRC/readback and PDB1Ã¢â‚¬â€œ3 compatibility. Byte-14 bit 2 persists pixel-perfect; global/visible fill bits and tolerance remain. Preserve tools0.bin/tools1.bin, no downgrade writes. Host checks cover corner restoration of existing semi-transparent pixels, alpha lock, symmetry/tile damage, undo/redo, persisted toggle and DS/3DS partial/full equivalence. Prior 4079 incremental frames and all other tests pass. NDS/3DSX/CIA builds pass (CIA version 128); native popup inspected. 0.8.0 emulator/hardware acceptance pending.


### App identity assets

The community five-square logo is used for the NDS banner icon, 3DSX SMDH, CIA icon and HOME Menu banner. Sources and generated rasters live in assets/. To regenerate icons run `python scripts/generate-icons.py` (Pillow required). `python scripts/verify-icons.py` verifies packed NDS/3DSX icon pixels after building. The CIA banner generator also uses Pillow. Icon-update candidates are in build/test-0.12.0-icons; native version remains 0.12.0.




