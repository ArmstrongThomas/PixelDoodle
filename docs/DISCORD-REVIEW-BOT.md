# Discord review delivery

The review bot uses a Laravel scheduled command, Discord REST v10 and signed
HTTP interactions at `/api/discord/interactions`. The details, credentials,
roles, least-required bot permissions, Windows scheduler setup and live
acceptance steps are in `docs/SETUP-GUIDE.md` and `docs/HANDS-ON-CHECKLIST.md`.

Both the website and Discord call the same moderation service. Button IDs carry
only artwork ID, decision and revision; Discord Ed25519 signatures, application,
guild, channel, linked website moderator, and configured role are checked before
mutation. Idempotent revision checks reject stale/duplicate decisions. Reject
opens a modal with an optional reason. User descriptions are rendered in an
embed with mentions disabled. The queue worker never sends a second message
when a send may already have succeeded; reconcile uncertain cases manually.
The eight-hour clock begins only at an accepted Discord message receipt.

Outbound bot requests and interaction handling are covered with mocked HTTP and
ephemeral signing keys in `tests/Feature/DiscordReviewTest.php`. No bot account or
live destination is configured in this repository.
