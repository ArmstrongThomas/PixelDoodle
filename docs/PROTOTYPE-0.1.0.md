# Prototype implementation and acceptance

Historical prototype notes. Current 0.9.0 size/device profiles, ownership, PDD9 format and validation are documented at the top of AGENTS.md and README.md; they supersede the fixed-size assumptions below.

2026-10-14. User authorized implementation and renamed the product Pixel Doodle. Repository: `G:/dev/3ds/PixelDoodle`. NDS, 3DSX and CIA use common C engine and UI sources. No remote/publication was created. Collab Doodle sources, data and outputs were not modified.

## Deliberate prototype simplifications

This supersedes planning-only statements for current implementation. Earlier designs remain the roadmap.

- Dense fixed 128x128 RGBA layers replace planned sparse tiles for now. Four layers on every target, Normal source-over opacity and visibility. Runtime DS mode is detected but does not change limits.
- Eight preallocated per-layer XOR transactions replace future variable tiled history. Each entry carries pixel/property XOR and before/after revision IDs. No allocation during strokes; reserved before image permits cancel. New edits invalidate redo; oldest history is evicted first. Empty transactions consume no entry.
- PdDocument is 917,760 bytes in the host build. NDS presentation buffers add 393,216 bytes; save/load scratch adds 262,152 bytes, about 1.50 MiB for these major allocations. This excludes stacks, executable, SDK/driver heap and small app fields. 3DS presentation buffers add 691,200 bytes instead. These are calculated, not measured device peak allocations.
- PDD1 files: fixed 32-byte LE header, eight layer-property bytes, four 128x128 LE ARGB word arrays (straight-alpha semantics). Exactly 262,184 bytes. Header: magic, version 1, width, height, layer count, sequence, payload length, CRC32. CRC covers first 28 header bytes plus payload. Native structs/padding are not serialized.
- Two full save slots replace the future indexed container/manifest. Select newest valid; write older/invalid; require flush/sync/close/readback. Stage load before changing art. No claim of power-loss atomicity. History resets after load.
- One project, session-only viewport/palette/selection, fixed stack, eight colors. Full recomposition on redraw is provisional; measure before expanding limits. No replay yet.

## Validation

| Check | Result |
|---|---|
| MSVC host build with warnings as errors | PASS |
| Alpha stacking, visibility, opacity, transparent eraser | PASS |
| Continuous line endpoints and canceled stroke | PASS |
| History eviction, undo/redo, branch invalidation, saved revision tracking | PASS |
| Save/load full layer equality and newest-copy selection | PASS |
| Truncated newest save fallback and failed-save result | PASS |
| Pan/menu gesture isolation | PASS |
| Four native DS 256x192 synthetic menu renders | Inspected |
| 3DSX compile/link/SMDH | PASS |
| CIA packaging, title 000400000CE47800, TMD version 16 | PASS |
| Azahar 2125.1.2 boot/render, drawing, layer selection | Observed |
| Azahar verified save, 262184-byte file | PASS |
| Azahar restart automatically loads saved project | PASS |
| Physical 3DS transfer | 3dslink exit 0; user reports works okay |
| Physical persistence/latency and CIA install | NOT RUN / awaiting feedback |
| Pinned BlocksDS build | PASS; BlocksDS 1.24.0 pinned image |
| DS Lite/DSi LL hardware | ROM deployed and readback verified; physical testing pending |

Docker startup was repaired by preserving the inaccessible socket-only runtime directories under dated backup names and disabling EnableDockerAI after backing up settings-store.json alongside the original. Docker engine 29.6.1 then answered successfully. No factory reset was used. The optional Docker AI feature remains disabled. Relevant upstream report: https://github.com/docker/desktop-feedback/issues/531 .

NDS compilation exposed an unchecked fatInitDefault result. The adapter now checks mounting and the app save directory before enabling persistence. Common core/UI and the accepted 3DS artifacts are unchanged.

## Exact artifacts

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| PixelDoodle.nds | 159744 | 18015eec100919117fc808a4ee52eb850077f05b5f637c8158e3ce161283fa65 |
| build/3ds/PixelDoodle.3dsx | 134668 | 7b2995289b9467519bf8aebdefe209128c237e144ea4f1024ad50f3b7a74a03f |
| build/3ds/PixelDoodle.cia | 174528 | 598030a4f18a8c7761581f381973bd33f5010dda37ad189741909a9aed675c4f |

3DSX launched in Azahar then sent unchanged to user-supplied 192.168.1.225. First sandboxed attempt failed; authorized local-network retry exited successfully. CIA was inspected only, not installed/sent. NDS deployed on 2026-10-14 to H:/Homebrew/Dev/PixelDoodle.nds (DSPICO, user-designated DSi LL) and I:/Homebrew/Dev/PixelDoodle.nds (SPAREPICO, user-designated DS Lite). Both FAT32 volumes were checked before copying; both 159744-byte readbacks match the hash above. Only the new ROM was copied. The user must safely eject and launch locally; deployment does not establish hardware acceptance.

## Hardware acceptance

Draw on layer 1, change color and draw on layer 2, hide/show it, cycle opacity, undo/redo. Pan while zoomed; release modifier before lifting and check no accidental ink. START Save and check verified message. Exit and relaunch; inspect layers/properties. Repeat save so both slots exist. Controlled interruption tests require sacrificial art later.

Remaining work: larger initial zoom/viewport outline, richer palette, Circle Pad, layer management, detailed errors and multiple projects. Fresh blank state says SAVED before its first file; START creates it. This prototype is not for irreplaceable art.

## Rendering performance candidate (2026-10-14)

User accepted boot and save/load on both DSi LL and DS Lite. Drawing latency and long angular segments were unacceptable on both; user also reports noticeable 3DS lag. This supersedes the earlier general 3DS acceptance statement for responsiveness.

Shared renderer now expands canvas pixels into zoomed runs instead of dividing twice for every screen pixel. Overview coordinate mapping is precomputed. Transparent/opaque display and fully opaque composite results take exact integer fast paths. Stationary touches no longer force repeated full renders (eraser transitions still redraw). NDS removes its second per-redraw VBlank wait, allowing input sampling each refresh when work fits the frame. No curve fitting or antialiasing was introduced: the aim is to retain more real touch positions, preserving the one-pixel pencil semantics. Actual hardware latency/frame rate improvement is not yet measured.

Validation: host history/storage/gesture tests pass, 131072 composite comparisons against the previous arithmetic pass, and DS/3DS canvas viewport pixel comparisons pass at all six zoom levels. NDS/3DSX/CIA builds and package checks pass. Fresh 3DS binaries have not been sent or accepted in Azahar/hardware yet.

Candidate SHA-256:
- NDS: 923f82772a4cc75ff15c3d32cc00111b4f5c9144d48769962b994da72ca1a46f
- 3DSX: e33a06bf10996bc87793c5c0c4c9ac7e808c17d5a6969f4f73dcd63c4da5d247
- CIA: c8a4021719f976ee7b32dfa3d725e9c6be1f94e168e3fe01c329bf4412083fc1

H:/Homebrew/Dev/PixelDoodle.nds updated and hash readback verified; previous ROM preserved under build/hardware-backup. I: (SPAREPICO) failed reading the old ROM before any write; Windows then reported HealthStatus Warning / OperationalStatus Full Repair Needed and Homebrew/Dev became inaccessible. No repair, formatting or write was attempted on I:. User asked to reconnect and check access. Existing artwork was not read or modified on either card.

DS Lite deployment follow-up: after the user reconnected I:, the original ROM was readable and matched its expected hash. Get-Volume still reported Warning / Full Repair Needed. Read-only chkdsk I: printed an initial Access is denied line but completed file/folder verification, exited 0 and reported no filesystem problems; no repair was run. The performance candidate was then copied to I:/Homebrew/Dev/PixelDoodle.nds and its SHA-256 readback verified. Previous ROM backed up locally. Both cards now contain the same performance candidate; device responsiveness acceptance remains pending.

## R2 DS Lite acceptance (2026-10-14)

R2 limits active-stroke compositing and bottom-screen copies to the latest segment's bounding rectangle. The top overview catches up on stylus lift. Shared implementation applies to NDS, DSi and 3DS. SELECT/Menu exposes session maximum drawing input/render/copy times and consecutive drawing sample gaps, in microseconds; these are not end-to-end stylus latency measurements.

Host checks passed, including 360 incremental/full-render comparisons across DS/3DS screen sizes, all zoom levels, alpha, eraser and undo. All three packages built and passed their package checks. melonDS 1.1 portable setup is at G:/dev/3ds/.tool-cache/melonDS-1.1/melonDS.exe, using built-in DS firmware. Emulator storage is not configured. User handles emulator mouse input; launch builds for them rather than automating drawing.

User reported R2 incredibly smooth in melonDS DS mode, then buttery smooth on physical DS Lite after manual copying. Photo 143027.jpg shows R2 timing maxima: input 3450 us, render 106 us, copy 11 us, sample gap 16716 us, N=37. This establishes responsiveness acceptance for the tested DS Lite workload, not all zoom/layer combinations. Earlier save/load acceptance on both devices remains valid; R2-specific persistence and R2 DSi/3DS hardware responsiveness have not been separately confirmed.

Accepted NDS SHA-256: a4cb2b5e4a13baac6ccce74cae45687a5df0c2946cf09fe51cf69cb3498d9c34. Automated R2 deployment to I: found no mounted volume; user subsequently copied manually. Always link the built NDS file for manual copying in future build handoffs.
