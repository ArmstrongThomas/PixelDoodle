# Pixel Doodle UI mockups

Open [pixel-doodle-workflow.html](pixel-doodle-workflow.html) directly in a
browser. No local server or build step is required.

The page includes the Canvas, Layers, Adjust, Color, Picker and Command Board
concepts, plus the DSi/3DS viewport switch. Its icon subset is stored locally
in `vendor/lucide-subset.js`, so the mockup works without network access.

Lucide is distributed under the ISC license. See
[`vendor/LUCIDE-LICENSE.txt`](vendor/LUCIDE-LICENSE.txt).

[pixel-doodle-files-menu.html](pixel-doodle-files-menu.html) proposes the
reordered Menu, the file browser with type icons and a type dropdown, the
top-screen file preview, a lowercase keyboard and a Layers home for Adjust. It
draws each screen at native DSi/3DS resolution with the glyphs from
`ui/menu_font.h` and colors from `ui/app.c`, and also opens without a server.

[pixel-doodle-animation.html](pixel-doodle-animation.html) holds the animation
plan: the measured DS memory budget, the optimization pass that frees memory for
frames, the tile-pool frame storage design with frame-count estimates, frame
shortcuts, and interactive timeline, onion skin and frame-sheet mockups.

The mockups are design references. The implemented C UI and
[`HANDS-ON-CHECKLIST.md`](../HANDS-ON-CHECKLIST.md) are authoritative for
behavior and acceptance status.

0.16.0 implements the animation core and an initial Aseprite-inspired layer/frame
grid. The older memory/frame-count estimates remain estimates; hardware free
heap must be measured again with corrected diagnostics. Playback, onion skin
and animated ASE I/O in the mockups are future work. See
[animation core](../ANIMATION-CORE.md) for current behavior.
