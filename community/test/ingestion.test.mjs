import test from 'node:test';import assert from 'node:assert/strict';import {deflateSync} from 'node:zlib';
import {decodePNG,encodePNG,thumbnail,prepareArtwork,chunk,MAX_PNG_BYTES} from '../png.mjs';
import {openRepository} from '../repository.mjs';import {createServer} from '../server.mjs';import {DatabaseSync} from 'node:sqlite';
import {mkdtempSync,rmSync} from 'node:fs';import {join} from 'node:path';import {tmpdir} from 'node:os';
const sig=Buffer.from([137,80,78,71,13,10,26,10]);
function sample({w=2,h=1,type=6,depth=8,raw=Buffer.from([0,10,20,30,0,40,50,60,128]),before=[],after=[],interlace=0}={}){const hdr=Buffer.alloc(13);hdr.writeUInt32BE(w);hdr.writeUInt32BE(h,4);hdr[8]=depth;hdr[9]=type;hdr[12]=interlace;return Buffer.concat([sig,chunk('IHDR',hdr),...before,chunk('IDAT',deflateSync(raw)),...after,chunk('IEND',Buffer.alloc(0))]);}
test('RGBA samples retain exact alpha/color; ancillary metadata removed',()=>{const b=sample({before:[chunk('tEXt',Buffer.from('private\0discard me'))]});const a=prepareArtwork(b);assert.deepEqual([...decodePNG(a.image).rgba],[10,20,30,0,40,50,60,128]);assert.equal(a.image.includes(Buffer.from('discard me')),false);assert.deepEqual(decodePNG(a.thumbnail).rgba,decodePNG(a.image).rgba);});
test('RGB, grayscale, grayscale-alpha, indexed transparency convert exactly',()=>{
 assert.deepEqual([...decodePNG(sample({type:2,raw:Buffer.from([0,1,2,3,4,5,6])})).rgba],[1,2,3,255,4,5,6,255]);
 assert.deepEqual([...decodePNG(sample({type:0,raw:Buffer.from([0,10,20]),before:[chunk('tRNS',Buffer.from([0,10]))]})).rgba],[10,10,10,0,20,20,20,255]);
 assert.deepEqual([...decodePNG(sample({type:4,raw:Buffer.from([0,10,0,20,127])})).rgba],[10,10,10,0,20,20,20,127]);
 assert.deepEqual([...decodePNG(sample({type:3,raw:Buffer.from([0,0,1]),before:[chunk('PLTE',Buffer.from([1,2,3,4,5,6])),chunk('tRNS',Buffer.from([0,128]))]})).rgba],[1,2,3,0,4,5,6,128]);
});
test('all five row filters independently reconstructed',()=>{
 const pixels=Buffer.from(Array.from({length:3*5*4},(_,i)=>(i*37)%256));const raw=[];
 function paeth(a,b,c){const p=a+b-c;const scores=[Math.abs(p-a),Math.abs(p-b),Math.abs(p-c)];return [a,b,c][scores.indexOf(Math.min(...scores))];}
 for(let y=0;y<5;y++){raw.push(y);for(let x=0;x<12;x++){const a=x>=4?pixels[y*12+x-4]:0,b=y?pixels[(y-1)*12+x]:0,c=y&&x>=4?pixels[(y-1)*12+x-4]:0;raw.push((pixels[y*12+x]-[0,a,b,Math.floor((a+b)/2),paeth(a,b,c)][y]+256)%256);}}
 assert.deepEqual(decodePNG(sample({w:3,h:5,raw:Buffer.from(raw)})).rgba,pixels);
});
test('thumbnail samples source pixels without interpolation or alpha blending',()=>{
 const rgba=Buffer.alloc(512*256*4);for(let y=0;y<256;y++)for(let x=0;x<512;x++){const at=(y*512+x)*4;rgba.set([x%256,y,99,(x+y)%256],at);}
 const a=thumbnail({width:512,height:256,rgba});assert.deepEqual([a.width,a.height],[128,64]);for(let y=0;y<a.height;y++)for(let x=0;x<a.width;x++)assert.deepEqual([...a.rgba.subarray((y*128+x)*4,(y*128+x)*4+4)],[(x*4+2)%256,y*4+2,99,(x*4+y*4+4)%256]);
 assert.deepEqual([thumbnail({width:1,height:512,rgba:Buffer.alloc(2048)}).width,thumbnail({width:1,height:512,rgba:Buffer.alloc(2048)}).height],[1,128]);
 assert.equal(decodePNG(encodePNG(a)).width,128);
});
test('reject malformed, oversized, unsupported and ambiguous PNGs',()=>{
 const crc=sample();crc[29]^=1;
 for(const b of [crc,sample().subarray(0,-3),Buffer.concat([sample(),Buffer.from([0])]),Buffer.alloc(MAX_PNG_BYTES+1),sample({w:513}),sample({w:0}),sample({depth:16}),sample({interlace:1}),sample({before:[chunk('acTL',Buffer.alloc(8))]}),sample({before:[chunk('iCCP',Buffer.from([0]))]}),sample({before:[chunk('ABCD',Buffer.alloc(0))]}),sample({raw:Buffer.alloc(10000)}),sample({raw:Buffer.from([5,1,2,3,4,5,6,7,8])}),sample({type:3,raw:Buffer.from([0,0,2]),before:[chunk('PLTE',Buffer.from([1,2,3]))]})])assert.throws(()=>decodePNG(b));
});
test('ingestion is pending/private, survives restart, and rejected imports leave no rows',async()=>{
 const dir=mkdtempSync(join(tmpdir(),'pd-ingest-'));const path=join(dir,'gallery.sqlite');let repo,server;
 try{repo=openRepository(path);repo.seedDemo();const before=repo.snapshot().artworks.length;assert.throws(()=>repo.ingest(sample(),{artist:'missing',title:'No'}));assert.throws(()=>repo.ingest(sample(),{artist:'demo-studio',title:'bad\nname'}));assert.throws(()=>repo.ingest(Buffer.from('bad'),{artist:'demo-studio',title:'No'}));assert.equal(repo.snapshot().artworks.length,before);
 const a=repo.ingest(sample(),{artist:'demo-studio',title:' Real work ',tags:['test','test']});assert.equal(a.status,'pending');assert.equal(repo.image(a.id),null);assert.equal(repo.image(a.id,'thumbnail'),null);repo.close();repo=openRepository(path);const saved=repo.snapshot().artworks.find(v=>v.id===a.id);assert.equal(saved.title,'Real work');assert.deepEqual(saved.tags,['test']);assert.equal(saved.synthetic,false);
 server=createServer(repo);await new Promise(r=>server.listen(0,'127.0.0.1',r));const base=`http://127.0.0.1:${server.address().port}`;
 for(const id of [a.id,'hidden-pending','hidden-denied','hidden-held','hidden-private'])for(const route of ['media','thumbs'])assert.equal((await fetch(`${base}/${route}/${id}.png`)).status,404);
 assert.equal((await(await fetch(base+'/api/gallery')).json()).total,12);assert.equal((await fetch(base+'/thumbs/study-1.png')).status,200);
 }finally{if(server)await new Promise(r=>server.close(r));repo?.close();rmSync(dir,{recursive:true,force:true});}
});
test('v1 migration creates derivatives without changing originals; failure rolls back',()=>{
 for(const valid of [true,false]){const dir=mkdtempSync(join(tmpdir(),'pd-migrate-'));const path=join(dir,'v1.sqlite');try{
 let repo=openRepository(path);repo.seedDemo();const before=repo.image('study-1');repo.close();let db=new DatabaseSync(path);db.exec('DROP TABLE moderation_audit; DROP TABLE moderation_queue; DROP TABLE derivatives; PRAGMA user_version=1;');if(!valid)db.prepare('UPDATE artworks SET image=? WHERE id=?').run(Buffer.from('bad'),'study-1');db.close();
 if(valid){repo=openRepository(path);assert.deepEqual(repo.image('study-1'),before);assert.equal(decodePNG(repo.image('study-1','thumbnail')).width,64);repo.close();}else{assert.throws(()=>openRepository(path));db=new DatabaseSync(path);assert.equal(db.prepare('PRAGMA user_version').get().user_version,1);assert.equal(db.prepare("SELECT count(*) AS n FROM sqlite_master WHERE name='derivatives'").get().n,0);db.close();}
 }finally{rmSync(dir,{recursive:true,force:true});}}
});

