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
  Console upload acceptance remains pending implementation and hardware testing.

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

- [ ] Turn on pairing only after client HTTPS/key storage are ready; test native
  begin/poll/approval/revoke, origin/CSRF and rate limits.
- [ ] Configure backups for both private databases and `.env`; test a restore,
  including SQLite WAL contents. Confirm service account permissions and restart.
- [ ] Ensure Collab Doodle remains healthy; its code, DB and vhost are separate.

Local automated checks: PHP 26 tests / 256 assertions; Node reference 33 groups.
Browser checks used synthetic local-only accounts/artwork. Actual server, mail,
OAuth credentials, Discord portal, uploaded user artwork and hardware tests remain
pending. No live email/Discord message was sent and nothing deployed.
