# Inspection and primary-source research

Reviewed 2026-10-14. No source code, assets, recordings, secrets or runtime data were copied into this repository. Research links below point to moving upstream branches unless stated otherwise; pin exact revisions and verify each file/dependency license before implementation reuse.

## Existing workspace findings

Read the workspace AGENTS.md, Doodle README and Doodle-NDS AGENTS/README. Doodle has no repository-local AGENTS.md at its root. The parent guide applies to Collab Doodle projects, so this new project has its own operating guide. Existing documentation contains historical milestones; source inspection is the basis for the component findings below.

Doodle HEAD at inspection: `65f8b17c7fa3ab02e9350eeca423843b43e685dd`, with substantial existing tracked changes. Doodle-NDS is an initialized repository without a HEAD commit and its source tree is untracked. Doodle-Server also has substantial existing modified/untracked files. All were left intact. Git status used the bundled Windows Git because devkitPro's MSYS Git failed to initialize; a command-local safe.directory exception permitted read-only NDS/server inspection without modifying global Git configuration.

| Inspected source | Useful foundation | Required boundary/change |
|---|---|---|
| Doodle `include/brush_render.h`; NDS `source/native_brush.c`, `include/native_brush.h` | Deterministic stamp geometry, integer coverage, sampling, RGB15 display conversion | Existing surfaces are RGB, not transparent layered RGBA; white erasing must become alpha erasing |
| Doodle `source/canvas_state.cpp`, `include/canvas_state.h` | Dirty bounds, canvas/display mapping, viewport state | Current zoom is 0.5/1/2/4x; new integer high-zoom transform and tiled document required |
| Doodle `include/input_bindings.h`, `include/ui_route.h` | Semantic actions, handedness presets, conflict handling, bounded back stack | Replace collaboration-specific actions/routes; new Undo/Redo/Save semantics |
| NDS `include/tool_menu.h`, `source/tool_menu.c` interface and documented buffered rendering | Familiar tabs, palette/keyboard, modal interaction, VBlank buffering | Extract UI presentation from channel/profile/support dependencies; add layers and project browser |
| NDS `include/pressure.h`, `source/drawing_settings.c` | Validated contact-resistance input, normalization, filtering, calibration and checksummed two-slot settings | Reuse approach with new paths and pixel-tool defaults; no claim of pressure on 3DS |
| NDS `source/main.c` | Actual `isDSiMode()` detection, allocation probe, cooperative VBlank scheduling | New offline shell, runtime budget selection and storage diagnostics |
| Doodle client README/build/test documentation | devkitPro/libctru packaging and host fixture patterns | New output names/title IDs; never run updater-mutating helpers |
| NDS README/vendor provenance | Pinned BlocksDS 1.24.0, host checks, TLS/seed caveats | Start offline; networking gets separate state and later budget/license review |
| Server `src/socketHandlers.js` drawing audit; `src/moderationStore.js` timeline; `src/ndsCollab.js` compact stamp parser | Existing ordered stroke recording and replay-oriented metadata; directly informs new recorder | Adapt event schema locally; no private server code/data transplant; add layers, alpha, undo, self-contained checkpoints and renderer version |

Collaborative authority, optimistic echo reconciliation, channel switching, identities, onboarding, staff/support/report screens, production endpoints and updater are intentionally outside the solo core. Existing brush comparison tests are useful test-pattern references, but new alpha/layer/pixel-perfect semantics require their own synthetic oracle fixtures.

No root project license was found in Doodle or Doodle-NDS by the license-file inventory. Public availability alone is not permission to redistribute source/assets. The user authorizes this development foundation, but attribution/ownership and any outside contributions must be established before source extraction and public licensing. Do not label all existing code MIT by assumption. Vendored Mbed TLS and zlib carry their own license/provenance files; audit only dependencies actually selected.

## Aseprite: important license scopes

The [official repository](https://github.com/aseprite/aseprite) and [EULA](https://github.com/aseprite/aseprite/blob/main/EULA.txt) do not offer a permissive license for the whole current application. The EULA restricts redistribution and allows source modification for personal use/contribution. The [official license-change announcement](https://dev.aseprite.org/2016/09/01/new-source-code-license/) explains the change from GPLv2.

However, [src/doc/blend_funcs.cpp](https://github.com/aseprite/aseprite/blob/main/src/doc/blend_funcs.cpp) explicitly declares MIT terms, and its adjacent [src/doc/LICENSE.txt](https://github.com/aseprite/aseprite/blob/main/src/doc/LICENSE.txt) grants MIT rights with notice preservation. This permits considering a narrowly scoped compositing adaptation after auditing exact files and transitive headers; it does not license the UI/application wholesale. Default recommendation is a small portable implementation against documented math, with this module as a reference. Preserve notices if any MIT implementation is adapted. No source copied in planning.

The [official layer documentation](https://www.aseprite.org/docs/layers/) provides useful layer naming, visibility, locking, ordering and transparency concepts. Pixel Doodle will begin with raster layers only. [Blend mode enum source](https://github.com/aseprite/aseprite/blob/main/src/doc/blend_mode.h) and the compositor distinguish named blend behavior; our four-mode subset gets its own stable IDs.

The [W3C compositing specification](https://www.w3.org/TR/compositing-1/) supplies an independent normative basis for source-over alpha and separable blending. Implement its relationships with frozen integer rounding and synthetic tests; avoid copying large documentation passages or relying on browser-specific results.

## Colors! and recorded process playback

The archived [original author announcement by Jesse, 25 June 2007](https://gbadev.net/forum-archive/thread/23/13513.html) describes DS pressure painting, save/load and replay of the painting process. This is a preserved first-person developer post, not current documentation; the original Collecting Smiles page could not be retrieved during research. The archive's later discussion also illustrates why approximate third-party brush implementations can produce different playback.

The current [official Colors Live information page](https://www.colorslive.com/info) distinguishes painting playback in its gallery from exported timelapse video. It is evidence of the product interaction, not a specification of the original DS `.drw` format. We have not verified a reusable license for original Colors! code or assets, and propose neither copying them nor `.drw` compatibility. Doodle's own existing recording is the closer technical foundation; Colors! supplies the intended user experience.

## Hardware and storage references

- [BlocksDS memory map](https://blocksds.skylyrac.net/docs/internal/memory_map/): DS/DSi main memory, compatibility-mode mappings and reserved regions. Budget actual allocatable RAM; mode matters.
- [BlocksDS filesystem guide](https://blocksds.skylyrac.net/docs/guides/filesystem/): DLDI flashcart and DSi SD paths. Supports the plan to keep flashcart-only DSi operation and avoid NAND writes.
- [libctru heap allocator](https://github.com/devkitPro/libctru/blob/master/libctru/source/system/allocateHeaps.c): runtime-derived heap allocation, supporting measured 3DS envelopes rather than assuming physical RAM is free.
- [devkitPro libctru repository](https://github.com/devkitPro/libctru): native 3DS homebrew foundation. Retain existing supported toolchain rather than changing it during planning.

Remaining feasibility work: actual offline executable footprints, worst-case tile-history overhead, full-canvas blend throughput, durable FAT behavior, browser/native replay parity, direct-upload TLS memory and production host capacity. These are explicit M0/M3/M4 gates, not established facts.
