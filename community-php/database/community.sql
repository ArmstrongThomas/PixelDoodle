CREATE TABLE artist_accounts(account TEXT PRIMARY KEY, artist TEXT NOT NULL UNIQUE REFERENCES artists(id));
CREATE TABLE artwork_details(artwork TEXT PRIMARY KEY REFERENCES artworks(id), description TEXT NOT NULL DEFAULT '', submitted_at INTEGER NOT NULL, request_id TEXT UNIQUE);
ALTER TABLE moderation_queue ADD COLUMN synced_revision INTEGER NOT NULL DEFAULT -1;
ALTER TABLE moderation_queue ADD COLUMN retry_at INTEGER;
ALTER TABLE moderation_queue ADD COLUMN retry_count INTEGER NOT NULL DEFAULT 0;
