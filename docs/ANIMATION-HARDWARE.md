# Animation hardware acceptance

Last updated 2026-10-22. Latest user-accepted device build: **0.18.0P on DSi**.
The received `143535.gif` confirms animation sent through Discord. Playback was
accepted in 0.17.0. Rapid separate-stroke work is explicitly deferred. Keep
Frames and Layers separate until the user requests their integration.

## 0.20.0: animated Aseprite export — pending on all devices

- Export a layered animation with different durations from a middle editing
  frame. Open the file in desktop Aseprite; check all frames, order, names,
  visibility, opacity, blend and palette. Check the device stays on its original
  frame, and undo/dirty state remain unchanged.
- Repeat export with overwrite and inspect .bak. Test a full/removed card and
  low-memory snapshot refusal: original artwork and previous output must survive.
- Export larger animations and measure time/memory on DS Lite and DSi.
- Keep native PDD masters. Reopening multi-frame ASE on the device is still
  explicitly unsupported; rejection must preserve current artwork.

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
