# Pixel Doodle website installation and setup

The PHP/Apache port is packaged as two ZIP files: the public webroot and the
private Laravel application. Extract the public ZIP to `F:\pixeldoodle` and the
private ZIP to `F:\pixeldoodle-app`. Keep databases, `.env`, logs and credentials
under `F:\pixeldoodle-data`, outside Apache's DocumentRoot.

## 1. Files and requirements

The target is the user's Apache virtual host at `127.0.0.1:8086`, exposed through
the already-configured Cloudflare Tunnel at `https://pixels.7db.pw/`. Use the
provided `apache.conf` and register it from Apache's main configuration. Required
Apache modules: `mod_rewrite`, `mod_headers`, `mod_remoteip`, and mod_php for the
same supported PHP 8.3+ installation Apache will load. PHP needs `pdo_sqlite`,
`sqlite3`, `openssl`, `curl`, `mbstring`, `fileinfo`, `sodium`, and `zlib`.
Restart Apache and check its error log before continuing.

Private app layout after extraction:

```text
F:\pixeldoodle-app\community-php\artisan
F:\pixeldoodle-app\community-php\vendor\
F:\pixeldoodle-app\community\public\   # shared website frontend
F:\pixeldoodle-app\community-php\.env   # create here, never in webroot
F:\pixeldoodle-data\application.sqlite
F:\pixeldoodle-data\gallery.sqlite
```

Before running Artisan or opening the site, verify that
`F:\pixeldoodle-app\community\public\index.html` exists. The private archive's
frontend must remain in the `community\public` folder; if your extracted ZIP
shows a file named `community\public` beside `index.html`, use the corrected
layout archive or restore the `public` folder before continuing.

Set `GALLERY_DATABASE` in `.env` to the pre-existing PHP gallery database when
migrating an existing Node service. Back it up first. For a new gallery, use the
path above. Never point either database setting at a Collab Doodle database.

## 2. Private environment and database

Copy `community-php/.env.example` to `community-php/.env`. Keep `APP_DEBUG=false`.
Set `APP_KEY` to a fresh Laravel application key once; retain it across restarts
and backups. The PHP auth database (`DB_DATABASE`) is separate from the gallery
(`GALLERY_DATABASE`). Create the private data folder first and grant the Apache
service identity write access only to that folder, Laravel `storage` and
`bootstrap/cache`. Do not make the app or webroot writable by visitors.

From `F:\pixeldoodle-app\community-php` run:

```powershell
php artisan package:discover
php artisan key:generate
php artisan migrate --force
php artisan gallery:migrate
php artisan config:clear
```

Only run `key:generate` for the initial blank key. The gallery migrator supports
schema versions 0 through 3 and upgrades them transactionally to version 4. It
refuses a nonempty unversioned database and inserts no synthetic artworks.
Schedule a consistent SQLite backup before migrating an existing gallery.

## 3. Apache and HTTPS

Include `F:/pixeldoodle-app/community-php/deploy/apache.conf` from Apache's main configuration. Its port-8086 virtual host DocumentRoot is
`F:/pixeldoodle`, and only `index.php`, `.htaccess`, `apache.conf` and optional
public static files belong there. The front controller points at the private
Laravel tree. The frontend files are served from the private package's sibling
`community/public` folder. Do not copy `vendor`, `.env`, source, databases or logs
to the public folder.

The virtual host trusts Cloudflare's forwarded visitor IP/scheme only from its
local connector at 127.0.0.1. Leave the proxy on the same host. Check the website
and `/api/gallery`, confirm the address bar stays HTTPS, and inspect Apache logs
for PHP or rewrite errors. Never expose port 8086 directly to the Internet.

## 4. Discord website sign-in

In the Discord Developer Portal, add this exact OAuth redirect URI:

```text
https://pixels.7db.pw/api/auth/callback/discord
```

Keep Public Client off. Website sign-in asks only for `identify` and `email`; do
not use the generated authorize URL as the callback. Put the OAuth client ID and
client secret in the private `.env` (the known application ID is already shown in
the example). Then enable `AUTH_ENABLED=true` and run `php artisan config:clear`.
Test sign-in, cancel, logout and a verified Discord email. Existing accounts are
not silently merged by matching email.

## 5. Email-code sign-in

The server's SMTP values were provided by the user and are set in `.env.example`:
`mail.7db.pw:587`, username/sender `pixeladmin@7db.pw`, sender name
`Pixel Doodle Admin`, seven-day `SESSION_LIFETIME`. Set the real `MAIL_PASSWORD`
only in the private `.env`; it is intentionally blank in the example and ZIP.
Port 587 uses SMTP with STARTTLS; leave `MAIL_SCHEME` unset unless the host
requires a different scheme. Enable `EMAIL_ENABLED=true` after setting the private
password and verify the sender domain/delivery. No live email has been sent by the
build. Codes are hashed, expire after five minutes and allow three attempts.

## 6. Artist profiles, uploads and the gallery

A verified signed-in user can open Your studio and create a display name and bio.
Artwork uploads from the website are disabled by default: the upload form is
hidden, and the browser upload API returns 404. Keep `WEB_UPLOADS_ENABLED=false`
or unset in `.env`. The validation and ingestion path remains in the application
for future use, but the intended submission path is Pixel Doodle on a handheld.
Handheld upload support is still in development; do not promise console uploads
until the device submission flow is implemented and tested. Once an artwork has
been submitted through an enabled trusted path, it remains private while pending
review, and the artist can view its status. Public gallery and image endpoints
return only approved artwork.

A new gallery starts empty, without fixture users/artwork. Visitors can explore
the public gallery and sign in to manage an artist profile. For an existing
account, `php artisan community:moderator user@example.com` grants the moderator
role after email verification and Discord login should use that same account if
Discord button access is desired. `--revoke` removes it. This command does not
create accounts or send messages.

## 7. Discord review bot

The bot is implemented with Discord signed HTTP interactions. It is separate
from website OAuth and needs its own bot token. Create a private review channel.
In the Developer Portal, configure the Interactions Endpoint URL as:

```text
https://pixels.7db.pw/api/discord/interactions
```

Discord verifies the signed endpoint during this step. Copy the application's
Public Key, Application ID, bot token, guild ID, review-channel ID and approved
moderator role IDs into the private `.env` values named `DISCORD_PUBLIC_KEY`,
`DISCORD_CLIENT_ID`, `DISCORD_BOT_TOKEN`, `DISCORD_GUILD_ID`,
`DISCORD_REVIEW_CHANNEL_ID`, and `DISCORD_MODERATOR_ROLE_IDS`. Bot invitation uses
only the `bot` OAuth scope and these channel permissions: Send Messages, Embed
Links and Attach Files. Do not add privileged intents or a generic webhook URL.
Before saving the endpoint, set `DISCORD_BOT_ENABLED=true`, the application ID,
and the application's 64-character Public Key in `.env`, then run
`php artisan config:clear`. The endpoint returns Discord's signed PING only when
that switch is enabled and Sodium is loaded by Apache's PHP. After Discord accepts
the URL, finish the bot token, guild/channel/role values, install the bot in the
private channel, and clear the config cache again. No tokens are present in either ZIP.

Grant moderator access to a verified website account with `community:moderator`.
If that existing account uses email sign-in, link its Discord user ID explicitly
from the server console after verifying the ID with the account owner:

```powershell
php artisan community:link-discord artist@example.com 123456789012345678
php artisan community:moderator artist@example.com
```

Discord Developer Mode lets you copy the user's ID. A Discord ID can belong to
only one website account; `community:link-discord email id --unlink` removes it.
For Discord buttons, the website account must be linked to the clicking Discord
user and that user must hold one of the configured roles. The website review queue
is a fallback and enforces the website moderator role.

Run one manual `php artisan community:review-tick` and check the private channel.
In Windows Task Scheduler create a task that runs as the same Windows identity
Apache uses for private-file access. On the Trigger tab set it to repeat every
one minute indefinitely. On the Action tab choose the full path to the PHP CLI
`php.exe`, arguments `artisan schedule:run`, and Start in
`F:\pixeldoodle-app\community-php`. Set multiple task instances to “Do not start
a new instance”; Laravel also takes a ten-minute database-backed overlap lock.
The scheduler sends at most ten queued review
posts per tick, reconciles uncertain sends without replay, updates changed posts,
and publishes only after a confirmed Discord receipt plus eight hours. A 429 is
retried with bounded delay/attempts; network or ambiguous failures become manual
review. A moderator can immediately approve, reject with an optional reason, or
hold with no timer. Website and Discord decisions share one transaction and audit.

Test with a private submission first: received image/details and buttons, optional
reject reason, role rejection, duplicate/stale click, website decision syncing to
Discord, worker restart, rate-limit response and eight-hour hold/deadline behavior.
Do not run a public test post until the private channel and roles are checked.

## 8. Backups and remaining acceptance

Back up `F:\pixeldoodle-data` and private `.env` separately. Use a SQLite-aware
backup method that handles WAL files, keep copies private, and test restoration.
Never interrupt or reconfigure the existing Collab Doodle virtual host while
setting up this site.

See [HANDS-ON-CHECKLIST.md](HANDS-ON-CHECKLIST.md) for remaining server-side
acceptance, email/Discord secrets and scheduled task setup. HTTP routes and browser
layouts were exercised against local synthetic databases; real Apache, live
provider credentials, actual mail, Discord portal settings and a production
submission remain to be tested on the server.

