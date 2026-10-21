# Pixel Doodle Community 0.4.0 — local gallery prototype
## Native community continuation (2026-10-16)

The PHP device API is implemented in the separate `community-php` application.
Native clients support paired Recent, Favorites and My Uploads views, bounded
thumbnail caching and confirmed import of the device-sized derivative. Native
submission metadata, preview and chunked upload UI remain incomplete; device and
hardware acceptance is pending. Older sections below describe earlier milestones.

Run from this folder with Node 24+:

```powershell
npm ci --ignore-scripts
node server.mjs
node --test
```

Default preview: http://127.0.0.1:4178. PORT overrides the port; PREVIEW_HOST optionally selects a specific interface; the default is loopback. No frontend build step. Run npm ci --ignore-scripts for the pinned authentication dependency and complete test suite. Do not expose this prototype as a production service.

Implemented: responsive Explore grid, query/tag filters, pagination, artist profiles, artwork detail, integer zoom, dark/checkerboard background, repeating tile preview and downloadable bounded RGBA PNGs compatible with the handheld community cache. All content is explicitly synthetic. Generated sample art is code-authored in fixtures.mjs; no external artwork or user files are read.

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


## Persistent gallery foundation

Node's built-in SQLite stores metadata and PNG bytes together, with foreign keys,
checked dimensions/statuses, schema version 1 and transactional demo seeding.
No third-party database package or external service is required. The server still
has no HTTP writes, accounts, ingestion or moderation endpoints. This stage only
imports the trusted synthetic fixtures; arbitrary file validation comes next.

From community/, initialize an empty database or explicitly seed an empty demo:

```powershell
node database.mjs init ../runtime/gallery.sqlite
node database.mjs seed-demo ../runtime/gallery.sqlite
$env:GALLERY_DATABASE = (Resolve-Path ../runtime/gallery.sqlite).Path
node server.mjs
```

Omit GALLERY_DATABASE to use in-memory fixtures. Startup never seeds automatically.
Seeding refuses nonempty databases. Unknown schema versions fail instead of
resetting data. Metadata and PNG bytes survive restart; image reads independently
require published/public. Current queries materialize metadata and are a bounded
prototype foundation, not a large production dataset design. Database files and
WAL/SHM sidecars stay in ignored runtime/. No real user data is included in Git.
Nine tests cover persistence, repeated-seed preservation, future-schema refusal,
HTTP visibility and existing gallery behavior. Both current previews share
runtime/community-preview.sqlite with synthetic data only.


## Validated local ingestion (0.4.0)

```powershell
node ingest.mjs ../runtime/gallery.sqlite drawing.png existing-artist-id "Drawing title" landscape
```

The CLI reads a bounded regular file and requires an existing artist. It is an
operator test tool, not account authorization. There is no browser upload or
HTTP write endpoint. Every import receives a random ID, is non-synthetic and
pending/private, and cannot be read through public metadata, full image or
thumbnail routes. No autoapproval or moderation command is implemented yet.

Supported input: static non-interlaced 8-bit RGB/RGBA, gray/gray-alpha, or indexed
PNG; 1–512 pixels per side; at most 2 MiB input and 1024 chunks. CRC, chunk order,
palette indices, transparency, row filters, end markers and exact bounded zlib
output are validated. Packed 1/2/4-bit palettes, 16-bit, interlace, APNG, ICC and
HDR-profile inputs are explicitly rejected. Export standard 8-bit RGB/RGBA for
this first import path. Ancillary metadata is stripped; RGB sample values are
preserved rather than color-managed. Implementation reference:
https://www.w3.org/TR/png-3/ .

Original uploads and filenames are not retained. Clean RGBA PNGs and metadata
are committed with their derivatives in one SQLite transaction. The current
schema limits the canonical full PNG to 1 MiB; a larger canonical file is refused
before writing. Derivatives fit within 128×128, retain aspect ratio (rounded to
whole pixels), use nearest-neighbor center sampling, preserve sampled alpha,
and never upscale. Detail downloads retain full canvas size; the separate
Handheld PNG link downloads the derivative. Gallery cards use /thumbs/:id.png.
Both /media and /thumbs independently require published/public status.

Schema v2 adds a derivative table. v1 migration generates thumbnails atomically
without changing original images; malformed legacy PNGs roll back the migration.
Back up the database before deployment migrations. Sixteen automated test groups
cover validation, exact pixels/alpha, malformed inputs, persistence, failed
migration preservation and all public visibility boundaries. Independent Pillow
decode confirms a 512×256 full image and its 128×64 sampled derivative. No
native/hardware change or GitHub push is part of this increment.

## Device-pairing foundation (internal, not enabled on HTTP)

`pairing.mjs` contains a SQLite-backed state machine inspired by
[RFC 8628](https://www.rfc-editor.org/rfc/rfc8628). It is not a complete OAuth
server or a substitute for maintained web authentication/session components.
The gallery server does not import it and exposes no pairing endpoints yet.

- Display code: eight unambiguous characters, displayed as XXXX-XXXX; ten-minute expiry.
- Private polling credential and eventual bearer token: independent 32-byte CSPRNG values. Only SHA-256 hashes are stored.
- Explicit authenticated approval or denial; inspecting a code does not approve it.
- Five-second polling interval, increasing backoff for early polls. One successful exchange only; a lost token response requires pairing again.
- Allowlisted scopes: gallery:read, favorites:write, artworks:submit. No administrator or identity-provider permissions. Scopes name future APIs, not currently available features.
- Tokens expire after 30 days, can be revoked by their owner, and stop working when account validation returns false. Twenty active devices per account.
- Durable per-subject creation/polling limits and per-account code-attempt limits, bounded pending requests and rate-limit rows. Transport must supply trustworthy subjects, never arbitrary user-provided limiter keys.
- Construction requires a trusted synchronous account validator returning literal true. Account IDs supplied to approve/list/revoke must come from a validated server session. No test account creation path exists in the HTTP server.

Before enabling this flow: integrate maintained Discord/email authentication,
secure host-only cookies, CSRF and origin checks, HTTPS, ingress/global limits,
account lifecycle handling and provider configuration. Never run approval based
on a body-supplied account ID. Handhelds still need verified TLS and independent
DS entropy provisioning. No tokens are printed, sent externally or written into
artwork. This increment is locally tested backend groundwork; the existing
Tailscale gallery stays unchanged.


## Web authentication adapter (not mounted yet)

`auth.mjs` uses pinned Better Auth 1.7.10 with its built-in Node SQLite support.
Use a separate auth database, canonical HTTPS origin, random secret of at least
32 characters, and configured Discord credentials and/or an email-delivery
callback. Missing configuration fails closed. Run its explicit migrate method
before use; never point it at a Collab Doodle or gallery database.

Email sign-in uses six-digit codes, five-minute lifetime, three attempts, hashed
storage and resend rotation. Password sign-in is off. Session cookies are secure,
HttpOnly, SameSite=Lax with a seven-day lifetime. Origin checks and database rate
limiting remain enabled. Account linking is disabled until the explicit linking
and conflict workflow is built; no implicit merging. Discord requests identify
and email only, with encrypted provider token storage. No bots or administrative
permissions. Do not use production credentials with the synthetic tests.

25 tests pass, including actual library handler calls for emailed-code sign-in,
secure cookie flags, rejected replay/foreign origin, Discord authorization URL
scopes and redirect validation. Email is captured only in test memory; no real
emails or Discord calls are sent. npm audit reports no known vulnerabilities for
the current lockfile (not a guarantee of safety). Auth is intentionally not
mounted on the HTTP/Tailscale preview. UI, provider delivery, callback acceptance,
account lifecycle and pairing integration still need implementation/configuration.

Maintain [the hands-on queue](../docs/HANDS-ON-CHECKLIST.md) every increment.
See [the setup guide](../docs/SETUP-GUIDE.md) for the separate OAuth and webhook
instructions, email provider decisions and future Cloudflare Tunnel setup.

## Configured account staging

`serve-auth.mjs` is a separate opt-in launcher, not used by the current preview.
Set AUTH_ORIGIN to the canonical HTTPS origin, AUTH_SECRET to a private random
secret, AUTH_DATABASE to a separate SQLite path, GALLERY_DATABASE to the gallery
DB, and DISCORD_CLIENT_ID / DISCORD_CLIENT_SECRET. Run from community/:

```powershell
node --env-file=.env serve-auth.mjs --migrate
node --env-file=.env serve-auth.mjs
```

It binds 127.0.0.1:4179 (PORT override), intended behind the chosen HTTPS staging
proxy. The proxy must preserve the canonical Host. Do not point a public tunnel
at it until staging access controls and provider setup are ready. HTTP preview
4178 remains gallery-only with an Account unavailable screen. No email delivery
adapter is wired into this launcher yet. Auth HTTP routes are allowlisted;
protected device actions use verified sessions plus exact-origin JSON requests.
Native begin/poll routes are still absent. 27 community tests pass; live consent,
mail delivery and UI interaction after login need the checklist's acceptance run.

## Native pairing transport (disabled by default)

Configured staging can opt in with ENABLE_DEVICE_PAIRING=true after deployment
checks. POST /api/device/pair/begin accepts {label, requestedScopes}; returns
userCode, private deviceCode, expiresIn, interval and verificationUri. POST
/api/device/pair/poll accepts {deviceCode}. Both require JSON <=1024 bytes and
canonical Host; absent Origin is allowed for native clients, foreign Origin is
rejected. Query parameters and additional fields are rejected. No cookies grant
approval here. The account page independently requires a verified session and
explicit approval. Never log request/response bodies for these routes.

Poll responses use HTTP 200 for protocol states (authorization_pending,
slow_down with interval, access_denied, expired_token, device_limit) and success
(accessToken, tokenType, expiresIn, scopes, deviceId). HTTP 429 means peer rate
limit: stop rapid retries. Credentials stay in POST bodies, never URLs. Native
TLS/credential storage integration is still pending. Proxy peers share limits;
do not trust forwarded headers without an explicit ingress trust policy.

Confirmed deployment address: https://pixels.7db.pw. Discord callback:
https://pixels.7db.pw/api/auth/callback/discord. Forward to 127.0.0.1:4179 with
canonical Host. Same host as Collab Doodle, separate service/databases. No live
configuration performed; earlier sections describe historical milestones.
