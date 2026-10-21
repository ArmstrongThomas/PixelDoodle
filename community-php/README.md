# Pixel Doodle Community PHP application

PHP 8.3+ / Laravel application for the Pixel Doodle Community website. This
replaces the local Node gallery backend for deployment at
`https://pixels.7db.pw/`. Node modules remain migration references. The public
frontend is supplied as the sibling `community/public` directory in the private
application ZIP. Deploy through the two ZIPs and instructions in
`../docs/SETUP-GUIDE.md`.

Current implementation: gallery reads, Discord OAuth, email-code sign-in,
verified account sessions, a one-time session-bound arithmetic check before email
OTP requests, explicit moderator roles, artist profiles, validated
PNG ingestion and thumbnails behind a default-off website-upload switch,
owner-only submission history, private review queue and image endpoints, website Approve/Reject/Hold actions, signed Discord HTTP
interactions with moderation buttons/modal, and a scheduled delivery/decision
worker. Queue submissions remain private until approved. Eight-hour autoapproval
starts only after a confirmed Discord message receipt. Version-4 gallery
migrations add profile ownership, submission descriptions/times/idempotency and
worker retry bookkeeping. Version 5 adds account-scoped device favorites and
bounded resumable console-upload sessions/chunks; its migration upgrades gallery
schema versions 0 through 4 transactionally and retains existing artwork.
Device pairing uses short-lived, single-use approval
codes, hashed polling/access secrets, authenticated owner approval, bounded
scopes, 30-day revocable grants and account-disable checks. Native begin/poll
and account manage/approve routes remain gated by
`ENABLE_DEVICE_PAIRING=false` by default. Pairing state lives in the separate
Laravel auth database. The 3DS client uses libctru HTTPS with certificate
verification enabled. An NDS/DSi client candidate now uses the sibling
Doodle-NDS Mbed TLS and pinned-root approach plus a separate, provisioned
Pixel Doodle seed. Build and hardware TLS/pairing checks remain pending; keep
the server gate disabled until both DSi and DS Lite pass them.

No real production provider, live Apache host, device TLS handshake, hardware
pairing or user account has been tested. The SD credential file is a bearer
secret protected by physical access controls, not encrypted at rest; revoke it
from the website if the card is copied or lost.
No bot/mail secrets, app key, database or user artwork ship in the archives.

## Run automated checks

From this directory with PHP PDO SQLite, SQLite3 and Sodium loaded:

```powershell
php -d extension=pdo_sqlite -d extension=sqlite3 -d extension=sodium vendor/phpunit/phpunit/phpunit
```

The suite uses isolated
SQLite files, fake mail and mocked Discord HTTP. It never sends a real email or
Discord request. For cross-language PNG fixtures, run
`node scripts/php-png-fixtures.mjs` from the repository root first.

## Handheld community API (local implementation)

Pairing grants stay scoped to the permissions the account owner approved.
gallery:read, favorites:write, and artworks:submit are checked on each
request. Revoke a device from Account settings or send POST
/api/device/revoke with that device's bearer token. A missing/expired scope is
an authorization error; clients must ask the owner to pair again with the
requested permission. Public gallery reads remain anonymous and expose only
published/public artwork.

GET /api/device/favorites?limit=12&offset=0 returns only currently
published/public favorites. POST /api/device/favorites/{artworkId} accepts
{"favorite":true|false} and is safe to retry. Private, pending, held, denied
and missing artwork cannot be added.

Console submissions use the authenticated /api/device/uploads routes. Begin
binds a client request UUID, metadata, expected byte count and SHA-256 to the
verified account's existing artist profile. The flattened PNG is limited to
2 MiB; it transfers in 32 KiB base64 JSON chunks, each independently
SHA-256-checked. Upload IDs are owner-scoped. Duplicate identical chunks and
finalize requests are safe to retry; conflicting retries fail. Status returns
received/missing chunk numbers, and cancel deletes an unfinished session.
Sessions expire after 24 hours. Limits are five active uploads per account,
128 active uploads and 256 MiB of declared active bytes globally. Finalize
checks the complete digest and passes the PNG to the existing canonical
decoder, derivative builder, quota and private moderation queue. Temporary
chunks are removed after finalize. Owner history is available through
GET /api/device/uploads; the existing website Studio page shows its status
and review reason. A verified artist profile is required before upload.

The NDS pairing adapter remains a cooperative state machine. Both native clients
now connect their Community UI to recent gallery, favorites and My Uploads reads,
plus favorite toggles and confirmed import of a device-sized PNG derivative.
The 3DS TLS/HTTP client exposes bounded bearer GET/POST calls with authenticated
redirects rejected; its owned worker handles pairing and community requests.
Native submission metadata/preview and chunked upload UI remain unimplemented.
Do not treat API tests or a successful build as device-to-moderation acceptance.