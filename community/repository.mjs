/** Isolated gallery storage. No account, upload or moderation API is exposed yet. */
import {randomUUID} from 'node:crypto';
import {moderationSchema,moderationService} from './moderation.mjs';
import {prepareArtwork} from './png.mjs';
import { DatabaseSync } from 'node:sqlite';
import { mkdirSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { artworks, artists, png } from './fixtures.mjs';

export function fixtureRepository() {
  const images=new Map();
  return {
    snapshot:()=>({artworks,artists}),
    image(id,variant='image'){const a=artworks.find(a=>a.id===id&&a.status==='published'&&a.visibility==='public');if(!a)return null;if(!images.has(id))images.set(id,png(a.scene,a.variant,a.width,a.height));const full=images.get(id);return variant==='thumbnail'?prepareArtwork(full).thumbnail:full;},
    close(){}
  };
}

export function openRepository(filename) {
  if(!filename)throw Error('A database path is required');
  if(filename!==':memory:'){filename=resolve(filename);mkdirSync(dirname(filename),{recursive:true});}
  const db=new DatabaseSync(filename);
  try {
    db.exec('PRAGMA foreign_keys=ON; PRAGMA journal_mode=WAL; PRAGMA busy_timeout=5000;');
    const version=db.prepare('PRAGMA user_version').get().user_version;
    if(![0,1,2,3].includes(version))throw Error('Unsupported gallery database version');
    if(version===0)db.exec(`BEGIN IMMEDIATE;
      CREATE TABLE artists(id TEXT PRIMARY KEY, name TEXT NOT NULL, bio TEXT NOT NULL);
      CREATE TABLE artworks(
        id TEXT PRIMARY KEY, artist TEXT NOT NULL REFERENCES artists(id), title TEXT NOT NULL,
        tags TEXT NOT NULL CHECK(json_valid(tags)),
        width INTEGER NOT NULL CHECK(width BETWEEN 1 AND 512), height INTEGER NOT NULL CHECK(height BETWEEN 1 AND 512),
        status TEXT NOT NULL CHECK(status IN ('pending','published','denied','held')),
        visibility TEXT NOT NULL CHECK(visibility IN ('public','private')),
        publishedAt TEXT NOT NULL, synthetic INTEGER NOT NULL CHECK(synthetic IN (0,1)),
        image BLOB NOT NULL CHECK(length(image) BETWEEN 1 AND 1048576)
      );
      CREATE INDEX artwork_public_order ON artworks(status,visibility,publishedAt DESC,id);
      PRAGMA user_version=1; COMMIT;`);
    if(version<2){
      db.exec('BEGIN IMMEDIATE');
      try {
        db.exec(`CREATE TABLE derivatives(artwork TEXT PRIMARY KEY REFERENCES artworks(id) ON DELETE CASCADE, width INTEGER NOT NULL CHECK(width BETWEEN 1 AND 128),height INTEGER NOT NULL CHECK(height BETWEEN 1 AND 128),image BLOB NOT NULL CHECK(length(image) BETWEEN 1 AND 131072));`);
        const insert=db.prepare('INSERT INTO derivatives VALUES (?,?,?,?)');
        for(const row of db.prepare('SELECT id,image FROM artworks').all()){const a=prepareArtwork(Buffer.from(row.image));insert.run(row.id,a.thumbnailWidth,a.thumbnailHeight,a.thumbnail);}
        db.exec('PRAGMA user_version=2; COMMIT;');
      }catch(e){db.exec('ROLLBACK');throw e;}
    }
    if(version<3){db.exec('BEGIN IMMEDIATE');try{db.exec(moderationSchema);db.exec('PRAGMA user_version=3; COMMIT;');}catch(e){db.exec('ROLLBACK');throw e;}}
  } catch(e){db.close();throw e;}
  return {
    moderation(options){return moderationService(db,options);},
    snapshot(){return {
      artists:db.prepare('SELECT id,name,bio FROM artists ORDER BY id').all(),
      artworks:db.prepare('SELECT id,artist,title,tags,width,height,status,visibility,publishedAt,synthetic FROM artworks').all().map(a=>({...a,tags:JSON.parse(a.tags),synthetic:!!a.synthetic}))
    };},
    image(id,variant='image'){const row=db.prepare(variant==='thumbnail'?"SELECT d.image FROM derivatives d JOIN artworks a ON a.id=d.artwork WHERE a.id=? AND a.status='published' AND a.visibility='public'":"SELECT image FROM artworks WHERE id=? AND status='published' AND visibility='public'").get(id);return row?Buffer.from(row.image):null;},
    ingest(bytes,{artist,title,tags=[]}={}){
      if(typeof artist!=='string'||! /^[a-z0-9-]{1,64}$/.test(artist)||typeof title!=='string'||!title.trim()||title.trim().length>120||/[\x00-\x1f\x7f]/.test(title)||!Array.isArray(tags)||tags.length>8||tags.some(t=>typeof t!=='string'||! /^[a-z0-9-]{1,32}$/.test(t)))throw Error('Invalid artwork metadata');
      if(!db.prepare('SELECT id FROM artists WHERE id=?').get(artist))throw Error('Artist does not exist');
      const a=prepareArtwork(bytes);
      if(a.image.length>1048576)throw Error('Canonical PNG exceeds the current 1 MiB storage limit');
      const id=randomUUID();db.exec('BEGIN IMMEDIATE');
      try{
        db.prepare('INSERT INTO artworks VALUES (?,?,?,?,?,?,?,?,?,?,?)').run(id,artist,title.trim(),JSON.stringify([...new Set(tags)]),a.width,a.height,'pending','private','',0,a.image);
        db.prepare('INSERT INTO derivatives VALUES (?,?,?,?)').run(id,a.thumbnailWidth,a.thumbnailHeight,a.thumbnail);
        db.prepare('INSERT INTO moderation_queue(artwork) VALUES (?)').run(id);
        db.exec('COMMIT');
      }catch(e){db.exec('ROLLBACK');throw e;}
      return {id,width:a.width,height:a.height,thumbnailWidth:a.thumbnailWidth,thumbnailHeight:a.thumbnailHeight,status:'pending',visibility:'private'};
    },
    // Explicit local fixture import only. Never seed a database on server startup.
    seedDemo(){
      db.exec('BEGIN IMMEDIATE');
      try {
        if(db.prepare('SELECT count(*) AS n FROM artworks').get().n||db.prepare('SELECT count(*) AS n FROM artists').get().n)throw Error('Demo seed requires an empty database');
        const person=db.prepare('INSERT INTO artists(id,name,bio) VALUES (?,?,?)');
        for(const a of artists)person.run(a.id,a.name,a.bio);
        const art=db.prepare('INSERT INTO artworks(id,artist,title,tags,width,height,status,visibility,publishedAt,synthetic,image) VALUES (?,?,?,?,?,?,?,?,?,?,?)');
        for(const a of artworks)art.run(a.id,a.artist,a.title,JSON.stringify(a.tags),a.width,a.height,a.status,a.visibility,a.publishedAt,1,png(a.scene,a.variant,a.width,a.height));
        const derivative=db.prepare('INSERT INTO derivatives VALUES (?,?,?,?)');
        for(const row of db.prepare('SELECT id,image FROM artworks').all()){const a=prepareArtwork(Buffer.from(row.image));derivative.run(row.id,a.thumbnailWidth,a.thumbnailHeight,a.thumbnail);}
        db.exec('COMMIT');
      }catch(e){db.exec('ROLLBACK');throw e;}
    },
    close(){db.close();}
  };
}
