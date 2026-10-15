CREATE TABLE moderation_queue(artwork TEXT PRIMARY KEY REFERENCES artworks(id),notification TEXT NOT NULL DEFAULT 'queued' CHECK(notification IN ('queued','sending','uncertain','delivered')),attempt TEXT,message_id TEXT,delivered_at INTEGER,due_at INTEGER,revision INTEGER NOT NULL DEFAULT 0);
CREATE TABLE moderation_audit(id INTEGER PRIMARY KEY,artwork TEXT NOT NULL REFERENCES artworks(id),actor TEXT NOT NULL,action TEXT NOT NULL,reason TEXT NOT NULL,at INTEGER NOT NULL);
CREATE INDEX moderation_due ON moderation_queue(due_at);
INSERT INTO moderation_queue(artwork) SELECT id FROM artworks WHERE status='pending' AND synthetic=0;
