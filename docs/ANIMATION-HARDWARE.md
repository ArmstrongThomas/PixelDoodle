# Animation hardware acceptance

Last updated 2026-10-22. Latest user-accepted device build: **0.18.0P on DSi**.
The received `143535.gif` confirms animation sent through Discord. Playback was
accepted in 0.17.0. Rapid separate-stroke work is explicitly deferred. Keep
Frames and Layers separate until the user requests their integration.
(2026-10-26: the user has now requested it; see 0.22.0 below.)

## 0.24.0: timeline block and cel moves - pending

See `docs/HANDS-ON-CHECKLIST.md` 0.24.0 for the full list. Focus:

- Menu has no Frames entry; Timeline is reached from the Layers rail.
- L + tap frame numbers marks/unmarks whole columns; verify the yellow marks,
  `n SEL`, and that a stylus tap with L does not also switch frames.
- Hold 0.3 s on a marked number and drag a block. Validate the hold threshold,
  drop column accuracy on 14 px (DSi) and 16 px (3DS) columns, moving a block
  near the first/last visible column, and that art/durations follow the frames.
- Hold 0.4 s on a cel and drag across layers/frames. Check the destination
  outline, same-cell cancel, slow-scroll not becoming a move, links/undo
  clearing, and that the current frame follows the destination.
- Exercise undo after a block move (paint undo must target the moved art) and
  confirm cel moves are not undoable.
- Verify tag ranges remain positional after frame moves, or report the desired
  behavior.
- Measure large multi-frame blocks and repeated moves on DS Lite and DSi; the
  permutation uses stack arrays and no heap allocation.
## 0.23.0: tags, linked cels and animated Aseprite import - pending

See `docs/HANDS-ON-CHECKLIST.md` 0.23.0 for the full list. Summary:

- Timeline tag band and frame-sheet TAG row on DSi and 3DS; tag range,
  direction, colour, rename and REMOVE with a stylus.
- Linked cels: link with previous, unlink, add a linked frame, and confirm one
  edit updates every member without ghost pixels or lost undo.
- Playback loops the innermost tag; check forward, reverse, ping-pong and a
  one-frame tag fallback on hardware timing.
- Export a tagged/linked project and open it in desktop Aseprite if available;
  import an animated `.aseprite` with tags and linked cels and edit a group.
- Import a deliberately unsupported file (groups, tilemap, indexed or an
  out-of-canvas cel): it must report an error and keep the current project.
- Measure large-animation import and the browser preview on DS Lite and DSi.
  A malformed file is rejected safely; allocation failure during frame creation
  can leave a partly imported project and must be reported clearly.
- Memory: tags and links add a few bytes per frame/layer and a small tag array;
  recompute the frame-pool budget on DS Lite with several tags and groups.
## 0.22.0: Layers + Timeline merge, sheets and canvas strip - pending on all devices

- Layers rail: LAYERS | TIMELINE tabs. Layer list still 6 rows (DSi) / 8 (3DS);
  visibility, lock, options, blend dropdown, DUPE/MERGE/RENAME, hold-drag
  reorder, + New Layer, Adjust and Delete Layer must behave as before.
- Timeline: tap cels/numbers/names; drag sideways and up/down to scroll; hold a
  frame number about 0.3 s and drag to reorder (paint undo must follow the art).
  + FRAME, DUPE, DELETE (confirm, clears undo), FRAME... sheet.
- Frame sheet: - / + by 10 ms, presets, THIS FRAME vs ALL FRAMES, MOVE < / >.
  Check durations in playback, GIF and ASE export.
- Onion sheet (timeline onion button, or hold the strip's onion 0.4 s):
  BEHIND/AHEAD 1-3 with fading, opacity 10-90 %, tint off shows neighbour
  colours. Compare drawing latency with 1/1 vs 3/3 and tint on/off on DS Lite
  and DSi at the largest canvases; check allocation refusal keeps the art.
- Canvas strip (2+ frames): prev/next wrap, n/m opens Timeline, play, onion tap
  vs hold, tick tap and scrub. Strokes must never paint under the strip; the
  bottom canvas row must still be reachable by panning. Deleting back to one
  frame restores the full-height canvas.
- Sample+L toggles onion, Sample+R plays; any button or touch stops.
- Upper screen on the Timeline tab: frame preview, details, five-frame
  filmstrip; playback from the timeline should stay smooth (thumbnail cache).

## 0.20.0: animated Aseprite export — pending on all devices

- Export a layered animation with different durations from a middle editing
  frame. Open the file in desktop Aseprite; check all frames, order, names,
  visibility, opacity, blend and palette. Check the device stays on its original
  frame, and undo/dirty state remain unchanged.
- Repeat export with overwrite and inspect .bak. Test a full/removed card and
  low-memory snapshot refusal: original artwork and previous output must survive.
- Export larger animations and measure time/memory on DS Lite and DSi.
- Keep native PDD masters. Reopening multi-frame ASE was explicitly
  unsupported in 0.20.0; 0.23.0 now imports it. Rejection must still preserve
  current artwork.

## 0.20.0: frame actions and ordering — pending on all devices

- Tap the selected frame number, move left/right across column boundaries,
  and confirm the selected art stays the same while its position changes.
- Paint on several frames, move in both directions, then undo/redo across
  frames. Each edit must target its original artwork. Save/reopen and export
  GIF/ASE to confirm order and durations follow the moved frames.
- Boundary moves should refuse; deleting still confirms and clears history.
- Play through more columns than fit, stop, and confirm the original editing
  frame is visible again. Try Play with one frame for the explanatory notice.

## 0.19.0: onion skin â€” pending on all devices

- Menu > Frames: toggle Onion On, return to canvas. Previous frame is faint
  red; next is blue. First/last frame must not wrap to the opposite endpoint.
- Paint and erase over ghosts, step frames, undo/redo between frames. Confirm
  clean current pixels remain, with no ghost pixels written into the project.
- Sample a ghost-only location: it must sample the actual current canvas.
- Toggle visibility/opacity, duplicate/delete a layer, insert/delete a frame,
  load another project of the same dimensions. Ghosts must update correctly.
- Save/reopen PDD, export PNG and send GIF: none should contain the ghosts.
- Play/stop: playback stays clean and returns to the original editing frame.
- Compare sustained drawing/panning with Onion Off/On on DS Lite, DSi, standard
  3DS and expanded New 3DS. Record diagnostics and canvas/layer/frame sizes.
- Test larger canvases and allocation refusal. Art must survive; Onion Off
  should release the alpha cache. Test repeat enable/disable and project loads.

## Existing animation foundation â€” broader testing remains

- Multi-frame PDD save/reopen and recovery after interrupted saving; use fresh
  filenames because older versions cannot read PDDA.
- Empty/duplicate frames, all eight layers, long names, scrolling past visible
  frame columns and layer rows; inspect both DS and 3DS layouts physically.
- Per-frame durations including short/long values, long playback sessions,
  stop via pen/button, and restoration of unsaved editing state.
- Frame deletion confirmation and explicit undo-history clearing; layer-wide
  visibility/order/opacity/locks; all-frame layer duplication and deletion.
- Memory pressure near the frame-pool budget. Failed switches/duplicates must
  preserve art and selection. Save should still work without packing headroom.
- GIF on DS Lite/3DS: per-frame timing, transparent clearing, high-color
  palette reduction, partial-alpha threshold, oversized refusal, network
  failure and repeat sends. The small DSi GIF success does not cover these.

No overnight hardware transfers, live webhook posts or updater publication are
needed. Host fixtures/builds establish software checks, not physical acceptance.
