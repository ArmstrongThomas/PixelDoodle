# Pixel Doodle UI mockups

Design references for the handheld UI. Each page is a single self-contained HTML
file: open it directly in a browser (no server, build step or network needed),
or read the source. The implemented C UI and
[`HANDS-ON-CHECKLIST.md`](../HANDS-ON-CHECKLIST.md) are authoritative for
behavior and acceptance status; check the table below before building from a
mockup.

| Mockup | Covers | Status |
|---|---|---|
| [pixel-doodle-workflow.html](pixel-doodle-workflow.html) | Original concepts: canvas, Layers, Adjust, Color/Picker, tool options, Command Board, DSi/3DS switch | Mostly implemented in 0.14.x in revised form. The Command Board menu was never built. |
| [pixel-doodle-files-menu.html](pixel-doodle-files-menu.html) | Menu order, file browser, type dropdown, upper-screen file preview, lowercase keyboard, Adjust in Layers | **Implemented in 0.21.0**, approved by the user. |
| [pixel-doodle-animation.html](pixel-doodle-animation.html) | Animation plan: DS memory budget, optimization pass, frame storage design, shortcuts; canvas frame strip, timeline, onion skin and frame sheets | Plan and parts implemented in 0.15.0–0.20.0 with a different UI; see below. |

## How the native-resolution mockups work

`pixel-doodle-files-menu.html` and `pixel-doodle-animation.html` draw every
screen on a canvas at the console's real resolution (DSi 256x192 for both
screens; 3DS 400x240 upper and 320x240 touch). They use the 5x7 glyph table
from `ui/menu_font.h` (embedded as JSON) and the colors from `ui/app.c`, so text
lengths and row counts match the hardware. The bottom screen accepts taps and
drags. Screen layouts live in small JavaScript functions inside each file
(`menuLayout`, `brLayout`, `kbLayout`, `adjLayout` in the files page;
`stripLayout`, `tlLayout`, `onionLayout`, `frameLayout` in the animation page),
which give exact coordinates for each control.

## Files-menu mockup versus 0.21.0

Implemented as drawn, with these deliberate differences:

- Frames (added after the mockup) shares the Menu's top row with New Project.
- Browser rows show a type tag (PDD, ASE, PNG...) instead of file size; sizes
  would need a card lookup per file. The selected file's size is shown on the
  upper screen.
- Save As has no separate name row; the footer's Name button opens the keyboard,
  whose field shows the extension that will be added.
- The keyboard's symbol rows are `1234567890`, `_-().,!@+&` and `[]{}=#$%`.

## Animation mockup versus what was built

- Built: memory optimization and Diagnostics (0.15.0); frames in the core and a
  separate Frames page modelled on Aseprite's grid (0.16.0); playback (0.17.0);
  GIF sharing (0.18.0); onion skin on/off, previous frame red and next blue
  (0.19.0); frame actions sheet with duration, move and delete, and animated
  Aseprite export (0.20.0). Pan+L/R and New 3DS ZL/ZR step frames.
- Not built: the canvas frame strip, the timeline's cel styling and tag band,
  the onion settings sheet (counts, opacity, tint), tags/loop ranges,
  Sample+L/R shortcuts. The user asked to keep Frames and Layers separate.
- The memory estimates are superseded by measurement: Diagnostics on the DSi
  reported about 14,139 KB free with an equal largest block. See
  [animation core](../ANIMATION-CORE.md) and
  [hardware checks](../ANIMATION-HARDWARE.md).

## Licences

Lucide icons in `pixel-doodle-workflow.html` are ISC; see
[`vendor/LUCIDE-LICENSE.txt`](vendor/LUCIDE-LICENSE.txt). The other two pages
use only project glyphs and drawing code.
