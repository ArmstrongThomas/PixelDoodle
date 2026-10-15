# Pixel Doodle PHP backend

Follow ../AGENTS.md and ../docs/HANDS-ON-CHECKLIST.md. PHP/Apache is the selected
production target. The sibling community/ Node modules are migration references,
not a second production service. community/public remains shared frontend source.

Read README.md for private app/public webroot layout. Never place .env, source,
vendor, logs or databases in F:/pixeldoodle. Do not deploy or write to the server
without the user's instruction. Do not send live Discord/email messages in tests.
Use the locked Composer dependencies; no additional scaffolding or frontend build
is needed for the existing gallery. PHP 8.3+, pdo_sqlite required for local tests.
Use APP_DEBUG=false for exposed previews. No authentication or mutation endpoint
may be enabled until its PHP implementation has behavioral regression coverage.
