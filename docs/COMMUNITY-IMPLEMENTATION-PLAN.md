# Complete the handheld community milestone

Owner: dedicated GPT-6 Luna implementation thread. Requested 2026-10-15.
Workspace: G:/dev/3ds/PixelDoodle. Work in this existing checkout; preserve all
uncommitted changes. This plan concerns roadmap slice 8, not animation/replay.
The user explicitly defers DS Lite, DSi and 3DS hardware testing and quirks.
Implement all slices below rather than stopping at the first prototype.

## Baseline and boundaries

Read AGENTS.md (newest entries first), docs/ROADMAP.md, docs/HANDS-ON-CHECKLIST.md,
community-php/README.md and platform adapters. Older GALLERY.md statements about
Node, browser uploads and HTTPc are superseded. PHP/Laravel + Apache is the
production server; retain the colorful current frontend and console-only uploads.
Read the actual routes/services/tests before creating competing implementations.
Existing offline gallery, pairing service, ingestion, derivatives and Discord
moderation should be extended, not rebuilt. Inventory existing functionality and
record concrete gaps first.

The 3DS now uses copied Collab Doodle Mbed TLS + bounded HTTP with JSON POST.
Host tests and live host begin/pending poll and hostname rejection pass. Actual
3DS socket/RNG acceptance is pending; Azahar automation failed targeting its
load dialog. NDS uses its own cooperative Mbed TLS adapter and unique provisioned
seed. Never restore HTTPc, disable certificate checks, use weak entropy, reuse
Doodle identities or clone seed files. Live site is https://pixels.7db.pw/.
Pairing is enabled by the user on production. Existing grants are read-only.

Do not send to hardware, write cards, publish, push, deploy, change production
configuration or create public test art/Discord notifications. Use isolated
local fixtures/services for mutations and end-to-end tests. No production secrets
in logs or archives. No need to ask the sleeping user routine design questions.
Record uncertain hardware behavior honestly and continue independent work.

## 1. Reliable shared community transport and grants

Define a shared bounded request/result interface with platform adapters. Run
3DS networking on an owned worker with bounded queues and lifecycle cleanup;
keep DS cooperative yielding. Cancellation, deadlines, back navigation, sleep/
resume, partial I/O and reconnect must not corrupt UI/project/storage state.
Do not network while drawing or hold the editor hostage to DNS/TLS. Keep limited
memory budgets measured per platform and release buffers when appropriate.

Complete pairing begin/poll contracts on both platforms (audit NDS's obsolete
begin-response scopes requirement), validate bounded JSON structurally, honor
poll interval/expiry, and handle denial, revocation and disabled accounts. Load
credentials for authenticated requests without logging them. Show account/device
status and disconnect/re-pair controls with confirmation. Preserve offline use.
Use existing server scope names gallery:read, favorites:write, artworks:submit.
Request explicit website approval for new scopes; never silently upgrade old
grants. Missing scopes should explain/re-pair, not fail mysteriously.

## 2. Online gallery and safe local downloads

Anonymous approved-art browsing: recent, search/tags, artist filter and paging.
Reuse shared gallery UI with clear Online/Cached modes, square thumbnails and
top-screen selected preview, title/artist/dimensions, loading/error/empty states.
Offer fit/actual-size and zoom/pan within current native UI conventions. Fetch
bounded device-sized derivatives; originals may exceed NTR capacity. Cache
atomically with count/byte limits and eviction that cannot delete user projects.
Reject malformed PNGs, excessive decoded dimensions, unsafe paths and redirects.
Downloads/import require explicit confirmation before replacing a project and
preserve unsaved artwork on cancellation/error. Offline cached art stays usable.

## 3. Favorites and personal submissions

Implement idempotent favorite/unfavorite and paged favorites, scoped to the
verified account/device. Never expose unpublished art through favorites or cached
server derivatives. Add My uploads with pending/published/rejected/held state,
artist-visible moderation reason and useful refresh/retry behavior. Implement
only required additive PHP endpoints; retain website account/device management.
No comments, DMs, feeds of private activity or social feature expansion.

## 4. Console submission from all platforms

Preview and explicitly submit an immutable saved artwork revision with title,
description and bounded tags using the existing keyboard. Explain exactly what
will be shared. Initially submit a flattened PNG; never upload private project,
recovery or credentials. Layered source is separate explicit opt-in only if the
existing ingestion/permissions path supports it safely; otherwise mark deferred.

Use scoped device authorization, bounded chunked/resumable upload sessions,
quotas, digest checking and idempotent finalize. Authenticate ownership on every
chunk/status/finalize route; do not trust platform strings as proof of hardware.
A console-only policy is enforced by grant/API boundaries, not user-agent checks.
Keep the browser upload feature retained but disabled/hidden by default.
Show progress/cancel/retry, recover interrupted uploads without double posting,
and snapshot art before upload so later edits cannot change the submitted bytes.
Stream on NTR; do not allocate whole maximum artwork plus TLS plus decoder copies.
Reuse existing validated ingestion/quarantine/thumbnail pipeline. Rejected input
must not create a public item, notification or abandoned unbounded blobs.

## 5. Integrate moderation and website completion

Reuse existing Discord interaction/review worker; do not create another bot.
A console upload enters the same durable pending queue as other ingestion.
Preserve image/artist/description/buttons, approve/reject optional reason/Hold,
audit trail and authorization. Eight-hour autoapproval starts only after confirmed
initial notification delivery. Uncertain/failed delivery cannot autoapprove;
Hold/reject race safely with timer and retries. Exercise this with mocked Discord.
Confirm website shows approved art and correct owner's statuses, hides upload
entrypoints, supports existing linked email+Discord settings without accidental
account merging, and lets users inspect/revoke devices and requested scopes.
Fix concrete integration gaps needed for these flows, not unrelated redesigns.

## 6. Validation, packaging and handoff

For each slice run meaningful shared C/UI tests and PHP tests. Add transport
fragmentation/cancellation/malformed-data checks, cross-account/scope/revocation
checks, cache capacity and safe import tests, interrupted/duplicate upload and
moderation race tests. Exercise full flow against an isolated test server:
pair -> approve -> browse -> favorite -> submit -> review -> publish -> revoke.
Cover DS/NTR and TWL resource profiles; preserve drawing regression performance.
Build NDS, 3DSX and CIA with existing scripts. Inspect native-size UI and use
Azahar/melonDS when available; automation failure is a limitation, not a pass.
Do not make hardware acceptance a blocker to finishing implementation locally.

Update AGENTS.md, README, roadmap, SETUP-GUIDE and running HANDS-ON-CHECKLIST.
Maintain separate Implemented / Locally verified / Hardware pending states.
Document any server migrations/settings and prepare tested public/private server
ZIPs with the existing post-extraction helper, preserving .env, APP_KEY, DBs,
assets and grants. Verify extracted tree includes real public directories.
Link exact client binaries and archives with hashes; do not deploy them.
Record remaining setup tasks and per-platform hardware test steps for the user.
Animation, timelapse/replay, full cloud sync and unrelated editor tools stay later.

## Reporting

Keep progress and decisions in this file or a linked handoff. Report meaningful
milestones and final results to parent chat 01a13bf3-bd1c-7c81-a19e-68ac783710d8
using send_message_to_thread: the user requested this reporting workflow in the
parent conversation. Include files changed, tests, limitations and hardware queue.
Do not claim complete merely because binaries compile or a request works on PC.
