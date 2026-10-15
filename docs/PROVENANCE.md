# Reuse provenance

2026-10-14. User explicitly requested reuse of their Collab Doodle clients. This is local development; no blanket redistribution license is asserted.

| Component | Origin/treatment |
|---|---|
| ui/menu_font.h | Copied from Doodle-NDS/source/menu_font.h, attributed there to the 3DS UI. Source SHA-256 d332c6791e6820b32c468b1bc81a62f62a168e2cd9f33e8907d8c24baea136c8. Attribution retained; confirm ownership/license before publication. |
| platform/3ds/pixel.rsf | Adapted Doodle CIA RSF with new title/product/unique ID. Inherits broad homebrew service permissions provisionally; narrow before release. |
| UI/input | New C implementation following Doodle theme, Balanced controls and gesture isolation. |
| NDS shell | New adapter using established video/runtime detection/storage/VBlank patterns. |
| Save slots | New document codec inspired by NDS settings recovery. New magic, paths, payload and CRC32; no existing data copied. |
| Core/history/compositor | Newly authored; no Aseprite code copied. |
| Build | Existing pinned BlocksDS digest and devkitPro flags, new local-only scripts/output names. Packaging follows makerom/banner pattern with original geometric placeholder and SDK default icon. |

No private server source, audit records, credentials, seeds, user art, databases, environment configuration or firmware copied. SDK dependencies remain external. Public distribution requires final dependency/asset/license and title-ID review.

## 0.2.0 dependencies

vendor/zlib is the unmodified zlib 1.3.2 inflate/CRC/Adler subset copied from Doodle-NDS/vendor/zlib. LICENSE and README retained. zlib permits use/modification/redistribution subject to its notice; no Doodle-specific code is in this subset. Aseprite codec is independently implemented from its public file specification; no Aseprite implementation source was copied. Desktop Aseprite is used only as an installed interoperability test tool.
