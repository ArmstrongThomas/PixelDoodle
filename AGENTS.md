## Complete local build set (2026-10-21)

The standard packages were rebuilt after the private candidate: root
`PixelDoodle.nds` (SHA-256
`C3F17B34103CEA6AFC9587F2AFF3FC01E8A62D36D2A94752E633B941B3963F34`),
`build/3ds/PixelDoodle.3dsx` (SHA-256
`28A0EE20C5A96A871769FE67953E80D4BF8136E8EAE589EB726AFC77343CA903`),
and `build/3ds/PixelDoodle.cia` (SHA-256
`AEDF62661EAEFD870DCEBEA9B34D71E8191E5D7F949F1329F19AE38ED01AAC54`).
The standard NDS header, ARM bounds and DLDI checks and CIA metadata check
pass. The private NDS and 3DSX hashes below remain current. This makes five
local packages in total; no private CIA was requested or produced.

## DSi upload lockup diagnosis (2026-10-21)

The first private NDS image exported `share.png` and then froze as soon as it
started Wi-Fi. The ROM linked DSWiFi on ARM9 but used BlocksDS's default
Maxmod-only ARM7 core. BlocksDS documents that this pairing hangs when ARM9
initializes Wi-Fi. `Makefile.nds` now selects
`sys/arm7/main_core/arm7_dswifi.elf`.

The share path also moves `Wifi_InitDefault()` out of the touch event and into
the cooperative network state machine. It requests DSi Wi-Fi mode so the
library can load Advanced Setup entries, and the status line exposes Wi-Fi
initialization, association/DHCP, DNS, certificate verification, upload, and
response stages.
Physical DSi 0.13.1 reached `WAITING FOR WI-FI / DHCP` without freezing, then
raised a Data Abort when association completed and DNS setup began. The cause
was `Wifi_GetIPInfo(NULL, NULL, ...)`: the DSWiFi API writes through the gateway
and subnet pointers. `start_dns()` now passes separate writable `struct in_addr`
objects for both.

The first corrected hardware image then reported `SAVED WI-FI CONNECTION
FAILED` with the working network in DSi Advanced Setup slot 4. The share path
was only passing `WIFI_ATTEMPT_DSI_MODE` when `isDSiMode()` was already true.
BlocksDS's DSWiFi examples pass that flag unconditionally so the library can
switch a DSi into TWL Wi-Fi mode and load the additional Advanced Setup APs;
it falls back safely on DS hardware. The share path now does the same.

The corrected private NDS candidate is
`build/private/nds/PixelDoodle-Private.nds` (SHA-256
`245BBB5FB5E0B43DDDF4FBB90F459B60964D544CC689732A234C6875F20895BC`).
The association notice reports the number of saved APs DSWiFi loaded so a
hardware retest can distinguish a missing Advanced Setup entry from an
association failure. Conditional DSi-mode initialization then reached TLS
setup but reported the combined seed/certificate error. TLS setup now reports
`TLS SEED MISSING OR INVALID`, `TLS CERTIFICATE LOAD FAILED`, and
`TLS CONFIGURATION FAILED` separately. NDS header, ARM bounds and DLDI checks
pass. The user provisioned a fresh host-generated seed after confirming the
card had none, then confirmed a successful webhook upload from physical DSi
hardware.

## Private Discord share candidate (2026-10-21)

Standard builds omit the postponed Community menu entry. Private NDS/DSi and
3DSX builds add Menu > Send Sprite, then let the user choose Send Active Layer
or Send Visible Composite. Active-layer PNG export preserves that layer's
opacity and alpha; visible export flattens all visible layers. Host fixtures
and both private builds pass. Artifacts:
`build/private/nds/PixelDoodle-Private.nds` (diagnostic NDS hashes and status
are in the DSi lockup section above) and
`build/private/3ds/PixelDoodle-Private.3dsx` (SHA-256
`7E1D69E3E74BE413D7AC9B606A5454F5478679F386013B72A6139BBB2FB68639`).
NDS header, ARM bounds and DLDI checks pass. Azahar booted an earlier private
3DSX. Both share choices reached the configured Discord webhook and the app
displayed their respective success notices. The first Azahar send reached
Discord but showed an HTTPS chunk error because the reply exceeded a 511-byte
response buffer; the 3DS client now discards the bounded reply body after
validating it. NDS accepts the validated response headers without requiring a
Content-Length on Discord's reply. The final embedded-webhook artifacts were
built and structurally checked; the exact final 3DSX was not reloaded in Azahar
because the user's open emulator session contains unsaved artwork.
melonDS 1.1 was found in `.tool-cache` and booted an earlier private NDS image
in NTR mode. Its menu shows both share choices; this emulator profile reports
unavailable storage, so a send stops at `SAVED SD CARD REQUIRED`. Physical DSi
webhook transfer is accepted after provisioning its private TLS seed. Private
builds embed the user's supplied webhook from ignored root `discord-webhook.txt`.
An optional console text file at `fat:/PixelDoodle/private/discord-webhook.txt`
or `sdmc:/3ds/PixelDoodle/private/discord-webhook.txt` overrides it when present;
an invalid override stops the send. Both final binaries were checked for the
embedded path. Treat the private binaries, text file, and per-card seed as
credentials; never copy them into tracked source or handoff notes. The user
copied and tested the DSi image on physical hardware.

## DSi online candidate and Docker recovery (2026-10-21)

The current PixelDoodle.nds was rebuilt through the pinned BlocksDS v1.24.0
Docker image on 2026-10-21. Host fixtures pass; NDS header, ARM bounds and
DLDI checks pass. SHA-256 is FD14F6B430EAE0F5A3D721D3EF89F04AFCF6F726594ED041D698CEF7E8BB76FA.
Physical DSi pairing and gallery acceptance are now the user's next test; no
device transfer was performed by Codex.

Docker Desktop 4.82.0 was failing at startup because stale Windows AF_UNIX
reparse-point sockets remained in `%LOCALAPPDATA%\Docker\run` and
`%LOCALAPPDATA%\docker-secrets-engine`. Both runtime directories were renamed
to `run.codex-quarantine.20261021` and
`docker-secrets-engine.codex-quarantine.20261021`; contents were preserved.
Docker Desktop was updated in place to 4.96.0, and the Linux engine now reports
29.9.1. No Docker data reset, WSL unregister, or volume/image pruning occurred.

## Community continuation status (2026-10-16, local)

The local implementation adds a schema-5 PHP device API for scoped favorites
and resumable console uploads, with digest/idempotence/owner checks and the
existing moderation ingestion path. Both native clients now have Cached, Recent,
Favorites and My Uploads modes, bounded page parsing and thumbnail caching,
favorite toggles, upload status/review reasons, and confirmed import of the
device-sized PNG derivative. Pair/re-pair explicitly requests gallery,
favorites and submit scopes; existing grants are not upgraded silently.

Native submission metadata/preview and resumable chunk upload UI remain
unimplemented, and live-device acceptance is pending. 3DS online work shares the
owned pairing worker; NDS networking remains cooperative. Host renderer and
HTTPS suites passed, including 131,072 composite comparisons and 16,316 drawing
frames. Dropdown touch capture/scrolling and grant behavior also pass host
fixtures. Invalid/expired/revoked grants now return 401; valid grants without
the requested scope return 403, and both clients preserve the grant for 403
while clearing it for 401. NDS hostname lookup is now bounded, nonblocking DNS
UDP across frame updates. The pinned BlocksDS v1.24.0 build passes on 2026-10-21
after Docker Desktop recovery. Fresh 3DSX/CIA build and CIA metadata checks pass.
PHP passes 36 tests / 318 assertions. Keep existing
account grants and live configuration unchanged. No deployment, public write,
hardware transfer or card write occurred in this increment.
# Pixel Doodle operating guide

## 0.13.1 pairing main-stack crash fix (2026-10-16)

Physical 3DS photo 143059: PC 0x0014FE40 maps to threadFree push {lr},
LR 0x00126FA0 to request_take. SP 0x07FFF258 and write FAR 0x07FFF254
point below main stack mapping. Community expansion had added a 32768-byte
automatic response array in pd_pairing_tick. Moved this to separate static
main-thread storage, wiped after each tick. Worker response remains separate;
worker still owns its larger explicit stack. GCC stack-usage now reports
pairing_tick_impl 1776 bytes and wrapper 8 bytes. Build rejects a tick frame
above 4096 bytes. Version bumped to 0.13.1 to distinguish this candidate.
Physical retest pending. No server changes required.


## 3DS software TLS replacement (2026-10-15)

Supersedes the HTTPc state diagnostics: hardware again returned S10/D8A0A03C.
Reused Collab Doodle tls_stream and bounded https_client, adapting JSON POST
with Content-Length and no redirects. Required chain/hostname verification,
SNI, TLS1.2 AEAD and system RNG remain enabled. SOC initializes lazily (1 MiB).
The build embeds data/cacert.pem and compiles vendored Mbed TLS and C++ sources.
No server deployment, credential reset or NDS changes are required.

Editor host suite and POST framing/fragmentation/chunking/bounds/error tests
pass. scripts/test-https-live.ps1 explicitly tests the production HTTP parser
and crypto configuration with Windows socket/RNG adapters: live begin/pending
poll pass and wrong-hostname verification fails closed. No grant approved or
saved. This does not validate the actual 3DS socket/RNG services.

Azahar automation failed twice with `failed to activate captured window` at
its load dialog. New emulator pairing acceptance and physical pairing remain
pending. Earlier HTTPc Azahar passes do not validate this replacement. User
requested no hardware transmission tonight; none performed. Requests remain
synchronous, so TLS verification can pause the UI; worker migration is follow-up.


## Pairing HTTP-service state diagnostic (2026-10-15)

The user tested the Root YE build on hardware and reported `HTTP WAIT TIMEOUT
S10`. libctru publicly documents only request states 0x5 (in progress) and 0x7
(download ready); S10 is an undocumented state returned successfully by the
HTTP service. The 3DS pairing request now makes one bounded 1.5-second response
status probe after timing out in an unknown state, reporting either the HTTP
status or the low-level Result without credentials. Host tests pass, the 3DSX
build passes, and the exact candidate boots to the app recovery screen in
Azahar. Azahar HTTP/TLS acceptance is not authoritative. The candidate
`build/3ds/PixelDoodle.3dsx` (SHA-256
`3FB7DDD2C9CEE7C926B31068A392AA771AAC127EAF35D913657FF7F4FE01FBE7`) was sent
to the user's ready 3DS at `192.168.1.225:17491` via 3dslink (exit 0, 316444
bytes). Physical diagnostic result is pending; ask for the new message and
continue assuming the user returns to netloader after each error.

## 3DS hardware acceptance and live pairing response fix (2026-10-15)

User tested the 0.13.0 editor on a New 3DS XL: symmetry/tile, selection and
selection tools, brush options, layers, Aseprite and other file flows all pass.
The only reported performance limit is a 512x canvas with both symmetry axes,
both tile axes, a large brush and about six layers; this combination is slow.
Do not infer extra application memory from the console's total physical RAM;
the platform adapter currently selects its expanded profile based on the
application memory region actually granted by the system.

- [x] Symmetry X/Y and Tile X/Y drawing.
- [x] Selection and selection actions/tools.
- [x] Brush size/shape/options.
- [x] Project dimensions, layers and file/Aseprite flows.
- [x] Typical drawing performance on this New 3DS XL.
- [ ] Extreme 512x + both symmetry axes + both tile axes + large brush + six
  layers: works but is noticeably slow; retain as a known resource limit.

The same test found pairing failed on hardware. The build does target the live
`https://pixels.7db.pw` endpoint. A direct begin request to the live PHP API
returned HTTP 200 with the expected URI/expiry/interval fields. The 3DS client
incorrectly required a `scopes` field in the begin response even though the PHP
API returns requested scopes in the post-approval token response. The client
now accepts the begin response and continues to require the exact
`gallery:read` scope before persisting an approved token. Rebuild and retry on
Azahar passed: the exact 3DSX now obtains a code from the live site and polls
successfully to “waiting for website approval.” No grant was approved, so
approval/storage/revocation are still pending on hardware. This exact build had
already been sent to the user's 3DS netloader (3dslink exit 0) before the
requested Azahar-first order was corrected; Azahar has now verified the same
artifact. Physical confirmation is pending. Candidate SHA-256:
`E1D3B6DE6FDCFC71D52A2AB063B2AE2EB49DF580331C5F9F458FD36C6F6AD4BA`.
Online gallery browsing is still not connected: Community > Gallery remains
the offline PNG cache and is the next community client milestone.

The user reported the Azahar-tested candidate still showed the generic pairing
failure on physical 3DS. Investigation found the production certificate chain
terminates through Let's Encrypt Root YE / ISRG Root X2, absent from the listed
3DS trust anchors. The next candidate embeds the official ISRG Root X1 DER
certificate (SHA-256 `96BCEC06264976F37460779ACF28C5A7CFE8A3C0AAE11A8FFCEE05C0BDDF08C6`)
and reports bounded request-stage errors without exposing secrets. Its build
passes in Azahar: pairing obtains a code and polling reaches “waiting for
website approval”; no grant was approved. Exact candidate
`build/3ds/PixelDoodle.3dsx` SHA-256
`CFC2ACE1F331B3C13E285F7EB3100F363B4D7EE9B591969EC60DBCE5D087134D` was then
sent via 3dslink to `192.168.1.225` after Azahar validation. Physical result is
pending; record the displayed stage-specific diagnostic before changing TLS
or request handling again.

Physical test of the embedded-root candidate displayed `HTTP STATUS READ
FAILED` even though `httpcBeginRequest` succeeded. The follow-up now uses
libctru's timeout-aware status read (15 seconds) and shows its numeric Result
as `HTTP STATUS ERR XXXXXXXX` on failure. Host suite and 3DS build pass; the
exact updated candidate was loaded in Azahar and again obtained a code and
reached “WAITING FOR WEBSITE APPROVAL” after its poll. No grant was approved.
Current candidate SHA-256:
`8B4C0590251015BF7BD1A1F3C7F8075FFD4320A95AED89098CA318003A8BDF2B`.

The user returned the physical 3DS to netloader and tested the 8B4C... build;
it displayed `HTTP STATUS ERR D8A0A03C` (HTTP module, invalid-state result).
Azahar reproduced the same issue with the request-state diagnostic showing
transient state `1` immediately after `httpcBeginRequest`. The client had
incorrectly aborted on any state other than libctru's documented in-progress
(`0x5`) and download-ready (`0x7`) values. It now continues polling through
transient states, bounded to 15 seconds. Host suite passes; the rebuilt exact
candidate passed Azahar pairing through “WAITING FOR APPROVAL” without approving
the emulator grant. It was then sent to the waiting physical 3DS via 3dslink
(exit 0, 315888 bytes). Physical confirmation is pending. Current candidate
SHA-256: `6018A8F83CE90BA12F25BD1C724C82FBF216DD373BE943E5B26C5437727EACCD`.

The user tested that candidate and it remained in `HTTP RESPONSE WAIT TIMEOUT`.
The production leaf certificate is issued by Let's Encrypt YE1; its published
default chain adds the new Root YE before ISRG Root X2/X1. The previous client
only trusted X1, so the next candidate adds the official self-signed Root YE
alongside X1, and includes the final numeric HTTP state in timeout diagnostics.
Host suite and build pass; the exact candidate passed Azahar through
“WAITING FOR APPROVAL” (emulator TLS remains non-authoritative). A 3dslink
transfer was attempted after the user's prior netloader-ready instruction but
the console at `192.168.1.225:17491` was not reachable; the sender was stopped
without a successful transfer. Candidate SHA-256:
`902059B02CA046BE36252DD189C60E4D695DB88F15736C47D3663D3763F0B3DF`.
Physical Root YE acceptance is pending.

## 0.13.0 NDS/DSi pairing candidate (2026-10-15)

The NDS/DSi Community Pair screen now uses the verified TLS 1.2/Mbed TLS 3.6.7
transport, pinned Mozilla root bundle and fail-closed host-provisioned seed
rotation pattern from the sibling Doodle-NDS client. Pixel Doodle has its own
seed under the platform's `<app-data>/private` folder; never reuse or copy a
Collab Doodle seed or identity. `scripts/provision-nds-seed.ps1` provisions one
card at a time using Windows CSPRNG. Pairing requests only `gallery:read`, and
network work is advanced only while the Pair page is open. The token is a
revocable bearer grant stored in alternating app-private slots.

The pinned BlocksDS v1.24.0 build now passes. Docker Desktop itself was healthy;
the initial failure was this workspace's named-pipe permissions. With approved
elevated Docker access, `scripts/build.ps1 -Target all` passed the host suite,
rebuilt the 3DSX/CIA and compiled the NDS TLS/pairing client using the already
cached pinned image. NDS header/ARM-bound/DLDI validation passed; CIA title ID
and version validation passed (title 000400000CE47800, version 208). Current
artifacts: `PixelDoodle.nds` SHA-256
`128CF5AB40FE4FA9BA771AF5215A7F63FD881B2AEDCB54FB8EF47DF93D1B8DCA`,
`build/3ds/PixelDoodle.3dsx` SHA-256
`0156A78E3EE2AA7E38E8092DD97BE9EE34CCD3A986463D84E8C491581D3E8397`,
and `build/3ds/PixelDoodle.cia` SHA-256
`D663C012C9EC231EA89FC8BEDA94AFD8B8061FA19BCF793015111B6A4CD99CEF`.
Azahar booted the 3DSX and reached Community/Pair, but the pairing request
failed in emulation; Azahar logs show its `httpcAddDefaultCert` is stubbed, so
TLS acceptance still requires hardware testing. No device grant was issued.
Certificate rejection, HTTP framing, pairing approval/denial/expiry/revocation
and separate DSi/DS Lite acceptance remain pending. Never use plaintext HTTP,
`rand()` as secure entropy, or the Collab Doodle seed. Pairing was enabled by
the user in the live server for testing; return it to the intended setting when
testing is complete. See `docs/GALLERY.md` and the hands-on list.

## Community web 0.1.0 local prototype (2026-10-15)

Native version stays 0.12.0; this increment adds community/ within the same repo. Separate loopback-only Node HTTP process, default 4178, no dependency install, Doodle reuse or production writes. Sites local-only workflow used; no hosted registration or publication. See community/README.md for API and commands. The synthetic in-memory fixture dataset includes explicitly nonpublic records to exercise read boundaries; it is not a durable service or real moderation queue.

Responsive gallery, search/tags, paged results, synthetic artist pages, detail with integer zoom/tile/background controls, and PNG download work locally. Generated 64x64 RGBA derivatives fit the existing handheld cache, but consoles are not connected to this HTTP service. Public metadata/media/counts filter published + public; hidden records return 404. Static routes allowlisted, read methods only, no-store and basic browser headers. No authentication, pairing, persistent storage, upload, favorites, moderation actions or webhook transmission implemented yet.

Five HTTP test groups and JS syntax checks pass. Browser search, tag filter, pagination, artist/detail, 8x/tile, empty/reset flows checked; native narrow and wide responsive layouts inspected. Preview http://127.0.0.1:4178 left running as an owned hidden Node process; its ID/logs are in ignored build/community-preview.*. Reuse/check that process before starting another; do not kill unrelated Node processes. Screenshot build/community-preview.jpg. User testing pending. Prior native NDS/3DSX candidates remain build/test-0.12.0; no rebuild/deploy was needed.

## 0.12.0 offline gallery-cache foundation (2026-10-15)

User deferred testing and asked to continue. Menu now splits its final row into Recovery / Community; all other existing menu coordinates stay unchanged. Community is explicitly Gallery / Local Cache. This is the first handheld browsing stage, not a connected community launch. It reads PNG derivatives placed manually in the app-owned community subfolder, with no network, login, publication or approval claims. Missing storage/cache shows instructions without writing or creating folders.

Six 32x32 RGBA thumbnails (24 KiB additional fixed PdApp storage), paged previous/next, selected 64x64 top preview and explicit Refresh. Reuses existing bounded listing and file-preview scratch; max 128 directory entries before PNG filtering, with truncation indicator. Only PNG names (case-insensitive) qualify; directories and other file formats are excluded. Preview uses NTR profile to cap derivatives at 128 per side on every device. Decode errors produce unreadable tiles and disable Open Copy for that item. Images are decoded sequentially only on page entry/change/selection/refresh; rendering and strokes do not do file I/O. Current PNG parser limits remain.

Open Copy uses the existing replacement confirmation, even for clean art, then imports PNG into an unsaved native project. It does not overwrite the cached PNG. Cancel, page changes and preview failures retain live pixels/history. Cache is manually supplied and is not an approval authority; later server responses must enforce approved-public visibility independently. No credentials, synthetic identities or fake pairing codes added. Native/account formats are unchanged.

Host gallery tests pass for page/thumbnail selection, unreadable/empty states, refresh, cancelled replacement and explicit import on both screen sizes. Full regressions including 16,316 incremental/full drawing comparisons pass. Native gallery, empty and confirmation renders inspected; confirmation remains visible and the gallery has an opaque background. NDS header/DLDI, 3DSX/CIA builds pass (CIA version 192; installation untested). Stable candidates: build/test-0.12.0/PixelDoodle.nds and PixelDoodle.3dsx. No emulator restart or hardware transfer; user will test later. Online service, authenticated pairing, favorites and direct uploads on all devices remain outstanding before the Community milestone is complete.

## 0.11.1 history shortcuts and compatibility guide (2026-10-15)

User will test later and authorized the next step; do not treat 0.10/0.11 as interaction-accepted. Canvas-only Zoom modifier + L/R performs Undo/Redo with the pen fully lifted (not the pen-up transition). Uses the configured semantic Zoom binding. Both physical shoulders retain PD_ERASE and add distinct PD_SHOULDER_L/R flags in both adapters; remapping passes these bits through. One action per shoulder key-down, no autorepeat, no history action with both shoulders held or simultaneous menu/save/tools presses. Menus, confirmation and toolbox block history shortcuts. Successful history clears the session selection immediately. Regular L/R erase remains unchanged. No settings or artwork format change.

Help adds a fourth Keys tab with the actual configured modifier. docs/ASEPRITE-COMPATIBILITY.md documents the current single-frame RGBA subset, per-device limits, preserved properties, native-only alpha lock, rejected metadata and non-preserved frame timing/palette names. Optional DS pressure remains deferred until calibration/hardware testing is available; it is not required to use these controls.

Host shortcut tests pass for both layouts, remapping, repeat/both-shoulder suppression, menus/toolbox/confirmation and pen-transition isolation. Full regressions including 16,316 incremental/full drawing comparisons pass. Keys help inspected at native DS/3DS resolutions. NDS header/DLDI, 3DSX and CIA builds pass (CIA version 177; installation untested). Stable artifacts: build/test-0.11.1/PixelDoodle.nds and PixelDoodle.3dsx. No emulator restart or hardware deployment; user will test later.

## 0.11.0 pencil dither and presets (2026-10-15)

User deferred further interaction/hardware testing; 0.10.0 remains pending. Pencil > Dither / Presets adds Solid and 25/50/75% ordered dither plus four reusable pencil slots. Load applies a slot; Store replaces the selected slot with current size, square/round tip, pixel-perfect preference and density. Color is deliberately not part of a preset. Defaults: 1px square pixel-perfect, 4px round solid, 8px square 50%, 8px round 25%. Stored preferences are device-local, not project data. Eraser, fill and shape tools remain solid. Pixel-perfect is suspended for dither and resumes on Solid with its previous preference retained.

Dither uses a 2x2 threshold pattern anchored to final original-canvas coordinates after wrapping/reflection. Empty pattern positions preserve the destination, including its alpha; no background color is stamped. Overlapping/repeated stamps keep the same gaps. Mirrored brush footprints sample the same canvas pattern (the pattern itself is not reflected). Odd-sized tiled canvases repeat their original pattern at the seam. Selection, visibility, locks, alpha lock, actual-pixel damage and one-stroke undo remain. No new stroke allocation or full-canvas redraw; bit operations determine coverage, no division for the dither test.

Existing PDB4 tool settings remain unchanged. New app-owned brush-presets0.bin / brush-presets1.bin use PBP1, 32 bytes: magic at 0, sequence at 4, active density at 8, zero reserved 9–11, four four-byte records (size, round, pixel-perfect, density) at 12–27, CRC32 at 28 over the first 28 bytes. Validate bounds, exact length, CRC and readback; newest valid slot wins, corrupt slot falls back. Writes occur on pen release. Missing storage allows session use with an explicit notice. Preserve both files across deployment. No artwork or recovery format change.

Host tests pass: independent coverage/gaps/repeated overlap, selection, all guide combinations, alpha lock, undo at 16/512 square; presets UI on both native sizes/handedness, persistence and corrupt-slot fallback. 16,316 incremental/full drawing comparisons and prior regression checks pass. Native popup renders inspected. NDS/3DSX/CIA builds pass (CIA version 176); installed CIA untested. Stable candidates: build/test-0.11.0/PixelDoodle.nds and PixelDoodle.3dsx. User interaction, performance and hardware acceptance remain pending. A delayed user reply authorized replacing both saved emulator sessions: launched the exact 0.11.0 candidates in Azahar and melonDS and verified both processes/windows remained open. No hardware transfer; interaction feedback is pending.

## 0.10.0 selection duplication/scaling and PNG artwork import (2026-10-15)

User accepted 0.9.1 on New 3DS XL: panning/drawing works well; extreme 512-square layers/guides still lag but are manageable. This does not establish identical performance on other hardware profiles.

Select > More is a compact two-column popup: Flip H/V, Rotate 90, Dup to Layer, Scale 2x/3x/4x, Half Size, Back. Duplicate copies selected pixels into a free layer directly above the source, preserves source opacity/blend, makes the copy visible/unlocked and leaves the original unchanged. One undo entry includes the new layer. This read-only source operation is allowed on locked source layers, like Copy, and supports large masks. Scaling accepts a selected bounding box up to 128 square (the existing bounded temporary clipboard), allows output through device canvas size, and rejects an output that cannot fit without changing art. Origin clamps inward to retain the whole output. Integer enlargement repeats pixels; half-size uses top-left nearest samples with ceil(width/2), ceil(height/2), preserving the odd final edge. Clears the selected source, source-over composites sampled nonzero-alpha pixels, and selects only placed nontransparent samples. No interpolation/antialiasing; selection holes/alpha remain. Requires a visible unlocked/non-alpha-locked target. One undo transaction; session clipboard is unchanged. No settings/project format changes.

Menu > Open includes PNG in the default project filter and previews it as artwork. Confirmed import creates one RGBA layer with actual dimensions and marks it imported/dirty for native Save As. Indexed PNG imports also retain the source palette. Import Palette and the Palettes filter retain strip preview behavior. Decoder supports non-interlaced 8-bit RGB/RGBA/gray/gray-alpha and 1/2/4/8-bit indexed/gray, all five filters, contiguous multi-IDAT and tRNS keys/palette alpha. Dimensions follow the device profile through 512. CRC, chunk order/bounds, decompression length, palette indices and unsupported critical chunks are checked before replacing current art. Rejects APNG, 16-bit channels, interlace and embedded ICC profiles explicitly; ancillary gamma/color metadata does not remap pixel sample values. File/raw/decompression buffers remain bounded temporary allocations, not a streaming decoder. Reference: https://www.w3.org/TR/png-3/ (PNG specification, read 2026-10-15).

Host tests pass for duplication/scale/undo/locks/alpha/holes/odd reduction/large output and native-size UI, plus PNG filtering/preview/confirmed-open/dirty state and existing 512 pan and 4079 stroke frame comparisons. scripts/test-png-import.py independently checks generated PNG samples, packed palettes/transparency, all filters, large sizes and malformed-input preservation. Desktop ASE/PNG interoperability suite passes. NDS/3DSX/CIA builds pass (CIA version 160); installed CIA untested. Transform popup inspected at both native resolutions. Stable build/test-0.10.0 artifacts launched in Azahar and melonDS after checking no prior sessions remained open. User interaction/hardware acceptance pending; no 0.10.0 hardware transfer yet.

## 0.9.1 panning responsiveness (2026-10-15)

User reported chunky/laggy panning on all three devices during 0.9.0 testing. Clean viewport-only changes now reuse the existing composite rather than recompositing every layer on each drag frame. Pending redraws, stroke commits/cancels, layer changes and other edits retain full refresh. Stationary/clamped touch pans no longer request repeated redraws. Top-screen overview still updates normally.

Touch panning now tracks subpixel camera remainders in screen pixels, removing whole-artwork-pixel snapping at high zoom. Shared world mapping, normal/tiled render paths, partial stroke damage, selection/shape overlays and tile border incorporate these remainders. Opening/recovery/new projects and zoom reset them; viewport clamps clear residual motion at limits. Circle Pad uses the screen-pixel panning entry point at two pixels per input step, independent of zoom. Existing app_pan API retains its world-unit contract; app_pan_pixels is the adapter API for smooth motion. No file/settings changes; all 0.9.0 capacity limits remain.

Host checks pass, including 512 cached/full pan comparisons at DS/3DS screen sizes, 128/512 canvases, 4–32x zoom and tile modes; test instrumentation confirms no pd_composite calls on clean pan frames. One-screen-pixel movement, stationary gestures and incremental drawing after fractional pans are checked. Existing 4079 drawing comparisons and large-canvas tests pass. All builds pass (CIA version 145). Stable build/test-0.9.1 NDS and 3DSX launched in the user-authorized emulator restarts; exact 3DSX sent to ready 192.168.1.225 netloader, exit 0. Physical improvement remains awaiting user feedback. Preserve test-0.9.0 as the prior candidate.

## 0.9.0 large canvases and device profiles (2026-10-15)

User accepted 0.8.0 pixel-perfect pencil on physical 3DS. 0.9.0 enables independent 1–512 dimensions on actual DSi TWL and 3DS; DS/NTR stays 1–128. Platform adapters set profile 1=NTR, 2=TWL, 3=3DS, 4=New 3DS with at least 96 MiB application-region memory. APT_CheckNew3DS plus osGetMemRegionSize gates profile 4; model name/physical total RAM alone does not enable it. New 3DS in a smaller application region uses ordinary 3DS budgets and is labeled STD RAM. Keep the 512 dimension cap on all 3DS models; greater layer/frame capacity is a later milestone.

Small documents retain the existing embedded 128-stride backing, avoiding a DS Lite allocator/stride migration. Large documents reserve an owned block with 8-pixel-aligned stride/rows, all allowed layer planes, before/composite planes and undo. DSi layer-plane budget is 2 MiB: 8 layers at 256 square, 2 at 512 square; 3DS remains 8 slots. These are conservative configured budgets, NOT measured free RAM or dynamically promised extra layers. Scratch/load allocations can still fail; failures leave the current artwork intact. Resident unused small backing remains about 1.13 MiB, so include it when measuring peak RAM. Large history is max(512 KiB, 2 planes); profile 4 requests 8 planes (8 MiB at 512 square), with fallback to the ordinary budget if allocation fails. Profile 4 new small projects request 2 MiB history. The metadata ceiling remains 32 transactions. No edit is applied without a pre-reserved full-plane undo capacity. History uses generalized tile addressing and a 512-byte tile bitmap; no stroke-time allocation. Large fill uses 32-bit indices and bounded temporary heap. Ellipse arithmetic uses 64-bit products.

New Project adds 256/512 presets where eligible, custom dimensions and a large-project warning before creation. Open/restore confirmation also explains large-project limits on eligible devices. Layer UI reports the actual slot cap and allocated history capacity. Copy/cut and flips/rotation retain 128x128 selection bounds in this first candidate, with explicit notices. Larger selection masks, clipped drawing/fill, move/delete, pixel-perfect, symmetry/wrapping and shape previews work with the new stride. Clipboard remains session-local 128-stride and crosses project-size changes. Mask storage is sized for the maximum, but per-stroke damage clearing/scanning uses the current plane size.

Small named/recovery saves still write PDD8 unchanged. Large saves use PDD9: same 32-byte header convention (version 9, width, height, 8 slots, enabled bits, payload bytes, CRC); 1580-byte metadata consists of 8 records of visibility/opacity/blend/flags/64-byte name, 8 order bytes, palette count and 256 RGBA words. Enabled layers alone follow as tightly packed width*height RGBA words in stable-slot order. PDD1–8 reads remain supported. ASE single-frame RGBA and PNG export support the new sizes. Readers reject device-incompatible sizes/layer slots before replacement; no silent flattening. File cap is 9 MiB. Encoding and import still buffer a bounded complete file and stage layer data; they are not streaming and may refuse under memory pressure. Temp verification/backups and recovery saved-revision semantics remain unchanged.

Host regression suite passes, including 4079 small incremental/full frames and new profile/bounds/high-index fill, dense history, large selections/move/clipboard, ellipse, pixel-perfect, allocation-failure preservation, native/ASE/PNG/recovery and DS/3DS UI/render tests. Desktop Aseprite round trips and independently decoded PNG dimensions/pixels pass for 129, 256, 512 and rectangular extremes. Native size/warning layouts inspected. NDS header and 3DSX/CIA builds pass (CIA version 144); no installed-CIA claim. Stable build/test-0.9.0 files launched in Azahar and melonDS and exact 3DSX sent to 192.168.1.225 via 3dslink, exit 0. User emulator and hardware acceptance of 0.9.0 is pending.

## Larger-canvas request and 0.8.0 hardware acceptance (2026-10-15)

User accepted pixel-perfect drawing on physical 3DS after successful 3dslink transfer of build/test-0.8.0/PixelDoodle.3dsx to 192.168.1.225 (exit 0). This establishes stylus drawing acceptance, not CIA installation or DSi large-canvas performance.

New requirement supersedes the earlier user cap of 128: support independent dimensions up to 512 on 3DS and actual DSi TWL mode. DS/NTR remains capped at 128, including a DSi running in NTR mode. Show an explicit large-canvas warning about slower operations and memory-dependent feature availability before creating/opening large artwork. This is a requested next milestone, NOT implemented in 0.8.0. See the capacity implementation plan near the top of docs/ROADMAP.md. Do not merely enlarge PD_SIDE: fixed embedded planes, history tile addressing, 16-bit fill indices, clipboard/masks, file size caps and legacy serialization must be migrated together. Preserve existing saves and the DS Lite responsiveness baseline.

## 0.8.0 pixel-perfect pencil (2026-10-15)

User accepted 0.7.0 locally. Pencil popup adds Pixel Perfect, off by default, active only for size-1 pencil strokes. Larger brushes, eraser and shape tools retain their raster behavior. The shared bounded stroke state removes the middle pixel of consecutive orthogonal unit steps making a diagonal corner. It restores the exact pixel values captured immediately before that stamp (including up to four unique reflected positions), preserving pre-existing art and earlier ink at overlaps. Tile coordinates remain unwrapped for path decisions; wrapped/reflected restoration participates in presentation damage. Alpha lock and selection clipping remain. State resets for new strokes, gaps and temporary erase/larger brush paths. No allocations or full redraws are introduced while drawing.

Tool slots now write PDB4, still 20 bytes with CRC/readback and PDB1–3 compatibility. Byte-14 bit 2 persists pixel-perfect; global/visible fill bits and tolerance remain. Preserve tools0.bin/tools1.bin, no downgrade writes. Host checks cover corner restoration of existing semi-transparent pixels, alpha lock, symmetry/tile damage, undo/redo, persisted toggle and DS/3DS partial/full equivalence. Prior 4079 incremental frames and all other tests pass. NDS/3DSX/CIA builds pass (CIA version 128); native popup inspected. 0.8.0 emulator/hardware acceptance pending.


## 0.7.0 existing Fill options (2026-10-15)

User accepted 0.6.0 locally and explicitly confirmed these belong in the existing Fill tool. Toolbox > Fill now opens one popup: Contiguous Region / Global (Replace Color), Match Active Layer / Visible Image, tolerance 0–255 with minus/plus and captured slider, Done. Default remains contiguous/exact/active layer. Global changes all matching pixels within the selection and canvas; contiguous traverses four-neighbor connectivity with enabled tile wrapping. Visible mode matches the unchanged composited image but writes only the active layer. Layer lock/visibility/alpha lock, mirrored seed points and one transaction per fill remain. No extra toolbox tool.

Tolerance compares each canonical RGBA channel to the original seed (maximum channel difference, not a drifting neighbor-to-neighbor threshold). Fully transparent RGB is ignored by canonicalizing zero-alpha colors. For visible matching, composite is snapshotted before any fill writes. Each mirrored region traverses independently to avoid tolerance-dependent bridge omissions; repeated writes use pre-fill values so alpha lock does not accumulate. Fixed 34 KiB temporary fill scratch remains. No project format changes.

Tool preferences now write PDB3 in the same 20-byte dual slots; PDB1/PDB2 read compatibility retained. Byte 14 stores global/visible bits; byte 15 tolerance. CRC/readback unchanged; preserve tools0.bin/tools1.bin and do not downgrade. Host checks cover global/disconnected versus contiguous matching, tolerance boundaries, visible boundaries on another layer, selection limits, transparent RGB canonicalization, UI and preference reload; prior shape/selection/history/render checks pass. Native popup inspected. NDS/3DSX/CIA builds pass (CIA version 112). 0.7.0 local/hardware acceptance pending.


## 0.6.0 shape tools and selection transforms (2026-10-15)

User accepted 0.5.1 locally. Toolbox now has a second column for Line, Rectangle and Ellipse. Shape options share pencil size/tip/color; rectangle and ellipse support outline or filled interiors (filled interiors ignore outline size). Outline width uses the pencil footprint. Integer ellipse rasterization supports odd/even bounds and degenerate single-pixel rows/columns. Drag previews replace the prior preview from the transaction snapshot; lift commits one undo entry. Pan, zoom, sample, temporary erase, toolbar or other button actions cancel pending shapes. Shape strokes honor selection masks, layer protection, symmetry and tiling. Extents are bounded to 128 pixels per axis; normal canvas endpoints clamp to dimensions, tiled coordinates remain unwrapped for seam crossings.

Preview rendering retains only old/new changed-pixel masks, updates their visible copies and defers top-screen work. No allocation during shape previews. Select > More adds Flip Horizontal, Flip Vertical and Rotate 90 CW for the masked active-layer content. Transforms retain alpha/source-over behavior and selection holes; one undo entry. They require visible layers without Lock/Alpha Lock. Rotation that cannot fit rejects; otherwise its origin clamps so no selected pixels are clipped. Transform scratch is one temporary PdClipboard (about 66 KiB), freed immediately; allocation failure leaves art unchanged. Ordinary clipboard contents survive transforms. Selection geometry itself remains session state, not pixel undo.

Host checks cover line/rectangle/ellipse rasterization, ellipse symmetry, fill/outline, mask/guide clipping, old-preview restoration and cancellation on both screen sizes, plus flips/rotation/alpha/fit refusal/undo. Prior 4079 ordinary incremental-frame comparisons pass. Native-size toolbox, shape popup and transform popup inspected. NDS/3DSX/CIA builds pass (CIA version 96). 0.6.0 emulator/hardware acceptance pending.


## 0.5.1 selection quick actions and alpha (2026-10-15)

Select mode now reserves a 26-pixel quick strip below the main toolbar: Rectangle/Lasso toggle, Replace, Add, Remove, Copy, Cut, Paste and Deselect. Modes highlight and show explanatory status text. Canvas centering/pan/zoom use the reduced viewport so the strip never hides its top edge. The full popup remains available for select-all/delete and labeled commands.

Supersedes 0.5.0 clipboard replacement semantics: move and paste use straight-alpha source-over. Fully transparent selected source pixels do not erase destination art; partial alpha blends against destination alpha/RGB. Move clears the original selected region, then composites from the unchanged transaction snapshot, so overlapping previews never accumulate blending. Paste honors alpha lock, preserving destination alpha. After paste the active mask includes only nontransparent clipboard pixels, avoiding subsequent movement of background beneath transparent clipboard areas. These are committed raster edits, not a floating paste layer. Copy/cut preserve original RGBA. Undo restores the exact prior pixels. Tests cover transparent and semitransparent move/paste over opaque and translucent destinations, alpha lock, quick-strip actions, and existing 4079 incremental frame comparisons. NDS/3DSX/CIA builds pass; native-size quick-strip layout inspected. 0.5.1 emulator/hardware acceptance pending.


## 0.5.0 selections and clipboard (2026-10-15)

User accepted 0.4.0 in melonDS/Azahar and requested lasso plus add/remove selection in this slice. Toolbox > Select opens Rectangle/Lasso and Replace/Add/Remove, Copy/Cut/Paste, Select All/Deselect, Delete and Done. Replace-mode drag inside an existing selection moves active-layer pixels with live preview and one undo transaction on lift. Pan, a tool shortcut, toolbar contact or temporary erase/sample cancels an in-flight move. Drawing tools and fill respect the mask. Selection geometry itself is session UI state (not in pixel undo); undo/layer changes/project replacement clear stale masks. Clipboard survives project/layer changes during the session, not application restart.

Shared fixed 128-stride masks preserve irregular shapes and holes. Lasso is a bounded 512-point even-odd polygon at pixel centers; an overflowing gesture preserves the old selection with a notice. Rectangle and lasso combine through the same replace/union/subtract operations. Move stays entirely within canvas bounds, never silently clips selected pixels; selection gestures on repeated tiles map to the original canvas and clamp at its edges. Symmetry does not duplicate selection geometry/moves. Paste uses selected RGBA replacement, including transparent selected pixels, with alpha-lock semantics; unselected holes remain untouched. Copy reads the active layer, not merged visible layers. Clipboard too large for the destination rejects unchanged. Move/cut/delete require a visible layer without Lock or Alpha Lock. Copy remains allowed on protected layers. Paste respects Lock/visibility/Alpha Lock.

App-owned clipboard/mask/lasso state adds approximately 75 KiB fixed RAM; document/history/file formats are unchanged. Selection drags defer top-screen work. Move preview recomposites the union of the old/new/source bounds; ordinary painting retains incremental damage. Host checks cover concave lasso against an independent point-in-polygon reference, overlap moves/cancel/undo, clipboard/protection, add/remove UI and clipping paint/fill. 4079 incremental/full render comparisons include selections and tiled drawing. NDS/3DSX/CIA builds pass. Native-size popup/canvas fixtures inspected. User-visible/hardware acceptance of 0.5.0 pending.


## 0.4.0 fill and tiled panning (2026-10-14)

User accepted the 0.3.0 responsiveness improvement locally. Enabled tile axes now pan from one viewport before the canvas through one viewport beyond it; non-tiled axes keep their original limits. Disabling an axis clamps its viewport immediately. The top overview viewport marker is clipped to the artwork so off-canvas panning cannot draw over the header/status.

Toolbox adds Fill: exact RGBA matching, four-connected, active layer only, one tap/one undo transaction. Uses a bounded iterative queue and visited bitmap (34 KiB temporary heap, freed after fill), no recursion. Allocation failure leaves artwork untouched. Lock/hidden layers refuse; Alpha Lock retains alpha and blends RGB using foreground alpha, skipping empty pixels/transparent erasure. Enabled tile axes connect opposing region edges. Symmetry fills independent regions from mirrored seed points, each matched against the pre-fill layer. No tolerance or all-layer sampling yet. L/R still temporarily erases. No project format change. Host tests cover region barriers, diagonals, mirrored seeds, tiled connectivity, 1x128 and full 128x128 canvases, alpha/protection, undo/redo/no-op, one-fill-per-gesture and pan limits. 3494 incremental/full render comparisons cover off-canvas panning. NDS/3DSX/CIA build checks pass; 0.4.0 local/hardware acceptance pending. Next tool milestone is rectangle selection/move/copy/cut/paste.


## 0.3.0 guided drawing follow-up (2026-10-14)

The header and package descriptions now share core/version.h (0.3.0); CIA metadata packs major/minor/patch into the platform version field (48 for 0.3.0). Toolbox toolbar button again opens all tools. Pencil/Eraser rows open their compact options popup; keep this tool/options separation for future selection variants. Guides uses a drawn lattice glyph, not a text #. Tiled canvas has a one-screen-pixel gold border around the original canvas, clipped below the toolbar; exports stay unchanged.

User reported severe DS/DSi guided drawing latency in the preceding candidate. Guided strokes now record actual changed pixels in a fixed 2 KiB app-owned bitset, composite only those pixels, repaint their visible wrapped copies, and defer top-screen work until pen lift. Ordinary drawing keeps its existing damage path. Host incremental/full equivalence covers 2618 stroke frames including symmetry and tiling at both screen sizes/all zooms, alpha/erase/undo. Native performance acceptance remains pending; do not claim measured hardware speed from host tests. No document or artwork format change. Preserve PDB2 tool slots.


Slice 3 recent-project records: preserve app-owned recent0.bin/recent1.bin (PRJ1, 4112 bytes, sequence/count/eight relative paths/CRC). Never store absolute or traversing paths. Previews must use scratch data, never the live document. Preview cache is 16 KiB; decode only after a file selection, never during drawing.

Slice 2 candidate writes PDD5 with project palettes; PDD1-PDD4 remain readable. Read docs/COLORS-SLICE2.md. PPL1 slots are now explicit defaults, not automatically overwritten by project palette edits. User explicitly rejects native keyboards: use the shared custom keyboard on DS and 3DS. Slice 2 was accepted locally and its hardware check explicitly waived for continuing slice 3. Recent-color selection must never reorder existing entries. Preserve these semantics; palette edits remain outside drawing undo.

Settings writes now use PDS2 (16 bytes, packed paired-control permutation at byte 10); PDS1 remains readable. Shared app_bindings translates the four original action groups for both platform adapters. L/R, START and SELECT remain fixed. Remapping drafts only apply after Settings Save. Preserve settings slots; no downgrade writes.

Editable global palettes now use app-owned palette0.bin/palette1.bin (PPL1, dual-slot CRC/readback). Read docs/PALETTE-SLICE1.md. Preserve these across deployment; they are distinct from settings and artwork. Missing storage permits session editing with a failed-save notice.

Latest layer follow-up uses PDD4 (blend modes/order extension), per-row controls and timed reorder. See the opening section of docs/LAYERS-SLICE1.md. Native PDD1–PDD3 reads remain supported. Layer memory indicators describe allocated document storage/slot capacity, not free heap. Merge Down is restricted to visible Normal pairs and explains refusal; do not silently flatten backdrop-dependent blends.

Layer candidate: new documents have one layer, capacity four; existing PDD1/PDD2 retain four. Read docs/LAYERS-SLICE1.md for PDD3 existence mask, undoable layer operations, compact Aseprite export and solo preview. This supersedes fixed-four-layer UI statements below. Do not downgrade named saves to PDD2.

Current priorities: docs/ROADMAP.md supersedes older planning defaults. User accepted 0.2.0 file/Aseprite flows on physical 3DS. Use Azahar and melonDS locally with user-operated interaction, then hardware before wrapping each slice. Slice 1 was accepted on physical 3DS after the remapping test. The final export-name prefill follow-up is host/build checked; slice 2 color/palette work is next.

This independent repository was renamed from PixelStudio at the user's request. Current scope is the first offline prototype. Read README.md and docs/PROTOTYPE-0.1.0.md before extending it. Update these documents when decisions change.

- Maintain ONE shared core and UI for NDS, 3DSX and CIA; platform folders are adapters, not feature forks. scripts/build.ps1 is the combined build entry point.
- User prioritizes physical 3DS testing through 3dslink. Supplied address: 192.168.1.225, default port 17491. Sending the tested prototype in this session is authorized; CIA installation is not. User reported the 3DS prototype works and authorized NDS deployment to H: (DSi LL / DSPICO) and I: (DS Lite / SPAREPICO) on 2026-10-14. Copy only the new app to Homebrew/Dev/PixelDoodle.nds and verify readback.
- All prototype targets use 128x128/four fixed layers/eight history transactions. Detect NTR/TWL but do not silently enable larger unmeasured profiles.

- Preserve all sibling repositories and existing changes. Do not transplant whole Doodle repositories or run their deployment/build-and-publish helpers.
- No GitHub creation, push, tag, PR, publishing, production writes, DNS changes or hardware deployment without an explicit user request for that action.
- Never import secrets, identities, seed files, recovery codes, user art, runtime databases, logs or private Doodle server source. Use synthetic fixtures.
- Offline drawing must not depend on an account, website, network availability or minimum-version gate.
- Detect NTR/TWL runtime; never infer DSi resources from model name. Keep per-platform limits and actual measurement records.
- Use the existing pinned BlocksDS toolchain as the NDS starting point and devkitPro/libctru for 3DS. Preserve cooperative scheduling if networking is added. Azahar is the default 3DS software test target; hardware transmission needs a request.
- New app paths, title IDs, credentials and service resources must be separate from Collab Doodle. Existing DSpico write authorization does not authorize new app deployment.
- Check provenance and file-specific licenses before copying any source/assets. Aseprite's overall EULA and the MIT-licensed document module are different scopes. No blanket license assertion for Doodle source.
- No performance, power-loss durability or hardware acceptance claims from build success alone.
- Future implementation should use a portable deterministic core, bounded allocation and parsing, transactional edits, and versioned replay semantics. Keep native/web pixel output identical before hardware display quantization.


- R2 drawing responsiveness accepted on physical DS Lite on 2026-10-14 (photo: input/render/copy maxima 3450/106/11 us; sample gap 16716 us, N=37). Preserve this baseline. R2 DSi/3DS hardware retest remains pending.
- User prefers to handle emulator interaction; launch melonDS for them. Always link the built PixelDoodle.nds in build handoffs so they can copy manually.

- 0.2.0 adds shared named PDD2 projects, app-folder browser, Aseprite single-frame RGBA import/export and PNG export. Read docs/FILES-0.2.0.md. Preserve PDD1 compatibility. Animation/timeline is explicitly requested but deferred beyond this file milestone; reject multi-frame imports until implemented.
- 0.2.0 Save As/Open/Export accepted by user in Azahar; exact stable b2b6fccb 3DSX sent via 3dslink successfully. Hardware 0.2.0 acceptance pending. CIA version 32, same experimental title ID.

Slice 3 follow-up: drag-scrolling filtered browser and GPL/HEX/PNG-strip previews; new 128x128 setup with background/layers/palette. Preserve hidden app-owned `.recovery/session.pdd`, `.bak`, `.tmp`. Recovery snapshots use pd_write_recovery and MUST NOT mark the named document saved or alter its path/history. Only after 120 seconds and 2 seconds idle with pen up, no editing/menu. Existing startup recovery blocks automatic overwrites until explicit restore/discard. Restore validates before replacement and makes untitled dirty art; fallback primary/temp/backup. Autosave toggle is session-local. User accepted prior previews/recents locally; this follow-up awaits emulator/hardware acceptance.

Slice 4 history foundation (2026-10-14): browser/palette previews/project setup/recovery accepted locally by user; slice 3 hardware acceptance remains pending. Shared undo now stores changed 8x8 XOR tiles in a fixed 512 KiB arena, up to 32 transactions. Dense edits fall back to 64 KiB raw XOR, preserving eight full-layer transactions. Property-only edits consume metadata only. Oldest transactions are evicted by byte/entry limits; branching discards redo. No allocation during drawing or commit. Layers preview reports actual undo cursor/count and used pixel-history KiB. PdDocument is 923288 bytes on the host (2560 bytes above prior metadata size); arena remains fully reserved. Canvas remains 128x128, four layers. Variable dimensions/more layers and measured hardware performance remain outstanding. Host sparse/dense eviction/branch/cancel/metadata and drawing-oracle checks pass; emulator acceptance pending.

Variable dimensions (2026-10-14): user caps canvases at 128 per side; larger profiles are no longer planned. New Project > Size offers 16/32/64/128 square presets and custom independent width/height 1–128. Tap W/H, type digits, tap Set, then Done. New small projects fit at an integer zoom; drawing, panning and overview respect dimensions. PDD6 adds validated dimensions in the existing width/height header fields while retaining the PDD5 fixed-stride payload and CRC; legacy PDD1–5 load as 128x128. Aseprite and PNG serialize only actual dimensions; recovery uses PDD6 too. Smaller canvases currently retain the same preallocated RAM capacity. Native/ASE roundtrip, bounds, rectangular drawing and desktop PNG dimensions/pixels are checked. Local/hardware acceptance pending.

Eight-layer candidate (2026-10-14): shared capacity is eight layers on NDS, DSi and 3DS, still defaulting to one. New Project supports one through eight starting layers. Quick vertical drag scrolls the layer list; hold 150 ms then drag reorders, with edge scrolling. Visibility/options remain per-row. Core uses eight stable slots and an eight-bit enabled mask; undo pixel budget stays 512 KiB. PdDocument is 1185976 host bytes; extra slots add about 256 KiB. Calculated NDS app/document + two presentation buffers + native import staging/file buffer is about 2.6 MiB before code/SDK/stack overhead; this is not measured free RAM. Hardware full-document stress/performance acceptance remains pending.

PDD7 writes eight property pairs, eight 64-byte names, eight fixed 128-stride pixel planes, eight blend bytes, eight order bytes, then the existing count/256-color palette payload. Header layer capacity is 8; width/height remain 1–128. CRC and atomic-write workflow unchanged. PDD1–6 reads preserve their four-slot wire layouts and expand into eight-slot memory. Recovery also uses PDD7; Aseprite import/export supports up to eight flat single-frame RGBA layers. Higher counts reject transactionally. Preserve existing projects and recovery files; old builds cannot read PDD7. User accepted variable sizes locally; this candidate awaits local/hardware tests.

Layer protections (2026-10-14): layer options now have undoable Lock and Alpha Lock toggles, with X to close. Lock blocks pixel writes, erase, deletion and destructive merges; visibility, opacity, rename and reorder remain available. Alpha Lock preserves existing per-pixel alpha, blends brush RGB by brush alpha, skips empty pixels and ignores erase. Merge into alpha-locked layers is refused. PDD8 appends eight validated protection bytes (bits 0/1) after the PDD7 palette payload; PDD1–7 remain readable. Native recovery preserves both flags. Aseprite editable bit preserves layer lock; alpha lock is native-only and export reports this. Document memory size unchanged. Host protection/UI/persistence checks pass. Eight-layer candidate and protection hardware acceptance remain pending before closing slice 4 and starting brush size/grid.

Brush/grid candidate (2026-10-14): toolbox > Brush/Grid offers independent pencil/eraser sizes 1–16, square/round tips and pixel-grid toggle. Grid shows at 8x+ only on the editing canvas, never in artwork, exports or sampled colors. Both render paths preserve the grid. Brush footprints clip to canvas bounds and contribute to incremental damage. Even sizes extend one extra pixel left/up. Alpha-locked paint samples the pre-stroke pixel so overlapping stamps do not repeatedly accumulate color within a stroke. Protection applies to all stamps. Tool preferences persist on pen release in tools0.bin/tools1.bin (PDB1, 20 bytes, sequence, two size/tip fields, grid flag, CRC at byte 16; dual-slot readback). Preserve these files during deployment. Host tests cover tips/sizes/edges/undo/preferences and 1455 incremental/full render frames. User accepted protections locally and requested continuing tools; slice 4 hardware acceptance remains pending. Fill is the next tool after brush/grid acceptance.


Guides and brush popup (2026-10-14): user accepted brush drawing on physical 3DS. Top pencil/eraser icon now opens a compact size 1–16 / square-round popup over the canvas. Tap outside to dismiss without drawing; Tools or B opens the toolbox. Selecting Pencil/Eraser also opens its popup. Toolbox # Guides separates pixel grid, Symmetry X/Y and Tile X/Y. Symmetry reflects actual stamped pixels about canvas center (including even tips). Tile axes both wrap brush footprints and allow drawing on repeated previews; unwrapped input coordinates prevent seam-crossing chords. Preview/grid do not alter exports. Tools slots now write PDB2, retaining PDB1 reads; byte 13 stores four guide bits, existing CRC/dual-slot workflow retained. Preferences are device-local, not project fields. Ordinary drawing retains its incremental render path; guided strokes currently redraw the full canvas, so guided-mode DS performance needs user testing. Core seam/mirror/undo, UI repeated-copy input, repeated-preview oracle, preference reload and prior 1455 incremental-frame checks pass; NDS/3DSX/CIA build and header checks pass. Local and hardware acceptance of this candidate pending.

## Community design 0.2.0 (2026-10-15)

Art-first dark/light gallery with persistent local theme preference, pixel-cut subject banners, gentle hover motion (reduced-motion supported), and staggered columns preserving actual canvas proportions. Fixtures now include portrait and landscape PNGs through 96px, with metadata matching encoded dimensions; native remains 0.12.0. Artist portfolio headers and direct artwork pages retain search/tags, pagination, zoom/tile/background and downloads. Local synthetic prototype only. Six HTTP groups pass including rectangular PNG validation; browser checks cover both themes and theme reload persistence.

### Community playground visual pass (2026-10-15)

User requested a much less restrained design. Added saturated purple/pink and warm light themes, oversized offset typography, pixel-cut tilted subject panels, sticker-style controls, colored artwork frames and gentle star motion. Existing aspect-ratio gallery, routes and theme persistence remain. Reduced-motion disables animation/transitions. Browser inspected dark/light and narrow layouts; JS syntax passes. Frontend-only, no service/native changes or deployment.

### Tailscale mobile preview (2026-10-15)

User explicitly requested mobile access over Tailscale. server.mjs supports PREVIEW_HOST, default still 127.0.0.1. An additional owned hidden Node process binds only 100.77.28.74:4178; local preview remains running. PID/logs: build/community-tailscale.*. URL http://100.77.28.74:4178/#gallery. Host HTTP/API checks pass; remote phone reachability awaits user. No public hosting or broad-interface binding. Keep PC awake and Tailscale connected while testing. Frontend files are served fresh; restart this specific process after server/fixture changes.

## Native logo packaging (2026-10-15)

User accepted the colorful community design and requested its pixel logo as NDS/3DSX/CIA app icons. assets/icon-nds.gif (32px) and icon-3ds.png (48px) rasterize the five-square website mark in lime with pink offset shadow on purple. Source SVG, large PNG preview and scripts/generate-icons.py retained; Pillow required to regenerate assets and CIA banner. NDS GAME_ICON and 3DS SMDH now use app-owned assets instead of SDK defaults. CIA HOME Menu banner uses the same logo. No runtime or format/version change: native remains 0.12.0, CIA metadata 192.

NDS/3DSX/CIA rebuild and existing header/identity checks pass. scripts/verify-icons.py independently decodes tiled NDS RGB555 and SMDH RGB565 and compares every icon pixel with the source; verifies exact SMDH embedded in 3DSX. Stable build/test-0.12.0-icons contains all three packages plus SMDH; prior candidates preserved. Physical launcher/CIA installation appearance untested; no emulator restart or hardware transfer.


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

## Internal device pairing foundation (2026-10-15)

community/pairing.mjs adds SQLite-backed expiring user/device codes, explicit
approval/denial, single-use exchange, scoped 30-day bearer grants, owner-only
revocation, account-disable checks and durable rate limiting. Hashes only are
stored; secrets never enter logs or artwork. Trusted account-validator injection
is mandatory; literal true required. Module is intentionally not wired to HTTP
until maintained web auth/session, CSRF, HTTPS and trusted limiter subjects are
implemented. No fake login or code UI and no provider calls. Existing preview
unchanged. Twenty-two community test groups pass. Next: choose/configure the
maintained Discord/email auth integration, then expose protected pairing UI/API
and verified console transport. No commit/push/native rebuild this increment.


## Running user checklist and auth adapter (2026-10-15)

User requested a persistent list of hands-on tests and setup tasks while away.
Maintain docs/HANDS-ON-CHECKLIST.md EVERY increment; record explicit results and
separate ready tests from blocked/unimplemented features. docs/SETUP-GUIDE.md
covers host/domain, Cloudflare staging tunnel, Discord OAuth, transactional
email, and separate private moderation webhook with official reference links.
Do not ask user for secrets in chat or publish staging without authorization.

Pinned Better Auth 1.7.10 plus package-lock added via npm --ignore-scripts.
community/auth.mjs provides HTTPS-only configured factory and explicit migration,
secure sessions, hashed email OTP, minimal Discord scopes, no implicit account
linking. Not mounted on preview and not connected to pairing module yet. Real
library handler tests use in-memory delivery and synthetic credentials, send no
external messages. All 25 community tests pass; npm audit currently reports zero
known vulnerabilities. Tailscale preview untouched. Native version unchanged;
no GitHub push or production action this increment.

## Account UI and protected HTTP integration (2026-10-15)

Account page now supports configured email/Discord sign-in, sign-out, device
inspection with explicit approval/denial, and owner device revocation. The default
preview reports sign-in unavailable and exposes no mutation routes. The bridge
requires canonical Host, exact Origin for JSON POSTs, 8 KiB bodies, real verified
session users and a narrow auth endpoint allowlist. User IDs in request bodies
are ignored; account ownership comes from the session. Incoming proxy IP headers
are discarded; current limit subjects use the direct peer (conservative shared
limits behind a proxy). Trusted proxy attribution remains a deployment follow-up.

For configured staging, community/serve-auth.mjs requires AUTH_ORIGIN (HTTPS),
AUTH_SECRET, AUTH_DATABASE (separate), GALLERY_DATABASE and Discord credentials.
Explicit --migrate prepares auth; normal startup refuses missing tables and binds
loopback port 4179 by default. The gallery preview on 4178 remains unauthenticated.
Email UI/adapter exists but no production mail provider is chosen/wired. Device
begin/poll endpoints and console transport are deliberately not exposed yet.
27 test groups pass including real HTTP/session approval/revocation/logout flows,
origin/host/body checks. Live providers and physical pairing remain untested.
Update docs/HANDS-ON-CHECKLIST.md every increment. No push/native build.

## Native pairing HTTP boundary (2026-10-15)

Public origin confirmed by user: https://pixels.7db.pw, same host machine as
Collab Doodle but isolated service/credentials/databases. Default staging port
4179 remains loopback-only. No tunnel, OAuth configuration or deployment done.
Opt-in ENABLE_DEVICE_PAIRING=true exposes POST /api/device/pair/begin and /poll.
Default preview and staging remain disabled. JSON bodies max 1 KiB, exact Host,
no foreign Origin, no query credentials, narrow request fields and direct-peer
rate subjects. begin returns verificationUri with no code in its URL; poll uses
private deviceCode in the body. Browser approval stays verified-session-only.
29 community tests pass; physical TLS/pairing and live provider acceptance remain
pending. No native rebuild, external messages or push. Update hands-on queue.

## PHP/Apache migration and Discord bot (2026-10-15)

User selected PHP/Apache at https://pixels.7db.pw, DocumentRoot F:/pixeldoodle.
community-php/ is the Laravel 13 backend, Composer locked. Gallery read API and
shared frontend ported with SQLite query-only access to v2/v3 gallery databases.
41 PHP assertions cover public filters/counts, hidden metadata/media, downloads,
frontend routes and disabled mutation endpoints. No live account data exists.
Node code remains a tested migration reference, not the production target.
Prior Node moderation schema v3/ledger passes 33 groups; its PHP port is pending.

community-php/.env.example supersedes community/.env.example, with production
URL, blank secrets and suggested private F:/pixeldoodle-data paths. deploy/index.php
and .htaccess are the only public entry files; private app path defaults to
F:/pixeldoodle-app/community-php (proposed, not deployed). PHP auth, ingestion,
pairing, moderation and live Apache acceptance remain pending. No server edits.

Discord bot plan accepted: private artwork image/title/description/uploader,
dimensions/deadline; Approve/Reject/Hold buttons, optional rejection reason modal,
updated/disabled action state, shared backend authorization and audit. See
 docs/DISCORD-REVIEW-BOT.md. No Discord messages or credential setup performed.

Apache origin port confirmed by user: 8086 (supersedes proposed 8083). Saved deploy/apache.conf and setup notes now use 127.0.0.1:8086. This confirms the port choice, not end-to-end deployment acceptance.

## PHP baseline parity and packaging hold (2026-10-15)

User stopped preview packaging and asked to finish porting first. No ZIP created.
The ignored build/web-package-20261015 directory is an obsolete incomplete staging
copy; never deploy it. PHP baseline now includes gallery, Socialite Discord auth,
SMTP email codes, session/Origin/CSRF boundaries, device pairing HTTP + account
ownership, strict PNG ingestion/derivatives, explicit gallery schema migration,
operator ingest command and internal moderation ledger/timer. 17 PHP tests / 178
assertions pass; 25 Node-derived color/filter fixtures compare exact RGBA output.
No raw provider tokens persisted; no implicit account merge. APP auth DB remains
separate from gallery. SQLite IMMEDIATE transactions protect decisions/exchanges.

Email/Discord provider acceptance, real Apache acceptance, HTTP uploads/profile
creation, account linking and native TLS/transport remain pending. Dedicated bot
buttons and sender/role integration are new next work, not yet implemented. No
live messages, deployments, GitHub pushes or native builds. Example env and current
commands documented atop community-php/README.md; old paragraphs are history.

Setup guide rewritten for current PHP/Apache deployment (2026-10-15): domain,
8086, public/private paths, PHP migrations, SMTP, OAuth callback vs authorization
URL, and pending interactive bot setup. User-supplied public Discord client ID
1560340963728294011 is now in the example env; client secret remains blank.
Screenshot shows correct callback/Public Client off; live sign-in not accepted.

## Protected PHP moderation API (2026-10-15)

Added is_moderator migration, explicit community:moderator email [--revoke] CLI,
and verified-session review queue/private image/decision endpoints. Roles are
re-read for each request; only existing active verified accounts can be promoted.
Decision authority remains Moderation service with revision checks and audit.
Notification attempt values omitted from queue responses. 19 PHP tests / 197
assertions pass including live role revoke, public/private media separation,
stale decisions and session actor instead of body-supplied actor. UI and bot
transport still pending. Updated setup guide and hands-on list. No server changes,
external messages, native builds, ZIP creation or GitHub writes.

SMTP configuration reported by user (2026-10-15): mail.7db.pw:587 with
pixeladmin@7db.pw username/from and Pixel Doodle Admin display name. Recorded
non-secret defaults in PHP .env.example and setup guide; password remains blank.
SESSION_LIFETIME stays 10080. Live mail acceptance remains pending; no mail sent.


## PHP community 0.1.0 deployable slice (2026-10-15)

Studio supports verified accounts, display profile editing, PNG submissions up to
2 MiB/512 square, descriptions/tags, stable request-ID retries, maximum 20
pending/held items per artist and private owner history. Gallery schema 4 adds
artist-account ownership, description/submission/idempotency metadata and bounded
notification retry bookkeeping; tested upgrade from copy of v3 retained all 16
fixture rows. Pending artwork remains private on all public browse/media routes.

The moderator screen is `#review`, owner view `#studio`. Website actions and Discord
interactions share revision-checked/audited policy. Discord interactions endpoint
`/api/discord/interactions` verifies Ed25519 signature/timestamp, configured app,
guild/channel, linked active site moderator and configured role. Reject has optional
reason modal. Worker `community:review-tick` sends at most 10 new cards/tick and syncs
20 edits, bounded 429 backoff and no automatic replay on uncertain outcome. A
confirmed message receipt alone begins the eight-hour timer. Schedule it every
minute with Task Scheduler running `php artisan schedule:run`. See setup-guide for
private bot values and permissions.

New admin commands: `community:link-discord {email} {discordId} {--unlink}` and
`community:moderator {email} {--revoke}`. Both require an existing active verified
account. Discord ID linking is unique; no automatic email-based account merge.
SMTP non-secrets and user-supplied sender defaults remain in `.env.example`; the
password is blank and no test mail is sent.

PHP 26 tests/256 assertions pass (test HTTP is mocked); Node reference 33 groups
pass. Browser checks used only a disposable loopback synthetic account and image.
Production staging passes provider discovery and displays auth routes, DB migrations
and one-minute review schedule; v3 gallery copy migration retained existing art.
Local archives: `build/PixelDoodle-Community-0.1.0-public.zip` and
`build/PixelDoodle-Community-0.1.0-app-private.zip`; extracted to F:/pixeldoodle
and F:/pixeldoodle-app respectively. The private archive contains Laravel app and
production vendor plus sibling frontend/docs. Scanned for live .env, SQLite DBs,
logs and test fixtures. No live Apache, email, Discord message, server deployment,
GitHub push or native build performed; user must do live setup from checklist.

## Device pairing and 3DS transport candidate (2026-10-15)

PHP pairing API uses authenticated owner approval, a 10-minute one-use code,
SHA-256-only stored poll/access secrets, 5-second polling with server backoff,
gallery-read scoped 30-day credentials, account-disable checks, and
owner-scoped revocation. API routes require the canonical HTTPS host; browser
writes use the existing session/CSRF/origin boundary. Pairing remains default
off in `.env.example` and production. 3DS libctru httpc keeps SSL certificate
verification enabled and rejects redirects. Console network activity occurs
only on the Community pairing screen; drawing remains offline. Credential data
is CRC-checked in two alternating app-owned SD slots, not encrypted at rest.

The local PHP suite passes 30 tests / 278 assertions. Host regression groups
pass, including 16,316 incremental/full drawing comparisons. 3DSX and CIA builds
pass. NDS now vendors the TLS 1.2/Mbed TLS 3.6.7 client, Mozilla trust bundle,
and a separate host-provisioned, rotating Pixel Doodle seed; pairing runs only
from the Community screen. Build and physical NDS TLS/pairing acceptance remain
pending. Keep `ENABLE_DEVICE_PAIRING=false` until certificate failure, approval,
denial, expiry and revocation have passed on DSi and DS Lite. Never use plaintext
HTTP, `rand()` as secure entropy, or the Collab Doodle seed. See `docs/GALLERY.md`
and the hands-on list.




