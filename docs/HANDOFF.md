# Pixel Doodle development handoff

## Current state

- Repository: `G:\dev\3ds\PixelDoodle`
- Branch: `main`
- Version: `0.14.7`
- Git state: the UI/color/grid work is committed before handoff
- Current work: touchscreen UI refinement for NDS/DSi and 3DS
- Server changes required by this candidate: none
- Hardware transfer performed: none

The standard and private packages were rebuilt after the final source changes.
Host tests, header checks, icon checks and CIA metadata checks pass.

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
   skipped movement or value jumps.
8. Verify Brush Details previews and Load/Store behavior.
9. Verify Selection Delete remains inside its button.
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
  `C7524E28BBF85923CCA8E6F355663162CC6F388CE5C1C62932FEFDEB8CC2BEC8`
- `build/3ds/PixelDoodle.3dsx`:
  `F62462EE8E47340C1479EBC110489B34C2D0A9BDB83825CF8D237778AC56B53C`
- `build/3ds/PixelDoodle.cia`:
  `DC160EFBAA925F745F4348A33AAD0226F9B9862B7751C1FEB3D76D0A9D9FA1C1`
- `build/private/nds/PixelDoodle-Private.nds`:
  `D005D4BCE1ED5DDC6211FD00BA818C1ACB9EABADC4A6881F1541B8328E472F6C`
- `build/private/3ds/PixelDoodle-Private.3dsx`:
  `51A1AD089E6C735F73FB23D79069FE5612C12CF8DC61FF09455860AB7B62049C`

## Prompt for the next LLM

Continue work on Pixel Doodle in `G:\dev\3ds\PixelDoodle` on branch `main`.
Read the root `AGENTS.md` and `docs/HANDOFF.md` first. The current candidate is
`0.14.7`. Start from the current committed `main` state. Preserve the DSi-first
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
