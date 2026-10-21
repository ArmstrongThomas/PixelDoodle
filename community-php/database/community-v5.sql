CREATE TABLE device_favorites(
 account TEXT NOT NULL,
 artwork TEXT NOT NULL REFERENCES artworks(id) ON DELETE CASCADE,
 created INTEGER NOT NULL,
 PRIMARY KEY(account,artwork)
);
CREATE INDEX device_favorites_order ON device_favorites(account,created DESC,artwork);

CREATE TABLE device_uploads(
 id TEXT PRIMARY KEY,
 account TEXT NOT NULL,
 artist TEXT NOT NULL REFERENCES artists(id),
 request_id TEXT NOT NULL,
 title TEXT NOT NULL,
 description TEXT NOT NULL,
 tags TEXT NOT NULL CHECK(json_valid(tags)),
 expected_size INTEGER NOT NULL CHECK(expected_size BETWEEN 1 AND 2097152),
 expected_sha256 TEXT NOT NULL CHECK(length(expected_sha256)=64),
 chunk_size INTEGER NOT NULL CHECK(chunk_size=32768),
 chunk_count INTEGER NOT NULL CHECK(chunk_count BETWEEN 1 AND 64),
 created INTEGER NOT NULL,
 expires INTEGER NOT NULL,
 state TEXT NOT NULL CHECK(state IN ('receiving','finalized','failed')),
 artwork TEXT REFERENCES artworks(id),
 UNIQUE(account,request_id)
);
CREATE INDEX device_uploads_account ON device_uploads(account,state,expires);
CREATE TABLE device_upload_chunks(
 upload TEXT NOT NULL REFERENCES device_uploads(id) ON DELETE CASCADE,
 number INTEGER NOT NULL CHECK(number BETWEEN 0 AND 63),
 sha256 TEXT NOT NULL CHECK(length(sha256)=64),
 bytes BLOB NOT NULL CHECK(length(bytes) BETWEEN 1 AND 32768),
 PRIMARY KEY(upload,number)
);
