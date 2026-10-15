import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdtempSync,rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join} from 'node:path';
import {DatabaseSync} from 'node:sqlite';
import {openRepository} from '../repository.mjs';
import {createServer} from '../server.mjs';

test('durable demo metadata and exact PNG bytes survive close/reopen; seeding is never implicit',()=>{
 const dir=mkdtempSync(join(tmpdir(),'pd-gallery-'));const path=join(dir,'gallery.sqlite');let repo;
 try{repo=openRepository(path);assert.equal(repo.snapshot().artworks.length,0);repo.seedDemo();const before=repo.snapshot(),image=repo.image('study-2');assert.equal(before.artworks.length,16);repo.close();repo=openRepository(path);assert.deepEqual(repo.snapshot(),before);assert.deepEqual(repo.image('study-2'),image);assert.throws(()=>repo.seedDemo(),/empty database/);assert.deepEqual(repo.snapshot(),before);assert.equal(repo.image('hidden-pending'),null);assert.equal(repo.image('hidden-private'),null);}finally{repo?.close();rmSync(dir,{recursive:true,force:true});}
});

test('database rejects unsupported schema without replacing existing data',()=>{
 const dir=mkdtempSync(join(tmpdir(),'pd-gallery-'));const path=join(dir,'future.sqlite');
 try{const db=new DatabaseSync(path);db.exec('CREATE TABLE future(value TEXT); INSERT INTO future VALUES (\'preserve\'); PRAGMA user_version=99;');db.close();assert.throws(()=>openRepository(path),/Unsupported/);const check=new DatabaseSync(path);assert.equal(check.prepare('SELECT value FROM future').get().value,'preserve');check.close();}finally{rmSync(dir,{recursive:true,force:true});}
});

test('durable public HTTP API keeps hidden metadata, counts and media private',async()=>{
 const repo=openRepository(':memory:');repo.seedDemo();const server=createServer(repo);await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));const base=`http://127.0.0.1:${server.address().port}`;
 try{const gallery=await(await fetch(base+'/api/gallery?limit=24')).json();assert.equal(gallery.total,12);assert.equal(gallery.items.length,12);assert.equal((await(await fetch(base+'/api/artists/demo-studio')).json()).count,6);for(const id of ['hidden-pending','hidden-denied','hidden-held','hidden-private'])for(const path of [`/api/artworks/${id}`,`/media/${id}.png`])assert.equal((await fetch(base+path)).status,404);const png=Buffer.from(await(await fetch(base+'/media/study-2.png')).arrayBuffer());assert.equal(png.readUInt32BE(16),64);assert.equal(png.readUInt32BE(20),96);assert.equal((await fetch(base+'/api/gallery',{method:'POST'})).status,405);}finally{await new Promise(resolve=>server.close(resolve));repo.close();}
});
