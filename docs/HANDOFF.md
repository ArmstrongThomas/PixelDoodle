# Pixel Doodle development handoff

## Current state

- Repository: `G:\dev\3ds\PixelDoodle`
- Branch: `main`
- Version: `0.14.9`
- Git state: 0.14.8 is commit `cfad4f1`; 0.14.9 dither is committed after it; the
  remaining modified community/HTTPS files differ only in line endings
- Current work: touchscreen UI refinement for NDS/DSi and 3DS
- Server changes required by this candidate: none
- Hardware transfer performed: none

The standard and private packages were rebuilt after the final source changes.
Host tests, header checks, icon checks and CIA metadata checks pass. Azahar
booted the exact standard 3DSX (`build/test-0.14.9`) to its startup recovery
prompt; that prompt was left untouched and no drawer interaction was driven.

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

1. Copy the private NDS build to the DSi/DSpico card manually.
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
- Do not publish to GitHub or transfer to hardware unless the user explicitly
  asks for that external action.

### Deferred product work

- Community functionality remains postponed and hidden from standard builds.
- NDS and 3DS hardware acceptance of the new touch interactions is pending.
- Existing legacy option pages still exist for compatibility and internal tests;
  the new tool drawer routes normal interaction through the compact side panel.
- Further UI passes should preserve the original `DUPE` spelling and the
  4x Pixel Grid threshold.

## Current artifact hashes

- `PixelDoodle.nds`:
  `E60066BC7CEA7D201701B80768F25A4A39270D2935917DD03614DF9B1EF49EBD`
- `build/3ds/PixelDoodle.3dsx`:
  `2DF45E0E94F349973EB669DB1A6094F6EA43B06F8946565C2CD4E50137D2AAFA`
- `build/3ds/PixelDoodle.cia` (CIA version 233):
  `B911E8AA932052D5765F8F1B212495D92D644A770BA233906C1441F60A1F0CC8`
- `build/private/nds/PixelDoodle-Private.nds`:
  `E180B9BF011BB1045E1246CF104F56FFF8A405268720B0251A702A281713906D`
- `build/private/3ds/PixelDoodle-Private.3dsx`:
  `E10F775CBD55854C5E54E8E2F5A94C51ED28DB4EF2A286F9E086363C4A959DF7`

## Prompt for the next LLM

Continue work on Pixel Doodle in `G:\dev\3ds\PixelDoodle` on branch `main`.
Read the root `AGENTS.md` and `docs/HANDOFF.md` first. The current candidate is
`0.14.9`. Start from the current committed `main` state. Preserve the DSi-first
workflow: the user manually copies NDS artifacts and tests them on physical
hardware.

Start by reviewing the latest hardware findings from the user. If none are
given, inspect the most recent NDS/Azahar screen state and run the documented
host suite before changing code. Prioritize touch interaction defects in the
Tools drawer, slider dragging, advanced option sheets, Layers, and the private
Discord sharing flow. Keep the single-column tool rail, compact right-side
options panel, transparent canvas area, 4x Pixel Grid threshold, and `DUPE`
label unless the user explicitly changes those requirements.

Use the native UI and controls in README.md. Build through
`scripts\build.ps1`, using elevated Docker Desktop for NDS builds. Do not send
anything to hardware or GitHub unless the user explicitly requests that exact
action. Preserve user settings, identities, webhook overrides, artwork, and
save files. Before handing work back, update `docs/HANDOFF.md`,
`docs/HANDS-ON-CHECKLIST.md`, hashes, and the anti-slop Delivery Gate.
