# Animation continuation — 2026-10-22

The user is asleep. No new hardware transfer or live Discord post was made.

## Saved work

- **`2ce8a56` — pushed to origin/main before proceeding:** frame/cel foundation,
  playback and private GIF sharing. User accepted playback and GIF delivery on
  DSi, including the received `143535.gif`.
- **`0f51d8c` — local:** cached read-only previous/next onion skin, version 0.19.0.
- **`e8967a3` — local:** streamed animated Aseprite export, allocation-free frame
  ordering with paint-undo remapping, and playback polish, version 0.20.0.

Unrelated whitespace-only changes remain unstaged. Do not reset them. Private
webhook credentials and credential-bearing build artifacts remain ignored.

## Candidate to test next

Current source and staged packages are **0.20.0** / **V0.20.0P** for private:

- Standard: `build/test-0.20.0/` — NDS, 3DSX, CIA.
- Private: `build/private/test-0.20.0/` — NDS, 3DSX.
- Exact sizes/SHA-256: `build/artifacts-0.20.0.json` and the top of HANDOFF.md.
- Older 0.19.0 and accepted 0.18.0 candidates remain in their versioned folders.

Get renewed DSi Link or 3dslink readiness before sending the exact staged file.
Do not assume the sleeping user's console is a waiting receiver. Do not remove
the SD card. No CIA was installed and no Azahar visual acceptance was claimed.

## What changed

Frames has an Onion toggle beside Play. Red marks the preceding frame and blue
the next; endpoints do not wrap. Ghosts affect only the bottom editing canvas,
stay out of sampling/files/playback, and use a cached pair of alpha planes.

Tap the selected frame number for frame actions. Move Left/Right keeps the
artwork selected and remaps paint undo/redo to follow it. Move itself is not
undoable. Delete still confirms and clears history. Frames and Layers remain
separate as requested.

Export > Aseprite now writes all frames/layers and their durations, with exact
RGBA and global metadata. It streams to disk, verifies readback before replacing
output, and restores the original editing frame. **Animated ASE import is still
unsupported. Keep native PDD masters.** Alpha Lock remains native-only.

## Verification

- Standard/private host and native builds; DS/DSi headers and DLDI, CIA version
  320, icons and pinned CA checks pass.
- New onion-cache/reload, frame-order/history, ASE streaming/state/failure tests
  pass alongside 16,436 render comparisons and 131,072 composite comparisons.
- Independent GIF and ASE parsers check actual pixels, frames and metadata.
- Installed desktop Aseprite runs in batch while Windows is locked: opens and
  saves the animated export; rendered RGBA matches within rounding differences,
  including transparent clearing and per-frame durations.
- Broader desktop interop passes existing native/ASE and compressed-cel cases,
  malformed-file preservation, palettes, variable sizes and large PNG pixels.
- DS/3DS native onion and frame-action fixtures were visually inspected.

## Next work

Use [ANIMATION-HARDWARE.md](ANIMATION-HARDWARE.md) for the physical test queue.
None of the new 0.19/0.20 device behavior is accepted yet. Default onion is off.
The quick separate-stroke issue stays deferred; do not reintroduce queued input.

After hardware feedback, priorities are transactional animated ASE import,
frame ranges/copy, local GIF and sprite sheets, tags/loop ranges, structural
undo and safe all-frame layer merging. Preserve the current editing model and
update the handoff and hardware checklist after each implementation.
