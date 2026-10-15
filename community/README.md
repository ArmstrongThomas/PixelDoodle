# Pixel Doodle Community 0.1.0 — local gallery prototype

Run from this folder with Node 22+:

```powershell
node server.mjs
node --test
```

Default preview: http://127.0.0.1:4178. PORT overrides the port; the listener always binds loopback. No installation/build step or third-party runtime dependency. Do not expose this prototype as a production service.

Implemented: responsive Explore grid, query/tag filters, pagination, artist profiles, artwork detail, integer zoom, dark/checkerboard background, repeating tile preview and downloadable 64x64 RGBA PNGs compatible with the handheld community cache. All content is explicitly synthetic. Generated sample art is code-authored in fixtures.mjs; no external artwork or user files are read.

Read-only API:

- GET /api/gallery?q=&tag=&artist=&limit=8&offset=0 — version 1, items, total, nextOffset and public tags. Limit 1–24; bounded text and offsets. Newest first with ID tie-breaker.
- GET /api/artworks/:id — public metadata and media URLs.
- GET /api/artists/:id — synthetic public artist metadata and public work count.
- GET /media/:id.png — bounded derivative. Add ?download=1 for attachment.

Every artwork/media route requires published AND public. Pending, denied, held, private and unknown IDs return 404. Counts/search/tag discovery exclude those records. No-store caching avoids retaining an old publication response in this prototype. Static routes are explicitly allowlisted; source and hidden files are never served. Writes return 405. This is a fixture repository with visibility boundaries, not a durable database or moderation engine.

Next: durable isolated database/storage and ingestion, approved-gallery derivative pipeline, verified handheld transport, then email/Discord auth, device grants, uploads, moderation state transitions and webhook outbox. No tokens, accounts, OAuth, emails, webhook calls, production deployment, favorites or upload endpoints exist yet. Preserve the eight-hour timer starting only after confirmed initial Discord notification, and explicit hold/deny semantics when those are implemented.

Validation: five Node HTTP test groups pass (pagination/search, visibility across all asset routes, artist counts, PNG structure/decompression, invalid input/static isolation/methods/headers). Browser-tested search, tags, empty results/reset, pagination, artist/detail routes, 8x zoom and tile preview. Two/three/four-column responsive layouts inspected, no page-wide horizontal overflow in measured layouts. The PNG derivative output is tested at 64x64, RGBA8; physical handheld download/import acceptance is still pending. Local preview is retained for user testing; no native build changed in this web increment.

## Community design 0.2.0 (2026-10-15)

Art-first dark/light gallery with persistent local theme preference, pixel-cut subject banners, gentle hover motion (reduced-motion supported), and staggered columns preserving actual canvas proportions. Fixtures now include portrait and landscape PNGs through 96px, with metadata matching encoded dimensions; native remains 0.12.0. Artist portfolio headers and direct artwork pages retain search/tags, pagination, zoom/tile/background and downloads. Local synthetic prototype only. Six HTTP groups pass including rectangular PNG validation; browser checks cover both themes and theme reload persistence.

For explicitly requested Tailscale preview access, set PREVIEW_HOST to this PC's Tailscale IPv4 before starting Node. The default remains loopback. Current mobile preview: http://100.77.28.74:4178/#gallery (phone must be connected to Tailscale). Separate owned process metadata is in build/community-tailscale.*; restart it after server/fixture edits, while frontend files refresh without restart.
