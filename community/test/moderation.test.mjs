import test from 'node:test';import assert from 'node:assert/strict';
import {openRepository} from '../repository.mjs';
import {DatabaseSync} from 'node:sqlite';import {mkdtempSync,rmSync} from 'node:fs';import {tmpdir} from 'node:os';import {join} from 'node:path';
function setup(){const repo=openRepository(':memory:');repo.seedDemo();let at=100000;const mod=repo.moderation({isModerator:id=>id==='staff',now:()=>at});const add=()=>repo.ingest(repo.image('study-1'),{artist:'demo-studio',title:'Review test'}).id;return {repo,mod,add,advance:ms=>{at+=ms;},review:id=>mod.list('staff').find(r=>r.artwork===id)};}
test('v2 migration backfills real pending work and decisions/audit survive restart',()=>{
 const dir=mkdtempSync(join(tmpdir(),'pd-review-')),path=join(dir,'gallery.sqlite');let repo;
 try{repo=openRepository(path);repo.seedDemo();const item=repo.ingest(repo.image('study-1'),{artist:'demo-studio',title:'Migrated'});repo.close();repo=null;
 let db=new DatabaseSync(path);db.exec('DROP TABLE moderation_audit; DROP TABLE moderation_queue; PRAGMA user_version=2;');db.close();
 repo=openRepository(path);let mod=repo.moderation({isModerator:()=>true});assert.equal(mod.list('staff').length,1);const c=mod.claim();repo.close();repo=null;
 repo=openRepository(path);mod=repo.moderation({isModerator:()=>true});assert.equal(mod.recoverInterrupted(),1);assert.equal(mod.delivered(item.id,c.attempt,'123'),false);mod.decide('staff',item.id,'deny',1,'Not suitable');repo.close();repo=null;
 db=new DatabaseSync(path);assert.equal(db.prepare('PRAGMA user_version').get().user_version,3);assert.equal(db.prepare('SELECT status FROM artworks WHERE id=?').get(item.id).status,'denied');assert.deepEqual(db.prepare('SELECT action FROM moderation_audit ORDER BY id').all().map(r=>r.action),['interrupted','deny']);db.close();
 }finally{repo?.close();rmSync(dir,{recursive:true,force:true});}
});
test('moderation requires a trusted role and stale reviews cannot overwrite decisions',()=>{
 const t=setup();try{const id=t.add();assert.throws(()=>t.mod.list('visitor'),/unauthorized/);assert.throws(()=>t.mod.decide('visitor',id,'approve',0),/unauthorized/);
 assert.throws(()=>t.repo.moderation({isModerator:()=>1}).list('staff'),/unauthorized/);
 assert.equal(t.repo.image(id),null);assert.equal(t.review(id).notification,'queued');
 t.mod.decide('staff',id,'hold',0,'Needs another look');assert.equal(t.review(id).status,'held');assert.equal(t.repo.image(id),null);
 assert.throws(()=>t.mod.decide('staff',id,'approve',0),/stale_review/);
 t.mod.decide('staff',id,'approve',1);assert.ok(t.repo.image(id));assert.ok(t.repo.image(id,'thumbnail'));
 assert.throws(()=>t.mod.decide('staff',id,'deny',2),/stale_review/);
 }finally{t.repo.close();}
});
test('eight-hour autoapproval starts at confirmed delivery, not submission or send attempt',()=>{
 const t=setup();try{const id=t.add();t.advance(9*3600000);assert.equal(t.mod.autoapprove(),0);const c=t.mod.claim();assert.equal(c.artwork,id);assert.equal(t.mod.claim(),null);
 t.advance(9*3600000);assert.equal(t.mod.autoapprove(),0);assert.equal(t.mod.delivered(id,'wrong','123'),false);assert.equal(t.mod.delivered(id,c.attempt,'123'),true);
 assert.equal(t.mod.delivered(id,c.attempt,'123'),false);t.advance(8*3600000-1);assert.equal(t.mod.autoapprove(),0);assert.equal(t.repo.image(id),null);
 t.advance(1);assert.equal(t.mod.autoapprove(),1);assert.ok(t.repo.image(id));assert.equal(t.mod.autoapprove(),0);
 }finally{t.repo.close();}
});
test('uncertain/interrupted sends, hold and denial never auto-publish',()=>{
 const t=setup();try{
 const uncertain=t.add(),c=t.mod.claim();t.mod.uncertain(uncertain,c.attempt);assert.equal(t.mod.delivered(uncertain,c.attempt,'123'),false);
 const interrupted=t.add();t.mod.claim();assert.equal(t.mod.recoverInterrupted(),1);assert.equal(t.review(interrupted).notification,'uncertain');assert.equal(t.mod.claim(),null);
 const held=t.add(),h=t.mod.claim();t.mod.decide('staff',held,'hold',0);t.mod.delivered(held,h.attempt,'234');assert.equal(t.review(held).due_at,null);
 const denied=t.add(),d=t.mod.claim();t.mod.delivered(denied,d.attempt,'345');assert.throws(()=>t.mod.decide('staff',denied,'deny',0),/stale_review/);t.mod.decide('staff',denied,'deny',1,'Optional explanation');
 t.advance(48*3600000);assert.equal(t.mod.autoapprove(),0);for(const id of [uncertain,interrupted,held,denied])assert.equal(t.repo.image(id),null);
 }finally{t.repo.close();}
});
