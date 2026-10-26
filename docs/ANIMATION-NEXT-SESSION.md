# Animation continuation - 2026-10-26 (0.24.0)

## Saved work

- **`2ce8a56` - pushed to origin/main before animation work:** frame/cel
  foundation, playback and private GIF sharing.
- **`0f51d8c` - local:** cached read-only onion skin, 0.19.0.
- **`e8967a3` - local:** streamed animated Aseprite export, frame ordering with
  paint-undo remapping, playback polish, 0.20.0.
- **`5207761`, `738b07d` - local:** file-screen redesign and follow-ups, 0.21.0.
- 0.22.0 (Layers + Timeline merge), 0.23.0 (tags, linked cels, animated
  Aseprite import/export) and 0.24.0 (timeline moves, Menu cleanup) are
  **uncommitted** in the working tree. Preserve unrelated line-ending changes;
  do not reset them. Private webhook credentials and credential-bearing builds
  remain ignored.

## Candidate to test next

Current source and staged packages are **0.24.0** / **V0.24.0P**:

- Standard: `build/test-0.24.0/` - NDS 403968 B, 3DSX 913896 B, CIA 730048 B
  (title version 384).
- Private: `build/private/test-0.24.0/` - NDS 792064 B, 3DSX 921408 B.
- Exact SHA-256: `build/artifacts-0.24.0.json` and `docs/HANDOFF.md`.
- Both NDS files were sent save-only to DSi Link at `192.168.1.165` on
  2026-10-26 (CRC-32 B2125FE5 private, EC4E0412 standard) into
  `Homebrew/Dev/Received/`. No 3DS transfer, CIA install or Azahar acceptance.

## What changed in 0.24.0

- Menu > Frames was removed at the user's request. Row 1 is New Project alone;
  Timeline is reached from the Layers rail.
- Whole-frame marks: pen lifted, hold L and tap frame numbers. Marked columns
  get yellow numbers/bars and `n SEL` on the memory line. A plain tap on an
  unmarked number clears marks; marked numbers stay marked when tapped.
- Block moves: hold a marked frame number for 0.3 s, drag to the target column
  and release. Marked frames start there and unmarked frames shift. One frame
  moves the same way. Frame data, durations and links move with the art; paint
  undo is remapped. Tag ranges remain positional.
- Cel moves: hold a cel for 0.4 s, drag to another layer/frame, release. The
  destination is outlined; same-cell cancels. Links on both cels and paint undo
  are cleared; the move is not undoable. Works in single-frame projects.
- Core `pd_frames_move` is an allocation-free permutation with current/history
  remapping; `pd_frame_move` now wraps it. `pd_cel_move` covers same-frame,
  cross-frame and no-animation moves with transactional ref replacement.
- Marks clear on add/dupe/delete/new-linked/open/new-project and after cel
  moves; block moves remap them.

## Verification

- Full host suite passes, including the new `move_checks` (block ordering,
  non-contiguous marks, allocation-free moves, undo/current remapping, cel
  moves, links, single-frame) and `move_ui_checks` (L+tap marks, block drop,
  hold-drag cel, mark clearing, no Menu Frames entry), plus 16-bit screen,
  HTTPS, render/composite oracles and independent GIF/ASE parsers.
- DSi/3DS fixtures for frame marks and cel drag were inspected.
- Private then standard native builds pass NDS/DSi headers, ARM bounds, DLDI,
  icons, TLS assets and CIA title version 384.
- No desktop Aseprite or device acceptance is claimed for the new gestures.

## Next work

Use [HANDS-ON-CHECKLIST.md](HANDS-ON-CHECKLIST.md) 0.24.0 and
[ANIMATION-HARDWARE.md](ANIMATION-HARDWARE.md) for the physical queue. After
acceptance, remaining animation work is structural undo (frame/tag/link
operations), sprite-sheet export, Aseprite layer-group import and safe
all-frame layer merging. The broader roadmap still has the Community milestone
(native submissions/chunk upload, live pairing and deployment) and process
recording/replay (M3) untouched. Update the handoff and checklists after each
increment; never describe unaccepted device behavior as working.
