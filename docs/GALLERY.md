# Companion gallery and sync

The [agreed roadmap](ROADMAP.md) supersedes historical invite-only accounts, deferred handheld uploads and replay-first priorities below. It includes email/Discord pairing, console browsing and eight-hour moderation.

Proposed architecture, not a deployment. `pixels.7db.pw` is a candidate hostname; availability, DNS, proxy rules and server capacity have not been inspected or changed.

## Local website prototype: Community web 0.1.0

community/ now contains a loopback-only read-only Node service and responsive gallery. It supports synthetic public search/tags/pagination, artist pages, artwork detail with zoom/tile/background, and 64-square PNG downloads usable in the native cache. Explicit published/public filtering covers metadata and media routes. Local fixtures are neither user accounts nor a database. See ../community/README.md. No online hosting, authentication, upload, webhook or native network integration exists yet. The next step is durable isolated storage and a validated publication/derivative pipeline before real account and upload flows.

## Implemented foundation: 0.12.0

The handheld Community entry browses manually supplied PNGs under the app's `community` folder. Six bounded thumbnails, paging, selected-art preview, refresh and confirmed import remain available offline. The Pair screen on 3DS and NDS/DSi begins the PHP one-time device flow over verified HTTPS, displays the website approval code, polls with server backoff and stores a validated, gallery-read scoped, revocable token in alternating app-private credential slots. NDS/DSi uses the TLS 1.2/Mbed TLS and Mozilla CA pattern from Doodle-NDS with a separate Pixel Doodle seed. Network work runs only while that screen is open; drawing and stylus paths never call it. This is a candidate awaiting NDS hardware acceptance; no upload, favorite operation or production rollout is enabled. The token is a bearer secret stored without at-rest encryption, so revoke it on the website if the SD card is lost or copied.

Provision a unique seed for each NDS/DSi card at `<PixelDoodle app-data>/private/seed.bin` with `scripts/provision-nds-seed.ps1`; never clone that folder to another device or use Collab Doodle's seed. Keep pairing default-off until certificate rejection, approval/denial/expiry and revocation are accepted on both DSi and DS Lite. Keep web uploads disabled; build console-only submission over an explicitly granted device identity. The agreed roadmap's moderation timer/hold/notification requirements still govern eventual publishing.

## Experience

Mobile-first gallery with crisp nearest-neighbor thumbnails, quiet neutral backgrounds, clear pixel dimensions, author and playback badge. Public Explore supports recent art, tags and search; artist pages collect published works. Artwork detail offers fit/1x/integer zoom, checkerboard/background switch, seamless tile preview, and a replay scrubber with Play/Pause, speed and skip-idle options. Still art loads before the optional player. Keyboard/touch controls, accessible labels, reduced-motion handling and no autoplay are baseline requirements.

My Library shows private drafts and published revisions with unmistakable visibility badges. Owner actions: upload, rename, tag, organize collections, download original, choose cover, publish/unpublish, choose whether project/replay downloads are allowed, delete and restore during a documented trash period. Publication preview shows precisely which still, replay and project revision will be exposed. Start with invite-only accounts, no comments/messages or anonymous uploads.

## Isolated service on the same host

Create a separate Node service/process and own SQLite database, storage root, backup policy, credentials and host-only cookie namespace. Use a distinct loopback port selected after deployment inventory, behind a separate hostname route. Existing Doodle processes, database, ports, updater and identities remain unchanged. Do not reuse Doodle moderation endpoints as a gallery API. Keeping the same physical server is an operational choice, not shared application state.

Use content blobs outside the public web root, immutable project revisions and explicit publication records. Separate gallery API from a bounded job worker for parsing, thumbnails, replay checkpoints and video. Initial single-worker concurrency, byte/pixel/time quotas and CPU/memory limits must be measured on the actual host before deployment; videos must not stall Doodle. If isolation/capacity cannot be achieved, move the worker or defer video export. Do not install or restart anything in this planning phase.

Suggested entities: Account, DeviceGrant, Project, Revision(parent, hash), Asset(type, bytes), Publication(revision, visibility, permissions), UploadSession, RenderJob and DeletionRecord. Owner account is authorization metadata, not embedded as trusted information in an uploaded file.

## Access controls

| Resource/action | Anonymous | Authenticated non-owner | Owner |
|---|---|---|---|
| Published still and opted-in replay | Read | Read | Manage |
| Private project, thumbnail, replay, checkpoints | Deny | Deny | Read/write |
| Original layered download | Only if explicitly published for download | Same | Read |
| Publish, unpublish, delete, change permissions | Deny | Deny | Allowed |
| Render job status/output | Deny unless separately published output | Deny | Allowed |

Every asset request, including thumbnails, range requests, checkpoints and video, enforces project/revision visibility server-side. IDs and obscure URLs are not authorization. Private responses use private/no-store caching and bypass shared caches; private media gets no social-card URLs. Owner access is not end-to-end encryption: service operators/backups can access stored data. Administrative access, if added, is audited and tightly scoped.

Start without unlisted links to keep public/private unambiguous. Unpublishing revokes origin access and purges caches; it cannot retract prior downloads. Avoid long-lived public immutable media URLs where visibility must change; check publication state or use short-lived access grants and documented cache lifetimes.

Browser authentication uses a maintained session/auth implementation with HttpOnly Secure host-only cookies, SameSite and CSRF protection for writes; choose and audit dependency versions during implementation. No tokens in art files, URLs or logs. Devices pair through an expiring one-time code confirmed in the authenticated phone browser, with rate limiting, explicit device approval, scoped revocable device tokens and no Doodle identity reuse. Do not implement a new password crypto scheme.

NDS/DSi pairing is implemented using the sibling Doodle-NDS Mbed TLS 3.6.7 client, pinned Mozilla roots and a unique, host-provisioned Pixel Doodle seed. The pinned BlocksDS build now passes, but hardware TLS/pairing checks remain pending. The 3DS candidate builds and boots in Azahar; its pairing request did not complete there, and Azahar logs report `httpcAddDefaultCert` is stubbed, so hardware TLS acceptance remains required. Do not substitute `rand()`, a Collab Doodle seed, or plaintext HTTP. The 3DS candidate uses libctru HTTPS and leaves certificate verification enabled. Both platforms must pass certificate failure, approval, denial, expiry and revocation tests before pairing is considered accepted.

## Upload and conflict model

First useful website version accepts manual PNG/project/recording upload from a phone or PC. Native sync later explicitly uploads the last successfully saved immutable revision. Queue only identifiers and local file references; never block pencil input on a request. Start networking in the library/sync screen so DNS/TLS pauses cannot interrupt drawing.

Use authenticated bounded upload sessions, fixed-size resumable chunks, full-file hash verification and idempotent finalization. Enforce per-account bytes, dimensions, expanded tile/replay bytes, operation count, checkpoint count and render time; reject malformed files before publication. Quarantine uploads, use a process-isolated parser, no client-provided filesystem paths, and no executable/HTML/SVG uploads. Thumbnails are generated from validated data. Sanitize titles and serve downloadable originals with appropriate content type/disposition.

Each save creates a revision referencing its parent. Uploads include the expected remote parent (ETag/If-Match semantics); conflict creates a named alternate revision, not last-writer-wins overwrite or automatic pixel merge. Download into a temporary generation, validate and confirm before replacing local working state. One device remains usable offline indefinitely. Sync deletion is explicit and distinct from deleting a local copy.

Recovery: transactional metadata commits, blob reference checks and garbage collection grace periods, SQLite online backups plus consistent blob manifests, encrypted/private operational backup access and a restore drill. Published deletions remove derivatives, pending jobs and storage per retention policy; disclose backup-retention delay. No backup is placed in a public client release.

## Acceptance before any live rollout

Use local/staging synthetic accounts and art. Test owner/non-owner/anonymous access across all asset variants; guessed IDs, path traversal, corrupt compressed data, cross-account uploads, CSRF, quota exhaustion, expired/revoked pairing, interrupted/resumed uploads, duplicate finalization, conflict copies, unpublish/cache behavior and account deletion. Test mobile layout and replay cancellation with large projects. Compare API/worker load to an isolated synthetic Doodle baseline without touching production. Deploy only after the user later requests it and a concrete isolated configuration and rollback plan are prepared.
