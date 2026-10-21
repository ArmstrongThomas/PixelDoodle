## Private server test overlay 0.3.7 (2026-10-21)

`build/PixelDoodle-Community-0.3.7-app-private.zip` is a private local test
overlay for the installed 0.3.6 server. It changes device API authentication
responses: malformed, expired or revoked grants return HTTP 401; a valid grant
missing the required scope returns HTTP 403. This has no schema or configuration
change. The PHP suite passes (36 tests / 318 assertions).

Install by extracting the ZIP over `F:/pixeldoodle-app` and running
`F:/pixeldoodle-app/Update-Community.bat`. Back up the private databases and
preserve `.env`, `APP_KEY`, assets and device grants. The updater runs Laravel
and gallery migrations; the current schema-5 migration is a no-op on an already
updated 0.3.6 installation. The archive contains no public webroot ZIP and was
not deployed. Re-test a revoked/expired grant (401) and a valid grant missing a
scope (403); confirm both clients clear credentials only for 401.

## Server update 0.3.6 (2026-10-16)

PHP fixture suite rerun outside sandbox: 35 tests / 317 assertions pass.
PHP is_readable returned false inside sandbox despite readable autoload.php;
this was not a missing Composer dependency. Server ZIP refresh includes schema 5
and current device APIs. Update-Community.bat clears config/routes/views,
discovers packages and runs Laravel + gallery migrations. No APP_KEY generation
or .env overwrite. Compiled bootstrap caches excluded from archives.
Extract private ZIP into F:/pixeldoodle-app and public ZIP into F:/pixeldoodle.
Back up private data and .env first; run F:/pixeldoodle-app/Update-Community.bat.
Keep existing pairing enabled and browser uploads disabled. No new credentials.
Native submission UI and hardware acceptance are still pending separately.

## Community milestone continuation (2026-10-16, local only)

This increment connects the native UI to the local device API. Both handhelds
now expose Cached, Recent, Favorites and My Uploads modes; remote pages use
bounded parsing and page-sized thumbnail caches. Public items show dimensions
and artist, favorites can be toggled with the scoped bearer API, and My Uploads
shows moderation state and a bounded review reason. Import uses the server's
device-sized PNG derivative and the existing unsaved-art confirmation. Pairing
requests the three community scopes only when the user explicitly pairs or
re-pairs; existing grants are not upgraded silently.

The shared Community dropdown now captures the opening touch, clips rows,
supports swipe and button scrolling, closes on outside tap and dismisses on Back.
Grant responses distinguish 401 invalid/expired/revoked credentials from 403
valid grants missing a scope. Both clients clear credentials only on 401 and
preserve them on 403 while showing explicit re-pair guidance. NDS hostname
resolution now uses bounded nonblocking DNS UDP across frame updates.

Still incomplete: native submission metadata/preview and resumable chunk upload
are not connected to the handheld UI. The server API supports those operations,
but native submission is not end-to-end. Host tests pass, including dropdown,
community mode/relaunch/auth fixtures, DNS packet cases, HTTPS framing/bounds,
131,072 render-oracle comparisons and 16,316 stroke-frame comparisons. PHP
feature tests pass (36 tests / 318 assertions). A fresh 3DSX/CIA build and CIA
metadata check pass. The NDS build was attempted with the pinned BlocksDS image
but Docker could not connect to its Linux engine, so current NDS binary/header
verification remains unconfirmed.

No production deployment/configuration, external notification, GitHub write,
hardware transfer or card write occurred. Hardware TLS/pairing and online flows
remain unaccepted. Keep the existing live pairing setting unchanged.

## DSi pairing candidate (2026-10-21)

The pinned BlocksDS v1.24.0 build now passes. NDS header, ARM bounds and DLDI
verification pass; artifact is `PixelDoodle.nds` at the repository root with
SHA-256 `FD14F6B430EAE0F5A3D721D3EF89F04AFCF6F726594ED041D698CEF7E8BB76FA`.
Docker Desktop's stale AF_UNIX runtime socket directories were preserved by
renaming them to quarantine paths, then Docker Desktop was updated in place to
4.96.0. The Linux engine responds as 29.9.1. No factory reset, Docker data
deletion, WSL unregister or physical device transfer occurred.

Next physical DSi test: start pairing from Menu > Community > Pair. Confirm the
console remains responsive during DNS/TLS setup, displays an approval code,
and reaches Connected after approving it on the website. Then switch among
Recent, Favorites and My Uploads, exercise the mode dropdown (including swipe
scroll), refresh and a favorite toggle, exit and relaunch, and confirm the grant
still works. A revoked grant should prompt re-pair; a valid grant missing a
scope should retain the grant and explain that re-pairing is needed. Basic
drawing/save responsiveness around the network flow should remain intact.
Native artwork submission is not implemented and is outside this test.
## Latest 3DS transport candidate

- [x] Collab Doodle software TLS replaces failed HTTPc transport.
- [x] Host HTTP parser tests and live verified begin/pending poll.
- [x] Live wrong-hostname rejection.
- [ ] New Azahar pairing pass (UI automation failed targeting load dialog).
- [ ] Hardware code/approval/poll/persistence/revocation.
- [x] Move the existing 3DS pairing TLS request off the UI thread into an owned worker.

No hardware transmission tonight, per user request. No server update needed.

# Pixel Doodle Community deployment checklist

## Before server setup

- [ ] PHP/Apache runtime is supported PHP 8.3+ with `pdo_sqlite`, `sqlite3`,
  `openssl`, `curl`, `mbstring`, `fileinfo`, `sodium` and `zlib` enabled in both
  CLI and Apache.
- [ ] Extract `*-public.zip` into `F:\pixeldoodle` and `*-private.zip` into
  `F:\pixeldoodle-app`. Keep `F:\pixeldoodle-data` private.
- [ ] Confirm `F:\pixeldoodle-app\community\public\index.html` exists after
  extraction before visiting the site.
- [ ] Add `deploy/apache.conf` from the public archive to Apache, verify `httpd
  -t`, restart the virtual host on port 8086, and confirm the Cloudflare Tunnel
  reaches `https://pixels.7db.pw/`.
- [ ] Back up the pre-existing website gallery DB and identify its exact path.
  Stop its old writer before schema migration; do not migrate a Collab Doodle DB.
- [ ] Copy `.env.example` to the private `.env`, create/secure data folders,
  preserve a fresh `APP_KEY`, run app/gallery migrations and configure HTTPS.

## Account and publishing checks

- [ ] Register the exact Discord OAuth callback; keep Public Client off; set
  client secret only in private `.env`, then test consent/cancel/logout.
- [ ] Set real SMTP password in private `.env`, enable email sign-in and test
  message delivery/spam, invalid/expired/replayed codes, resend and logout.
- [ ] Create and edit an artist profile. Confirm the studio has no browser upload
  form and clearly says artwork submission is console-only.
- [ ] POST directly to `/api/account/studio/upload`; confirm it returns 404 and
  creates no artwork while `WEB_UPLOADS_ENABLED` is false or unset.
- [ ] Verify existing submission status/reason views and moderation still work.
  Native My Uploads listing is implemented; native console submission is not.

## Discord moderation checks

- [ ] Create a private review channel, bot user and moderator role. Set Discord
  Public Key and application ID, enable `DISCORD_BOT_ENABLED=true`, load Sodium
  in Apache PHP, and clear Laravel config before saving the endpoint URL:
  `https://pixels.7db.pw/api/discord/interactions`.
- [ ] After Discord accepts the endpoint, configure the bot token, guild/channel
  and role IDs in private `.env`; give the bot only Send Messages, Embed Links
  and Attach Files.
- [ ] Grant an existing verified account moderator access; explicitly link its Discord
  user ID with community:link-discord if it previously signed in using email. Test a
  non-moderator separately.
- [ ] Run `php artisan community:review-tick` manually on one private test upload.
  Confirm image, title, description, artist and dimensions in the locked channel.
- [ ] Test Approve, Reject with/without reason, Hold, duplicate/stale buttons,
  channel/guild/role rejection, concurrent website decision, edited message and
  worker restart.
- [ ] Schedule `php artisan schedule:run` in Windows Task Scheduler every minute.
  Test a confirmed post's eight-hour deadline using a staging clock or test DB;
  confirm Hold/Reject cancels its timer and public approval occurs once.
- [ ] Check handling of rate limit, explicit failure and uncertain network timeout;
  uncertain sends must not be resent or start the approval clock automatically.

## Operations

- [ ] Keep `ENABLE_DEVICE_PAIRING=false` in production. In an isolated staging
  database and HTTPS host, first test the current 3DS candidate on hardware:
  reject an untrusted/expired certificate and redirects, begin a pairing code,
  approve and deny from the signed-in website, confirm the one-time grant is
  stored under `sdmc:/3ds/PixelDoodle`, then revoke it from another signed-in
  browser session. Confirm rate limits, expired/replayed codes, disabled
  accounts, and cross-account device revocation. Do not copy the bearer file
  into screenshots, logs or support bundles.
- [ ] On each NDS/DSi card, run `scripts/provision-nds-seed.ps1 -AppDataRoot
  <card>\PixelDoodle` once to provision a unique TLS seed. Never clone that
  private folder to another card or reuse Collab Doodle's seed. Verify that the
  NDS client rejects invalid certificates/hostnames before approving its TLS.
- [ ] After NDS/DSi TLS checks pass, set `ENABLE_DEVICE_PAIRING=true` in the PHP
  server `.env`, clear config cache, then test approval/denial, expiry, grant
  persistence, account disable and revocation separately on DSi and DS Lite.
  Keep pairing default-off until those hardware checks pass.
- [ ] Configure backups for both private databases and `.env`; test a restore,
  including SQLite WAL contents. Confirm service account permissions and restart.
- [ ] Ensure Collab Doodle remains healthy; its code, DB and vhost are separate.

Local automated checks for the 2026-10-16 continuation: host regression suite
passes, including 131,072 composite-oracle comparisons and 16,316 drawing-frame
comparisons; 3DSX/CIA build and CIA identity checks pass (title 000400000CE47800,
version 208). The pinned BlocksDS v1.24.0 NDS ROM built and NDS/DSi header, ARM
bounds and DLDI checks pass. PHP tests previously passed (35 / 317 assertions);
current checkout lacks `community-php/vendor/autoload.php`, preventing a rerun.
Docker required elevated access to its named pipe. All-platform build command:
`powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Target all`.
Azahar booted the 3DSX and reached Community/Pair, but its HTTPS pairing request
failed; emulator logs show default-certificate handling is stubbed. No persistent
device grant was issued. Next hardware checks are TLS rejection and pairing
approval/denial/expiry/revocation on 3DS, DSi and DS Lite. The user enabled
pairing on the live server for testing. Node reference 33 groups. Browser checks
used synthetic local-only accounts/artwork. Live email/Discord, uploaded user art
and other hardware flows remain untested; no deployment or GitHub write occurred.






