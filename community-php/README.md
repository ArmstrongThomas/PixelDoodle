# Pixel Doodle Community PHP application

PHP 8.3+ / Laravel application for the Pixel Doodle Community website. This
replaces the local Node gallery backend for deployment at
`https://pixels.7db.pw/`. Node modules remain migration references. The public
frontend is supplied as the sibling `community/public` directory in the private
application ZIP. Deploy through the two ZIPs and instructions in
`../docs/SETUP-GUIDE.md`.

Current implementation: gallery reads, Discord OAuth, email-code sign-in,
verified account sessions, explicit moderator roles, artist profiles, validated
PNG ingestion and thumbnails behind a default-off website-upload switch,
owner-only submission history, private review queue and image endpoints, website Approve/Reject/Hold actions, signed Discord HTTP
interactions with moderation buttons/modal, and a scheduled delivery/decision
worker. Queue submissions remain private until approved. Eight-hour autoapproval
starts only after a confirmed Discord message receipt. Version-4 gallery
migrations add profile ownership, submission descriptions/times/idempotency and
worker retry bookkeeping. Device pairing stays separately opt-in.

No real production provider, live Apache host or user account has been tested.
No bot/mail secrets, app key, database or user artwork ship in the archives.

## Run automated checks

From this directory with PHP PDO SQLite, SQLite3 and Sodium loaded:

```powershell
php -d extension=pdo_sqlite -d extension=sqlite3 -d extension=sodium vendor/phpunit/phpunit/phpunit
```

Expected at packaging: 26 tests / 256 assertions. The suite uses isolated
SQLite files, fake mail and mocked Discord HTTP. It never sends a real email or
Discord request. For cross-language PNG fixtures, run
`node scripts/php-png-fixtures.mjs` from the repository root first.
